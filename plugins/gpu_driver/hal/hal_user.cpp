/*
 * hal_user.cpp — HAL 用户态实现
 *
 * 实现 struct gpu_hal_ops 的 10 个函数指针的用户态版本。
 * 上下文定义在 hal_user.h（调用者需要分配内存）。
 */
#include "hal_user.h"
#include <cstdlib>
#include <cstring>
#include <thread>

#define HAL_HEAP_BASE 0x100000000ULL

/* ── 内部回调实现 ────────────────────────────────── */

static int user_reg_read(void *ctx, uint64_t offset, uint64_t *out_val) {
  auto *hc = static_cast<struct hal_user_context *>(ctx);
  if (offset >= HAL_REGS_COUNT * sizeof(uint64_t))
    return -22; /* -EINVAL */
  unsigned idx = (unsigned)(offset / sizeof(uint64_t));
  std::lock_guard<std::mutex> lock(hc->regs_lock);
  *out_val = hc->regs[idx];
  return 0;
}

static int user_reg_write(void *ctx, uint64_t offset, uint64_t val) {
  auto *hc = static_cast<struct hal_user_context *>(ctx);
  if (offset >= HAL_REGS_COUNT * sizeof(uint64_t))
    return -22;
  unsigned idx = (unsigned)(offset / sizeof(uint64_t));
  std::lock_guard<std::mutex> lock(hc->regs_lock);
  hc->regs[idx] = val;
  return 0;
}

static int user_mem_read(void *ctx, uint64_t dev_addr, void *host_buf, uint64_t size) {
  auto *hc = static_cast<struct hal_user_context *>(ctx);
  uint64_t heap_off = dev_addr - HAL_HEAP_BASE;
  if (heap_off + size > HAL_HEAP_SIZE || host_buf == nullptr)
    return -22;
  std::lock_guard<std::mutex> lock(hc->heap_lock);
  memcpy(host_buf, hc->heap + heap_off, size);
  return 0;
}

static int user_mem_write(void *ctx, uint64_t dev_addr, const void *host_buf, uint64_t size) {
  auto *hc = static_cast<struct hal_user_context *>(ctx);
  uint64_t heap_off = dev_addr - HAL_HEAP_BASE;
  if (heap_off + size > HAL_HEAP_SIZE || host_buf == nullptr)
    return -22;
  std::lock_guard<std::mutex> lock(hc->heap_lock);
  memcpy(hc->heap + heap_off, host_buf, size);
  return 0;
}

static int user_mem_alloc(void *ctx, uint64_t size, uint64_t *out_dev_addr) {
  auto *hc = static_cast<struct hal_user_context *>(ctx);
  std::lock_guard<std::mutex> lock(hc->heap_lock);

  /* 首次使用时懒初始化 buddy */
  if (!hc->buddy_initialized) {
    gpu_buddy_init(&hc->buddy, HAL_HEAP_BASE, HAL_HEAP_SIZE);
    hc->buddy_initialized = true;
  }

  uint64_t addr = 0;
  int ret = gpu_buddy_alloc(&hc->buddy, size, &addr);
  if (ret == 0 && out_dev_addr)
    *out_dev_addr = addr;
  return ret;
}

static int user_mem_free(void *ctx, uint64_t dev_addr) {
  auto *hc = static_cast<struct hal_user_context *>(ctx);
  std::lock_guard<std::mutex> lock(hc->heap_lock);
  return gpu_buddy_free(&hc->buddy, dev_addr);
}

static int user_fence_create(void *ctx, uint64_t *out_fence_id) {
  auto *hc = static_cast<struct hal_user_context *>(ctx);
  std::lock_guard<std::mutex> lock(hc->fence_lock);
  for (int i = 0; i < HAL_MAX_FENCES; i++) {
    if (!hc->fence_signaled[i]) {
      hc->fence_signaled[i] = true; /* 占用槽位 */
      *out_fence_id = i;
      return 0;
    }
  }
  return -12; /* -ENOMEM */
}

static int user_fence_read(void *ctx, uint64_t fence_id, uint64_t *out_val) {
  auto *hc = static_cast<struct hal_user_context *>(ctx);
  if (fence_id >= HAL_MAX_FENCES)
    return -22;
  std::lock_guard<std::mutex> lock(hc->fence_lock);
  *out_val = hc->fence_signaled[fence_id] ? 1 : 0;
  return 0;
}

static void user_doorbell_ring(void *ctx, uint32_t queue_id) {
  auto *hc = static_cast<struct hal_user_context *>(ctx);
  hc->doorbell_count++;
  (void)queue_id;
}

static void user_interrupt_raise(void *ctx, uint32_t vector) {
  auto *hc = static_cast<struct hal_user_context *>(ctx);
  hc->interrupt_count++;
  (void)vector;
}

static void user_time_wait(void *ctx, uint64_t us) {
  (void)ctx;
  std::this_thread::sleep_for(std::chrono::microseconds(us));
}

/* ── 公开初始化函数 ────────────────────────────────── */

void hal_user_init(struct gpu_hal_ops *hal, struct hal_user_context *ctx) {
  /* 清零上下文 */
  memset(ctx, 0, sizeof(*ctx));

  /* 分配设备内存堆 */
  ctx->heap = (uint8_t *)malloc(HAL_HEAP_SIZE);

  /* 挂载回调 */
  hal->ctx = ctx;
  hal->register_read = user_reg_read;
  hal->register_write = user_reg_write;
  hal->mem_read = user_mem_read;
  hal->mem_write = user_mem_write;
  hal->mem_alloc = user_mem_alloc;
  hal->mem_free = user_mem_free;
  hal->fence_create = user_fence_create;
  hal->fence_read = user_fence_read;
  hal->doorbell_ring = user_doorbell_ring;
  hal->interrupt_raise = user_interrupt_raise;
  hal->time_wait = user_time_wait;
}

void hal_user_destroy(struct hal_user_context *ctx) {
  free(ctx->heap);
  ctx->heap = nullptr;
}
