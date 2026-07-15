/*
 * kfd_process.h — KFD process management public API (C-12 B.1.5)
 *
 * Source: linux/drivers/gpu/drm/amd/amdkfd/kfd_process.c (Linux 6.12 LTS)
 *   Real implementation: ~800 lines with full mm_struct, signal handling,
 *   XNACK mode, and debugger support.
 *   UsrLinuxEmu: simplified — single-GPU assumption, no IOMMU binding,
 *   no signal/eviction notification.
 *
 * Thread safety: all functions internally synchronize via processes_lock.
 *
 * Migration to real kernel:
 *   1. Replace kfd_process.c with upstream implementation
 *   2. This header signature remains unchanged
 *   3. All callers (kfd_queue.c, kfd_module.c) compile without changes
 */
#pragma once
#include "kfd_types.h"
#include <sys/types.h>  /* pid_t */

#ifdef __cplusplus
extern "C" {
#endif

/* Forward-declare types (complete definitions in kfd_priv.h) */
struct kfd_process;
struct kfd_node;

/* Initialize KFD process subsystem. Idempotent. Returns 0. */
int kfd_process_init(void);

/* Tear down KFD process subsystem. Destroys all live processes (debug: logs each). */
void kfd_process_exit(void);

/* Create a new kfd_process.
 * @out: receives pointer to allocated process (caller does NOT free)
 * @pid: OS pid for the process
 * Returns 0 on success, -ENOMEM on alloc failure, -EBUSY if pid already has a process.
 */
int kfd_process_create(struct kfd_process **out, pid_t pid);

/* Destroy a kfd_process. Frees all per-process resources.
 * Returns 0 on success, -EINVAL if p is NULL.
 */
int kfd_process_destroy(struct kfd_process *p);

/* Look up a process by PID.
 * @out: receives pointer (caller does NOT free; valid until process_destroy)
 * Returns 0 on success, -ENOENT if not found.
 */
int kfd_process_find_by_pid(pid_t pid, struct kfd_process **out);

/* Get total count of live processes. */
int kfd_process_count(void);

/* Get the GPU ID and GPU index for a given (process, node) pair.
 * C-12 single-GPU: gpu_id always = node->id, gpuidx always = 0.
 * Implementation: scan p->pdds[] for matching pdd->dev.
 *
 * @p: process context
 * @dev: target kfd_node
 * @out_gpuid: receives GPU ID (0 for single-GPU)
 * @out_gpuidx: receives per-process GPU index (0 for single-GPU)
 * Returns 0 on success, -ENOENT if dev not attached to process.
 *
 * CRITICAL FIX: kfd_queue.c:104 calls this — currently UNDEFINED.
 */
int kfd_process_gpuid_from_node(struct kfd_process *p, struct kfd_node *dev,
                                u32 *out_gpuid, u32 *out_gpuidx);

#ifdef __cplusplus
}
#endif
