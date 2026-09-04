// sim_hardware/include/pcie/host_bridge.h — Tier 1+2 PCIe Host Bridge 抽象
// per ADR-091 v0.2 §D3.1 + Change-2 design.md §5.3
#pragma once

#include <cstdint>
#include <cstddef>

namespace usr_linux_emu::sim_hardware::pcie {

/// Tier 2 (Root Complex Mirror) 拓扑枚举返回的设备描述
/// Per Change-2 v0.4 design §5.3:
/// - bdf: bus:device.function packed (5+5+3 = 13 bits stored in 16)
/// - endpoint_kind: null-terminated string (e.g. "PcieEndpointMock")
struct DiscoveredDevice {
    uint16_t bdf;
    uint16_t vendor_id;
    uint16_t device_id;
    uint32_t class_code;
    char     endpoint_kind[32];
};

/// 枚举 PCIe 拓扑（在 Change-2 Tier 2 中真实调用 PcieRootComplexTLM）
///
/// 参数规则（per design §5.3 + spec REQ-SIMHW-HOSTBRIDGE-001）:
/// - `out_count` 必填；空指针返回 -EINVAL
/// - `max_devices == 0` 且 `devices == nullptr` 是合法查询形式
/// - `max_devices > 0` 且 `devices == nullptr` 返回 -EINVAL
/// - 返回 `*out_count = 实际设备总数`（不是写入数量）
/// - `topology_path` 必填；空指针返回 -EINVAL；缺文件 -ENOENT；畸形 JSON -EINVAL
///
/// 返回 0 成功，负 errno 失败。
int host_bridge_enumerate(DiscoveredDevice* devices, size_t max_devices,
                          size_t* out_count, const char* topology_path);

/// Tier 1 直通写入（无需枚举，用于 BAR bypass 路径）
/// 委托到 active CpptlmBridge 的 mmio_read/write（per design §5.3 + §10）
int host_bridge_bypass_write(uint8_t bar, uint64_t offset,
                             const void* src, size_t len);

int host_bridge_bypass_read(uint8_t bar, uint64_t offset,
                            void* dst, size_t len);

}  // namespace usr_linux_emu::sim_hardware::pcie
