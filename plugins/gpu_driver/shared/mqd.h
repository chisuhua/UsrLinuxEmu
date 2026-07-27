// shared/mqd.h — Memory-mapped Queue Descriptor (ADR-054)
//
// MQD is the shared-memory contract between ② driver (writes via BAR) and
// ③ hardware sim (reads via backing store). HQD control bits reside in
// BAR0 MMIO registers (offset 0x4000 + channel_id * 64), accessed via
// writel/readl from include/linux_compat/io.h.
//
// MQD backing: DMA coherent pool (ADR-073), mirrors real amdgpu GART
// allocation pattern. Allocated per-channel during Queue creation.
//
// Cross-repo: This file is symlinked by TaskRunner for shared ABI.
// Changes here trigger ADR-035 §Rule 5.1 sync protocol.

#pragma once

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// MQD state constants (ADR-054 §D4 state transition table)
#define MQD_STATE_IDLE      0
#define MQD_STATE_ACTIVE    1
#define MQD_STATE_PREEMPTED 2

// MQD struct — 128 bytes, 8-byte aligned, packed for ABI stability
typedef struct {
  // Ring Buffer State (32 bytes)
  uint64_t ring_base_va;   // GPU VA of ring buffer
  uint64_t ring_size;      // ring buffer size in bytes
  uint64_t wptr;           // written by ② (driver, via BAR)
  uint64_t rptr;           // written by ③ (hardware, after consumption)

  // Batch State (16 bytes)
  uint64_t gpfifo_addr;    // current batch GPFIFO VA
  uint32_t entry_count;    // total entries in batch
  uint32_t current_index;  // current consumption position

  // Scheduling (8 bytes — packed for BAR alignment)
  uint8_t  priority;       // 0=low, 1=normal, 2=high (reserved for ADR-045)
  uint8_t  _sched_pad;     // explicit pad to keep timeslice 2-byte aligned
  uint16_t timeslice_remaining; // entries remaining in current RR slice
  uint32_t doorbell_id;    // doorbell register index

  // Preempt Context (24 bytes) — saved on PREEMPT, restored on resume
  uint64_t saved_gpfifo_addr;
  uint32_t saved_index;
  uint32_t saved_entries;
  uint8_t  _preempt_pad[8];

  // Profiling (36 bytes) — populated by ③ at DISPATCH
  uint64_t ts_queries[4];  // pending timestamp query handles (ADR-057)
  uint32_t cycle_count;    // dispatch cycle counter

  // State (4 bytes) + reserved for future expansion (8 bytes)
  uint32_t state;          // MQD_STATE_IDLE / ACTIVE / PREEMPTED
  uint64_t _reserved;      // future expansion (ADR-045 priority, ADR-046 preempt detail)
}  __attribute__((packed)) MQD;

// Compile-time size assertion (must be power-of-2 aligned for BAR mapping)
// Use static_assert for C++ compatibility (_Static_assert is C11-only)
#ifdef __cplusplus
static_assert(sizeof(MQD) == 128, "MQD must be 128 bytes");
static_assert(sizeof(MQD) % 8 == 0, "MQD must be 8-byte aligned");
#else
_Static_assert(sizeof(MQD) == 128, "MQD must be 128 bytes");
_Static_assert(sizeof(MQD) % 8 == 0, "MQD must be 8-byte aligned");
#endif

// HQD BAR0 register offsets (per channel, base=0x4000)
#define HQD_REG_OFFSET(channel_id)  (0x4000 + (channel_id) * 64)

// HQD control register (offset 0x00 within channel window)
#define HQD_CTL_OFFSET     0x00
#define HQD_CTL_ACTIVE     0x00000001  // writel(1) to activate
#define HQD_CTL_DEACTIVATE 0x00000000  // writel(0) to deactivate
#define HQD_CTL_PREEMPT    0x00000002  // writel(2) to preempt

// HQD status register (offset 0x04 within channel window, read-only)
#define HQD_STATUS_OFFSET  0x04
#define HQD_STATUS_IDLE    0x0
#define HQD_STATUS_BUSY    0x1
#define HQD_STATUS_PREEMPTED 0x2

#ifdef __cplusplus
}
#endif
