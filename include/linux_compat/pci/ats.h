/*
 * ats.h — PCIe Address Translation Services protocol (user-space)
 *
 * Mirrors real kernel drivers/pci/ats.h concepts for portability.
 * When porting to kernel, replace with <linux/ats.h> (or pcie/ats.h).
 *
 * Stage 1.1 (IOMMU + ATS): per docs/roadmap/stage-1-kernel-emu.md §子阶段 1.1.
 *
 * Supports the 4 core ATS messages per PCI Express Base Specification 6.0
 * §6.18:
 *   - Translation Request
 *   - Translation Completion
 *   - Invalidation Request
 *   - Invalidation Completion
 *
 * Stage-1.1 NON-GOALS (explicitly deferred):
 *   - Page Walk protocol
 *   - ATS cache hint
 *   - PASID (Process Address Space ID)
 *
 * These are deferred to stage-1.4 KFD integration (ADR-027 spec-driven).
 */

#pragma once

#include <linux_compat/types.h>
#include <linux_compat/iommu/iommu.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Translation Request: device-TLB miss → IOMMU responds with completion.
 */
struct ats_translation_request {
	unsigned long iova;
	uint8_t pasid;		/* stage-1.1: reserved, must be 0 */
	uint8_t _padding[3];
};

/*
 * Completion status codes per PCIe Base Specification 6.0 §6.18.2.
 */
enum ats_completion_status {
	ATS_COMPLETION_SUCCESS    = 0,
	ATS_COMPLETION_UNMAPPED   = 1,
	ATS_COMPLETION_INVALID    = 2,
	ATS_COMPLETION_TIMEOUT    = 3,
};

/*
 * Translation Completion: IOMMU → device response to Translation Request.
 */
struct ats_translation_completion {
	phys_addr_t translated_address; /* 0 if UNMAPPED */
	enum ats_completion_status completion_status;
};

/*
 * Invalidation Request: device asks IOMMU to invalidate cached translation.
 */
struct ats_invalidation_request {
	unsigned long iova;
	unsigned long size;
};

/*
 * Invalidation Completion status codes.
 */
enum ats_invalidation_status {
	ATS_INVALIDATION_SUCCESS = 0,
	ATS_INVALIDATION_FAILURE = 1,
};

/*
 * Invalidation Completion: IOMMU response to Invalidation Request.
 */
struct ats_invalidation_completion {
	enum ats_invalidation_status status;
};

/*
 * ATS entry points (called by stage-1.1 device-side pseudo-driver or directly).
 * Returns 0 on success, negative errno on failure (-EINVAL, -ETIMEDOUT).
 */
int ats_handle_translation_request(struct iommu_domain *domain,
				   const struct ats_translation_request *req,
				   struct ats_translation_completion *completion);
int ats_handle_invalidation_request(struct iommu_domain *domain,
				    const struct ats_invalidation_request *req,
				    struct ats_invalidation_completion *completion);

#ifdef __cplusplus
}
#endif
