/*
 * sim/sim_device_va_allocator.h — Per-device VA sub-range allocator (C ABI)
 *
 * Phase 4 cu-mempool-alloc-real-va change (ADR-058).
 * Allocates [va_base, va_limit) sub-ranges for cuMemPool pools from a single
 * per-device buddy allocator. Mirrors Nvidia UVM uvm_range_allocator pattern
 * (per-semaphore-pool sub-range from a global range allocator).
 *
 * Thread Safety: std::mutex protects the underlying gpu_buddy (which is
 * lock-free per ADR-020 libgpu_core purity constraint).
 *
 * Architecture: ③ Hardware Simulation layer (per ADR-036 three-way separation).
 */

#ifndef SIM_DEVICE_VA_ALLOCATOR_H
#define SIM_DEVICE_VA_ALLOCATOR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Device VA range (high address space, safe from user process mmap collisions):
 *   base = 86 TiB (verified common application-memory window for GCC ASan and
 *          Clang TSan on Linux x86-64; tested Jul 2026)
 *   size = 16 GiB (mirrors AMD KFD gpuvm aperture sizing)
 */
#define SIM_DEVICE_VA_BASE 0x560000000000ULL   /* 86 TiB */
#define SIM_DEVICE_VA_SIZE 0x400000000ULL      /* 16 GiB */

/* Allocate a VA sub-range of `size` bytes from the device-wide pool.
 * On success, *base_out receives the base address (multiple of 4KB).
 * Returns 0 on success, -ENOMEM on exhaustion, -EINVAL on bad arg.
 *
 * The returned sub-range is automatically aligned to 4KB. The allocator
 * may allocate more than `size` bytes internally (power-of-2 rounding).
 */
int sim_device_va_alloc(uint64_t size, uint64_t *base_out);

/* Free a previously allocated VA sub-range. `base` must match a value
 * previously returned by sim_device_va_alloc.
 * Returns 0 on success, -EINVAL if base was not allocated.
 */
int sim_device_va_free(uint64_t base);

/* Reset all state (test-only helper).
 * Releases all allocated sub-ranges and re-initializes the underlying buddy.
 * Caller must ensure no outstanding sub-range references survive.
 */
void sim_device_va_reset_for_test(void);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* SIM_DEVICE_VA_ALLOCATOR_H */