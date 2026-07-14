/*
 * test_kfd_threading_standalone.cpp — C-12 B.1.10 thread infrastructure PoC
 *
 * Verifies ADR-060 (Linux Kernel Message Notification Threading):
 *   - kernel_thread_base: raw pthread_* wrapper, RAII base, GCC 13 workaround
 *   - kernel_workqueue: single worker, drain-on-stop, flush(timeout)
 *
 * Build:
 *   - Default (GCC): baseline build, all tests pass
 *   - Opt-in TSan:   cmake -DENABLE_TSAN=ON (Clang only, B.1.10.7)
 *
 * Coverage (10 TEST_CASEs, B.1.10.6 minimum is 4+):
 *   [thread][base]      - kernel_thread_base lifecycle / idempotency
 *   [workqueue][basic]  - enqueue/execute, FIFO order
 *   [workqueue][drain]  - stop() drains pending tasks (drain contract)
 *   [workqueue][flush]  - flush() timeout semantics
 *   [workqueue][stress] - concurrent producers (mutex correctness)
 *   [workqueue][intro]  - queue_empty / in_flight_empty introspection
 *   [workqueue][life]   - start/stop idempotency
 */

#include <catch_amalgamated.hpp>

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

#include "kernel/thread/kernel_thread_base.h"
#include "kernel/thread/kernel_workqueue.h"

using namespace usr_linux_emu;
using namespace std::chrono_literals;

namespace {

// Concrete thread for testing kernel_thread_base: counts iterations while
// running. Demonstrates the "derived class must call stop() in dtor first"
// pattern from ADR-060 §1.1.
class counting_thread : public kernel_thread_base {
 public:
  counting_thread() = default;
  ~counting_thread() override { stop(); }  // MUST be first, per ADR-060 §1.1

  int count() const { return count_.load(std::memory_order_acquire); }

 private:
  void run() override {
    while (is_running()) {
      count_.fetch_add(1, std::memory_order_acq_rel);
      // std::this_thread::sleep_for is safe (no gthr-default.h weakref).
      std::this_thread::sleep_for(1ms);
    }
  }
  std::atomic<int> count_{0};
};

}  // namespace

// =====================================================================
// kernel_thread_base tests
// =====================================================================

TEST_CASE("kernel_thread_base — start/stop basic lifecycle",
          "[thread][base]") {
  counting_thread t;
  REQUIRE_FALSE(t.is_running());

  t.start();
  REQUIRE(t.is_running());

  std::this_thread::sleep_for(50ms);
  REQUIRE(t.count() > 0);

  t.stop();
  REQUIRE_FALSE(t.is_running());
}

TEST_CASE("kernel_thread_base — start is idempotent",
          "[thread][base]") {
  counting_thread t;
  t.start();
  t.start();  // should be a no-op (CAS sees running_ == true)
  REQUIRE(t.is_running());
  t.stop();
}

TEST_CASE("kernel_thread_base — stop is idempotent",
          "[thread][base]") {
  counting_thread t;
  t.start();
  t.stop();
  t.stop();  // should be a no-op
  REQUIRE_FALSE(t.is_running());
}

TEST_CASE("kernel_thread_base — explicit stop before destruction",
          "[thread][base]") {
  // Verifies the ADR-060 §1.1 pattern: explicit stop() before dtor.
  // (The negative case — destruction without stop — would trip the assert
  // in ~kernel_thread_base; we don't test that path to keep the test binary
  // runnable.)
  counting_thread t;
  t.start();
  t.stop();
  // Destructor runs here without aborting → pattern works.
  SUCCEED("explicit stop before dtor is the correct pattern");
}

// =====================================================================
// kernel_workqueue tests
// =====================================================================

TEST_CASE("kernel_workqueue — enqueue and execute basic",
          "[workqueue][basic]") {
  kernel_workqueue wq;
  wq.start();

  std::atomic<int> counter{0};
  wq.enqueue([&counter] { counter.fetch_add(1); });

  REQUIRE(wq.flush(1000ms));
  REQUIRE(counter.load() == 1);

  wq.stop();
}

