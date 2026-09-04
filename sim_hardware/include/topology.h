// sim_hardware/include/topology.h — PC 拓扑加载器（从 JSON 加载）
// per ADR-091 v0.2 §D3.4 + Change-2 design.md §5.5
#pragma once

#include <string>
#include <vector>

#include "pcie/bypass.h"

namespace usr_linux_emu::sim_hardware::pcie {

/// Topology BAR 描述（per design §9.2 schema）
struct TopologyBar {
    uint8_t  index;
    uint64_t size_bytes;
    bool     prefetchable;
    bool     is_mmio;
    bool     is_64bit;
};

/// Topology 设备描述（per design §5.5 TopologyDevice）
struct TopologyDevice {
    uint16_t bdf;
    uint16_t vendor_id;
    uint16_t device_id;
    uint32_t class_code;
    char     endpoint_kind[32];
    std::vector<TopologyBar> bars;
};

}  // namespace usr_linux_emu::sim_hardware::pcie

namespace usr_linux_emu::sim_hardware::topology {

struct Topology {
    uint32_t schema_version = 0;
    std::string platform_name;
    bool enable_root_complex = true;
    bool enable_link_layer = true;
    bool enable_phy = false;
    pcie::BypassMode default_bypass_mode = pcie::BypassMode::kFull;
    pcie::DrainPolicy default_drain_policy = pcie::DrainPolicy::kGracefulDrain;
    std::vector<pcie::TopologyDevice> devices;
};

/// 加载拓扑 JSON。返回 0 成功，-ENOENT 文件不存在，-EINVAL JSON 格式错误。
/// Schema 校验 per design.md §9.3 + §9.4（严格 schema：未知字段返回 -EINVAL）。
int topology_load_json(const std::string& path, Topology* out);

/// 写出 topology JSON 到 path。返回 0 成功，-ENOENT 路径无效。
int topology_write_default_json(const std::string& path, const Topology* src);

}  // namespace usr_linux_emu::sim_hardware::topology
