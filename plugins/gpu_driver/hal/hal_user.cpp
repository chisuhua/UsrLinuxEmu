/*
 * hal_user.cpp — HAL 用户态实现
 *
 * 实现 struct gpu_hal_ops 的 10 个函数指针的用户态版本。
 * 上下文定义在 hal_user.h（调用者需要分配内存）。
 */
#include "hal_user.h"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <chrono>

/* ── 内部回调实现 ────────────────────────────────── */

static int user_reg_read(void *ctx, uint64_t offset, uint64_t *out_val) {
  auto *hc = static_cast<struct hal_user_context *>(ctx);
  if (offset >= HAL_REGS_COUNT * sizeof(uint64_t))
    return -EINVAL;
  auto idx = static_cast<size_t>(offset / sizeof(uint64_t));
  std::lock_guard<std::mutex> lock(hc->regs_lock);
  *out_val = hc->regs[idx];
  return 0;
}

static int user_reg_write(void *ctx, uint64_t offset, uint64_t val) {
  auto *hc = static_cast<struct hal_user_context *>(ctx);
  if (offset >= HAL_REGS_COUNT * sizeof(uint64_t))
    return -EINVAL;
  auto idx = static_cast<size_t>(offset / sizeof(uint64_t));
  std::lock_guard<std::mutex> lock(hc->regs_lock);
  hc->regs[idx] = val;
  return 0;
}

static int user_mem_read(void *ctx, uint64_t dev_addr, void *host_buf, uint64_t size) {
  auto *hc = static_cast<struct hal_user_context *>(ctx);
  uint64_t heap_off = dev_addr - HAL_HEAP_BASE;
  if (heap_off + size > HAL_HEAP_SIZE || host_buf == nullptr)
    return -EINVAL;
  std::lock_guard<std::mutex> lock(hc->heap_lock);
  memcpy(host_buf, hc->heap + heap_off, size);
  return 0;
}

static int user_mem_write(void *ctx, uint64_t dev_addr, const void *host_buf, uint64_t size) {
  auto *hc = static_cast<struct hal_user_context *>(ctx);
  uint64_t heap_off = dev_addr - HAL_HEAP_BASE;
  if (heap_off + size > HAL_HEAP_SIZE || host_buf == nullptr)
    return -EINVAL;
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
  return -ENOMEM;
}

static int user_fence_read(void *ctx, uint64_t fence_id, uint64_t *out_val) {
  auto *hc = static_cast<struct hal_user_context *>(ctx);
  if (fence_id >= HAL_MAX_FENCES)
    return -EINVAL;
  std::lock_guard<std::mutex> lock(hc->fence_lock);
  *out_val = hc->fence_signaled[fence_id] ? 1 : 0;
  return 0;
}

static void user_doorbell_ring(void *ctx, uint32_t queue_id) {
  auto *hc = static_cast<struct hal_user_context *>(ctx);
  hc->doorbell_count.fetch_add(1, std::memory_order_relaxed);
  if (hc->doorbell_ring_cb) {
    hc->doorbell_ring_cb(hc->doorbell_ring_cb_ctx, queue_id);
  }
}

static void user_interrupt_raise(void *ctx, uint32_t vector) {
  auto *hc = static_cast<struct hal_user_context *>(ctx);
  hc->interrupt_count.fetch_add(1, std::memory_order_relaxed);
  // TODO(vector-dispatch): MSI-X vector routing for per-IH handlers
  (void)vector;
}

static void user_time_wait(void *ctx, uint64_t us) {
  (void)ctx;
  std::this_thread::sleep_for(std::chrono::microseconds(us));
}

/* ── ADR-061: IOMMU page mapping（hal-iommu-full 实现）── */

static int user_iommu_map(void *ctx, uint64_t va, uint64_t size, uint32_t domain_id) {
  (void)domain_id;
  auto *hc = static_cast<struct hal_user_context *>(ctx);

  if (size == 0)
    return -EINVAL;

  std::lock_guard<std::mutex> lock(hc->iommu_lock);

  /* Check double-map: VA already mapped? */
  if (hc->iommu_mappings.count(va) > 0)
    return -EEXIST;

  hc->iommu_mappings[va] = size;
  return 0;
}

static int user_iommu_unmap(void *ctx, uint64_t va, uint64_t size) {
  auto *hc = static_cast<struct hal_user_context *>(ctx);

  std::lock_guard<std::mutex> lock(hc->iommu_lock);

  auto it = hc->iommu_mappings.find(va);
  if (it == hc->iommu_mappings.end())
    return -ENOENT;

  /* Size check: must match mapped size */
  if (size != 0 && it->second != size)
    return -EINVAL;

  hc->iommu_mappings.erase(it);
  return 0;
}

/* ── ADR-062 stub: KFD event signal（真机 KFD 路径，C-12 阶段不实施）── */

static int user_event_signal(void *ctx, uint32_t pasid, uint32_t event_id, uint64_t events) {
  auto *hc = static_cast<struct hal_user_context *>(ctx);
  (void)pasid;

  if (event_id >= 256) return -EINVAL;
  if (events == 0) return -EINVAL;

  {
    std::lock_guard<std::mutex> lock(hc->event_lock);
    hc->event_signaled[event_id] = true;
  }
  hc->event_cv.notify_one();
  return 0;
}

