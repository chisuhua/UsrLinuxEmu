#pragma once

#include <cstdint>

namespace usr_linux_emu::sim_hardware::cpptlm {

struct PcieEndpointIP {
  uint16_t vendor_id{0};
  uint16_t device_id{0};
  uint32_t class_code{0};
  void* bar_router_h{nullptr};
  void* completer_h{nullptr};
  void* requester_h{nullptr};
  void* msix_h{nullptr};
};

PcieEndpointIP* pcie_endpoint_create(uint16_t vid, uint16_t did, uint32_t cc);
void pcie_endpoint_destroy(PcieEndpointIP* ep);

}  // namespace usr_linux_emu::sim_hardware::cpptlm
