/*
 * sim/sim_device_va_allocator.cpp — Per-device VA sub-range allocator
 *
 * Phase 4 cu-mempool-alloc-real-va change (ADR-058).
 * Wraps a single global gpu_buddy instance + std::mutex.
 *
 * libgpu_core purity (ADR-020) is preserved: this file adds range-awareness
 * via the per-pool buddy (D2 of ADR-058) without modifying gpu_buddy.h.
 */

#include "sim_device_va_allocator.h"
#include "gpu_buddy.h"

#include <cerrno>
#include <cstdint>
#include <mutex>

namespace {

/* The single per-device buddy allocator. base+size fixed at SIM_DEVICE_VA_BASE/SIZE.
 * gpu_buddy_init is lazy on first alloc to ensure static init order is irrelevant.
 */
struct gpu_buddy device_buddy_;
std::mutex device_buddy_mutex_;
bool device_buddy_initialized_ = false;

void ensure_device_buddy_initialized_locked(void) {
  /* Caller must hold device_buddy_mutex_. */
  if (!device_buddy_initialized_) {
    gpu_buddy_init(&device_buddy_, SIM_DEVICE_VA_BASE, SIM_DEVICE_VA_SIZE);
    device_buddy_initialized_ = true;
  }
}

}  /* anonymous namespace */

extern "C" {

int sim_device_va_alloc(uint64_t size, uint64_t *base_out) {
  if (!base_out || size == 0)
    return -EINVAL;
  if (size > SIM_DEVICE_VA_SIZE)
    return -ENOMEM;

  std::lock_guard<std::mutex> lock(device_buddy_mutex_);
  ensure_device_buddy_initialized_locked();
  uint64_t addr = 0;
  int rc = gpu_buddy_alloc(&device_buddy_, size, &addr);
  if (rc != 0)
    return rc;  /* -ENOMEM or -EINVAL from gpu_buddy */
  *base_out = addr;
  return 0;
}

int sim_device_va_free(uint64_t base) {
  std::lock_guard<std::mutex> lock(device_buddy_mutex_);
  if (!device_buddy_initialized_)
    return -EINVAL;  /* nothing was ever allocated */
  return gpu_buddy_free(&device_buddy_, base);
}

void sim_device_va_reset_for_test(void) {
  std::lock_guard<std::mutex> lock(device_buddy_mutex_);
  if (device_buddy_initialized_) {
    gpu_buddy_reset(&device_buddy_);
    /* Note: gpu_buddy_reset clears records but keeps base/size intact.
     * Subsequent alloc will reuse the same base/size range. */
  }
}

}  /* extern "C" */