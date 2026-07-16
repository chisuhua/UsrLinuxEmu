// src/kernel/thread/kernel_workqueue.cpp
//
// Implementation of kernel_workqueue. See header for design rationale.

#include "kernel/thread/kernel_workqueue.h"

#include <cassert>
#include <utility>

namespace usr_linux_emu {

kernel_workqueue::~kernel_workqueue() {
  stop();  // Drain any pending tasks before destruction (ADR-060 §1.1).
}

void kernel_workqueue::start() {
  bool expected = false;
  if (!started_.compare_exchange_strong(expected, true,
                                          std::memory_order_acq_rel,
                                          std::memory_order_acquire)) {
    return;  // already started
  }
  worker_ = std::make_unique<worker_thread>(this);
  worker_->start();
}

void kernel_workqueue::stop() {
  bool expected = true;
  {
    // Update the wait predicate while holding the same mutex used by
    // worker_loop(). This prevents stop notification from racing with the
    // worker's transition into cv_.wait().
    std::lock_guard<std::mutex> lock(mutex_);
    if (!started_.compare_exchange_strong(expected, false,
                                            std::memory_order_acq_rel,
                                            std::memory_order_acquire)) {
      return;  // not started (or already stopped)
    }
  }
  cv_.notify_all();  // wake worker to observe started_ = false
  if (worker_) {
    worker_->stop();  // joins worker thread (drain contract: queue empty)
  }
  // Defensive: clear any leftover items (shouldn't happen if drain works).
  std::lock_guard<std::mutex> lock(mutex_);
  queue_.clear();
}

void kernel_workqueue::enqueue(std::function<void()> fn) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push_back(std::move(fn));
  }
  cv_.notify_one();
}

bool kernel_workqueue::flush(std::chrono::milliseconds timeout) {
  std::unique_lock<std::mutex> lock(mutex_);
  return cv_.wait_for(lock, timeout, [this] {
    return queue_.empty() &&
           in_flight_.load(std::memory_order_acquire) == 0;
  });
}

bool kernel_workqueue::queue_empty() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return queue_.empty();
}

bool kernel_workqueue::in_flight_empty() const {
  return in_flight_.load(std::memory_order_acquire) == 0;
}

void kernel_workqueue::worker_loop() {
  while (true) {
    std::function<void()> task;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this] {
        return !queue_.empty() ||
               !started_.load(std::memory_order_acquire);
      });
      // Stop signal + empty queue → exit (drain-then-exit).
      if (!started_.load(std::memory_order_acquire) && queue_.empty()) {
        break;
      }
      task = std::move(queue_.front());
      queue_.pop_front();
    }
    in_flight_.fetch_add(1, std::memory_order_acq_rel);
    task();
    in_flight_.fetch_sub(1, std::memory_order_acq_rel);
    cv_.notify_all();  // wake flush() waiters
  }
}

}  // namespace usr_linux_emu
