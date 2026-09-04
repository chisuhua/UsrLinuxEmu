#pragma once

/**
 * @file pcie_enable_device.h
 * @brief T4.3: Enable PCI device (write PCI_COMMAND MEMORY|IO).
 *
 * Stage 5.5.2 Wave 4 — minimal linux-style pci_enable_device that
 * sets the MEM and IO bits in the PCI_COMMAND register.
 */

#include "pcie/pcie_emu.h"

namespace usr_linux_emu {
namespace pci {

/**
 * @brief Enable a PCI device (set MEMORY|IO bits in PCI_COMMAND).
 *
 * @param emu PcieEmu instance.
 * @return 0 on success, negative errno on failure.
 */
int pci_enable_device(PcieEmu* emu);

}  // namespace pci
}  // namespace usr_linux_emu
