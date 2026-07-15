/*
 * sim_event.h — Sim-layer KFD event signaling (C ABI)
 *
 * Phase B.4 day-1 stub: counter + return 0 (per ADR-062 §D3).
 * Phase C/E: real event page write (amdgpu_kfd_event_page_set).
 *
 * Architecture: ③ 硬件模拟层 (Hardware Simulation)
 * Per ADR-036 three-way separation.
 */

#pragma once

#include "kfd_types.h"  /* u32, u64 */

#ifdef __cplusplus
extern "C" {
#endif

/* sim_signal_event — Signal a KFD event in the sim layer.
 *
 * Phase B.4 day-1 stub: counter + return 0.
 * Phase C/E: real event page write.
 *
 * @pasid: target process PASID (0 = broadcast)
 * @event_id: KFD event slot ID
 * @events: 64-bit event mask
 * Returns 0 on success, -EINVAL on invalid args.
 */
int sim_signal_event(u32 pasid, u32 event_id, u64 events);

/* sim_signal_event_count — test helper: count of successful calls since init. */
int sim_signal_event_count(void);

#ifdef __cplusplus
}
#endif