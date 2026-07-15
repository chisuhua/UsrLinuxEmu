/*
 * gpu_hal.h — 硬件抽象层接口
 *
 * 定义 drv/ 与 sim/ 之间的 10 个 HAL 函数指针。
 * 移植到内核时替换实现函数，接口不变。
 *
 * C/C++ 双语言兼容：C 编译用于内核模块，C++ 编译用于用户态仿真。
 * 移植替换：stdint.h → linux/types.h (u64/u32)
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct gpu_hal_ops {
  /* HAL 实现上下文（用户态 → sim state，内核态 → hw regs base） */
  void *ctx;

  /* ── 可能失败 → 返回 Linux 错误码（0=成功，负值=错误） ──────── */

  /* 读/写硬件寄存器 */
  int (*register_read)(void *ctx, uint64_t offset, uint64_t *out_val);
  int (*register_write)(void *ctx, uint64_t offset, uint64_t val);

  /* 设备内存 DMA 读/写 */
  int (*mem_read)(void *ctx, uint64_t dev_addr, void *host_buf, uint64_t size);
  int (*mem_write)(void *ctx, uint64_t dev_addr, const void *host_buf, uint64_t size);

  /* 设备内存（VRAM）分配/释放 */
  int (*mem_alloc)(void *ctx, uint64_t size, uint64_t *out_dev_addr);
  int (*mem_free)(void *ctx, uint64_t dev_addr);

  /* Fence 创建/状态读取 */
  int (*fence_create)(void *ctx, uint64_t *out_fence_id);
  int (*fence_read)(void *ctx, uint64_t fence_id, uint64_t *out_val);

  /* ── 弹射式操作 → void（不会失败） ──────────────────────── */

  void (*doorbell_ring)(void *ctx, uint32_t queue_id);
  void (*interrupt_raise)(void *ctx, uint32_t vector);
  void (*time_wait)(void *ctx, uint64_t us);

  /* ── ADR-061 扩展（KFD page migration） ────────────────── */

  int (*iommu_map)(void *ctx, uint64_t va, uint64_t size, uint32_t domain_id);
  int (*iommu_unmap)(void *ctx, uint64_t va, uint64_t size);

  /* ── ADR-062 扩展（KFD event signal） ──────────────────── */

  int (*event_signal)(void *ctx, uint32_t pasid, uint32_t event_id, uint64_t events);
};

/* ── inline 包装函数：零开销简化调用 ──────────────────────── */

static inline int hal_register_read(struct gpu_hal_ops *hal, uint64_t off, uint64_t *out) {
  return hal->register_read(hal->ctx, off, out);
}

static inline int hal_register_write(struct gpu_hal_ops *hal, uint64_t off, uint64_t val) {
  return hal->register_write(hal->ctx, off, val);
}

static inline int hal_mem_read(struct gpu_hal_ops *hal, uint64_t dev, void *hst, uint64_t sz) {
  return hal->mem_read(hal->ctx, dev, hst, sz);
}

static inline int hal_mem_write(struct gpu_hal_ops *hal, uint64_t dev, const void *hst,
                                uint64_t sz) {
  return hal->mem_write(hal->ctx, dev, hst, sz);
}

static inline int hal_mem_alloc(struct gpu_hal_ops *hal, uint64_t sz, uint64_t *out) {
  return hal->mem_alloc(hal->ctx, sz, out);
}

static inline int hal_mem_free(struct gpu_hal_ops *hal, uint64_t addr) {
  return hal->mem_free(hal->ctx, addr);
}

static inline int hal_fence_create(struct gpu_hal_ops *hal, uint64_t *out_id) {
  return hal->fence_create(hal->ctx, out_id);
}

static inline int hal_fence_read(struct gpu_hal_ops *hal, uint64_t id, uint64_t *out) {
  return hal->fence_read(hal->ctx, id, out);
}

static inline void hal_doorbell_ring(struct gpu_hal_ops *hal, uint32_t qid) {
  hal->doorbell_ring(hal->ctx, qid);
}

static inline void hal_interrupt_raise(struct gpu_hal_ops *hal, uint32_t vec) {
  hal->interrupt_raise(hal->ctx, vec);
}

static inline void hal_time_wait(struct gpu_hal_ops *hal, uint64_t us) {
  hal->time_wait(hal->ctx, us);
}

/* ── ADR-061 inline wrapper（KFD page migration） ───────────── */

static inline int hal_iommu_map(struct gpu_hal_ops *hal, uint64_t va, uint64_t size,
                                uint32_t domain_id) {
  return hal->iommu_map(hal->ctx, va, size, domain_id);
}

static inline int hal_iommu_unmap(struct gpu_hal_ops *hal, uint64_t va, uint64_t size) {
  return hal->iommu_unmap(hal->ctx, va, size);
}

/* ── ADR-062 inline wrapper（KFD event signal） ─────────────── */

static inline int hal_event_signal(struct gpu_hal_ops *hal, uint32_t pasid,
                                   uint32_t event_id, uint64_t events) {
  return hal->event_signal(hal->ctx, pasid, event_id, events);
}

#ifdef __cplusplus
}
#endif
