// sim_hardware/src/platform.cpp — 占位实现（Wave 1C）
// 真实实现见 Change-2
#include "platform.h"

namespace usr_linux_emu::sim_hardware {

namespace {
thread_local PlatformConfig g_cfg;
}

int platform_load(const PlatformConfig& cfg) {
    g_cfg = cfg;
    return 0;
}

const PlatformConfig& platform_get(void) {
    return g_cfg;
}

}  // namespace usr_linux_emu::sim_hardware