// include/kernel/thread/kernel_workqueue.h
//
// kernel_workqueue — workqueue that runs enqueued tasks on a worker thread.
//
// Per ADR-060 (Linux Kernel Message Notification Threading for KFD
// Simulation):
//   - Single worker by default (DP-2)
//   - enqueue() is non-blocking
//   - stop() drains queue + waits for in-flight (drain contract)
//   - flush(timeout) returns false on timeout (test hang detection)
//   - Uses std::mutex + std::condition_variable (safe per ADR-060 DP-5)
//
// Thread Safety:
//   - enqueue() is thread-safe (multiple producers OK)
//   - flush() is thread-safe (multiple waiters OK)
//   - start()/stop() are idempotent but NOT thread-safe (call serially)
//   - queue_empty()/in_flight_empty() are thread-safe (introspection)

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>

#include "kernel/thread/kernel_thread_base.h"

namespace usr_linux_emu {

class kernel_workqueue {
 public:
  kernel_workqueue() = default;
  ~kernel_workqueue();

  // Non-copyable, non-movable (owns a thread + mutex).
  kernel_workqueue(const kernel_workqueue&) = delete;
  kernel_workqueue& operator=(const kernel_workqueue&) = delete;
  kernel_workqueue(kernel_workqueue&&) = delete;
  kernel_workqueue& operator=(kernel_workqueue&&) = delete;

  // Spawn the worker thread. Idempotent (no-op on already-started).
  void start();

  // Signal stop and join the worker. Idempotent.
  // Drain contract: pending queue items are executed before stop() returns.
  void stop();

  // Enqueue a task. Non-blocking. The task is executed on the worker thread.
  // Precondition: start() has been called (no-op otherwise — task is queued
  // but never executed; this is a usage error).
  void enqueue(std::function<void()> fn);

  // Wait for queue + in-flight tasks to drain. Returns true on drain,
  // false on timeout. Test-only hang detection.
  bool flush(std::chrono::milliseconds timeout);

  // Introspection (test/observability).
  bool queue_empty() const;
  bool in_flight_empty() const;

 private:
  void worker_loop();

  // Concrete worker: bridges kernel_workqueue::worker_loop into
  // kernel_thread_base::run(). Standard RAII worker pattern: outer class
  // owns the work, inner class is the thread bound to a method.
  class worker_thread : public kernel_thread_base {
   public:
    explicit worker_thread(kernel_workqueue* owner) : owner_(owner) {}
    ~worker_thread() override { stop(); }  // MUST be first line (ADR-060 §1.1)

   private:
    void run() override { owner_->worker_loop(); }
    kernel_workqueue* owner_;
  };

  std::unique_ptr<worker_thread> worker_;
  mutable std::mutex mutex_;
  std::condition_variable cv_;
  std::deque<std::function<void()>> queue_;
  std::atomic<bool> started_{false};
  std::atomic<int>  in_flight_{0};
};

}  // namespace usr_linux_emu
