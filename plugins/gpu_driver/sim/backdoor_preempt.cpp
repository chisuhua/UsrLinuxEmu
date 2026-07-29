// backdoor_preempt.cpp - C-ABI backdoor for preemption/semaphore testing
//
// ADR-057 D5: These symbols exist in the plugin .so but are NEVER called
// via drv/ or exposed through GPU_IOCTL_*. They provide a testing backdoor
// for sim-level verification.

#include "backdoor_preempt.h"

#include <cerrno>

#include "semaphore_manager.h"
#include "hardware/hardware_puller_emu.h"
#include "hardware/channel_manager.h"

/*
 * Global pointers set by plugin.cpp when the corresponding components
 * are initialized. Null when not available (safe fallback).
 */
SemaphoreManager* g_backdoor_sem_mgr = nullptr;
HardwarePullerEmu* g_backdoor_puller = nullptr;
ChannelManager* g_backdoor_channel_mgr = nullptr;

extern "C" {

int backdoor_force_preempt(uint32_t channel_id) {
  if (!g_backdoor_puller) return -ENODEV;
  g_backdoor_puller->triggerPreempt(channel_id);
  return 0;
}

int backdoor_force_resume(uint32_t channel_id) {
  if (!g_backdoor_channel_mgr) return -ENODEV;
  (void)channel_id;
  // Resume is implicit in CHANNEL_SWITCH - the Puller will pick up
  // the preempted channel when nextReadyChannel() returns it.
  return 0;
}

int64_t backdoor_read_sem(uint64_t handle) {
  if (!g_backdoor_sem_mgr) return -ENODEV;
  uint64_t val = g_backdoor_sem_mgr->query(handle);
  if (val == UINT64_MAX) return -EINVAL;
  return static_cast<int64_t>(val);
}

uint32_t backdoor_preempt_count(uint32_t channel_id) {
  (void)channel_id;
  // Placeholder: full preempt counter tracking is a future extension.
  return 0;
}

}  // extern "C"
