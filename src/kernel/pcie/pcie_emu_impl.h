#pragma once

/**
 * @file pcie_emu_impl.h
 * @brief Internal header for PcieEmuImpl (concrete PcieEmu subclass).
 *
 * Not exposed in include/ - this is implementation detail of src/kernel/pcie/.
 */

#include "pcie/pcie_emu.h"

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

namespace usr_linux_emu {
namespace pcie_internal {

/** Standard PCI capability IDs (subset per design.md Decision 5). */
constexpr uint8_t PCI_CAP_ID_PM         = 0x01;  // Power Management
constexpr uint8_t PCI_CAP_ID_VENDOR     = 0x09;  // Vendor Specific
constexpr uint8_t PCI_CAP_ID_MSI        = 0x05;  // MSI
constexpr uint8_t PCI_CAP_ID_PCIE       = 0x10;  // PCI Express
constexpr uint8_t PCI_CAP_ID_MSIX       = 0x11;  // MSI-X
constexpr uint8_t PCI_CAP_ID_ANY        = 0xFF;  // Wildcard (used by compat layer)

/** Fixed config-space offsets for the 5 standard capabilities. */
constexpr uint16_t kCapOffsetPM          = 0x40;
constexpr uint16_t kCapOffsetMSI         = 0x50;
constexpr uint16_t kCapOffsetPCIe        = 0x68;
constexpr uint16_t kCapOffsetMSIX        = 0x80;
constexpr uint16_t kCapOffsetVendor      = 0xA0;

/** PCI config space constants. */
constexpr uint16_t PCI_CONFIG_SPACE_SIZE = 4096;          // 4KB total
constexpr uint16_t PCI_CFG_STATUS        = 0x06;          // Status register
constexpr uint16_t PCI_CFG_CAP_PTR       = 0x34;          // Capabilities pointer
constexpr uint16_t PCI_CFG_COMMAND       = 0x04;          // Command register
constexpr uint16_t PCI_CFG_HEADER_TYPE   = 0x0E;          // Header type
constexpr uint16_t PCI_CFG_BAR0          = 0x10;          // BAR0 offset

/** PCI BAR value bit 0 indicates I/O space (vs MMIO). */
constexpr uint32_t PCI_BAR_IO_TYPE_BIT   = 0x1u;

constexpr uint16_t PCI_STATUS_CAP_LIST   = (1u << 4);     // Bit 4 of STATUS
constexpr uint16_t PCI_COMMAND_BME       = (1u << 2);     // Bus Master Enable
constexpr uint16_t PCI_COMMAND_MSE       = (1u << 1);     // Memory Space Enable
constexpr uint16_t PCI_COMMAND_IO        = (1u << 0);     // I/O Space Enable

/** MSI-X table / PBA constraints (per design.md Decision 3). */
constexpr uint16_t MSIX_MAX_VECTORS      = 2048;
constexpr uint16_t MSIX_DEFAULT_VECTORS  = 16;
constexpr size_t   PBA_SIZE_BYTES        = 256;           // 2048 bits / 8

/**
 * One entry in the MSI-X vector table (in BAR memory).
 * Format matches Linux kernel msix_entry layout conceptually.
 */
struct MsixEntry {
  uint32_t addr_lo;
  uint32_t addr_hi;
  uint32_t data;
  uint32_t control;  // bit 0 = mask
};

/**
 * One PCI capability in the chain.
 * - config_offset: where the cap_id byte lives in config space
 * - next_offset: where the next capability starts, or 0 if last
 * - data: cap-specific register bytes (excluding the cap_id/next_ptr header)
 */
struct PciCapability {
  uint8_t cap_id;
  uint16_t config_offset;
  uint16_t next_offset;
  std::vector<uint8_t> data;
};

/**
 * Concrete PcieEmu implementation.
 *
 * Layout in config space:
 *   0x00: vendor_id (16b) | device_id (16b)
 *   0x04: command (16b)   | status (16b)
 *   0x06: status (bit 4 = capabilities list present)
 *   0x34: capabilities pointer (offset of first capability)
 *   0x40+: capability chain (PM, MSI, PCIe, MSI-X, Vendor)
 *
 * State: 4KB config space buffer + capability chain + MSI-X table/PBA + BARs.
 */
class PcieEmuImpl : public PcieEmu {
 public:
  PcieEmuImpl();
  ~PcieEmuImpl() override = default;

