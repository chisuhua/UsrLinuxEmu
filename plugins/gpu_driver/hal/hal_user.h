/*
 * hal_user.h — HAL 用户态初始化接口
 *
 * hal_user_init() 初始化用户态 HAL 实现，挂载所有 10 个回调。
 * 调用者需要提供 struct hal_user_context 的内存（通常栈或静态分配）。
 *
 * struct hal_user_context 定义公开（调用者需要分配内存），
 * 但内部字段是私有的——只应通过 hal_* 函数访问。
 */
#pragma once

#include <cstddef>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <unordered_map>
#include "gpu_buddy.h"
#include "gpu_hal.h"

/* Stage 4.7.2: GpuQueueEmu instance storage for HAL opaque queue handles. */
#include "../sim/gpu_queue_emu.h"

/* Stage 4.7.3: HardwarePullerEmu instance storage for HAL opaque puller handles. */
#include "../sim/hardware/hardware_puller_emu.h"

/* Forward declaration - SemaphoreManager from sim layer (Stage 4.5) */
class SemaphoreManager;

#define HAL_REGS_COUNT 256
#define HAL_HEAP_BASE  (0x100000000ULL)  /* GPU device memory base address */
#define HAL_HEAP_SIZE  (256ULL * 1024 * 1024)
#define HAL_MAX_FENCES 128

struct hal_user_context {
  /* 以下是内部实现细节，调用者不应直接访问 */
  uint64_t regs[HAL_REGS_COUNT];
  std::mutex regs_lock;

  /* 设备内存堆（动态分配，初始化时由 hal_user_init 创建） */
  uint8_t *heap;
  struct gpu_buddy buddy;
  std::mutex heap_lock;
  bool buddy_initialized;

  bool fence_signaled[HAL_MAX_FENCES];
  uint64_t fence_counter;
  std::mutex fence_lock;
  std::atomic<uint64_t> doorbell_count{0};
  std::atomic<uint64_t> interrupt_count{0};

  /* Interrupt handlers (4 vectors, Stage 4.7.3 fix-hal-user-missing-interrupt-wiring) */
  void (*interrupt_handlers[4])(uint64_t user_data);
  uint64_t interrupt_handler_data[4];

  /* Doorbell 回调（由仿真层设置，HAL 在 doorbell_ring 时调用） */
  void (*doorbell_ring_cb)(void* cb_ctx, uint32_t queue_id);
  void* doorbell_ring_cb_ctx;

  /* IOMMU 映射跟踪（ADR-061: hal-iommu-full + ADR-062: hal-event-signal） */
  std::unordered_map<uint64_t, uint64_t> iommu_mappings;  /* VA → size */
  std::mutex iommu_lock;

  /* Event signal/wait/notify 状态（ADR-062: hal-event-signal） */
  std::mutex event_lock;
  std::condition_variable event_cv;
  bool event_signaled[256];

  /* Stage 4.5: SemaphoreManager for fence→sem migration */
  SemaphoreManager* sem_mgr = nullptr;

  /* Stage 4.7.2: GpuQueueEmu instance storage for HAL opaque queue handles. */
  std::unordered_map<hal_queue_handle_t, std::shared_ptr<GpuQueueEmu>> queues;
  std::mutex queue_lock;
  uint64_t next_queue_handle = 1;

  /* Stage 4.7.3: back-pointer to the gpu_hal_ops that owns this context. */
  struct gpu_hal_ops* hal_ops = nullptr;

  /* Stage 4.7.4: Green Context handles (ADR-056). */
  std::unordered_map<uint64_t, class GreenContext*> green_context_handles;
  std::mutex green_context_lock;
  uint64_t next_green_context_handle = 1;

  /* Stage 4.7.3: HardwarePullerEmu instance storage for HAL opaque puller handles. */
  std::unordered_map<hal_puller_handle_t, std::shared_ptr<HardwarePullerEmu>> pullers;
  std::mutex puller_lock;
  uint64_t next_puller_handle = 1;

  /* Cross-handle resolution: hal_queue_handle_t -> raw GpuQueueEmu*.
   * Used by puller_register_queue to resolve a queue handle back to the
   * underlying object without exposing shared_ptr semantics through the
   * HAL C interface. */
  std::unordered_map<hal_queue_handle_t, GpuQueueEmu*> queue_ptrs;
};

void hal_user_init(struct gpu_hal_ops *hal, struct hal_user_context *ctx);
void hal_user_destroy(struct hal_user_context *ctx);
int hal_user_set_doorbell_cb(struct hal_user_context* ctx,
                               void (*cb)(void*, uint32_t),
                               void* cb_ctx);