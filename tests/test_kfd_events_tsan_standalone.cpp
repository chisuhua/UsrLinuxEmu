/*
 * test_kfd_events_tsan_standalone.cpp — C-12 B.4.6.6 TSan hardening
 *
 * Race condition detection for kfd_events_thread_ async path.
 * Exercises:
 *   - Multiple producers enqueueing concurrently
 *   - Drain contract under load
 *   - init/exit ordering
 *   - Memory ordering on atomic counters
 *
 * Build: cmake -DENABLE_TSAN=ON (global TSan instrumentation per B.1.10.7)
 * Run: ./bin/test_kfd_events_tsan_standalone --reporter compact
 */

#include <catch_amalgamated.hpp>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include "kernel/thread/kernel_workqueue.h"
#include "hal/gpu_hal.h"
#include "hal/hal_mock.h"
#include "sim/sim_event.h"

extern "C" {
  int  kfd_events_init(void);
  void kfd_events_exit(void);
  void *kfd_events_get_workqueue(void);
}

static int test_signal_via_hal(void *ctx, uint32_t pasid, uint32_t event_id, uint64_t events) {
  (void)ctx;
  sim_signal_event(pasid, event_id, events);
  return 0;
}

TEST_CASE("tsan: many concurrent producers drain correctly", "[kfd_events][tsan][b46]") {
  REQUIRE(kfd_events_init() == 0);
  struct gpu_hal_ops hal{};
  struct hal_mock_state state{};
  hal_mock_init(&hal, &state);
  hal.event_signal = test_signal_via_hal;

  int baseline = sim_signal_event_count();

  constexpr int kProducers = 4;
  constexpr int kSignalsPerProducer = 50;
  std::vector<std::thread> producers;
  for (int p = 0; p < kProducers; p++) {
    producers.emplace_back([&hal, p]() {
      for (int i = 0; i < kSignalsPerProducer; i++) {
        hal_event_signal(&hal, p + 1, i % 8, 1ULL << (i % 8));
      }
    });
  }
  for (auto &t : producers) t.join();

  /* Drain */
  usr_linux_emu::kernel_workqueue *wq =
      static_cast<usr_linux_emu::kernel_workqueue *>(kfd_events_get_workqueue());
  REQUIRE(wq != nullptr);
  REQUIRE(wq->flush(std::chrono::milliseconds(5000)));

  /* Verify: every signal must have produced exactly one sim_signal_event call */
  int expected = baseline + kProducers * kSignalsPerProducer;
  REQUIRE(sim_signal_event_count() == expected);

  kfd_events_exit();
}

TEST_CASE("tsan: init/exit ordering safe under concurrent signal", "[kfd_events][tsan][b46]") {
  REQUIRE(kfd_events_init() == 0);
  struct gpu_hal_ops hal{};
  struct hal_mock_state state{};
  hal_mock_init(&hal, &state);
  hal.event_signal = test_signal_via_hal;

  std::thread producer([&hal]() {
    for (int i = 0; i < 100; i++) {
      hal_event_signal(&hal, 1, 1, 1);
    }
  });
  producer.join();

  usr_linux_emu::kernel_workqueue *wq =
      static_cast<usr_linux_emu::kernel_workqueue *>(kfd_events_get_workqueue());
  REQUIRE(wq != nullptr);
  REQUIRE(wq->flush(std::chrono::milliseconds(5000)));

  kfd_events_exit();
}

TEST_CASE("tsan: drain contract holds under high enqueue rate", "[kfd_events][tsan][b46]") {
  REQUIRE(kfd_events_init() == 0);
  struct gpu_hal_ops hal{};
  struct hal_mock_state state{};
  hal_mock_init(&hal, &state);
  hal.event_signal = test_signal_via_hal;

  int baseline = sim_signal_event_count();

  /* Enqueue 1000 tasks from a single thread */
  for (int i = 0; i < 1000; i++) {
    hal_event_signal(&hal, 1, 1, 1);
  }

  usr_linux_emu::kernel_workqueue *wq =
      static_cast<usr_linux_emu::kernel_workqueue *>(kfd_events_get_workqueue());
  REQUIRE(wq != nullptr);
  REQUIRE(wq->flush(std::chrono::milliseconds(10000)));
  REQUIRE(sim_signal_event_count() == baseline + 1000);

  kfd_events_exit();
}

TEST_CASE("tsan: atomic counter increment from workqueue context", "[kfd_events][tsan][b46]") {
  REQUIRE(kfd_events_init() == 0);
  struct gpu_hal_ops hal{};
  struct hal_mock_state state{};
  hal_mock_init(&hal, &state);
  hal.event_signal = test_signal_via_hal;

  /* Validate that sim_signal_event_count is incremented atomically
   * (no race on read in test). TSan will report if non-atomic. */
  std::atomic<int> reader_count{0};
  std::thread reader([&reader_count]() {
    for (int i = 0; i < 100; i++) {
      (void)sim_signal_event_count();  /* atomic load */
      reader_count.fetch_add(1, std::memory_order_relaxed);
      std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
  });

  for (int i = 0; i < 100; i++) {
    hal_event_signal(&hal, 1, 1, 1);
  }

  usr_linux_emu::kernel_workqueue *wq =
      static_cast<usr_linux_emu::kernel_workqueue *>(kfd_events_get_workqueue());
  REQUIRE(wq != nullptr);
  REQUIRE(wq->flush(std::chrono::milliseconds(5000)));

  reader.join();
  REQUIRE(reader_count.load() == 100);

  kfd_events_exit();
}