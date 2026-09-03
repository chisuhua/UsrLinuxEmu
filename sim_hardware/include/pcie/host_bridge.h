// sim_hardware/include/pcie/host_bridge.h — Tier 1+2 PCIe Host Bridge 抽象
// per ADR-091 v0.2 §D3.1 + Change-1 design.md §3
#pragma once

#include <cstdint>
#include <cstddef>

namespace usr_linux_emu::sim_hardware::pcie {

/// Tier 1 (Software Bypass) 旁路路径
struct HostBypassTarget {
    uint64_t host_va_base;   // 模拟 host 端的 VA 起点
    uint64_t host_va_limit;
};

/// Tier 2 (Root Complex Mirror) 拓扑枚举返回的设备描述
struct DiscoveredDevice {
    uint16_t segment;        // PCI segment
    uint8_t  bus;            // bus number
    uint8_t  device;
    uint8_t  function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint32_t class_code;
    uint32_t bar[6];         // 6 x BAR 值（32-bit，低位表示 type/prefetchable 等）
    uint8_t  bar_count;
};

/// 枚举 PCIe 拓扑（在 Change-2 Tier 2 中真实调用 PcieRootComplexTLM）
/// 返回 0 成功（devs 填充），-ENODEV 无设备，负 errno 其他错误。
int host_bridge_enumerate(DiscoveredDevice* devs, size_t max_devs, size_t* out_count);

/// Tier 1 直通写入（无需枚举，用于 BAR bypass 路径）
int host_bridge_bypass_write(uint8_t bar, uint64_t offset,
                             const void* src, size_t len);

int host_bridge_bypass_read(uint8_t bar, uint64_t offset,
                            void* dst, size_t len);

}  // namespace usr_linux_emu::sim_hardware::pcie