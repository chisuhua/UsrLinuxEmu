/*
 * kfd_events.c — KFD 事件子系统实现 (C-12 B.4.1)
 *
 * 提供异步事件信号基础设施。
 * 当前为 day-1 实现：
 *   - kfd_events_signal() 将事件入队到 events_wq_
 *   - 工作线程暂不写入事件页面（B.4.6 Phase C/E 待 Agent B）
 *   - 入队即返回 0（火后不管语义）
 *
 * Thread safety (per ADR-060):
 *   - kfd_events_signal() 对 events_wq_ 的访问受 workqueue 内部 mutex 保护
 *   - kfd_events_exit() 调用 workqueue->stop()（drain contract）
 *   - events_wq_ 分配在 init 时（单线程安全），访问为原子读
 *
 * Migration to real kernel:
 *   1. 删除此文件，替换为 drivers/gpu/drm/amd/amdkfd/kfd_events.c
 *   2. 将 new kernel_workqueue() 替换为 alloc_workqueue("kfd_events", ...)
 *   3. 将 events_wq_->enqueue() 替换为 queue_work()
 */
#include "kfd_events.h"  /* first — own header */
#include "kfd_types.h"   /* u32, u64 */
#include "kernel/thread/kernel_workqueue.h"
#include <errno.h>

/* ── static state ──────────────────────────────────────────────────────── */

/*
 * events_wq_: 事件工作队列（per ADR-060 §2.1）。
 * day-1 (B.4.1): 入队后仅记录（TODO B.4.6 写入事件页面）。
 * Phase C/E: 工作线程将调用 hal_event_signal() 完成实际写入。
 */
static usr_linux_emu::kernel_workqueue *events_wq_;

/* ── public API ───────────────────────────────────────────────────────── */

int kfd_events_init(void) {
  if (events_wq_ != NULL)
    return 0;  /* idempotent: already initialized */

  events_wq_ = new usr_linux_emu::kernel_workqueue();
  if (events_wq_ == NULL)
    return -ENOMEM;

  events_wq_->start();
  return 0;
}

void kfd_events_exit(void) {
  if (events_wq_ == NULL)
    return;  /* idempotent: nothing to clean up */

  /* drain contract: stop() processes all pending work before returning */
  events_wq_->stop();
  delete events_wq_;
  events_wq_ = NULL;
}

int kfd_events_signal(u32 pasid, u32 event_id, u64 events) {
  /* validate pre-init state */
  if (events_wq_ == NULL)
    return -EAGAIN;

  /* validate args:
   * - event_id: must be in valid range (KFD_EVENT_MAX = 8 per Linux 6.12)
   * - events:  must be non-zero (at least one event bit set)
   */
  if (event_id >= 8)
    return -EINVAL;
  if (events == 0)
    return -EINVAL;

  /* enqueue async work.
   * day-1: lambda captures by value, fires into workqueue, returns immediately.
   * TODO B.4.6: 在工作线程中调用 hal_event_signal(hal, pasid, event_id, events)
   */
  events_wq_->enqueue([pasid, event_id, events]() {
    /* B.4.6 (Phase C/E, Agent B):
     *   1. 获取全局 HAL ops（hal_ops_，由 plugin_init 设置）
     *   2. 调用 hal_event_signal(hal_ops_, pasid, event_id, events)
     *   3. hal_mock 实现将写入事件页面（sim_signal_event）
     * day-1: 入队后仅记录（无操作）
     */
    (void)pasid;
    (void)event_id;
    (void)events;
  });

  return 0;
}

void *kfd_events_get_workqueue(void) {
  return events_wq_;
}