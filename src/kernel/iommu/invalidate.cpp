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
#include <kernel/uvm/mmu_notifier_internal.h>
#include <kernel/uvm/mm_shim.h>

#include <cstdio>

extern "C" {

int iommu_invalidate_register_notifier_internal(struct iommu_domain *d,
					       struct mmu_notifier *mnp)
{
	if (!d || !mnp)
		return IOMMU_ERR_EINVAL;

	struct mm_struct *mm = static_cast<struct mm_struct *>(d->priv);
	if (!mm)
		return IOMMU_ERR_EINVAL;

	int ret = mmu_notifier_register(mnp, mm);
	if (ret != 0) {
		std::fprintf(stderr,
			     "[iommu] register_notifier failed (ret=%d) "
			     "domain=%p mnp=%p\n",
			     ret, (void *)d, (void *)mnp);
		return ret;
	}

	/* Stage 2.1.2: propagate mm_shim (PID + VMA context) to the
	 * notifier's priv so user callbacks can read it via mn->priv. */
	auto* state = usr_linux_emu::iommu_domain_priv(d);
	if (state && state->mm_shim) {
		mnp->priv = state->mm_shim;
	}

	std::fprintf(stderr,
		     "[iommu] register_notifier OK domain=%p mnp=%p mm_shim=%p\n",
		     (void *)d, (void *)mnp, (void*)mnp->priv);
	return IOMMU_ERR_OK;
}

int iommu_invalidate_unregister_notifier_internal(struct iommu_domain *d,
						 struct mmu_notifier *mnp)
{
	if (!d || !mnp)
		return IOMMU_ERR_EINVAL;

	mmu_notifier_unregister(mnp);

	std::fprintf(stderr,
		     "[iommu] unregister_notifier OK domain=%p mnp=%p\n",
		     (void *)d, (void *)mnp);
	return IOMMU_ERR_OK;
}

/*
 * Optional public iommu_flush_iotlb() for callers that don't go through
 * the iommu_ops vtable. Equivalent to (and forwards to) the vtable hook.
 *
 * Tier-2 §5 upgrades the vtable hook (iommu_ops->flush_iotlb) in
 * dma_remap.cpp; this function delegates to it.
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
