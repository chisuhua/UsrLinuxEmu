/*
 * iommu_domain.cpp — IOMMU domain allocation and lifecycle (user-space)
 *
 * Stage 1.1 (IOMMU + ATS): per docs/roadmap/stage-1-kernel-emu.md §子阶段 1.1.
 * Mirrors design.md Decision 1 (Linux 6.6/6.12 LTS API signatures).
 */

#include "iommu_internal.h"

#include <cstdlib>
#include <cstring>
#include <algorithm>

extern "C" {

struct iommu_domain *iommu_domain_alloc(enum iommu_domain_type type)
{
	struct iommu_domain *domain = (struct iommu_domain *)std::malloc(sizeof(struct iommu_domain));
	if (!domain)
		return nullptr;

	std::memset(domain, 0, sizeof(*domain));
	domain->type = type;

	/* Default geometry: 64-bit IOVA aperture, 16 MiB range (stage-1.1 minimal) */
	domain->geometry.aperture_start = 0;
	domain->geometry.aperture_end = 0x00FFFFFFUL;
	domain->geometry.force_aperture = 0;

	/* Default page size bitmap: 4 KiB (0x1000) only (per design.md Decision 2) */
	domain->pgsize_bitmap = 0x1000;

	/* Attach internal state */
	auto *state = new (std::nothrow) usr_linux_emu::iommu_domain_state();
	if (!state) {
		std::free(domain);
		return nullptr;
	}
	state->aperture_start = domain->geometry.aperture_start;
	state->aperture_end = domain->geometry.aperture_end;
	state->capacity_bytes = 0x1000000UL; /* 16 MiB initial */
	domain->priv = state;

	/* Register with global state */
	auto *g = usr_linux_emu::iommu_emu_global_state();
	if (g) {
		g->domains.push_back(domain);
	}

	return domain;
}

void iommu_domain_free(struct iommu_domain *domain)
{
	if (!domain)
		return;

	/* Caller is responsible for unmapping; we just release resources */
	if (domain->priv) {
		auto *state = static_cast<usr_linux_emu::iommu_domain_state *>(domain->priv);
		delete state;
		domain->priv = nullptr;
	}

	auto *g = usr_linux_emu::iommu_emu_global_state();
	if (g) {
		auto it = std::find(g->domains.begin(), g->domains.end(), domain);
		if (it != g->domains.end())
			g->domains.erase(it);
	}

	std::free(domain);
}

}  /* extern "C" */
