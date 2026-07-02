/**
 * @file pci.h
 * @brief Linux-compat subset of <linux/pci.h> for user-space PCIe simulation.
 *
 * Per design.md Decision 5: spec-driven incremental subset.
 * Implements only what the 1.4 KFD integration needs; explicitly omits DMA,
 * IRQ framework (MSI-X handler is used instead), FLR, ACS, SR-IOV.
 *
 * Driver code uses these wrappers against a PcieEmu* device pointer.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "pcie/pcie_emu.h"

namespace usr_linux_emu {
namespace linux_compat {
namespace pci {

// Standard PCI capability IDs (subset; aligned with PcieEmuImpl).
constexpr uint8_t PCI_CAP_ID_PM     = 0x01;
constexpr uint8_t PCI_CAP_ID_MSI    = 0x05;
constexpr uint8_t PCI_CAP_ID_VENDOR = 0x09;
constexpr uint8_t PCI_CAP_ID_PCIE   = 0x10;
constexpr uint8_t PCI_CAP_ID_MSIX   = 0x11;
constexpr uint8_t PCI_CAP_ID_ANY    = 0xFF;

// IORESOURCE flags (Linux kernel values).
constexpr unsigned long IORESOURCE_IO   = 0x00000100;
constexpr unsigned long IORESOURCE_MEM  = 0x00000200;

// PCI power states.
constexpr uint8_t PCI_D0 = 0;
constexpr uint8_t PCI_D1 = 1;
constexpr uint8_t PCI_D2 = 2;
constexpr uint8_t PCI_D3hot = 3;

// Standard config space offsets.
constexpr uint8_t PCI_VENDOR_ID = 0x00;
constexpr uint8_t PCI_DEVICE_ID = 0x02;
constexpr uint8_t PCI_COMMAND   = 0x04;
constexpr uint8_t PCI_STATUS    = 0x06;

// pci_dev is a thin wrapper around PcieEmu* in this user-space layer.
// Using a typedef keeps the API surface close to Linux while permitting
// easy migration: drivers store pcie_emu* but pass it to pci_* APIs.
using pci_dev = PcieEmu;

// === Config space read/write (per §6.1) ===
inline int pci_read_config_byte(const pci_dev* dev, int where, uint8_t* val) {
  if (!dev || !val) return -14;  // -EFAULT
  *val = dev->read_config_byte(static_cast<uint16_t>(where));
  return 0;
}

inline int pci_read_config_word(const pci_dev* dev, int where, uint16_t* val) {
  if (!dev || !val) return -14;
  *val = dev->read_config_word(static_cast<uint16_t>(where));
  return 0;
}

inline int pci_read_config_dword(const pci_dev* dev, int where, uint32_t* val) {
  if (!dev || !val) return -14;
  *val = dev->read_config_dword(static_cast<uint16_t>(where));
  return 0;
}

inline int pci_write_config_byte(const pci_dev* dev, int where, uint8_t val) {
  if (!dev) return -14;
  const_cast<pci_dev*>(dev)->write_config_byte(static_cast<uint16_t>(where), val);
  return 0;
}

inline int pci_write_config_word(const pci_dev* dev, int where, uint16_t val) {
  if (!dev) return -14;
  const_cast<pci_dev*>(dev)->write_config_word(static_cast<uint16_t>(where), val);
  return 0;
}

inline int pci_write_config_dword(const pci_dev* dev, int where, uint32_t val) {
  if (!dev) return -14;
  const_cast<pci_dev*>(dev)->write_config_dword(static_cast<uint16_t>(where), val);
  return 0;
}

// === BAR / resource API (per §6.2) ===
inline uint64_t pci_resource_start(const pci_dev* dev, int bar) {
  if (!dev) return 0;
  return dev->get_bar_address(bar);
}

inline uint64_t pci_resource_len(const pci_dev* dev, int bar) {
  if (!dev) return 0;
  return dev->get_bar_size(bar);
}

inline unsigned long pci_resource_flags(const pci_dev* dev, int bar) {
  if (!dev) return 0;
  if (dev->get_bar_size(bar) == 0) return 0;  // unconfigured BAR
  // Determine MMIO vs IO from the BAR's config-space value: bit 0 set = I/O space.
  // assign_bar() writes the PCI-encoded value (bit 0 set for I/O, cleared for MMIO).
  const uint32_t bar_value = dev->read_config_dword(
      static_cast<uint16_t>(0x10 + bar * 4));
  return (bar_value & 0x1u) ? IORESOURCE_IO : IORESOURCE_MEM;
}

// === Device enable/disable (per §6.3) ===
inline int pci_enable_device(pci_dev* dev) {
  if (!dev) return -14;
  dev->enable_bus_master();
  return 0;
}

inline void pci_disable_device(pci_dev* dev) {
  if (!dev) return;
  dev->disable_bus_master();
}

// === Capability chain walk (per §6.4) ===
inline int pci_find_capability(const pci_dev* dev, int cap) {
  if (!dev) return 0;
  return dev->find_capability(static_cast<uint8_t>(cap));
}

inline int pci_find_next_capability(const pci_dev* dev, int pos, int cap) {
  if (!dev || pos == 0) return 0;
  // Walk config space starting at pos, reading cap_id at each offset.
  // Each capability: [cap_id] [next_ptr] [data...]. next_ptr == 0 ends.
  uint8_t next = static_cast<uint8_t>(pos & 0xFF);
  while (next != 0) {
    const uint8_t id = dev->read_config_byte(next);
    if (cap == PCI_CAP_ID_ANY || id == cap) {
      return next;
    }
    next = dev->read_config_byte(static_cast<uint16_t>(next + 1));
  }
  return 0;
}

}  // namespace pci
}  // namespace linux_compat
}  // namespace usr_linux_emu
