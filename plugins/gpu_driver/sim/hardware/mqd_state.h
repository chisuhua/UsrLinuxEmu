// sim/hardware/mqd_state.h - MQD State Machine (ADR-054)
//
// State machine for Memory Queue Descriptor transitions.
// Per ADR-054 §D4 state transition table:
//
// | Current \ Event  | activate    | deactivate   | preempt      | resume      |
// |------------------|-------------|--------------|--------------|-------------|
// | IDLE             | -> ACTIVE   | -EINVAL      | -EINVAL      | -EINVAL     |
// | ACTIVE           | -EINVAL     | -> IDLE      | -> PREEMPTED | -EINVAL     |
// | PREEMPTED         | -> ACTIVE   | -> IDLE      | (no-op, 0)   | -> ACTIVE   |
//
// The functions operate directly on a MQD pointer (caller-allocated),
// following the C-ABI convention used by the sim layer.
//
// Cross-repo: This file is sim-internal (③ hardware sim). The MQD struct
// itself lives in shared/mqd.h (②③ shared contract).

#pragma once

#include "mqd.h"  // shared/mqd.h - ②③ MQD contract (gpu_sim include path has shared/)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Activate a queue: IDLE -> ACTIVE
 *
 * Loads MQD context into HQD registers and marks the queue as ACTIVE.
 * Per ADR-054 §D2: sets MQD.state = ACTIVE.
 *
 * @param mqd  Pointer to a caller-allocated MQD struct
 * @return 0 on success, -EINVAL if mqd is NULL or state is not IDLE
 */
int mqd_state_activate(MQD* mqd);

/**
 * @brief Deactivate a queue: ACTIVE/PREEMPTED -> IDLE
 *
 * Saves current Puller state to MQD and marks the queue as IDLE.
 * Per ADR-054 §D2: sets MQD.state = IDLE.
 *
 * @param mqd  Pointer to a caller-allocated MQD struct
 * @return 0 on success, -EINVAL if mqd is NULL or state is IDLE
 */
int mqd_state_deactivate(MQD* mqd);

/**
 * @brief Preempt an active queue: ACTIVE -> PREEMPTED
 *
 * Saves current Puller state (gpfifo_addr, current_index) to MQD
 * preempt context fields and marks the queue as PREEMPTED.
 * Per ADR-054 §D4: only valid from ACTIVE state.
 *
 * @param mqd  Pointer to a caller-allocated MQD struct
 * @return 0 on success, -EINVAL if mqd is NULL or state is not ACTIVE
 */
int mqd_state_preempt(MQD* mqd);

/**
 * @brief Resume a preempted queue: PREEMPTED -> ACTIVE
 *
 * Restores saved preempt context and marks the queue as ACTIVE again.
 * Per ADR-054 §D4: only valid from PREEMPTED state.
 *
 * @param mqd  Pointer to a caller-allocated MQD struct
 * @return 0 on success, -EINVAL if mqd is NULL or state is not PREEMPTED
 */
int mqd_state_resume(MQD* mqd);

#ifdef __cplusplus
}  // extern "C"
#endif
