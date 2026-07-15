/*
 * sim_event.c — Sim-layer KFD event signaling (C implementation)
 *
 * Phase B.4 day-1 stub: atomic counter + validation.
 * Phase C/E: real event page write (amdgpu_kfd_event_page_set).
 *
 * Naming conventions per design.md §Naming:
 *   - Functions:  sim_<feature>_<verb>
 *   - No STL — pure C11 with stdatomic.h
 */

#include "sim_event.h"
#include <stdatomic.h>

static atomic_int sim_signal_count_ = 0;

int sim_signal_event(u32 pasid, u32 event_id, u64 events) {
  if (pasid > 0xFFFF) return -22;  /* -EINVAL */
  if (event_id > 1024) return -22;  /* -EINVAL */
  if (events == 0) return -22;     /* -EINVAL */
  atomic_fetch_add(&sim_signal_count_, 1);
  /* TODO Phase C/E: write to user-mapped event page (amdgpu_kfd_event_page_set) */
  return 0;
}

int sim_signal_event_count(void) {
  return atomic_load(&sim_signal_count_);
}