#include "cpptlm/endpoint.h"

namespace usr_linux_emu::sim_hardware::cpptlm {

PcieEndpointIP* pcie_endpoint_create(uint16_t vid, uint16_t did, uint32_t cc) {
  auto* endpoint = new PcieEndpointIP{vid, did, cc, nullptr, nullptr, nullptr, nullptr};
  endpoint->bar_router_h = endpoint;
  endpoint->completer_h = endpoint;
  endpoint->requester_h = endpoint;
  endpoint->msix_h = endpoint;
  return endpoint;
}

void pcie_endpoint_destroy(PcieEndpointIP* ep) {
  delete ep;
}

}  // namespace usr_linux_emu::sim_hardware::cpptlm
