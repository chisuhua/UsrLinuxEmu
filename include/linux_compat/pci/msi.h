/**
 * @file msi.h
 * @brief Linux-compat subset of MSI-X API for user-space PCIe simulation.
 *
 * Per design.md Decision 5: only pci_enable_msix, pci_disable_msix,
 * pci_msix_vec_count. No MSI (non-X) implementation in this layer.
 */

#pragma once

#include <cstdint>

#include "pcie/pcie_emu.h"

namespace usr_linux_emu {
namespace linux_compat {
namespace msi {

// Mirrors Linux kernel struct msix_entry layout.
struct msix_entry {
  uint32_t vector;     // kernel-assigned: vector index
  uint32_t entry;      // driver-supplied: entry index in the table
};

// === MSI-X API (per §6.5) ===
inline int pci_msix_vec_count(const PcieEmu* dev) {
  if (!dev) return 0;
  return static_cast<int>(dev->msix_vector_count());
}

inline int pci_enable_msix(PcieEmu* dev, const msix_entry* entries, int nvec) {
  if (!dev) return -14;
  if (nvec <= 0) return -22;  // -EINVAL
  if (nvec > 2048) return -22;
  // entries table content is ignored at this layer; the sim owns the table.
  (void)entries;
  return dev->setup_msix(static_cast<uint16_t>(nvec), 0);
}

inline void pci_disable_msix(PcieEmu* dev) {
  if (!dev) return;
  dev->disable_msix();
}

}  // namespace msi
}  // namespace linux_compat
}  // namespace usr_linux_emu
