/*
 * pcie_integration.cpp — PCIe device → iommu_group top-level integration
 *
 * Stage 1.1 (IOMMU + ATS): provides iommu_emu_init() lifecycle and a
 * registration API for stage-1.0 PciDevice instances. Per design.md
 * Decision 5 (1 PCIe device = 1 iommu_group) and Decision 6
 * (no full enumeration — driver code calls iommu_register_pci_device()).
 *
 * NOTE on stage-1.0 ↔ stage-1.1 contract:
 *   - stage-1.0 provides `include/kernel/pcie_device.h` (PciDevice abstract
 *     class + PcieRootComplex). It does NOT expose a global enumeration API
 *     for "all PCI devices discovered so far".
 *   - To avoid forcing a stage-1.0 contract change, stage-1.1 adopts a
 *     push model: PciDevice-derived objects call
 *     iommu_register_pci_device(this) during construction.
 *   - stage-1.4 (KFD integration) may shift to a pull model by adding
 *     a PciDevice enumeration API to stage-1.0's interface, but this is
 *     explicitly out of scope for stage-1.1.
 */

#include "iommu_internal.h"

#include <linux_compat/iommu/iommu.h>
#include <linux_compat/iommu/iommu_domain.h>
#include <linux_compat/iommu/iommu_group.h>

#include <cstdio>
#include <cstdlib>

extern "C" {

/*
 * Register a PCIe device with the IOMMU emulator. Creates a fresh
 * iommu_group containing only this device, attaches a default domain
 * (IOMMU_DOMAIN_DMA), and returns the group ID. On failure, returns
 * NULL with errno set.
 *
 * Called by stage-1.0 PciDevice-derived constructors. Idempotent: a
 * device that has already been registered returns its existing group.
 */
struct iommu_group *iommu_register_pci_device(void *pci_dev_handle)
{
	if (!pci_dev_handle)
		return nullptr;

	auto *g_state = usr_linux_emu::iommu_emu_global_state();
	if (!g_state || !g_state->initialized)
		return nullptr;

	/* Idempotency check: return existing group if device already registered */
	auto existing = g_state->device_to_group.find(static_cast<struct device *>(pci_dev_handle));
	if (existing != g_state->device_to_group.end())
		return existing->second;

	/* 1 device = 1 group (per design.md Decision 5) */
	auto *group = iommu_group_alloc();
	if (!group)
		return nullptr;

	if (iommu_group_add_device(group, static_cast<struct device *>(pci_dev_handle)) != IOMMU_ERR_OK) {
		iommu_group_free(group);
		return nullptr;
	}

	/*
	 * Allocate a default domain for this group with the standard DMA
	 * operations attached. KFD in stage-1.4 will use this domain to
	 * call iommu_map / iommu_unmap per its topology.
	 */
	auto *domain = iommu_domain_alloc(IOMMU_DOMAIN_DMA);
	if (!domain) {
		iommu_group_free(group);
		return nullptr;
	}
	domain->ops = iommu_default_ops_get();
	iommu_group_set_default_domain(group, domain);

	std::fprintf(stderr,
		     "[iommu] registered pci device %p as group id=%d "
		     "(domain %p, ops %p)\n",
		     pci_dev_handle, group->id, (void *)domain, (void *)domain->ops);
	return group;
}

/*
 * Unregister a previously registered PCIe device.
 */
int iommu_unregister_pci_device(void *pci_dev_handle)
{
	if (!pci_dev_handle)
		return IOMMU_ERR_EINVAL;

	auto *g_state = usr_linux_emu::iommu_emu_global_state();
	if (!g_state || !g_state->initialized)
		return IOMMU_ERR_ENOSYS;

	auto *dev = static_cast<struct device *>(pci_dev_handle);
	auto it = g_state->device_to_group.find(dev);
	if (it == g_state->device_to_group.end())
		return IOMMU_ERR_ENODEV;

	struct iommu_group *group = it->second;
	return iommu_group_remove_device(group, dev);
}

/*
 * Init entry point is defined in iommu_emu_state.cpp (canonical location).
 * Group 6 adds the register API; init/shutdown stay in emu_state.
 */

}  /* extern "C" */
