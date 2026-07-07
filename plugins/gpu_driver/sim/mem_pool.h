/*
 * sim/mem_pool.h — Simulated memory pool (C ABI)
 *
 * Phase 3.2 of the sim-stream-primitive-support change (ACCEPTED 2026-07-05).
 * Implements CUDA mem pool semantics using the Fix-2 Option B (VA subrange
 * approach): a pool reserves a contiguous VA range inside its parent VA
 * Space, and individual allocations come from within that range.
 *
 * Architecture: ③ Hardware Simulation layer.
 * Per ADR-036 three-way separation.
 *
 * Thread Safety (per design.md): NOT required (single-threaded).
 * Out of scope for Phase 3.2 PoC:
 *   - Real gpu_buddy backing (skipped per Decision 4 to avoid polluting
 *     libgpu_core)
 *   - Release-threshold based reclamation (recorded only, not enforced)
 *   - Cross-process shared memory pool
 */

#ifndef SIM_MEM_POOL_H
#define SIM_MEM_POOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Public pool handle (opaque) */
typedef uint64_t sim_pool_handle_t;

/* Pool properties (input to create) */
typedef struct {
  uint64_t va_space_handle;  /* Parent VA Space handle */
  uint64_t size;             /* Pool size in bytes (≤ 4GB for PoC) */
  uint64_t va_base;           /* OUT: pool VA subrange base, set by create */
  uint64_t va_limit;          /* OUT: pool VA subrange end (= va_base + size) */
  uint32_t flags;             /* GPU_MEM_POOL_FLAGS_* (recorded only) */
  uint32_t _reserved;
} sim_mem_pool_props_t;

/* Pool attribute enum */
typedef enum {
  SIM_MEM_POOL_ATTR_RELEASE_THRESHOLD                = 1,
  SIM_MEM_POOL_ATTR_REUSE_FOLLOW_EVENT_DEPENDENCIES  = 2,
} sim_mem_pool_attr_t;

/* Pool error codes (Fix-2) */
#define SIM_POOL_ERR_OK               0
#define SIM_POOL_ERR_INVALID_HANDLE  -1
#define SIM_POOL_ERR_NOSPC           -2
#define SIM_POOL_ERR_INVAL           -3
#define SIM_POOL_ERR_NOT_SUPPORTED   -4

/* Pool flags (subset of CU_MEMPOOL_*) */
#define GPU_MEM_POOL_FLAGS_DEFAULT  0u

/* ── API ─────────────────────────────────────────────────────────────── */

/* Create pool; on success writes pool_handle_out and OUT fields
 * props->va_base, props->va_limit (allocated VA subrange). */
int sim_mem_pool_create(sim_mem_pool_props_t *props,
                        uint64_t *pool_handle_out);

/* Destroy pool. BOs allocated from it are not tracked individually (PoC). */
int sim_mem_pool_destroy(uint64_t pool_handle);

/* Sync allocation. *va_out falls within pool's [va_base, va_limit]. */
int sim_mem_pool_alloc(uint64_t pool_handle, uint64_t size, uint64_t *va_out);

/*
 * Async allocation: same as alloc + emits sim fence_id (≥ 1<<32).
 * Returns int64_t: <0 error, >= 1<<32 valid fence_id (per Fix-9).
 */
int64_t sim_mem_pool_alloc_async(uint64_t pool_handle, uint64_t size,
                                 uint32_t stream_id, uint64_t *va_out);

/*
 * Async free: removes any matching va from pool's allocated set + fence.
 */
int64_t sim_mem_pool_free_async(uint64_t va, uint32_t stream_id);

/* Attribute setters/getters. Recorded only (release threshold not enforced). */
int sim_mem_pool_set_attr(uint64_t pool_handle, sim_mem_pool_attr_t attr,
                          const void *value, size_t value_size);
int sim_mem_pool_get_attr(uint64_t pool_handle, sim_mem_pool_attr_t attr,
                          void *value_out, size_t value_size);

/* Trim pool: trivial in PoC (no reclamation). */
int sim_mem_pool_trim(uint64_t pool_handle, uint64_t min_bytes);

/*
 * Export pool as a shareable handle (POSIX FD).
 * Only CU_MEM_HANDLE_TYPE_POSIX_FILE_DESCRIPTOR=1 is supported.
 * Returns 0 on success and writes an O_CLOEXEC pipe read-end fd to *fd_out.
 * Phase 4 (2026-07-07) added for cuMemPoolExportToShareableHandle.
 */
int sim_mem_pool_export_shareable(uint64_t pool_handle, uint32_t handle_type,
                                  uint32_t flags, int32_t* fd_out);

/* Test-only helper. */
void sim_mem_pool_reset_for_test(void);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  /* SIM_MEM_POOL_H */