TEST_CASE("kernel_workqueue — multiple enqueue drain in order (FIFO)",
          "[workqueue][basic]") {
  kernel_workqueue wq;
  wq.start();

  std::vector<int> results;
  std::mutex results_mtx;
  for (int i = 0; i < 5; ++i) {
    wq.enqueue([i, &results, &results_mtx] {
      std::lock_guard<std::mutex> lock(results_mtx);
      results.push_back(i);
    });
  }

  REQUIRE(wq.flush(2000ms));
  REQUIRE(results.size() == 5);
  // Single worker: tasks run in FIFO order.
  REQUIRE(results == std::vector<int>{0, 1, 2, 3, 4});

  wq.stop();
}

TEST_CASE("kernel_workqueue — stop drains pending tasks (drain contract)",
          "[workqueue][drain]") {
  kernel_workqueue wq;
  wq.start();

  std::atomic<int> counter{0};
  for (int i = 0; i < 100; ++i) {
    wq.enqueue([&counter] {
      counter.fetch_add(1);
      std::this_thread::sleep_for(1ms);  // ensure not all done at stop() time
    });
  }
  // Don't flush; just stop. The drain contract guarantees all 100 run.
  wq.stop();
  REQUIRE(counter.load() == 100);
}

TEST_CASE("kernel_workqueue — flush timeout returns false",
          "[workqueue][flush]") {
  kernel_workqueue wq;
  wq.start();

  // Task sleeps 200ms.
  wq.enqueue([] { std::this_thread::sleep_for(200ms); });

  // 10ms timeout: either task is still queued or in-flight → returns false.
  REQUIRE_FALSE(wq.flush(10ms));

  // 1000ms timeout: task finishes within ~200ms → returns true.
  REQUIRE(wq.flush(1000ms));

  wq.stop();
}

TEST_CASE("kernel_workqueue — concurrent enqueue stress (4 producers)",
          "[workqueue][stress]") {
  kernel_workqueue wq;
  wq.start();

  constexpr int kProducers = 4;
  constexpr int kTasksPerProducer = 250;
  std::atomic<int> counter{0};

  std::vector<std::thread> producers;
  producers.reserve(kProducers);
  for (int p = 0; p < kProducers; ++p) {
    producers.emplace_back([&] {
      for (int i = 0; i < kTasksPerProducer; ++i) {
        wq.enqueue([&counter] { counter.fetch_add(1); });
      }
    });
  }
  for (auto& t : producers) t.join();

  REQUIRE(wq.flush(5000ms));
  REQUIRE(counter.load() == kProducers * kTasksPerProducer);

  wq.stop();
}

TEST_CASE("kernel_workqueue — queue_empty / in_flight_empty introspection",
          "[workqueue][intro]") {
  kernel_workqueue wq;
  wq.start();

  REQUIRE(wq.queue_empty());
  REQUIRE(wq.in_flight_empty());

  std::atomic<bool> task_started{false};
  std::atomic<bool> release_task{false};
  wq.enqueue([&] {
    task_started.store(true, std::memory_order_release);
    while (!release_task.load(std::memory_order_acquire)) {
      std::this_thread::sleep_for(1ms);
    }
  });

  // Wait for the task to start running (up to 1s).
  for (int i = 0; i < 100 && !task_started.load(); ++i) {
    std::this_thread::sleep_for(10ms);
  }
  REQUIRE(task_started.load());
  // Task is now executing: queue empty, in-flight non-empty.
  REQUIRE(wq.queue_empty());
  REQUIRE_FALSE(wq.in_flight_empty());

  // Release the task; it should finish.
  release_task.store(true, std::memory_order_release);
  REQUIRE(wq.flush(2000ms));
  REQUIRE(wq.queue_empty());
  REQUIRE(wq.in_flight_empty());

  wq.stop();
}

TEST_CASE("kernel_workqueue — start/stop idempotency",
          "[workqueue][life]") {
  kernel_workqueue wq;
  wq.start();
  wq.start();  // no-op
  wq.stop();
  wq.stop();   // no-op
  // No assertions — just verifying no crash / hang.
  SUCCEED("start/stop are idempotent");
}
