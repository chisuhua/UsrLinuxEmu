/*
 * page_state_machine.cpp — Three-state page state machine
 *
 * Stage 1.3 UVM/HMM §3.6: PAGE_STATE_CPU / GPU / MIGRATING.
 * Per design.md Decision 5: minimal state machine for zone_device.
 *
 * Architecture: ① 内核环境模拟层 (Kernel Environment Simulation)
 */

#include <linux_compat/mmu_notifier.h>

extern "C" {

enum page_state {
  PAGE_STATE_CPU       = 0,
  PAGE_STATE_GPU       = 1,
  PAGE_STATE_MIGRATING = 2,
};

int page_state_transition(enum page_state *current, enum page_state target) {
  if (!current)
    return -EINVAL;

  /* Same state → no-op success */
  if (*current == target)
    return 0;

  /* Valid transitions:
   *   CPU       → MIGRATING
   *   GPU       → MIGRATING
   *   MIGRATING → CPU
   *   MIGRATING → GPU
   */
  switch (*current) {
  case PAGE_STATE_CPU:
  case PAGE_STATE_GPU:
    if (target != PAGE_STATE_MIGRATING)
      return -EINVAL;
    break;
  case PAGE_STATE_MIGRATING:
    if (target != PAGE_STATE_CPU && target != PAGE_STATE_GPU)
      return -EINVAL;
    break;
  default:
    return -EINVAL;
  }

  *current = target;
  return 0;
}

const char *page_state_name(enum page_state s) {
  switch (s) {
  case PAGE_STATE_CPU:       return "CPU";
  case PAGE_STATE_GPU:       return "GPU";
  case PAGE_STATE_MIGRATING: return "MIGRATING";
  default:                   return "UNKNOWN";
  }
}

} // extern "C"