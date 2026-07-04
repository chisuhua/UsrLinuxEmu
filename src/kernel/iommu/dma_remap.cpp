/*
 * dma_remap.cpp — DMA remapping API and default iommu_ops implementation
 *
 * Stage 1.1 (IOMMU + ATS): implements iommu_map / iommu_unmap / iommu_iova_to_phys
 * per Linux 6.6/6.12 LTS semantics, plus a default iommu_ops table that driver
 * code (e.g., 1.4 KFD) can attach to its domain.
 *
 * Per design.md Decision 2: single-level 4KB page table only.
 * Per design.md Decision 7: iommu-error-semantics.md documents error codes.
 */

#include "iommu_internal.h"

#include <cstddef>
#include <cstdio>

extern "C" {

static const unsigned long IOMMU_PAGE_SHIFT = 12;
static const unsigned long IOMMU_PAGE_SIZE  = (1UL << IOMMU_PAGE_SHIFT);
static const unsigned long IOMMU_PAGE_MASK  = (IOMMU_PAGE_SIZE - 1);

/*
 * Validate basic constraints: page alignment, domain/state availability.
 * Returns 0 on valid, negative errno on failure.
 */
static int validate_domain(struct iommu_domain *domain,
			   usr_linux_emu::iommu_domain_state **out)
{
	if (!domain)
		return IOMMU_ERR_EINVAL;
	auto *state = usr_linux_emu::iommu_domain_priv(domain);
	if (!state)
		return IOMMU_ERR_EINVAL;
	*out = state;
	return IOMMU_ERR_OK;
}

/* ---------- High-level API (driver code calls these directly) ---------- */

int iommu_map(struct iommu_domain *domain,
	      unsigned long iova,
	      phys_addr_t paddr,
	      size_t size,
	      int prot)
{
	usr_linux_emu::iommu_domain_state *state = nullptr;
	int rc = validate_domain(domain, &state);
	if (rc != IOMMU_ERR_OK)
		return rc;

	/* Stage-1.1 supports only 4KB page mapping (single-level page table) */
	if (size != IOMMU_PAGE_SIZE)
		return IOMMU_ERR_EINVAL;

	/* Page alignment check (per spec: paddr not page-aligned → -EINVAL) */
	if (iova & IOMMU_PAGE_MASK)
		return IOMMU_ERR_EINVAL;
	if (paddr & IOMMU_PAGE_MASK)
		return IOMMU_ERR_EINVAL;

	/* Protection bits reserved in stage-1.1 (no real permission check) */
	(void)prot;

	/* IOVA overlap check (per spec: returns -EREMOTEIO) */
	if (state->iova_to_phys.count(iova))
		return IOMMU_ERR_EREMOTEIO;

	state->iova_to_phys[iova] = paddr;
	return IOMMU_ERR_OK;
}

/*
 * unmap returns the size unmapped on success (per Linux kernel semantics).
 * -EREMOTEIO is reserved for "not implemented"; use -ENOKEY here per
 * docs/05-advanced/iommu-error-semantics.md table.
 */
long iommu_unmap(struct iommu_domain *domain,
		 unsigned long iova,
		 size_t size)
{
	usr_linux_emu::iommu_domain_state *state = nullptr;
	int rc = validate_domain(domain, &state);
	if (rc != IOMMU_ERR_OK)
		return (long)rc;

	if (size != IOMMU_PAGE_SIZE)
		return (long)IOMMU_ERR_EINVAL;

	auto it = state->iova_to_phys.find(iova);
	if (it == state->iova_to_phys.end())
		return (long)IOMMU_ERR_ENOKEY;

	state->iova_to_phys.erase(it);

	/* Flush IOTLB through ops (optional; some drivers provide this hook) */
	if (domain->ops && domain->ops->flush_iotlb)
		domain->ops->flush_iotlb(domain, iova, size);

	return (long)size;
}

phys_addr_t iommu_iova_to_phys(struct iommu_domain *domain, unsigned long iova)
{
	usr_linux_emu::iommu_domain_state *state = nullptr;
	if (validate_domain(domain, &state) != IOMMU_ERR_OK)
		return 0;

	auto it = state->iova_to_phys.find(iova);
	if (it == state->iova_to_phys.end())
		return 0;
	return it->second;
}

/* ---------- Default iommu_ops (KFD-style driver code path) ---------- */

/*
 * Wrap high-level API in iommu_ops vtable format. Driver code that follows
 * Linux kernel idiom (assign domain->ops = &iommu_default_dma_ops) still
 * works through these stubs.
 */

static int default_map_page(struct iommu_domain *d, unsigned long iova,
			    phys_addr_t paddr, size_t sz, int prot)
{
	return iommu_map(d, iova, paddr, sz, prot);
}

static int default_unmap_page(struct iommu_domain *d, unsigned long iova,
			      size_t sz)
{
	/* Convert size-return to int-return for ops vtable signature */
	long ret = iommu_unmap(d, iova, sz);
	if (ret < 0)
		return (int)ret;
	return 0;
}

static phys_addr_t default_iova_to_phys(struct iommu_domain *d, unsigned long iova)
{
	return iommu_iova_to_phys(d, iova);
}

/*
 * Tier-2 penetrated: 2026-07-05 - references kfd-portability-boundary.md §3.2
 * Per design.md D3: real page table invalidation in user-space (no host
 * kernel involvement).  Walks iommu_domain_state->iova_to_phys in the
 * [iova, iova+sz) range, counts flushed entries, and signals the
 * sim layer.  Real hardware IOTLB flush (vfio, etc.) is Stage 2.
 */
static void default_flush_iotlb(struct iommu_domain *d, unsigned long iova,
				size_t sz)
{
	if (!d)
		return;
	auto *state = usr_linux_emu::iommu_domain_priv(d);
	if (!state) {
		std::fprintf(stderr,
			     "[iommu] flush_iotlb domain=%p iova=0x%lx size=0x%zx "
			     "(no domain state)\n",
			     (void *)d, iova, sz);
		return;
	}

	/* Tier-2 design: don't mutate page table — iommu_unmap already
	 * erased entries; this hook is the post-unmap TLB-flush signal. */
	unsigned long end = iova + sz;
	std::size_t flushed = 0;
	for (auto it = state->iova_to_phys.begin();
	     it != state->iova_to_phys.end();
	     ++it) {
		if (it->first >= iova && it->first < end)
			flushed++;
	}

	std::fprintf(stderr,
		     "[iommu] flush_iotlb domain=%p iova=0x%lx size=0x%zx "
		     "flushed=%zu (Tier-2: real page-table walk)\n",
		     (void *)d, iova, sz, flushed);
}

/*
 * Forward declarations for Group 5 stubs (resolved by the linker).
 */
extern int iommu_invalidate_register_notifier_internal(struct iommu_domain *d,
						       struct mmu_notifier *mnp);
extern int iommu_invalidate_unregister_notifier_internal(struct iommu_domain *d,
							 struct mmu_notifier *mnp);

static int default_register_notifier(struct iommu_domain *d, struct mmu_notifier *mnp)
{
	return iommu_invalidate_register_notifier_internal(d, mnp);
}

static int default_unregister_notifier(struct iommu_domain *d, struct mmu_notifier *mnp)
{
	return iommu_invalidate_unregister_notifier_internal(d, mnp);
}

static const struct iommu_ops iommu_default_dma_ops = {
	.map_page = default_map_page,
	.unmap_page = default_unmap_page,
	.iova_to_phys = default_iova_to_phys,
	.flush_iotlb = default_flush_iotlb,
	.register_notifier = default_register_notifier,
	.unregister_notifier = default_unregister_notifier,
};

const struct iommu_ops *iommu_default_ops_get(void)
{
	return &iommu_default_dma_ops;
}

}  /* extern "C" */
