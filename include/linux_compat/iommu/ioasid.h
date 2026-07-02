/*
 * ioasid.h — IO Address Space ID allocator (user-space)
 *
 * Mirrors real kernel include/linux/ioasid.h for portability.
 * When porting to kernel, replace with <linux/ioasid.h>.
 *
 * Stage 1.1 (IOMMU + ATS): provides 32-bit ID allocator for SVM/HMM
 * scenarios in stage-1.3. This change provides the allocator surface
 * only; consumers come in later phases.
 */

#pragma once

#include <linux_compat/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * IOASID: opaque handle wrapping a 32-bit IO address space ID.
 * The handle is allocated via ioasid_alloc() and freed via ioasid_free().
 */
struct ioasid {
	unsigned int id;	/* 32-bit IO address space ID */
	void *priv;		/* implementation-specific data */
};

/*
 * Allocate a new IOASID. The first-fit allocation strategy is used
 * (scan from the last allocated ID upward; wrap on overflow).
 * Returns NULL with errno=IOMMU_ERR_ENOSPC (-28) if no free IDs.
 */
struct ioasid *ioasid_alloc(void);

/*
 * Allocate a specific ID. Returns NULL with errno=IOMMU_ERR_ENOSPC (-28)
 * if the ID is already in use.
 */
struct ioasid *ioasid_alloc_id(unsigned int id);

/*
 * Free a previously allocated IOASID. Returns IOMMU_ERR_ENODEV (-19)
 * if the handle was not allocated by this allocator (or has already
 * been freed).
 */
int ioasid_free(struct ioasid *ioasid);

/*
 * Look up an IOASID by ID. Returns NULL if no such ID is allocated.
 */
struct ioasid *ioasid_find(unsigned int id);

/*
 * Get the numeric ID from an IOASID handle.
 */
unsigned int ioasid_id(struct ioasid *ioasid);

#ifdef __cplusplus
}
#endif