  // === PcieEmu original interface ===
  uint32_t get_vendor_id() const override;
  uint32_t get_device_id() const override;
  uint64_t get_bar_address(int index) const override;
  uint64_t get_bar_size(int index) const override;
  void assign_bar(int index, uint64_t base, size_t size, bool is_mmio) override;
  int read_mmio(uint64_t bar_offset, void* buffer, size_t size) override;
  int write_mmio(uint64_t bar_offset, const void* buffer, size_t size) override;
  int read_ram(uint64_t bar_offset, void* buffer, size_t size) override;
  int write_ram(uint64_t bar_offset, const void* buffer, size_t size) override;
  void enable_bus_master() override;
  void disable_bus_master() override;

  // === Config space access (config_space.cpp) ===
  uint8_t  read_config_byte(uint16_t offset) const override;
  uint16_t read_config_word(uint16_t offset) const override;
  uint32_t read_config_dword(uint16_t offset) const override;
  void write_config_byte(uint16_t offset, uint8_t value) override;
  void write_config_word(uint16_t offset, uint16_t value) override;
  void write_config_dword(uint16_t offset, uint32_t value) override;

  // === MSI-X (msi_x.cpp) ===
  int setup_msix(uint16_t nr_vectors, uint32_t table_offset) override;
  int disable_msix() override;
  int inject_msix_interrupt(int vector_id) override;
  void register_msix_handler(MsixHandler handler) override;

  // === Capability chain (capability_walk.cpp) ===
  uint16_t find_capability(uint8_t cap_id) const override;

  // === Device lifecycle ===
  bool is_device_enabled() const override;
  uint8_t get_current_power_state() const override;

  // === Internal helpers (used by capability_walk.cpp constructor) ===
  void add_capability(uint8_t cap_id);

  // Read-only accessors for tests and other internal modules.
  const std::vector<PciCapability>& capabilities() const { return capabilities_; }
  const std::array<MsixEntry, MSIX_MAX_VECTORS>& msix_table() const { return msix_table_; }
  const std::array<uint8_t, PBA_SIZE_BYTES>& pba() const { return pba_; }
  const std::array<uint8_t, PCI_CONFIG_SPACE_SIZE>& config_space() const { return config_space_; }
  uint16_t msix_vector_count() const { return msix_nr_vectors_; }
  bool msix_enabled() const { return msix_enabled_; }
  uint32_t msix_table_offset() const { return msix_table_offset_; }

 private:
  // === Storage ===
  std::array<uint8_t, PCI_CONFIG_SPACE_SIZE> config_space_{};  // zero-initialized
  std::vector<PciCapability> capabilities_;
  std::array<MsixEntry, MSIX_MAX_VECTORS> msix_table_{};
  std::array<uint8_t, PBA_SIZE_BYTES> pba_{};

  // === BARs (6 standard) ===
  struct Bar {
    uint64_t base = 0;
    uint64_t size = 0;
    bool is_mmio = true;
    bool enabled = false;
  };
  std::array<Bar, 6> bars_{};

  // === MMIO/RAM region (single contiguous, sized by BAR0) ===
  std::vector<uint8_t> bar0_mmio_;
  std::vector<uint8_t> bar1_ram_;

  // === MSI-X state ===
  bool msix_enabled_ = false;
  uint16_t msix_nr_vectors_ = 0;
  uint32_t msix_table_offset_ = 0;
  MsixHandler msix_handler_;

  // === Device lifecycle ===
  bool device_enabled_ = false;
  uint8_t current_power_state_ = 0;  // 0 = D0
  uint32_t vendor_id_ = 0;
  uint32_t device_id_ = 0;
};

}  // namespace pcie_internal
}  // namespace usr_linux_emu
