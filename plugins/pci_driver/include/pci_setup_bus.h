#pragma once

/**
 * @file pci_setup_bus.h
 * @brief T4.2: BAR resource assignment from topology to PcieEmu.
 *
 * Stage 5.5.2 Wave 4 — assigns BAR address/size from topology to a
 * PcieEmu instance. Uses the TopologyDevice's bars array (from
 * sim_hardware) to drive the assignment.
 */

#include <cstddef>
#include <cstdint>

#include "pcie/pcie_emu.h"
#include "pcie/host_bridge.h"
#include "topology.h"

namespace usr_linux_emu {
namespace pci {

/**
 * @brief Assign PCI BAR resources for a discovered device.
 *
 * Reads the topology JSON and finds the TopologyDevice matching the
 * DiscoveredDevice's vendor/device/class. For each BAR in the
 * TopologyDevice, calls PcieEmu::assign_bar with a deterministic base
 * address (0xF0000000 + index * size).
 *
 * @param emu           PcieEmu instance (must outlive this call).
 * @param dev           DiscoveredDevice to match.
 * @param topology_path Path to the topology JSON.
 * @return 0 on success, negative errno on failure.
 */
int pci_bus_assign_resources(
    PcieEmu* emu,
    const usr_linux_emu::sim_hardware::pcie::DiscoveredDevice* dev,
    const char* topology_path);

}  // namespace pci
}  // namespace usr_linux_emu
