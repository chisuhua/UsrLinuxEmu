#pragma once

#include <cstddef>
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

// Drain accounting (T3.1) — MMIO/config operations bracket enter/exit
void bypass_enter_tlp(void);
void bypass_exit_tlp(void);
std::size_t bypass_in_flight_count(void);
// Set drain timeout in ms; 0 = wait forever (default).
void bypass_set_drain_timeout_ms(int ms);

// Test scope flag (T3.2) — kImmediateAbort only allowed in test scope.
// In production, kImmediateAbort returns -EACCES.
void bypass_set_test_scope(bool in_test_scope);
bool bypass_is_test_scope(void);

}  // namespace usr_linux_emu::sim_hardware

namespace usr_linux_emu::sim_hardware::pcie {
using BypassMode = ::usr_linux_emu::sim_hardware::BypassMode;
using DrainPolicy = ::usr_linux_emu::sim_hardware::DrainPolicy;
using ::usr_linux_emu::sim_hardware::bypass_apply_mode;
using ::usr_linux_emu::sim_hardware::bypass_get_mode;
using ::usr_linux_emu::sim_hardware::bypass_enter_tlp;
using ::usr_linux_emu::sim_hardware::bypass_exit_tlp;
using ::usr_linux_emu::sim_hardware::bypass_in_flight_count;
using ::usr_linux_emu::sim_hardware::bypass_set_drain_timeout_ms;
using ::usr_linux_emu::sim_hardware::bypass_set_test_scope;
using ::usr_linux_emu::sim_hardware::bypass_is_test_scope;
}  // namespace usr_linux_emu::sim_hardware::pcie