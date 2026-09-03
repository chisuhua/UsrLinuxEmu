// sim_hardware/include/cpptlm/endpoint.h — PcieEndpointIP 17-port composition
// per ADR-088 §D5.2 + ADR-091 v0.2 §D3.3
#pragma once

#include <cstdint>

namespace usr_linux_emu::sim_hardware::cpptlm {

/// 17-port PCIe endpoint composition（只声明，Wave 1C 占位）
/// 在 Change-2 中真实组合 PcieBarRouter + PcieCompleter + PcieRequester + MSI-X 等
struct PcieEndpointIP {
    uint16_t vendor_id;
    uint16_t device_id;
    uint32_t class_code;
    void*    bar_router_h;     // opaque handle
    void*    completer_h;
    void*    requester_h;
    void*    msix_h;
};

/// 构造 endpoint（Wave 1C 占位，返回 nullptr）
PcieEndpointIP* pcie_endpoint_create(uint16_t vid, uint16_t did, uint32_t cc);

/// 销毁 endpoint
void pcie_endpoint_destroy(PcieEndpointIP* ep);

}  // namespace usr_linux_emu::sim_hardware::cpptlm