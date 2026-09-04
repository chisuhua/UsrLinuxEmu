#pragma once

#include <cstdint>

namespace usr_linux_emu::sim_hardware {

enum class BypassMode : uint8_t {
  kFull = 0,
  kBypass = 1,
  kPartial = 2,
};

enum class DrainPolicy : uint8_t {
  kGracefulDrain = 0,
  kImmediateAbort = 1,
};

int bypass_apply_mode(BypassMode mode, DrainPolicy policy);
BypassMode bypass_get_mode(void);

}  // namespace usr_linux_emu::sim_hardware

namespace usr_linux_emu::sim_hardware::pcie {
using BypassMode = ::usr_linux_emu::sim_hardware::BypassMode;
using DrainPolicy = ::usr_linux_emu::sim_hardware::DrainPolicy;
using ::usr_linux_emu::sim_hardware::bypass_apply_mode;
using ::usr_linux_emu::sim_hardware::bypass_get_mode;
}  // namespace usr_linux_emu::sim_hardware::pcie
