#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>

namespace usr_linux_emu {

/**
 * @brief PCI Express device emulation interface.
 *
 * Extends the original 11-method abstraction with config space access,
 * MSI-X lifecycle, handler registration, and capability chain walk.
 *
 * Scope boundary: existing methods MUST NOT change signature; only additions.
 */
class PcieEmu {
 public:
  virtual ~PcieEmu() = default;

  // === Original interface (DO NOT MODIFY) ===
  virtual uint32_t get_vendor_id() const = 0;
  virtual uint32_t get_device_id() const = 0;

  virtual uint64_t get_bar_address(int index) const = 0;
  virtual uint64_t get_bar_size(int index) const = 0;

  virtual void assign_bar(int index, uint64_t base, size_t size, bool is_mmio) = 0;

  virtual int read_mmio(uint64_t bar_offset, void* buffer, size_t size) = 0;
  virtual int write_mmio(uint64_t bar_offset, const void* buffer, size_t size) = 0;

  virtual int read_ram(uint64_t bar_offset, void* buffer, size_t size) = 0;
  virtual int write_ram(uint64_t bar_offset, const void* buffer, size_t size) = 0;

  virtual void enable_bus_master() = 0;
  virtual void disable_bus_master() = 0;

  // === Config space access (§2.1) ===
  // 4KB config space (256B PCI + 3840B PCIe ext)
  // offset range: 0x000 ~ 0xFFF
  virtual uint8_t  read_config_byte(uint16_t offset) const = 0;
  virtual uint16_t read_config_word(uint16_t offset) const = 0;
  virtual uint32_t read_config_dword(uint16_t offset) const = 0;
  virtual void write_config_byte(uint16_t offset, uint8_t value) = 0;
  virtual void write_config_word(uint16_t offset, uint16_t value) = 0;
  virtual void write_config_dword(uint16_t offset, uint32_t value) = 0;

  // === MSI-X lifecycle (§2.2, §2.3) ===
  // setup_msix: allocate vector table with nr_vectors (1..2048, default 16)
  // Returns 0 on success, -EINVAL if nr_vectors > 2048 or == 0
  virtual int setup_msix(uint16_t nr_vectors, uint32_t table_offset) = 0;
  // disable_msix: free vector table + clear PBA
  virtual int disable_msix() = 0;
  // inject_msix_interrupt: set PBA bit, invoke registered handler
  // Returns 0 on success, -ENXIO if not enabled, -EINVAL if vector_id OOR
  virtual int inject_msix_interrupt(int vector_id) = 0;

  // MSI-X handler registration (§2.3) - MUST be available before §4
  // Driver registers callback; sim triggers via inject_msix_interrupt()
  using MsixHandler = std::function<void(int vector_id)>;
  virtual void register_msix_handler(MsixHandler handler) = 0;

  // === Capability chain walk (§2.4) ===
  // Returns config space offset of the first capability matching cap_id,
  // or 0 if not found. PCI_CAP_ID_ANY (0xFF) returns first capability.
  virtual uint16_t find_capability(uint8_t cap_id) const = 0;

  // === Device lifecycle hooks (used by §6.3) ===
  // Returns true if the device is currently enabled.
  virtual bool is_device_enabled() const = 0;
  // Returns current power state (0=D0, 1=D1, 2=D2, 3=D3hot).
  virtual uint8_t get_current_power_state() const = 0;

  // === MSI-X introspection (used by compat layer + tests) ===
  // Number of vectors configured (0 if MSI-X not enabled).
  virtual uint16_t msix_vector_count() const = 0;
  // Whether MSI-X is currently enabled.
  virtual bool msix_enabled() const = 0;
};

/**
 * @brief Factory: construct the default PcieEmu implementation (PcieEmuImpl).
 *
 * Defined in src/kernel/pcie/pcie_emu.cpp. Returns a heap-allocated
 * PcieEmu* owned by the caller.
 */
PcieEmu* create_pcie_emu();

}  // namespace usr_linux_emu
