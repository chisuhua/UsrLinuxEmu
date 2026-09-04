// plugins/pci_driver/pci_setup_bus.cpp — T4.2 (Stage 5.5.2 Wave 4)
// BAR resource assignment from topology to PcieEmu.
// Per design.md §13.3 mock target:
//   host_bridge_enumerate(devs, 16, &count, topology_path)
//     → pci_setup_bus(pci_dev) → mock bar_allocate (host_bridge_bypass or
//                                  direct assign_bar)

#include "pci_setup_bus.h"

#include "topology.h"

namespace usr_linux_emu {
namespace pci {

int pci_bus_assign_resources(
    PcieEmu* emu,
    const usr_linux_emu::sim_hardware::pcie::DiscoveredDevice* dev,
    const char* topology_path) {
  if (!emu || !dev || !topology_path) return -EINVAL;

  // Load topology to find BAR sizes for this discovered device.
  sim_hardware::topology::Topology topo{};
  int rc = sim_hardware::topology::topology_load_json(topology_path, &topo);
  if (rc != 0) return rc;

  // Match by vendor_id + device_id + bdf.
  const sim_hardware::pcie::TopologyDevice* match = nullptr;
  for (const auto& td : topo.devices) {
    if (td.vendor_id == dev->vendor_id &&
        td.device_id == dev->device_id &&
        td.bdf == dev->bdf) {
      match = &td;
      break;
    }
  }
  if (!match) return -ENOENT;

  // Assign each BAR with a deterministic base address.
  // Linux convention: pack MMIO BARs starting at 0xF0000000, contiguous.
  uint64_t mmio_base = 0xF0000000ULL;
  uint64_t io_base = 0xE000ULL;
  for (const auto& bar : match->bars) {
    if (bar.index < 0 || bar.index >= 6) continue;
    if (bar.size_bytes == 0) continue;
    uint64_t base = bar.is_mmio ? mmio_base : io_base;
    emu->assign_bar(bar.index, base, bar.size_bytes, bar.is_mmio);
    if (bar.is_mmio) {
      mmio_base += bar.size_bytes;
    } else {
      io_base += bar.size_bytes;
    }
  }
  return 0;
}

}  // namespace pci
}  // namespace usr_linux_emu
