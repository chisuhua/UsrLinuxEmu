/*
 * iommu.h — IOMMU public types, flags, and error codes (user-space)
 *
 * Mirrors real kernel include/linux/iommu.h for portability.
 * When porting to kernel, replace with <linux/iommu.h>.
 *
 * Stage 1.1 (IOMMU + ATS): per docs/roadmap/stage-1-kernel-emu.md §子阶段 1.1.
 * ABI consistency is NOT guaranteed per ADR-027 decision 3 — only API
 * signatures are aligned with Linux 6.6/6.12 LTS.
 */

#pragma once

#include <linux_compat/types.h>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Physical address type. Aligned with Linux kernel typedef
 * include/linux/types.h. Declared here (not in linux_compat/types.h)
 * because phys_addr_t is IOMMU-specific.
 */
typedef uint64_t phys_addr_t;

/*
 * Protection bits for iommu_map(). Mirrors Linux kernel definitions
 * (include/linux/iommu.h).
 */
#define IOMMU_READ		(1 << 0)
#define IOMMU_WRITE		(1 << 1)
#define IOMMU_CACHE		(1 << 2)
#define IOMMU_NOEXEC		(1 << 3)
#define IOMMU_MMIO		(1 << 4)
#define IOMMU_PRIVILEGED	(1 << 5)

/*
 * Domain types. Aligned with Linux kernel __IOMMU_DOMAIN_* flags.
 */
enum iommu_domain_type {
	IOMMU_DOMAIN_UNMANAGED	= 0,
	IOMMU_DOMAIN_DMA	= 1,
	IOMMU_DOMAIN_PAGING	= 2,
};

/*
 * Geometry flags. Mirrors Linux kernel definitions.
 */
#define IOMMU_DOMAIN_BIT_MASK	0xffffffffUL

/*
 * Return codes consistently aligned with Linux kernel semantics.
 * Per spec Requirement: 错误码语义与 Linux 内核一致.
 * See docs/05-advanced/iommu-error-semantics.md for full mapping table.
 */
#define IOMMU_ERR_OK		(0)
#define IOMMU_ERR_EINVAL	(-22)   /* invalid argument */
#define IOMMU_ERR_ENOMEM	(-12)   /* out of memory */
#define IOMMU_ERR_ENOSPC	(-28)   /* no space left (ioasid exhaustion) */
#define IOMMU_ERR_EREMOTEIO	(-121)  /* IOVA overlap (DMA remap failure) */
#define IOMMU_ERR_ENOKEY	(-126)  /* IOVA not mapped (unmap) */
#define IOMMU_ERR_ETIMEDOUT	(-110)  /* IOTLB invalidate timeout */
#define IOMMU_ERR_ENOSYS	(-38)   /* feature not implemented */
#define IOMMU_ERR_ENODEV	(-19)   /* no such device */
#define IOMMU_ERR_EBUSY		(-16)   /* resource busy */

/*
 * Lifecycle entry points for the IOMMU emulator. These MUST be called
 * before any iommu_domain_alloc / iommu_group_alloc / ioasid_alloc.
 * iommu_emu_init() is idempotent; iommu_emu_shutdown() releases all
 * resources and resets the global state.
 *
 * Per stage-1.1 design (decisions 5-6), no automatic PCI enumeration
 * is performed during init — driver code calls iommu_register_pci_device()
 * for each device that should be tracked.
 */
int iommu_emu_init(void);
void iommu_emu_shutdown(void);

/*
 * PCIe device registration API. Driver code (e.g., stage-1.0 PciDevice
 * constructors) calls iommu_register_pci_device() to:
 *   1. Create a fresh iommu_group (1 device = 1 group, per design.md Decision 5)
 *   2. Add the device as the sole member
 *   3. Attach a default domain with iommu_default_ops_get() ops
 *
 * Idempotent: re-registering the same handle returns the existing group.
 */
struct iommu_group;
struct iommu_group *iommu_register_pci_device(void *pci_dev_handle);
int iommu_unregister_pci_device(void *pci_dev_handle);

#ifdef __cplusplus
}
#endif
