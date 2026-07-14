// include/kernel/thread/kernel_thread_base.h
//
// kernel_thread_base — RAII wrapper around pthread_t.
//
// Per ADR-060 (Linux Kernel Message Notification Threading for KFD
// Simulation): we use raw pthread_* instead of std::thread to avoid the
// GCC 13 + glibc gthr-default.h weakref bug (see
// docs/05-advanced/kfd-portability-report.md §4.2).
//
// Lifecycle:
//   1. Construct (running_ = false)
//   2. start()  → spawn pthread, run() executes in new thread
//   3. Derived class signals stop (e.g., setting a flag run() polls)
//   4. stop()   → join pthread
//   5. Destruct (must be after stop())
//
// Destroy-order rule (ADR-060 §1.1):
//   Derived class MUST call stop() in OWN destructor first line.
//   Base dtor assert(!is_running()) catches violations.
//
// Thread Safety:
//   - start() / stop() are NOT thread-safe with each other (call serially).
//   - is_running() is thread-safe.

#pragma once

#include <atomic>
#include <pthread.h>

namespace usr_linux_emu {

class kernel_thread_base {
 public:
  kernel_thread_base() = default;
  virtual ~kernel_thread_base();

  // Non-copyable, non-movable (pthread_t + atomic cannot be safely moved).
  kernel_thread_base(const kernel_thread_base&) = delete;
  kernel_thread_base& operator=(const kernel_thread_base&) = delete;
  kernel_thread_base(kernel_thread_base&&) = delete;
  kernel_thread_base& operator=(kernel_thread_base&&) = delete;

  // Spawn the thread. Calls run() in the new thread.
  // Idempotent: returns silently on already-started.
  void start();

  // Signal stop + join the thread. Idempotent. Safe on never-started.
  // Derived class is responsible for making run() exit BEFORE stop() is
  // called (e.g., by setting a flag that run() polls).
  void stop();

  // True if the thread is currently running (between start() and stop()).
  bool is_running() const {
    return running_.load(std::memory_order_acquire);
  }

 protected:
  // Thread body. Derived class implements.
  // Called once after start(). Must exit when is_running() == false.
  virtual void run() = 0;

 private:
  // pthread entry point. Trampoline that calls self->run().
  static void* entry(void* arg);

  pthread_t thread_{};
  std::atomic<bool> running_{false};
};

}  // namespace usr_linux_emu
