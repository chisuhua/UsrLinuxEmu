/**
 * @file pcie_emu.cpp
 * @brief PcieEmuImpl main implementation: constructor, BAR, MMIO, bus master.
 *
 * Wires together config_space + capability_walk + msi_x modules via the
 * PcieEmuImpl class declared in pcie_emu_impl.h.
 */

#include "pcie_emu_impl.h"

#include <algorithm>
#include <cstring>
#include <iostream>

namespace usr_linux_emu {
namespace pcie_internal {

namespace {
constexpr const char* kTag = "PcieEmuImpl";
constexpr uint32_t kDefaultVendorId = 0x1234;
constexpr uint32_t kDefaultDeviceId = 0x5678;
}  // namespace

}  // namespace pcie_internal

PcieEmu* create_pcie_emu() {
  return new pcie_internal::PcieEmuImpl();
}

namespace pcie_internal {

PcieEmuImpl::PcieEmuImpl() {
  // Zero-initialize all members (config_space_ is zero-initialized by default).
  // Set defaults for vendor/device IDs in config space (offset 0x00).
  vendor_id_ = kDefaultVendorId;
  device_id_ = kDefaultDeviceId;
  write_config_word(0x00, static_cast<uint16_t>(vendor_id_ & 0xFFFF));
  write_config_word(0x02, static_cast<uint16_t>(device_id_ & 0xFFFF));

  // Header type 0 (endpoint) at offset 0x0E.
  write_config_byte(PCI_CFG_HEADER_TYPE, 0x00);

  // Mark capabilities list present in STATUS register (bit 4 at offset 0x06).
  const uint16_t status = read_config_word(PCI_CFG_STATUS);
  write_config_word(PCI_CFG_STATUS, static_cast<uint16_t>(status | PCI_STATUS_CAP_LIST));

  // Set the 5 standard capabilities. Order matters: first cap gets cap_ptr
  // set to its offset, subsequent caps' next_ptr is patched in.
  add_capability(PCI_CAP_ID_PM);
  add_capability(PCI_CAP_ID_MSI);
  add_capability(PCI_CAP_ID_PCIE);
  add_capability(PCI_CAP_ID_MSIX);
  add_capability(PCI_CAP_ID_VENDOR);
}

uint32_t PcieEmuImpl::get_vendor_id() const {
  return vendor_id_;
}

uint32_t PcieEmuImpl::get_device_id() const {
  return device_id_;
}

uint64_t PcieEmuImpl::get_bar_address(int index) const {
  if (index < 0 || index >= 6) return 0;
  return bars_[index].base;
}

uint64_t PcieEmuImpl::get_bar_size(int index) const {
  if (index < 0 || index >= 6) return 0;
  return bars_[index].size;
}

void PcieEmuImpl::assign_bar(int index, uint64_t base, size_t size, bool is_mmio) {
  if (index < 0 || index >= 6) {
    std::cerr << "[" << kTag << "] assign_bar: invalid index=" << index << "\n";
    return;
  }
  bars_[index].base = base;
  bars_[index].size = size;
  bars_[index].is_mmio = is_mmio;
  bars_[index].enabled = (size > 0);

  // Reflect BAR into config space (offset 0x10 + index*4). For simplicity,
  // we only support 32-bit BARs.
  if (is_mmio) {
    write_config_dword(static_cast<uint16_t>(PCI_CFG_BAR0 + index * 4),
                       static_cast<uint32_t>(base & 0xFFFFFFF0u));
  } else {
    write_config_dword(static_cast<uint16_t>(PCI_CFG_BAR0 + index * 4),
                       static_cast<uint32_t>(base & 0xFFFFFFFCu) | 0x1u);
  }

  // Allocate backing storage for BAR 0 (MMIO) and BAR 1 (RAM) for sim use.
  if (index == 0 && size > 0) {
    bar0_mmio_.assign(size, 0);
  } else if (index == 1 && size > 0) {
    bar1_ram_.assign(size, 0);
  }
}

int PcieEmuImpl::read_mmio(uint64_t bar_offset, void* buffer, size_t size) {
  if (bar_offset + size > bar0_mmio_.size()) {
    return -22;  // -EINVAL
  }
  std::memcpy(buffer, bar0_mmio_.data() + bar_offset, size);
  return 0;
}

int PcieEmuImpl::write_mmio(uint64_t bar_offset, const void* buffer, size_t size) {
  if (bar_offset + size > bar0_mmio_.size()) {
    return -22;  // -EINVAL
  }
  std::memcpy(bar0_mmio_.data() + bar_offset, buffer, size);
  return 0;
}

int PcieEmuImpl::read_ram(uint64_t bar_offset, void* buffer, size_t size) {
  if (bar_offset + size > bar1_ram_.size()) {
    return -22;  // -EINVAL
  }
  std::memcpy(buffer, bar1_ram_.data() + bar_offset, size);
  return 0;
}

int PcieEmuImpl::write_ram(uint64_t bar_offset, const void* buffer, size_t size) {
  if (bar_offset + size > bar1_ram_.size()) {
    return -22;  // -EINVAL
  }
  std::memcpy(bar1_ram_.data() + bar_offset, buffer, size);
  return 0;
}

void PcieEmuImpl::enable_bus_master() {
  const uint16_t cmd = read_config_word(PCI_CFG_COMMAND);
  write_config_word(PCI_CFG_COMMAND, static_cast<uint16_t>(cmd | PCI_COMMAND_BME));
  device_enabled_ = true;
  current_power_state_ = 0;  // D0
}

void PcieEmuImpl::disable_bus_master() {
  const uint16_t cmd = read_config_word(PCI_CFG_COMMAND);
  write_config_word(PCI_CFG_COMMAND, static_cast<uint16_t>(cmd & ~PCI_COMMAND_BME));
  device_enabled_ = false;
}

bool PcieEmuImpl::is_device_enabled() const {
  return device_enabled_;
}

uint8_t PcieEmuImpl::get_current_power_state() const {
  return current_power_state_;
}

}  // namespace pcie_internal
}  // namespace usr_linux_emu
