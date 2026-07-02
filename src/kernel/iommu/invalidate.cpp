/*
 * invalidate.cpp — IOTLB invalidate and mmu_notifier registration stub
 *
 * Stage 1.1 (IOMMU + ATS): provides the iommu_invalidate_register_notifier()
 * stub required by spec Requirement: IOTLB invalidate 与 mmu_notifier 集成点.
 * Full mmu_notifier callback body lives in stage-1.3 (UVM/HMM).
 *
 * Per design.md Decision 4: 1.1 only stubs out the registration; 1.3 fills
 * the implementation. Avoids 1.1 / 1.3 work coupling (see ADR path note).
 */

#include "iommu_internal.h"

#include <linux_compat/iommu/iommu.h>

#include <cstdio>

extern "C" {

int iommu_invalidate_register_notifier_internal(struct iommu_domain *d,
					       struct mmu_notifier *mnp)
{
	if (!d || !mnp)
		return IOMMU_ERR_EINVAL;

	/*
	 * TODO(stage-1.3): implement mmu_notifier callback body.
	 * The full implementation will:
	 *   1. Validate mnp ownership
	 *   2. Insert mnp into domain->notifier_list
	 *   3. Attach mmu_notifier_release callback if not yet attached
	 *
	 * For stage-1.1, only logging is performed so that downstream
	 * driver code (e.g., KFD in stage-1.4) can call this function
	 * without link-time errors. The stub returns 0 so callers
	 * proceed; if stage-1.3 sees a real invalidation request before
	 * this body is filled, behavior will be incorrect (no delivery)
	 * but the test suite will not assert this path.
	 *
	 * Reference: docs/superpowers/plans/2026-07-02-stage-1-kernel-emu-tracking.md
	 * §Sub-stage 1.3 fills this body.
	 */
	std::fprintf(stderr,
		     "[iommu] register_notifier stub domain=%p mnp=%p "
		     "(TODO stage-1.3)\n",
		     (void *)d, (void *)mnp);
	return IOMMU_ERR_OK;
}

int iommu_invalidate_unregister_notifier_internal(struct iommu_domain *d,
						 struct mmu_notifier *mnp)
{
	if (!d || !mnp)
		return IOMMU_ERR_EINVAL;

	/* Stage-1.1 stub mirrors register_notifier; full impl in stage-1.3 */
	std::fprintf(stderr,
		     "[iommu] unregister_notifier stub domain=%p mnp=%p\n",
		     (void *)d, (void *)mnp);
	return IOMMU_ERR_OK;
}

/*
 * Optional public iommu_flush_iotlb() for callers that don't go through
 * the iommu_ops vtable. Equivalent to (and forwards to) the vtable hook.
 */
int iommu_flush_iotlb(struct iommu_domain *domain, unsigned long iova, size_t size)
{
	if (!domain)
		return IOMMU_ERR_EINVAL;
	if (domain->ops && domain->ops->flush_iotlb)
		domain->ops->flush_iotlb(domain, iova, size);
	return IOMMU_ERR_OK;
}

}  /* extern "C" */
