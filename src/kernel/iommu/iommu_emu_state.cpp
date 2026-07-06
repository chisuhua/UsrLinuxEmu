/*
 * iommu_emu_state.cpp — Global IOMMU emulator state instantiation + init/shutdown
 *
 * Stage 1.1 (IOMMU + ATS): provides the global state singleton reached via
 * iommu_emu_global_state(). iommu_emu_init() is the lifecycle entry point
 * (called by Group 6 module load hook); it performs PCIe device enumeration
 * (Groups 6 wiring) and creates 1:1 iommu_groups.
 */

#include "iommu_internal.h"

#include <kernel/uvm/mm_shim.h>
#include <linux_compat/mmu_notifier.h>

#include <cstdlib>
#include <cstring>

namespace usr_linux_emu {

static struct iommu_emu_state g_state;

struct iommu_emu_state *iommu_emu_global_state(void)
{
	return &g_state;
}

}  /* namespace usr_linux_emu */

extern "C" {

int iommu_emu_init(void)
{
	auto *g = usr_linux_emu::iommu_emu_global_state();
	if (!g)
		return -1;
	if (g->initialized)
		return 0;

	std::memset(g->ioasid_table, 0, sizeof(g->ioasid_table));
	g->ioasid_next = 1;
	g->initialized = true;

	/*
	 * Group 6 task 6.1 wires PCIe enumeration here. Stage-1.1 scope:
	 * the PciDevice abstract class is already provided by stage-1.0
	 * (include/kernel/pcie_device.h) but the full module traversal
	 * (ModuleLoader-based enumeration) is wired by Group 6.
	 */
	return 0;
}

void iommu_emu_shutdown(void)
{
	auto *g = usr_linux_emu::iommu_emu_global_state();
	if (!g || !g->initialized)
		return;

	/* Free groups first (each will free its default_domain) */
	for (auto *group : g->groups) {
		if (group) {
			/* Remove all members before freeing */
			while (group->members) {
				struct device *dev = group->members->dev;
				iommu_group_remove_device(group, dev);
			}
			iommu_group_free(group);
		}
	}
	g->groups.clear();

	/* Free remaining domains (those not attached as a default_domain) */
	for (auto *domain : g->domains) {
		if (domain)
			iommu_domain_free(domain);
	}
	g->domains.clear();

	/* Free IOASIDs */
	for (unsigned int i = 1; i < 0xFFFF; i++) {
		if (g->ioasid_table[i]) {
			std::free(g->ioasid_table[i]);
			g->ioasid_table[i] = nullptr;
		}
	}

	g->device_to_group.clear();
	g->initialized = false;
}

}  /* extern "C" */

namespace usr_linux_emu {

struct iommu_domain_state *iommu_domain_priv(struct iommu_domain *domain)
{
	if (!domain)
		return nullptr;
	return static_cast<usr_linux_emu::iommu_domain_state *>(domain->priv);
}

struct iommu_group_state *iommu_group_priv(struct iommu_group *group)
{
	if (!group)
		return nullptr;
	return static_cast<usr_linux_emu::iommu_group_state *>(group->priv);
}

}  /* namespace usr_linux_emu */

extern "C" {

/* Stage 2.1.2: attach a us_mm_shim to an iommu_domain.  Called by
 * Stage 2.1.2 setup code (e.g., GPU driver open path) to associate a
 * PID + VMA list with the domain so iommu_invalidate_register_notifier
 * can propagate context to mmu_notifier callbacks. */
int iommu_domain_attach_mm_shim(struct iommu_domain* domain,
                                struct us_mm_shim* shim) {
  if (!domain || !shim) return -22;  /* -EINVAL */
  auto* state = usr_linux_emu::iommu_domain_priv(domain);
  if (!state) return -22;
  state->mm_shim = shim;
  return 0;
}

/*
 * Issue #21 fix: explicit public helper to associate an mm_struct* with an
 * iommu_domain.  Replaces the old implicit "d->priv == mm_struct*"
 * contract that Stage 2.1.2's mm_shim propagation silently broke (UB
 * exposed by clang+g++ stack layout).  All callers that previously wrote
 * `domain->priv = &mm;` should now use:
 *     iommu_domain_attach_mm(domain, &mm);
 * (which sets state->mm without touching priv).
 */
int iommu_domain_attach_mm(struct iommu_domain* domain,
                           struct mm_struct* mm) {
  if (!domain || !mm) return -22;  /* -EINVAL */
  auto* state = usr_linux_emu::iommu_domain_priv(domain);
  if (!state) return -22;
  state->mm = mm;
  return 0;
}

}  /* extern "C" */
