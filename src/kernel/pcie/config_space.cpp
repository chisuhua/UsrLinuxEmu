/**
 * @file config_space.cpp
 * @brief PcieEmuImpl config space read/write methods.
 *
 * Per design.md Decision 2: single 4KB buffer, offset 0x000-0xFFF.
 * Out-of-range access logs an error and leaves buffer unchanged.
 */

#include "pcie_emu_impl.h"

#include <cstring>
#include <iostream>

namespace usr_linux_emu {
namespace pcie_internal {

namespace {
constexpr const char* kTag = "PcieEmuImpl/config_space";
}

uint8_t PcieEmuImpl::read_config_byte(uint16_t offset) const {
  if (offset >= PCI_CONFIG_SPACE_SIZE) {
    std::cerr << "[" << kTag << "] read_config_byte offset=" << offset
              << " out of range\n";
    return 0xFF;
  }
  return config_space_[offset];
}

uint16_t PcieEmuImpl::read_config_word(uint16_t offset) const {
  if (offset > PCI_CONFIG_SPACE_SIZE - 2) {
    std::cerr << "[" << kTag << "] read_config_word offset=" << offset
              << " out of range\n";
    return 0xFFFF;
  }
  uint16_t value = 0;
  std::memcpy(&value, &config_space_[offset], sizeof(value));
  return value;
}

uint32_t PcieEmuImpl::read_config_dword(uint16_t offset) const {
  if (offset > PCI_CONFIG_SPACE_SIZE - 4) {
    std::cerr << "[" << kTag << "] read_config_dword offset=" << offset
              << " out of range\n";
    return 0xFFFFFFFFu;
  }
  uint32_t value = 0;
  std::memcpy(&value, &config_space_[offset], sizeof(value));
  return value;
}

void PcieEmuImpl::write_config_byte(uint16_t offset, uint8_t value) {
  if (offset >= PCI_CONFIG_SPACE_SIZE) {
    std::cerr << "[" << kTag << "] write_config_byte offset=" << offset
              << " out of range\n";
    return;
  }
  config_space_[offset] = value;
}

void PcieEmuImpl::write_config_word(uint16_t offset, uint16_t value) {
  if (offset > PCI_CONFIG_SPACE_SIZE - 2) {
    std::cerr << "[" << kTag << "] write_config_word offset=" << offset
              << " out of range\n";
    return;
  }
  std::memcpy(&config_space_[offset], &value, sizeof(value));
}

void PcieEmuImpl::write_config_dword(uint16_t offset, uint32_t value) {
  if (offset > PCI_CONFIG_SPACE_SIZE - 4) {
    std::cerr << "[" << kTag << "] write_config_dword offset=" << offset
              << " out of range\n";
    return;
  }
  std::memcpy(&config_space_[offset], &value, sizeof(value));
}

}  // namespace pcie_internal
}  // namespace usr_linux_emu
