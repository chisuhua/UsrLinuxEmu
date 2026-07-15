/*
 * kfd_mmu.h — KFD MMU 子系统接口 (C-12 B.3.1)
 *
 * 提供 IOMMU 映射/取消映射的硬件无关接口。
 * 底层通过 HAL (struct gpu_hal_ops) 的 iommu_map/unmap 函数指针
 * 调用具体实现（用户态 → hal_mock，内核态 → amdgpu_ir）。
 *
 * 线程安全：kfd_mmu_map/unmap 内部持锁（per-process MMU lock），
 * 多个进程可并发映射（不同进程持有不同锁）。
 *
 * Migration to real kernel:
 *   1. 删除此文件
 *   2. 使用 drivers/gpu/drm/amd/amdkfd/kfd_mmu.c 替换
 *   3. API 签名 (kfd_mmu_init/exit/map/unmap) 保持不变
 */
#pragma once
#include "kfd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize KFD MMU subsystem. Idempotent. Returns 0. */
int kfd_mmu_init(void);

/* Tear down KFD MMU subsystem. */
void kfd_mmu_exit(void);

/* Map a virtual address range for the given process.
 * Calls hal_iommu_map() under the hood (B.3.4).
 * @p: process context
 * @va: virtual address (page-aligned)
 * @size: size in bytes
 * @domain_id: GPU domain (per-process aperture index)
 * Returns 0 on success, negative errno on failure.
 */
int kfd_mmu_map(struct kfd_process *p, u64 va, u64 size, u32 domain_id);

/* Unmap a virtual address range. Calls hal_iommu_unmap(). */
int kfd_mmu_unmap(struct kfd_process *p, u64 va, u64 size);

/* Get the MMU workqueue accessor (per ADR-060 §2.1 + Migration:400).
 * day-1 stub: returns NULL or a placeholder; future Phase C async opt-in.
 * Current callers MUST handle NULL gracefully.
 *
 * Opaque pointer: caller does NOT need to know the workqueue type.
 * Internal implementation (kfd_mmu.c) uses usr_linux_emu::kernel_workqueue.
 */
void *kfd_mmu_get_workqueue(void);

#ifdef __cplusplus
}
#endif