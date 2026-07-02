/*
 * ats_protocol.cpp — PCIe ATS protocol handler (Translation + Invalidation)
 *
 * Stage 1.1 (IOMMU + ATS): implements 4 core ATS messages per PCIe Base
 * Specification 6.0 §6.18. Per design.md Decision 3, only the 4 core
 * messages are supported; Page Walk / cache hint / PASID are deferred.
 *
 * In-process handler — UsrLinuxEmu runs in user-space, so "device → IOMMU"
 * traffic is just a function call between two modules (no actual PCIe TLP
 * framing).
 */

#include "iommu_internal.h"

#include <linux_compat/iommu/iommu.h>
#include <linux_compat/pci/ats.h>

#include <cstdio>

extern "C" {

int ats_handle_translation_request(struct iommu_domain *domain,
				   const struct ats_translation_request *req,
				   struct ats_translation_completion *completion)
{
	if (!domain || !req || !completion)
		return IOMMU_ERR_EINVAL;

	/*
	 * Stage-1.1 NON-GOAL: PASID is reserved. KFD's use of PASID (with
	 * PRI/PRG response routing) lives in stage-1.4 if required.
	 */
	if (req->pasid != 0) {
		completion->translated_address = 0;
		completion->completion_status = ATS_COMPLETION_INVALID;
		return IOMMU_ERR_OK;
	}

	phys_addr_t paddr = iommu_iova_to_phys(domain, req->iova);
	if (paddr == 0) {
		completion->translated_address = 0;
		completion->completion_status = ATS_COMPLETION_UNMAPPED;
	} else {
		completion->translated_address = paddr;
		completion->completion_status = ATS_COMPLETION_SUCCESS;
	}
	return IOMMU_ERR_OK;
}

int ats_handle_invalidation_request(struct iommu_domain *domain,
				    const struct ats_invalidation_request *req,
				    struct ats_invalidation_completion *completion)
{
	if (!domain || !req || !completion)
		return IOMMU_ERR_EINVAL;

	/*
	 * Stage-1.1 IOTLB is conceptual (we use unordered_map). For real
	 * IOMMU HW this would invalidate the device-TLB cache for the range
	 * [iova, iova + size). Stage-1.1 only logs and dispatches the IOTLB
	 * flush through the domain->ops->flush_iotlb hook if present.
	 */
	std::fprintf(stderr, "[ats] invalidate iova=0x%lx size=0x%lx\n",
		     req->iova, req->size);

	if (domain->ops && domain->ops->flush_iotlb)
		domain->ops->flush_iotlb(domain, req->iova, req->size);

	completion->status = ATS_INVALIDATION_SUCCESS;
	return IOMMU_ERR_OK;
}

}  /* extern "C" */
