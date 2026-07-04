/*
 * kfd_sim_bridge.h — Bridge between KFD ioctl handlers and sim primitives.
 *
 * Stage 1.4 Tier-1 delivery: 4 KFD ioctl handlers penetrate to sim layer
 * via this bridge, upgrading from log-only to real runtime behavior.
 *
 * Stage 1.4 Tier-2: gpu_ioctl_register_mmu_cb penetrates to a single-callback
 * MMU event registry here.  Re-registration is rejected with -EALREADY
 * (single-callback constraint); callback body invocation is Stage 3+.
 *
 * Architecture: ② 可移植的驱动代码 → kfd_sim_bridge → ③ 硬件模拟 (sim)
 * Per ADR-036 three-way separation.
 *
 * NOTE: This bridge is internal to the gpu_driver plugin. It is NOT a
 * HAL interface (no struct gpu_hal_ops extension per ADR-027 + ADR-035).
 * The bridge is a pure Tier-1 sim glue layer.
 */
#pragma once

#include <linux_compat/types.h>
#include "shared/gpu_ioctl.h"

#ifdef __cplusplus
extern "C" {
#endif

void kfd_sim_reset(void);
u64 kfd_sim_lookup_pfn(u64 gpu_va);
u32 kfd_sim_get_page_count(void);

long kfd_sim_handle_map_memory(struct gpu_map_memory_args *args);
long kfd_sim_handle_unmap_memory(struct gpu_unmap_memory_args *args);
long kfd_sim_handle_get_process_aperture(struct gpu_get_process_aperture_args *args);
long kfd_sim_handle_update_queue(struct gpu_update_queue_args *args);

/* Tier-2 §3.1 — REGISTER_MMU_EVENT_CB penetration */
long kfd_sim_register_mmu_cb(struct gpu_mmu_event_cb_args *args);
bool kfd_sim_mmu_cb_is_registered(void);
u64  kfd_sim_get_mmu_cb_fn(void);
u64  kfd_sim_get_mmu_cb_user_data(void);

/* Tier-2 §3.2 — REGISTER_FIRMWARE_CB penetration (placeholder; firmware load Stage 2+) */
long kfd_sim_register_firmware_cb(struct gpu_firmware_cb_args *args);
bool kfd_sim_firmware_cb_is_registered(void);
u64  kfd_sim_get_firmware_cb_fn(void);
u64  kfd_sim_get_firmware_cb_user_data(void);

#ifdef __cplusplus
}
#endif