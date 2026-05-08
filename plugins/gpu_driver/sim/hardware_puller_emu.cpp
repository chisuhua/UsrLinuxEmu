#include "hardware/hardware_puller_emu.h"

HardwarePullerEmu::HardwarePullerEmu() : state_("IDLE") {}

const char* HardwarePullerEmu::currentState() const { return state_; }

bool HardwarePullerEmu::pull(uint32_t queue_id, struct gpu_gpfifo_entry* out_entry) {
  (void)queue_id;
  (void)out_entry;
  return false;  // No entries available in T7 stub
}