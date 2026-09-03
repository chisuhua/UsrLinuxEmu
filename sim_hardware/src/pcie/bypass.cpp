// sim_hardware/src/pcie/bypass.cpp — 占位实现（Wave 1C）
#include "pcie/bypass.h"

namespace usr_linux_emu::sim_hardware::pcie {

namespace {
BypassMode g_mode = BypassMode::kFull;
}

int bypass_apply_mode(BypassMode mode, DrainPolicy policy) {
    (void)policy;
    g_mode = mode;
    return 0;
}

BypassMode bypass_get_mode(void) {
    return g_mode;
}

}  // namespace usr_linux_emu::sim_hardware::pcie