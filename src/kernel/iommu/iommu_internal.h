/*
 * iommu_internal.h — Internal state structures for UsrLinuxEmu IOMMU emulation
 *
 * NOT part of the linux_compat API surface. This header defines UsrLinuxEmu
 * implementation details that are shared across multiple IOMMU modules but
 * are not exposed to driver code.
 *
 * Stage 1.1 (IOMMU + ATS): per docs/roadmap/stage-1-kernel-emu.md §子阶段 1.1.
 *
 * C++ namespace: usr_linux_emu (project namespace; NOT kernel-compat C).
 */

#pragma once

#include <linux_compat/iommu/iommu.h>
#include <linux_compat/iommu/iommu_domain.h>
#include <linux_compat/iommu/iommu_group.h>
#include <linux_compat/iommu/ioasid.h>
#include <linux_compat/types.h>
#include <linux_compat/mmu_notifier.h>

#include <kernel/sim_proxy.h>

#include <cstddef>
#include <vector>
#include <unordered_map>

struct us_mm_shim;
struct sim_page_migration;

namespace usr_linux_emu {

/*
 * Per-domain internal state. Allocated alongside iommu_domain and
 * reached via domain->priv. Holds the IOVA -> phys mapping table.
 */
struct iommu_domain_state {
	std::unordered_map<unsigned long, phys_addr_t> iova_to_phys;
	unsigned long aperture_start;
	unsigned long aperture_end;
	unsigned long capacity_bytes;
	/*
	 * Backing mm_struct pointer for the mmu_notifier framework.
	 *
	 * Why this field exists: prior to Stage 2.1.2 the iommu wrapper
	 * did `mm = static_cast<mm_struct*>(domain->priv)`, treating priv
	 * as a raw mm_struct*.  Stage 2.1.2 added mm_shim propagation which
	 * reinterpreted the same pointer as iommu_domain_state*, producing
	 * UB whenever a caller followed the legacy contract (test cases
	 * and any driver not using iommu_domain_alloc).  Holding mm_struct*
	 * inside the canonical state object lets the library use a single
	 * contract (priv == iommu_domain_state*) for both reads.
	 *
	 * May be nullptr if the domain is not yet attached to a process.
	 */
	struct mm_struct* mm = nullptr;
	::us_mm_shim* mm_shim = nullptr;
	struct sim_page_migration* sim_pm = nullptr;
};

/*
 * Stage-1.1 enforces one-device-per-group topology (design.md Decision 5).
 * Mapping is index-based (group.id == device enumeration index).
 */
struct iommu_group_state {
	int group_id;
	std::vector<struct iommu_group_member *> members;
};

/*
 * Global IOMMU emulator state. Instantiated once at iommu_emu_init().
 */
struct iommu_emu_state {
	bool initialized;
	std::vector<struct iommu_domain *> domains;
	std::vector<struct iommu_group *> groups;
	std::unordered_map<struct device *, struct iommu_group *> device_to_group;

	/* IOASID allocator bookkeeping */
	struct ioasid *ioasid_table[0x10000]; /* sparse, 64K entries max */
	unsigned int ioasid_next;
};

/*
 * Internal helpers (C++ namespace; only used across src/kernel/iommu/).
 */
struct iommu_emu_state *iommu_emu_global_state(void);

iommu_domain_state *iommu_domain_priv(struct iommu_domain *domain);
iommu_group_state *iommu_group_priv(struct iommu_group *group);

int iommu_domain_attach_mm_shim(struct iommu_domain* domain,
                                ::us_mm_shim* shim);

int iommu_domain_attach_sim_pm(struct iommu_domain* domain,
                                struct sim_page_migration* pm);

}  /* namespace usr_linux_emu */

/* C-linkage facade for module-load hooks (Group 6 task 6.3). */
extern "C" {

/* iommu_emu_init: enumerate PCIe devices and create 1:1 groups (Group 6) */
int iommu_emu_init(void);

/* iommu_emu_shutdown: free all domains, groups, and IOASIDs */
void iommu_emu_shutdown(void);

}  /* extern "C" */
