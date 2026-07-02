/**
 * @file capability_walk.cpp
 * @brief PcieEmuImpl PCI capability chain management.
 *
 * Per design.md Decision 4: vector<PciCapability> with cap_id, config_offset,
 * next_offset, data. Chain is laid out in config space at offsets >= 0x40.
 *
 * Standard capabilities (5) per design.md Decision 5:
 *   Power Management (0x01), MSI (0x05), Vendor Specific (0x09),
 *   PCIe (0x10), MSI-X (0x11).
 *
 * Default config space layout:
 *   0x40: PM
 *   0x50: MSI
 *   0x68: PCIe
 *   0x80: MSI-X
 *   0xA0: Vendor Specific
 */

#include "pcie_emu_impl.h"

#include <cstring>

namespace usr_linux_emu {
namespace pcie_internal {

namespace {
/** Add a fixed-size block of cap-specific data to the capability. */
void write_cap_data(std::vector<uint8_t>& data, size_t size) {
  data.assign(size, 0);
}
}  // namespace

uint16_t PcieEmuImpl::find_capability(uint8_t cap_id) const {
  for (const auto& cap : capabilities_) {
    if (cap_id == PCI_CAP_ID_ANY || cap.cap_id == cap_id) {
      return cap.config_offset;
    }
  }
  return 0;
}

void PcieEmuImpl::add_capability(uint8_t cap_id) {
  PciCapability cap{};
  cap.cap_id = cap_id;

  switch (cap_id) {
    case PCI_CAP_ID_PM: {
      // Power Management: 8 bytes total
      //   0x00: cap_id (0x01)
      //   0x01: next_ptr
      //   0x02: PMC (PM capabilities) - D0 supported, no D1/D2
      //   0x04: PMCSR (PM control/status) - power state in bits 0-1
      //   0x06: PMCSR_BSE (bridge extensions, unused for endpoint)
      cap.config_offset = kCapOffsetPM;
      write_cap_data(cap.data, 6);
      cap.data[0] = 0x00;  // PMC: D0 supported, no auxiliary current
      cap.data[2] = 0x00;  // PMCSR: D0 power state
      break;
    }
    case PCI_CAP_ID_MSI: {
      // MSI: 14 bytes minimum
      //   0x00: cap_id (0x05)
      //   0x01: next_ptr
      //   0x02: message control (enable, multiple message capable/enable)
      //   0x04: message address lo
      //   0x08: message address hi
      //   0x0C: message data
      cap.config_offset = kCapOffsetMSI;
      write_cap_data(cap.data, 12);
      cap.data[0] = 0x00;  // message control: disabled, 1 vector capable
      break;
    }
    case PCI_CAP_ID_PCIE: {
      // PCIe: 0x3C bytes
      //   0x00: cap_id (0x10)
      //   0x01: next_ptr
      //   0x02: PCIe capabilities (version in bits 0-3)
      //   0x0C: link capabilities (link speed in bits 0-3)
      cap.config_offset = kCapOffsetPCIe;
      write_cap_data(cap.data, 0x3C);
      cap.data[0] = 0x02;  // PCIe capabilities: version 2
      // link capabilities at offset 0x0A within cap (0x0C - 0x02 = 0x0A)
      cap.data[0x0A] = 0x02;  // link speed: 5.0 GT/s (Gen2)
      cap.data[0x0B] = 0x00;
      break;
    }
    case PCI_CAP_ID_MSIX: {
      // MSI-X: 12 bytes
      //   0x00: cap_id (0x11)
      //   0x01: next_ptr
      //   0x02: message control (table size N-1 in bits 0-10, enable in 15)
      //   0x04: table BIR (bits 0-2) + offset (bits 3-31)
      //   0x08: PBA BIR (bits 0-2) + offset (bits 3-31)
      cap.config_offset = kCapOffsetMSIX;
      write_cap_data(cap.data, 8);
      // table size N-1: default 16 - 1 = 15
      const uint16_t table_size_field = static_cast<uint16_t>(MSIX_DEFAULT_VECTORS - 1);
      cap.data[0] = static_cast<uint8_t>(table_size_field & 0xFF);
      cap.data[1] = static_cast<uint8_t>((table_size_field >> 8) & 0x07);
      cap.data[4] = 0x00;  // table BIR=0, offset high
      cap.data[5] = 0x00;
      cap.data[6] = 0x00;  // PBA BIR=0
      cap.data[7] = 0x00;
      break;
    }
    case PCI_CAP_ID_VENDOR: {
      // Vendor Specific: cap_id + next_ptr + length + vendor data
      cap.config_offset = kCapOffsetVendor;
      write_cap_data(cap.data, 4);
      cap.data[0] = 0x04;  // length (incl. cap_id/next_ptr/length byte)
      break;
    }
    default:
      // Unknown cap_id - skip silently (callers are responsible for valid IDs)
      return;
  }

  // Link the new capability into the chain by patching next_ptr of the
  // previous capability. The Capabilities Pointer (offset 0x34) is set to
  // the first capability's offset in the constructor after all caps are added.
  if (!capabilities_.empty()) {
    auto& prev = capabilities_.back();
    prev.next_offset = cap.config_offset;
    // Write the updated next_ptr into config space.
    config_space_[prev.config_offset + 1] =
        static_cast<uint8_t>(prev.next_offset & 0xFF);
  } else {
    // First capability - update the Capabilities Pointer at 0x34.
    config_space_[PCI_CFG_CAP_PTR] = static_cast<uint8_t>(cap.config_offset & 0xFF);
  }

  cap.next_offset = 0;  // New tail: end of chain.
  // Write cap_id and next_ptr=0 into config space.
  config_space_[cap.config_offset] = cap.cap_id;
  config_space_[cap.config_offset + 1] = 0;

  // Copy the cap.data into config space starting at offset+2.
  for (size_t i = 0; i < cap.data.size(); ++i) {
    config_space_[cap.config_offset + 2 + i] = cap.data[i];
  }

  capabilities_.push_back(std::move(cap));
}

}  // namespace pcie_internal
}  // namespace usr_linux_emu
