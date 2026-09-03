// sim_hardware/include/pcie/bypass.h — PcieBypassMux 3 态运行时切换
// per ADR-091 v0.2 §D3.2 + Change-1 design.md §4
#pragma once

namespace usr_linux_emu::sim_hardware::pcie {

enum class BypassMode {
    kFull,        // 不 bypass（默认，所有 TLP 经 RC 路由）
    kBypass,      // 完全 bypass（TLP 直送 endpoint，绕过 RC）
    kPartial,     // 部分 bypass（部分 TLP 类型 bypass）
};

enum class DrainPolicy {
    kGracefulDrain,   // 等 in-flight TLP 完成后切换
    kImmediateAbort,  // 立即 abort in-flight TLP（仅测试用）
};

/// 切换 Bypass mode。返回 0 成功，负 errno 失败。
int bypass_apply_mode(BypassMode mode, DrainPolicy policy);

/// 查询当前 mode
BypassMode bypass_get_mode(void);

}  // namespace usr_linux_emu::sim_hardware::pcie