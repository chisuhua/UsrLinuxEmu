/*
 * iommu_domain.h — IOMMU domain abstraction and ops vtable (user-space)
 *
 * Mirrors real kernel include/linux/iommu.h for portability.
 * When porting to kernel, replace with <linux/iommu.h>.
 *
 * Stage 1.1 (IOMMU + ATS): per docs/roadmap/stage-1-kernel-emu.md §子阶段 1.1.
 *
 * Provides three core data structures:
 *   - struct iommu_domain: per-device translation context
 *   - struct iommu_ops: vtable of hooks (map/unmap/iova_to_phys/flush_iotlb/...)
 *   - struct iommu_domain_geometry: address-space geometry descriptor
 *
 * Per ADR-027 decision 3: ABI alignment is NOT guaranteed; only signatures
 * match Linux 6.6/6.12 LTS header conventions.
 */

#pragma once

#include <linux_compat/types.h>
#include <linux_compat/iommu/iommu.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct iommu_domain;
struct iommu_group;
struct device;
struct mm_struct;

/*
 * Fault handler callback. Aligned with Linux kernel iommu_fault_handler_t.
 */
typedef int (*iommu_fault_handler_t)(struct iommu_domain *domain,
				     struct device *dev,
				     unsigned long iova,
				     int flags,
				     void *token);

/*
 * Notification callback for IOTLB invalidation. This is a forward
 * declaration — full mmu_notifier integration lives in stage-1.3
 * (UVM/HMM). For stage-1.1 only the registration API is provided
 * (see src/kernel/iommu/invalidate.cpp TODO marker).
 */
struct mmu_notifier;
typedef void (*iommu_notifier_fn_t)(struct mmu_notifier *mnp,
				    unsigned long action,
				    void *data);

/*
 * Domain geometry: describes the IOVA space bounds.
 * Mirrors Linux kernel struct iommu_domain_geometry.
 */
struct iommu_domain_geometry {
	unsigned long aperture_start;	/* first IOVA */
	unsigned long aperture_end;	/* last IOVA (inclusive) */
	unsigned long force_aperture;	/* !0: enforce bounds */
};

/*
 * IOMMU ops vtable. Mirrors Linux kernel struct iommu_ops (essential hooks).
 * Each hook returns 0 on success, negative errno on failure.
 *
 * Required hooks (driver MUST provide all of these):
 *   - map_page / unmap_page
 *   - iova_to_phys
 *   - flush_iotlb
 *   - register_notifier / unregister_notifier
 */
struct iommu_ops {
	int (*map_page)(struct iommu_domain *domain,
			unsigned long iova,
			phys_addr_t paddr,
			size_t size,
			int prot);
	/* returns size unmapped on success, negative errno on failure */

	int (*unmap_page)(struct iommu_domain *domain,
			  unsigned long iova,
			  size_t size);

	phys_addr_t (*iova_to_phys)(struct iommu_domain *domain,
				    unsigned long iova);
	/* returns 0 if IOVA is not mapped (see docs/05-advanced/iommu-error-semantics.md) */

	void (*flush_iotlb)(struct iommu_domain *domain,
			    unsigned long iova,
			    size_t size,
			    void *flushed_entries);

	int (*register_notifier)(struct iommu_domain *domain,
				 struct mmu_notifier *mnp);
	/* returns 0 on success; TODO(stage-1.3): full mmu_notifier callback */

	int (*unregister_notifier)(struct iommu_domain *domain,
				   struct mmu_notifier *mnp);

	/* Stage-1.1 non-goal: page_walk and ATS cache hint ops omitted.
	 * Per ADR-027 spec-driven, they will be added in stage-1.4
	 * KFD integration only if KFD driver code demonstrably requires them.
	 */
};

/*
 * IOMMU domain: per-device translation context.
 * Aligned with Linux kernel struct iommu_domain (minimal field set for
 * stage-1.1; additional fields may be added in subsequent phases).
 */
struct iommu_domain {
	enum iommu_domain_type type;
	const struct iommu_ops *ops;
	unsigned long pgsize_bitmap;	/* supported page sizes */
	void *priv;			/* implementation-specific state */
	iommu_fault_handler_t handler;
	void *handler_token;
	struct iommu_domain_geometry geometry;
};

/*
 * Allocate a new IOMMU domain of the given type.
 * Returns NULL on allocation failure (errno set to ENOMEM).
 */
struct iommu_domain *iommu_domain_alloc(enum iommu_domain_type type);

/*
 * Release an IOMMU domain previously allocated via iommu_domain_alloc().
 * All IOVA mappings must be unmapped before calling this.
 */
void iommu_domain_free(struct iommu_domain *domain);

/*
 * Attach an mm_struct* to the domain's backing state.
 * Required before iommu_invalidate_register_notifier() so the wrapper
 * has a stable mm to pass to mmu_notifier_register().  Replaces the
 * older "domain->priv = &mm" implicit contract that produced UB once
 * Stage 2.1.2 added mm_shim propagation (Issue #21).  Returns 0 on
 * success, -EINVAL on null inputs or unallocated domain.
 */
int iommu_domain_attach_mm(struct iommu_domain *domain,
                           struct mm_struct *mm);

/*
 * High-level DMA remapping API (Linux 6.6/6.12 LTS iommu.h semantics).
 * Returns 0/negative errno. unmap returns the size unmapped on success
 * (or negative errno). iova_to_phys returns 0 for unmapped IOVA.
 */
int iommu_map(struct iommu_domain *domain,
	      unsigned long iova,
	      phys_addr_t paddr,
	      size_t size,
	      int prot);

long iommu_unmap(struct iommu_domain *domain,
		 unsigned long iova,
		 size_t size);

phys_addr_t iommu_iova_to_phys(struct iommu_domain *domain, unsigned long iova);

/*
 * Default iommu_ops table (single-level 4KB page table, scope stage-1.1).
 * Driver code can assign domain->ops = iommu_default_ops_get().
 */
const struct iommu_ops *iommu_default_ops_get(void);

#ifdef __cplusplus
}
#endif
