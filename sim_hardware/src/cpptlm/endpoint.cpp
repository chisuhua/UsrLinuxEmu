// sim_hardware/src/cpptlm/endpoint.cpp — 占位实现（Wave 1C）
#include "cpptlm/endpoint.h"

namespace usr_linux_emu::sim_hardware::cpptlm {

PcieEndpointIP* pcie_endpoint_create(uint16_t vid, uint16_t did, uint32_t cc) {
    (void)vid; (void)did; (void)cc;
    return nullptr;  // Change-2 真实构造 17-port composition
}

void pcie_endpoint_destroy(PcieEndpointIP* ep) {
    (void)ep;
}

}  // namespace usr_linux_emu::sim_hardware::cpptlm