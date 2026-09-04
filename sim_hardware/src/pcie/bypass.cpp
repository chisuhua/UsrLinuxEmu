#include "pcie/bypass.h"

#include <atomic>

namespace usr_linux_emu::sim_hardware {
namespace {
std::atomic<BypassMode> g_mode{BypassMode::kFull};
}

int bypass_apply_mode(BypassMode mode, DrainPolicy policy) {
  (void)policy;
  g_mode.store(mode, std::memory_order_release);
  return 0;
}

BypassMode bypass_get_mode(void) {
  return g_mode.load(std::memory_order_acquire);
}
}  // namespace usr_linux_emu::sim_hardware
