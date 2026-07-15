/*
 * kfd_events.h — KFD 事件子系统接口 (C-12 B.4.1)
 *
 * 提供事件信号机制：将 KFD 事件（如队列更新完成、页错误解决）
 * 异步路由到目标进程的事件页面。
 *
 * 设计（per ADR-060 §2.1 + ADR-062）：
 *   - kfd_events_signal() 同步入队，异步处理
 *   - 实际写入事件页面由 kfd_events_thread_ 工作线程完成 (B.4.6 Phase C/E)
 *   - 使用 kernel_workqueue 作为底层调度原语
 *
 * Migration to real kernel:
 *   1. 删除此文件
 *   2. 使用 drivers/gpu/drm/amd/amdkfd/kfd_events.c 替换
 *   3. API 签名保持不变
 */
#pragma once
#include "kfd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize KFD events subsystem. Idempotent. Returns 0. */
int kfd_events_init(void);

/* Tear down KFD events subsystem. Drains pending events. */
void kfd_events_exit(void);

/* Signal an event (synchronous entry point).
 * Per ADR-060 §2.1 + ADR-062: actual work happens async in kfd_events_thread_.
 * This function enqueues the event and returns immediately (0 on success).
 *
 * @pasid: target process PASID (0 if broadcast)
 * @event_id: KFD event slot ID (0..KFD_EVENT_MAX-1)
 * @events: 64-bit event mask
 * Returns 0 on enqueue success, -EINVAL on bad args, -EAGAIN if not initialized.
 */
int kfd_events_signal(u32 pasid, u32 event_id, u64 events);

/* Get events workqueue accessor (for HAL mock impl to enqueue into).
 * Opaque pointer: caller does NOT need to know the workqueue type.
 * Internal implementation (kfd_events.c) uses usr_linux_emu::kernel_workqueue.
 */
void *kfd_events_get_workqueue(void);

#ifdef __cplusplus
}
#endif