#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <queue>

/*
 * SemaphoreManager - Timeline semaphore primitives (ADR-049)
 *
 * Thread safety:
 * - value_ is std::atomic<uint64_t> with release/acquire semantics
 * - waiter list protected by mutex_ (unlock before invoking callback)
 *
 * Usage:
 *   SemaphoreManager mgr;
 *   uint64_t h = mgr.create(0);      // create with initial value 0
 *   mgr.signal(h, 1);                 // monotonic increment to 1
 *   uint64_t v = mgr.query(h);        // read current value
 *   mgr.wait(h, 1, callback_fn, 0);   // register waiter (non-blocking)
 *   mgr.destroy(h);                   // destroy, wake waiters with error
 */
class SemaphoreManager {
 public:
  SemaphoreManager() = default;
  ~SemaphoreManager();

  /* Create a timeline semaphore with initial value. Returns 0 on error. */
  uint64_t create(uint64_t initial);

  /* Signal: monotonic increment. Returns 0 on success, -EINVAL if value <= current. */
  int signal(uint64_t handle, uint64_t value);

  /* Register waiter callback (non-blocking). FIFO ordering. Returns 0 on success. */
  int wait(uint64_t handle, uint64_t expected,
           std::function<void(uint64_t)> callback, uint64_t user_data);

  /* Query current value. Returns value on success, UINT64_MAX on invalid handle. */
  uint64_t query(uint64_t handle);

  /* Destroy. Wakes waiters with error. Returns 0 on success, -EINVAL if invalid. */
  int destroy(uint64_t handle);

 private:
  struct SemWaiter {
    uint64_t expected;
    std::function<void(uint64_t)> callback;
    uint64_t user_data;
  };

  struct Semaphore {
    std::atomic<uint64_t> value{0};
    std::queue<SemWaiter> waiters;
    bool destroyed = false;
  };

  std::mutex mutex_;
  uint64_t next_handle_ = 1;
  std::map<uint64_t, Semaphore> semaphores_;
};
