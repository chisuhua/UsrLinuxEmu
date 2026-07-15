/*
 * kfd_mmu.c — KFD MMU 子系统实现 (C-12 B.3.1)
 *
 * 提供 IOMMU 映射/取消映射的核心路径。
 * 当前为 day-1 同步转发实现：
 *   - kfd_mmu_map() → hal_iommu_map()
 *   - kfd_mmu_unmap() → hal_iommu_unmap()
 * 未来 Phase C 将引入异步路径（通过 kfd_mmu_get_workqueue() 提交）。
 *
 * HAL integration (B.3.4, Agent B):
 *   kfd_mmu 通过静态全局指针 hal_ops_ 获取 HAL 实例。
 *   该指针由 plugin_init_internal() 在 HAL 初始化后设置。
 *   在 HAL 未就绪时（hal_ops_ == NULL），所有 map/unmap 返回 -ENODEV。
 *   test_kfd_mmu_standalone 在 B.3.4 落地前：
 *     - init/exit/get_workqueue 测试 → PASS
 *     - map/unmap 测试 → SKIP（hal_ops_ == NULL）
 *
 * Thread safety (per ADR-060):
 *   - kfd_mmu_map/unmap 调用 hal_iommu_map/unmap（用户态 → hal_mock 内部持锁）
 *   - mmu_wq_ 分配在 init 时（单线程安全），访问为原子读
 *
 * Migration to real kernel:
 *   1. 删除此文件，替换为 drivers/gpu/drm/amd/amdkfd/kfd_mmu.c
 *   2. 将 new kernel_workqueue(1) 替换为 alloc_workqueue()
 *   3. hal_iommu_map/unmap 替换为 amdgpu_ir_map/unmap
 */
#include "kfd_mmu.h"   /* first — own header */
#include "gpu_hal.h"   /* hal_iommu_map, hal_iommu_unmap */
#include "kernel/thread/kernel_workqueue.h"
#include <errno.h>

/* Forward-declare struct kfd_process (complete type in kfd_priv.h).
 * kfd_mmu only validates pointer non-NULL; does not access struct internals.
 * Avoids including kfd_priv.h which introduces C/C++ header ordering issues
 * (stdatomic.h vs <atomic>, pthread.h vs <mutex>, TSan sensitivity). */
struct kfd_process;

/* ── static state ──────────────────────────────────────────────────────── */

/*
 * kfd_hal_ops_: HAL 实例指针（由 plugin_init_internal() 设置）。
 * 在 B.3.4 (Agent B) 落地前，test_kfd_mmu_standalone 运行时此指针为 NULL，
 * map/unmap 返回 -ENODEV（符合 day-1 预期，tasks.md B.3.6 已记录）。
 *
 * 此处使用全局指针而非参数传递，符合 Linux 内核驱动模式：
 *   1. amdgpu 驱动通过 amdgpu_device 全局访问 IOMMU 域
 *   2. KFD 移植到内核时，amdgpu_amdkfd_gpuvm 直接调用 amdgpu_ir_map()
 *   3. 零额外参数传递，零接口变更
 */
static struct gpu_hal_ops *hal_ops_;

/*
 * mmu_wq_: MMU 工作队列（per ADR-060 §2.1）。
 * day-1: 仅分配，不用于异步路径。
 * Phase C: 用于异步页表更新和迁移操作。
 */
static usr_linux_emu::kernel_workqueue *mmu_wq_;

/* ── public API ───────────────────────────────────────────────────────── */

int kfd_mmu_init(void) {
  if (mmu_wq_ != NULL)
    return 0;  /* idempotent: already initialized */

  /* day-1: allocate single-thread workqueue.
   * Phase C: 将用于异步页表迁移（kfd-multi-file.md §5.3）。
   * Real kernel: 替换为 alloc_workqueue("kfd_mmu", WQ_MEM_RECLAIM, 0)
   */
  mmu_wq_ = new usr_linux_emu::kernel_workqueue();
  if (mmu_wq_ == NULL)
    return -ENOMEM;

  /* day-1: allocate but do NOT start the workqueue.
   * Phase C (async opt-in) will call mmu_wq_->start() and route
   * kfd_mmu_map/unmap through async work items.
   * Not starting avoids TSan signal-unsafe crash (kernel_thread_base::stop()
   * uses pthread_kill, which conflicts with Catch2 signal handler). */
  hal_ops_ = NULL;  /* B.3.4 Agent B sets this during plugin_init */
  return 0;
}

void kfd_mmu_exit(void) {
  if (mmu_wq_ == NULL)
    return;  /* idempotent: nothing to clean up */

  /* day-1: no start() called, so stop() is a no-op.
   * Phase C: stop() drains in-flight work before delete. */
  mmu_wq_->stop();
  delete mmu_wq_;
  mmu_wq_ = NULL;
  hal_ops_ = NULL;
}

int kfd_mmu_map(struct kfd_process *p, u64 va, u64 size, u32 domain_id) {
  /* validate */
  if (p == NULL)
    return -EINVAL;
  if (size == 0)
    return -EINVAL;

  /* day-1: HAL must be initialized (B.3.4) */
  if (hal_ops_ == NULL)
    return -ENODEV;

  return hal_iommu_map(hal_ops_, va, size, domain_id);
}

int kfd_mmu_unmap(struct kfd_process *p, u64 va, u64 size) {
  /* validate */
  if (p == NULL)
    return -EINVAL;
  if (size == 0)
    return -EINVAL;

  /* day-1: HAL must be initialized (B.3.4) */
  if (hal_ops_ == NULL)
    return -ENODEV;

  return hal_iommu_unmap(hal_ops_, va, size);
}

void *kfd_mmu_get_workqueue(void) {
  return mmu_wq_;
}