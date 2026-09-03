// sim_hardware/include/platform.h — PC 平台抽象（per ADR-091 v0.2 §D3）
#pragma once

#include <cstdint>
#include <string>

namespace usr_linux_emu::sim_hardware {

enum class PlatformType {
    kPcX86Mock,         // 通用的 x86 PC mock 平台（Wave 1C default）
    kAmdNavi,           // AMD RDNA（Change-3 真实硬件 profile）
    kNvidiaAda,         // NVIDIA Ada（Change-3 真实硬件 profile）
};

struct PlatformConfig {
    PlatformType type = PlatformType::kPcX86Mock;
    std::string topology_path = "sim_hardware/topology/default_topology.json";
    bool enable_pcie_root_complex = true;
    bool enable_link_layer = true;
    bool enable_phy_digital = false;  // Wave 1C default 关
};

/// 加载 PC 平台配置。返回 0 成功，负 errno 失败。
int platform_load(const PlatformConfig& cfg);

/// 获取当前平台配置（线程局部）。
const PlatformConfig& platform_get(void);

}  // namespace usr_linux_emu::sim_hardware