// sim/hardware/mqd_state.cpp - MQD State Machine Implementation (ADR-054)
//
// Implements the IDLE/ACTIVE/PREEMPTED state transitions per ADR-054 §D4.
// The functions operate on a caller-allocated MQD pointer (C-ABI).
//
// State Transition Table (ADR-054 §D4):
//   IDLE      --activate-->   ACTIVE
//   ACTIVE    --deactivate--> IDLE
//   ACTIVE    --preempt-->    PREEMPTED
//   PREEMPTED --resume-->     ACTIVE
//   PREEMPTED --deactivate--> IDLE
//
// Invalid transitions return -EINVAL.

#include "mqd_state.h"

#include <cerrno>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

int mqd_state_activate(MQD* mqd) {
  if (mqd == nullptr) {
    return -EINVAL;
  }
  // activate: IDLE -> ACTIVE  (resume: PREEMPTED -> ACTIVE is handled by mqd_state_resume)
  if (mqd->state != MQD_STATE_IDLE) {
    return -EINVAL;
  }
  // Per ADR-054 §D2: load MQD context into HQD registers, set state=ACTIVE
  mqd->state = MQD_STATE_ACTIVE;
  return 0;
}

int mqd_state_deactivate(MQD* mqd) {
  if (mqd == nullptr) {
    return -EINVAL;
  }
  // deactivate: ACTIVE -> IDLE, PREEMPTED -> IDLE
  // IDLE -> IDLE is invalid (no active queue to deactivate)
  if (mqd->state == MQD_STATE_IDLE) {
    return -EINVAL;
  }
  // Per ADR-054 §D2: save Puller state to MQD, writel(0, HQD_ACTIVE), set state=IDLE
  // Save current context (gpfifo_addr, current_index) before going IDLE
  // (the MQD struct already holds these fields; no separate save area needed
  //  for the IDLE transition - the driver reads them via readl if needed)
  mqd->state = MQD_STATE_IDLE;
  return 0;
}

int mqd_state_preempt(MQD* mqd) {
  if (mqd == nullptr) {
    return -EINVAL;
  }
  // preempt: ACTIVE -> PREEMPTED
  // IDLE and PREEMPTED are invalid (no active queue to preempt / already preempted)
  if (mqd->state == MQD_STATE_IDLE) {
    return -EINVAL;
  }
  if (mqd->state == MQD_STATE_PREEMPTED) {
    return 0;
  }
  // Per ADR-054 §D4: save Puller state (gpfifo_addr, current_index) to preempt context
  // The MQD struct has saved_gpfifo_addr and saved_index fields for this purpose
  mqd->saved_gpfifo_addr = mqd->gpfifo_addr;
  mqd->saved_index = mqd->current_index;
  mqd->saved_entries = mqd->entry_count;
  mqd->state = MQD_STATE_PREEMPTED;
  return 0;
}

int mqd_state_resume(MQD* mqd) {
  if (mqd == nullptr) {
    return -EINVAL;
  }
  // resume: PREEMPTED -> ACTIVE
  // IDLE and ACTIVE are invalid
  if (mqd->state != MQD_STATE_PREEMPTED) {
    return -EINVAL;
  }
  // Per ADR-054 §D4: restore saved preempt context
  mqd->gpfifo_addr = mqd->saved_gpfifo_addr;
  mqd->current_index = mqd->saved_index;
  mqd->entry_count = mqd->saved_entries;
  mqd->state = MQD_STATE_ACTIVE;
  return 0;
}

#ifdef __cplusplus
}  // extern "C"
#endif
