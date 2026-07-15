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
#include <mutex>
#include "gpu_buddy.h"
#include "gpu_hal.h"

#define HAL_REGS_COUNT 256
#define HAL_HEAP_SIZE (256ULL * 1024 * 1024)
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

  /* Doorbell 回调（由仿真层设置，HAL 在 doorbell_ring 时调用） */
  void (*doorbell_ring_cb)(void* cb_ctx, uint32_t queue_id);
  void* doorbell_ring_cb_ctx;
};

void hal_user_init(struct gpu_hal_ops *hal, struct hal_user_context *ctx);
void hal_user_destroy(struct hal_user_context *ctx);
int hal_user_set_doorbell_cb(struct hal_user_context* ctx,
                               void (*cb)(void*, uint32_t),
                               void* cb_ctx);
