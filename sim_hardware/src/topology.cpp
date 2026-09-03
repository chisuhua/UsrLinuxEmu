// sim_hardware/src/topology.cpp — 占位实现（Wave 1C）
#include "topology.h"

#include <fstream>
#include <cerrno>
#include <cstring>

namespace usr_linux_emu::sim_hardware::topology {

int topology_load_json(const std::string& path, Topology* out) {
    if (!out) return -EINVAL;
    std::ifstream f(path);
    if (!f.is_open()) return -ENOENT;
    // Wave 1C 占位：不解析 JSON 内容；Change-2 用 nlohmann/json 或 cJSON 解析
    out->platform_name = "pc-x86-mock";
    out->enable_root_complex = true;
    out->enable_link_layer = true;
    return 0;
}

int topology_write_default_json(const std::string& path) {
    std::ofstream f(path);
    if (!f.is_open()) return -ENOENT;
    // 占位写入；Change-2 真实写 default topology
    f << "{}";
    return 0;
}

}  // namespace usr_linux_emu::sim_hardware::topology