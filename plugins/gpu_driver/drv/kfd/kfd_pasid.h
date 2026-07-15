/*
 * kfd_pasid.h — KFD PASID allocator public API (C-12 B.1.3)
 *
 * Source: linux/drivers/gpu/drm/amd/amdkfd/kfd_pasid.c (Linux 6.12 LTS)
 *   Real implementation uses idr_alloc()/idr_remove() from <linux/idr.h>.
 *   UsrLinuxEmu uses bitmap because no idr in linux_compat yet.
 *
 * PASID range: [1, 0xFFFF] (0 reserved). Thread-safe via internal mutex.
 *
 * Migration to real kernel:
 *   1. Replace kfd_pasid.c with upstream idr-based implementation
 *   2. This header signature remains unchanged
 *   3. All callers (B.1.5 kfd_process.c) compile without changes
 */
#pragma once
#include "kfd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize PASID allocator. Idempotent. Returns 0 on success. */
int kfd_pasid_init(void);

/* Tear down PASID allocator. After this, kfd_allocate_pasid returns -ENOSPC. */
void kfd_pasid_exit(void);

/* Allocate a PASID in [1, 0xFFFF] (0 reserved). 
 * @out_pasid: receives allocated PASID
 * Returns 0 on success, -ENOSPC if exhausted.
 */
int kfd_allocate_pasid(u32 *out_pasid);

/* Free a previously allocated PASID. Returns 0 on success, -EINVAL if invalid. */
int kfd_free_pasid(u32 pasid);

/* Test-only: get count of currently allocated PASIDs. */
int kfd_pasid_allocated_count(void);

#ifdef __cplusplus
}
#endif