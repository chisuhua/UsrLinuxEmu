/**
 * @file msi_x.cpp
 * @brief PcieEmuImpl MSI-X vector table + PBA + interrupt injection.
 *
 * Per design.md Decision 3: vector table (max 2048 entries) + PBA (256 bytes).
 * PBA is a bit array where bit N in PBA[N/8] tracks vector N's pending state.
 */

#include "pcie_emu_impl.h"

#include <cstring>
#include <iostream>

namespace usr_linux_emu {
namespace pcie_internal {

namespace {
constexpr const char* kTag = "PcieEmuImpl/msi_x";
}

int PcieEmuImpl::setup_msix(uint16_t nr_vectors, uint32_t table_offset) {
  if (nr_vectors == 0 || nr_vectors > MSIX_MAX_VECTORS) {
    std::cerr << "[" << kTag << "] setup_msix: invalid nr_vectors=" << nr_vectors << "\n";
    return -22;  // -EINVAL
  }

  // Reset table and PBA, then enable.
  for (auto& entry : msix_table_) {
    entry = MsixEntry{};
  }
  pba_.fill(0);

  msix_nr_vectors_ = nr_vectors;
  msix_table_offset_ = table_offset;
  msix_enabled_ = true;

  // Update the MSI-X message control field in config space: table size = N - 1.
  const uint16_t table_size_field = static_cast<uint16_t>(nr_vectors - 1);
  // Message control field is at cap_offset + 2 (low byte: bits 0-7 of size)
  // and cap_offset + 3 (high byte: bits 0-2 of size, bits 3-7 reserved).
  config_space_[kCapOffsetMSIX + 2] = static_cast<uint8_t>(table_size_field & 0xFF);
  config_space_[kCapOffsetMSIX + 3] =
      static_cast<uint8_t>((config_space_[kCapOffsetMSIX + 3] & 0xF8) |
                           ((table_size_field >> 8) & 0x07));

  return 0;
}

int PcieEmuImpl::disable_msix() {
  if (!msix_enabled_) {
    return 0;  // idempotent
  }
  for (auto& entry : msix_table_) {
    entry = MsixEntry{};
  }
  pba_.fill(0);
  msix_enabled_ = false;
  msix_nr_vectors_ = 0;
  msix_table_offset_ = 0;
  return 0;
}

int PcieEmuImpl::inject_msix_interrupt(int vector_id) {
  if (!msix_enabled_) {
    return -6;  // -ENXIO
  }
  if (vector_id < 0 || vector_id >= msix_nr_vectors_) {
    std::cerr << "[" << kTag << "] inject_msix_interrupt: vector_id=" << vector_id
              << " out of range (nr_vectors=" << msix_nr_vectors_ << ")\n";
    return -22;  // -EINVAL
  }

  // Set the PBA bit for this vector.
  pba_[vector_id / 8] |= static_cast<uint8_t>(1u << (vector_id % 8));

  // Invoke the registered handler (if any).
  if (msix_handler_) {
    msix_handler_(vector_id);
  }
  return 0;
}

void PcieEmuImpl::register_msix_handler(MsixHandler handler) {
  msix_handler_ = std::move(handler);
}

}  // namespace pcie_internal
}  // namespace usr_linux_emu
