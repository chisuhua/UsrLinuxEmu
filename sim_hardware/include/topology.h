// sim_hardware/include/topology.h — PC 拓扑加载器（从 JSON 加载）
// per ADR-091 v0.2 §D3.4 + Change-1 design.md §5
#pragma once

#include <string>
#include <vector>

#include "pcie/host_bridge.h"

namespace usr_linux_emu::sim_hardware::topology {

struct Topology {
    std::string platform_name;
    bool enable_root_complex = true;
    bool enable_link_layer = true;
    std::vector<pcie::DiscoveredDevice> devices;
};

/// 加载拓扑 JSON。返回 0 成功，-ENOENT 文件不存在，-EINVAL JSON 格式错误。
int topology_load_json(const std::string& path, Topology* out);

/// 写入默认拓扑（Wave 1C 仅 pc-x86-mock）
int topology_write_default_json(const std::string& path);

}  // namespace usr_linux_emu::sim_hardware::topology