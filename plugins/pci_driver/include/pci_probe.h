#pragma once

/**
 * @file pci_probe.h
 * @brief T4.1: PCI probe bridge — bridge pci_driver to sim_hardware
 *        host_bridge_enumerate.
 *
 * Stage 5.5.2 Wave 4 — minimal invasive entry point that delegates
 * enumeration to usr_linux_emu::sim_hardware::pcie::host_bridge_enumerate.
 *
 * Boundary (per change-2 design.md §13.3 mock target):
 *   pci_driver (Q2) -> sim_hardware (Q3) is the LEGITIMATE direction.
 *   This header is the single bridge point — pci_driver.cpp #includes
 *   sim_hardware headers ONLY here, not elsewhere.
 */

#include <cstddef>
#include <cstdint>

#include "pcie/host_bridge.h"

namespace usr_linux_emu {
namespace pci {

/**
 * @brief Probe-equal of host_bridge_enumerate for the pci_driver layer.
 *
 * Thin wrapper around sim_hardware::pcie::host_bridge_enumerate so callers
 * in pci_driver can avoid touching sim_hardware headers directly.
 *
 * @param devices      Output array of DiscoveredDevice.
 * @param max_devices  Max number of entries to write.
 * @param out_count    On success, set to the total number of devices
 *                     available in topology (not the number written).
 * @param topology_path  Path to the topology JSON.
 * @return 0 success, negative errno on failure (see host_bridge.h).
 */
int pci_probe_enumerate_from_sim_hardware(
    usr_linux_emu::sim_hardware::pcie::DiscoveredDevice* devices,
    size_t max_devices,
    size_t* out_count,
    const char* topology_path);

}  // namespace pci
}  // namespace usr_linux_emu
