#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Backdoor symbols for preemption/semaphore testing (ADR-057 D5).
 *
 * These symbols exist in the plugin .so but are NEVER called via drv/
 * or exposed through GPU_IOCTL_*. They provide a testing backdoor for
 * sim-level verification without polluting the production ioctl interface.
 */

/* Force preempt on a channel. Returns 0 on success. */
int backdoor_force_preempt(uint32_t channel_id);

/* Force resume on a channel. Returns 0 on success. */
int backdoor_force_resume(uint32_t channel_id);

/* Read current semaphore value. Returns value or -1 on error. */
int64_t backdoor_read_sem(uint64_t handle);

/* Get preemption count for a channel. */
uint32_t backdoor_preempt_count(uint32_t channel_id);

#ifdef __cplusplus
}
#endif