static int user_event_wait(void *ctx, uint32_t event_id, uint64_t timeout_us) {
  auto *hc = static_cast<struct hal_user_context *>(ctx);

  if (event_id >= 256) return -EINVAL;

  std::unique_lock<std::mutex> lock(hc->event_lock);

  if (timeout_us == 0) {
    if (hc->event_signaled[event_id]) {
      return 0;
    }
    return -110;
  }

  if (timeout_us == UINT64_MAX) {
    hc->event_cv.wait(lock, [&]() {
      return hc->event_signaled[event_id];
    });
    return 0;
  }

  bool signaled = hc->event_cv.wait_for(lock,
      std::chrono::microseconds(timeout_us), [&]() {
        return hc->event_signaled[event_id];
      });
  if (signaled) {
    return 0;
  }
  return -110;
}

static int user_event_notify(void *ctx, uint32_t event_id) {
  auto *hc = static_cast<struct hal_user_context *>(ctx);

  if (event_id >= 256) return -EINVAL;

  {
    std::lock_guard<std::mutex> lock(hc->event_lock);
    hc->event_signaled[event_id] = true;
  }
  hc->event_cv.notify_all();
  return 0;
}

static int user_mem_map_bo(struct gpgpu_device* dev, uint64_t bo_offset,
                           size_t size, void** user_map) {
  (void)dev; (void)bo_offset; (void)size; (void)user_map;
  return -ENOSYS;
}

/* ── 公开初始化函数 ────────────────────────────────── */

void hal_user_init(struct gpu_hal_ops *hal, struct hal_user_context *ctx) {
  /* Zero-initialize POD members (avoid memset which is UB with std::mutex and std::atomic) */
  memset(ctx->regs, 0, sizeof(ctx->regs));
  memset(&ctx->buddy, 0, sizeof(ctx->buddy));
  memset(ctx->fence_signaled, 0, sizeof(ctx->fence_signaled));
  memset(ctx->event_signaled, 0, sizeof(ctx->event_signaled));
  ctx->heap = nullptr;
  ctx->buddy_initialized = false;
  ctx->fence_counter = 0;
  ctx->doorbell_count.store(0, std::memory_order_relaxed);
  ctx->interrupt_count.store(0, std::memory_order_relaxed);
  ctx->doorbell_ring_cb = nullptr;
  ctx->doorbell_ring_cb_ctx = nullptr;
  /* NOTE: std::mutex members (regs_lock, heap_lock, fence_lock) retain their
     default-constructed valid state; hal_user_init() must not destroy them. */

  /* 分配设备内存堆 */
  ctx->heap = static_cast<uint8_t*>(std::malloc(HAL_HEAP_SIZE));

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
  hal->iommu_map = user_iommu_map;
  hal->iommu_unmap = user_iommu_unmap;
  hal->event_signal = user_event_signal;
  hal->event_wait = user_event_wait;
  hal->event_notify = user_event_notify;
  hal->mem_map_bo = user_mem_map_bo;
  /* Stage 4.5: preemption stubs (wired when SemaphoreManager available) */
  hal->hal_preempt = [](void*, uint32_t) -> int { return 0; };
  hal->hal_resume = [](void*, uint32_t) -> int { return 0; };
  hal->hal_sem_create = [](void*, uint64_t init, uint64_t* out) -> int {
    static uint64_t next_handle = 1;
    *out = next_handle++;
    return 0;
  };
  hal->hal_sem_signal = [](void*, uint64_t, uint64_t) -> int { return 0; };
  hal->hal_sem_wait = [](void*, uint64_t, uint64_t,
                          void (*)(uint64_t), uint64_t) -> int { return 0; };
  hal->hal_sem_query = [](void*, uint64_t, uint64_t* out) -> int {
    *out = 0;
    return 0;
  };
  hal->hal_sem_destroy = [](void*, uint64_t) -> int { return 0; };
}

void hal_user_destroy(struct hal_user_context *ctx) {
  free(ctx->heap);
  ctx->heap = nullptr;
}

/** @brief Set the doorbell ring callback (set-once contract).
 *
 *  The callback, once set, is immutable — no reset/replace API exists.
 *  Must be called BEFORE any concurrent `doorbell_ring` invocation.
 *
 *  @param ctx    HAL user context
 *  @param cb     Callback invoked on every doorbell ring
 *  @param cb_ctx Opaque context forwarded to the callback
 *  @return 0 on success, -EBUSY if callback already set
 */
int hal_user_set_doorbell_cb(struct hal_user_context* ctx,
                                void (*cb)(void*, uint32_t),
                                void* cb_ctx) {
  if (ctx->doorbell_ring_cb != nullptr) {
    return -EBUSY;
  }
  ctx->doorbell_ring_cb = cb;
  ctx->doorbell_ring_cb_ctx = cb_ctx;
  return 0;
}
