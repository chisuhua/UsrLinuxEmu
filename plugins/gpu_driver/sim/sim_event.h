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

/* Event page constants (per KFD ABI) */
#define SIM_EVENT_PAGE_SIZE   4096     /* 4 KB, matches Linux page size */
#define SIM_EVENT_SLOTS       1024     /* max event_id */
#define SIM_EVENT_PAGE_SLOTS  (SIM_EVENT_PAGE_SIZE / 8)  /* 512 uint64_t slots */

/* sim_event_page_alloc — Allocate a 4KB event page for a process.
 *
 * Per-process singleton: subsequent calls with the same @pid return -EEXIST.
 * Caller MUST NOT free the returned page; use sim_event_page_free().
 *
 * @pid:      target process PID (> 0; pid 0 reserved for broadcast)
 * @page_ptr: out — pointer to 4096-byte zero-initialized page (8-byte aligned)
 *
 * Returns 0 on success, -EINVAL on invalid pid, -EEXIST on duplicate alloc,
 *         -ENOMEM on allocation failure.
 */
int sim_event_page_alloc(u32 pid, void** page_ptr);

/* sim_event_page_free — Release the event page for @pid.
 * Returns 0 on success, -ENOENT if no page exists for @pid.
 */
int sim_event_page_free(u32 pid);

/* sim_event_page_get — Lookup the event page for @pid.
 * Used by sim_signal_event to write bits.
 *
 * @pid:      target process PID
 * @page_ptr: out — pointer to the page, or NULL if not allocated
 *
 * Returns 0 on success (page may be NULL), -EINVAL on invalid pid.
 */
int sim_event_page_get(u32 pid, void** page_ptr);

#ifdef __cplusplus
}
#endif