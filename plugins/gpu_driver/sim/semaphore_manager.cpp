// semaphore_manager.cpp - Timeline semaphore implementation (ADR-049)
//
// Implements timeline semaphore primitives with waiter callback registration
// (non-blocking) and FIFO ordering. Thread-safe via atomic value + mutex-guarded
// waiter list.

#include "semaphore_manager.h"

#include <cerrno>

SemaphoreManager::~SemaphoreManager() {
  for (auto& [handle, sem] : semaphores_) {
    std::unique_lock<std::mutex> lock(mutex_);
    sem.destroyed = true;
    while (!sem.waiters.empty()) {
      auto w = std::move(sem.waiters.front());
      sem.waiters.pop();
      lock.unlock();
      if (w.callback) {
        w.callback(w.user_data);
      }
      lock.lock();
    }
  }
}

uint64_t SemaphoreManager::create(uint64_t initial) {
  std::lock_guard<std::mutex> lock(mutex_);
  uint64_t h = next_handle_++;
  auto& sem = semaphores_[h];
  sem.value.store(initial, std::memory_order_release);
  return h;
}

int SemaphoreManager::signal(uint64_t handle, uint64_t value) {
  std::unique_lock<std::mutex> lock(mutex_);
  auto it = semaphores_.find(handle);
  if (it == semaphores_.end() || it->second.destroyed) {
    return -EINVAL;
  }
  Semaphore& sem = it->second;
  uint64_t cur = sem.value.load(std::memory_order_acquire);
  if (value <= cur) {
    return -EINVAL;
  }
  sem.value.store(value, std::memory_order_release);

  // Collect ready waiters (FIFO order)
  std::queue<SemWaiter> ready;
  while (!sem.waiters.empty()) {
    SemWaiter& w = sem.waiters.front();
    if (value >= w.expected) {
      ready.push(std::move(w));
      sem.waiters.pop();
    } else {
      break;  // FIFO: remaining waiters expect higher values
    }
  }
  lock.unlock();

  // Invoke ready callbacks outside the lock (avoid deadlock)
  while (!ready.empty()) {
    auto w = std::move(ready.front());
    ready.pop();
    if (w.callback) {
      w.callback(w.user_data);
    }
  }
  return 0;
}

int SemaphoreManager::wait(uint64_t handle, uint64_t expected,
                            std::function<void(uint64_t)> callback,
                            uint64_t user_data) {
  std::unique_lock<std::mutex> lock(mutex_);
  auto it = semaphores_.find(handle);
  if (it == semaphores_.end() || it->second.destroyed) {
    return -EINVAL;
  }
  Semaphore& sem = it->second;
  uint64_t cur = sem.value.load(std::memory_order_acquire);
  if (cur >= expected) {
    // Condition already met; invoke callback immediately
    lock.unlock();
    if (callback) {
      callback(user_data);
    }
    return 0;
  }
  // Register waiter
  sem.waiters.push({expected, std::move(callback), user_data});
  return 0;
}

uint64_t SemaphoreManager::query(uint64_t handle) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = semaphores_.find(handle);
  if (it == semaphores_.end() || it->second.destroyed) {
    return UINT64_MAX;
  }
  return it->second.value.load(std::memory_order_acquire);
}

int SemaphoreManager::destroy(uint64_t handle) {
  std::unique_lock<std::mutex> lock(mutex_);
  auto it = semaphores_.find(handle);
  if (it == semaphores_.end() || it->second.destroyed) {
    return -EINVAL;
  }
  Semaphore& sem = it->second;
  sem.destroyed = true;

  // Wake all registered waiters with error
  auto waiters = std::move(sem.waiters);
  lock.unlock();

  while (!waiters.empty()) {
    auto w = std::move(waiters.front());
    waiters.pop();
    if (w.callback) {
      w.callback(w.user_data);
    }
  }
  return 0;
}
