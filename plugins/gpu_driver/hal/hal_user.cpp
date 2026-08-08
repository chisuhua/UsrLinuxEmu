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

#include "../sim/semaphore_manager.h"  // Stage 4.5: fence→sem migration
#include "../sim/fence_id.h"             // Stage 4.6 L2 foundation: fence_id_* fn-ptrs
#include "../sim/vram_store.h"           // Stage 4.1: g_vram_store for BAR2 mmap
#include "../sim/hardware/method_codec.h" // Stage 4.6 L2 foundation: method_codec_encode fn-ptr

// Stage 4.6 L2 foundation (ADR-072 §Decision 4) — Phase 2: 28 new fn-ptrs
#include "../sim/graph.h"               // sim_graph_*
#include "../sim/mem_pool.h"            // sim_mem_pool_*
#include "../sim/stream_capture.h"      // sim_stream_capture_*
#include "../sim/hardware/hardware_puller_emu.h" // Stage 4.7.3: puller_create impl
#include "../sim/scheduler/global_scheduler.h"   // Stage 4.7.3: puller_create scheduler arg
#include "../sim/green_context.h"                // Stage 4.7.4: green_context create/destroy
#include "../sim/pdl.h"                           // Stage 4.7.4: PDL kernel launch
#include "../sim/backdoor_preempt.h"             // Stage 4.7.3: backdoor_force_preempt/resume

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
  /* Stage 4.5: prefer SemaphoreManager when available */
  if (hc->sem_mgr) {
    uint64_t h = hc->sem_mgr->create(0);
    if (h == 0) return -ENOMEM;
    *out_fence_id = h;
    return 0;
  }
  /* Legacy path: fixed-size array */
  std::lock_guard<std::mutex> lock(hc->fence_lock);
  for (int i = 0; i < HAL_MAX_FENCES; i++) {
    if (!hc->fence_signaled[i]) {
      hc->fence_signaled[i] = true;
      *out_fence_id = i;
      return 0;
    }
  }
  return -ENOMEM;
}

static int user_fence_read(void *ctx, uint64_t fence_id, uint64_t *out_val) {
  auto *hc = static_cast<struct hal_user_context *>(ctx);
  /* Stage 4.5: prefer SemaphoreManager when available */
  if (hc->sem_mgr) {
    uint64_t val = hc->sem_mgr->query(fence_id);
    if (val == UINT64_MAX) return -EINVAL;
    *out_val = (val > 0) ? 1 : 0;
    return 0;
  }
  /* Legacy path: fixed-size array */
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
  if (vector >= 4) return;
  auto handler = hc->interrupt_handlers[vector];
  if (handler) {
    handler(0);  /* legacy signature has no user_data */
  }
}

static int user_interrupt_register(void *ctx, uint32_t vector,
                                  void (*handler)(uint64_t user_data)) {
  auto *hc = static_cast<struct hal_user_context *>(ctx);
  if (vector >= 4) return -EINVAL;
  hc->interrupt_handlers[vector] = handler;
  hc->interrupt_handler_data[vector] = 0;
  hc->interrupt_count.fetch_add(1, std::memory_order_relaxed);
  return 0;
}

static void user_interrupt_raise_ex(void *ctx, uint32_t vector,
                                   uint64_t user_data) {
  auto *hc = static_cast<struct hal_user_context *>(ctx);
  if (vector >= 4) return;
  hc->interrupt_handler_data[vector] = user_data;
  hc->interrupt_count.fetch_add(1, std::memory_order_relaxed);
  auto handler = hc->interrupt_handlers[vector];
  if (handler) {
    handler(user_data);
  }
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
  (void)dev;
  if (!usr_linux_emu::g_vram_store.initialized) {
    return -ENODEV;
  }
  if (usr_linux_emu::g_vram_store.pool_backing == nullptr) {
    return -ENODEV;
  }
  if (bo_offset > usr_linux_emu::g_vram_store.vram_size) {
    return -EINVAL;
  }
  if (bo_offset + size > usr_linux_emu::g_vram_store.vram_size) {
    return -EINVAL;
  }
  *user_map = static_cast<uint8_t*>(usr_linux_emu::g_vram_store.pool_backing) + bo_offset;
  return 0;
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
  ctx->hal_ops = hal;
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
  hal->interrupt_register = user_interrupt_register;
  hal->interrupt_raise_ex = user_interrupt_raise_ex;
  hal->time_wait = user_time_wait;
  hal->iommu_map = user_iommu_map;
  hal->iommu_unmap = user_iommu_unmap;
  hal->event_signal = user_event_signal;
  hal->event_wait = user_event_wait;
  hal->event_notify = user_event_notify;
  hal->mem_map_bo = user_mem_map_bo;
  hal->hal_preempt = [](void* ctx, uint32_t channel_id) -> int {
    return backdoor_force_preempt(channel_id);
  };
  hal->hal_resume = [](void* ctx, uint32_t channel_id) -> int {
    return backdoor_force_resume(channel_id);
  };
  hal->hal_sem_create = [](void* ctx, uint64_t init, uint64_t* out) -> int {
    auto* hc = static_cast<struct hal_user_context*>(ctx);
    if (!hc->sem_mgr) return -ENODEV;
    uint64_t h = hc->sem_mgr->create(init);
    if (h == 0) return -ENOMEM;
    *out = h;
    return 0;
  };
  hal->hal_sem_signal = [](void* ctx, uint64_t handle, uint64_t value) -> int {
    auto* hc = static_cast<struct hal_user_context*>(ctx);
    if (!hc->sem_mgr) return -ENODEV;
    return hc->sem_mgr->signal(handle, value);
  };
  hal->hal_sem_wait = [](void* ctx, uint64_t handle, uint64_t expected,
                          void (*callback)(uint64_t, uint64_t),
                          uint64_t user_data) -> int {
    auto* hc = static_cast<struct hal_user_context*>(ctx);
    if (!hc->sem_mgr) return -ENODEV;
    return hc->sem_mgr->wait(
        handle, expected,
        [callback, user_data](uint64_t actual) { callback(actual, user_data); },
        user_data);
  };
  hal->hal_sem_query = [](void* ctx, uint64_t handle, uint64_t* out) -> int {
    auto* hc = static_cast<struct hal_user_context*>(ctx);
    if (!hc->sem_mgr) return -ENODEV;
    uint64_t val = hc->sem_mgr->query(handle);
    if (val == UINT64_MAX) return -EINVAL;
    *out = val;
    return 0;
  };
  hal->hal_sem_destroy = [](void* ctx, uint64_t handle) -> int {
    auto* hc = static_cast<struct hal_user_context*>(ctx);
    if (!hc->sem_mgr) return -ENODEV;
    return hc->sem_mgr->destroy(handle);
  };
  hal->hal_green_context_create = [](void* ctx, uint64_t tsg_id,
                                     uint64_t* out_handle) -> int {
    auto* hc = static_cast<struct hal_user_context*>(ctx);
    if (!out_handle) return -EINVAL;
    GreenContext* gc = GreenContext::create(tsg_id);
    if (!gc) return -ENOMEM;
    std::lock_guard<std::mutex> lock(hc->green_context_lock);
    uint64_t h = hc->next_green_context_handle++;
    hc->green_context_handles[h] = gc;
    *out_handle = h;
    return 0;
  };
  hal->hal_green_context_destroy = [](void* ctx, uint64_t handle) -> int {
    auto* hc = static_cast<struct hal_user_context*>(ctx);
    std::unique_lock<std::mutex> lock(hc->green_context_lock);
    auto it = hc->green_context_handles.find(handle);
    if (it == hc->green_context_handles.end()) return -EINVAL;
    GreenContext* gc = it->second;
    hc->green_context_handles.erase(it);
    lock.unlock();
    return gc->destroy();
  };
  hal->hal_pdl_launch = [](void*, uint64_t kernel_addr, uint64_t kernargs_va,
                           uint32_t grid_x, uint32_t block_x,
                           uint64_t* out_signal_handle) -> int {
    PdlLauncher launcher;
    return launcher.launch(kernel_addr, kernargs_va, grid_x, block_x,
                            out_signal_handle);
  };
  hal->hal_pdl_signal_completion = [](void* ctx, uint64_t handle,
                                      uint64_t value) -> int {
    auto* hc = static_cast<struct hal_user_context*>(ctx);
    if (!hc->sem_mgr) return -ENODEV;
    return hc->sem_mgr->signal(handle, value);
  };

  /* ── Stage 4.6 L2 foundation (ADR-072 §Decision 4) — B-class fix (Phase 1) ─
   * 5 new fn-ptrs enabling drv/ to call sim-layer functions without
   * #including sim/ headers. Production impl delegates to existing sim
   * functions; mock impl lives in hal_mock.cpp. */
  hal->fence_id_alloc = [](void*) -> int64_t {
    return sim_fence_id_alloc();
  };
  hal->fence_id_signal = [](void*, uint64_t fence_id) -> void {
    sim_fence_id_signal(fence_id);
  };
  hal->fence_id_check = [](void*, uint64_t fence_id, bool* signaled) -> int {
    return sim_fence_id_check(fence_id, signaled);
  };
  hal->method_codec_encode = [](void*, const gpu_method_packet* pkt,
                              const uint32_t* data) -> int {
    /* Result vector discarded by drv/ callers (see gpgpu_device.cpp). */
    (void)method_codec_encode(*pkt, data);
    return 0;
  };
  hal->heap_ptr = [](void* ctx, uint64_t gpu_va) -> void* {
    auto* hc = static_cast<struct hal_user_context*>(ctx);
    return hc->heap + (gpu_va - HAL_HEAP_BASE);
  };

  /* ── Stage 4.6 L2 foundation (ADR-072 §Decision 4) — Phase 2 ─── */

  /* ── graph lambdas (7): delegate to sim_graph_* ──────────────── */
  hal->graph_create = [](void*, uint64_t* out) -> int {
    return sim_graph_create(out);
  };
  hal->graph_destroy = [](void*, uint64_t h) -> int {
    return sim_graph_destroy(h);
  };
  hal->graph_add_kernel_node = [](void*, uint64_t g, uint32_t kidx,
                                  uint32_t gx, uint32_t gy, uint32_t gz,
                                  uint32_t bx, uint32_t by, uint32_t bz,
                                  uint64_t* kernargs_bo) -> int {
    return sim_graph_add_kernel_node(g, kidx, gx, gy, gz, bx, by, bz,
                                    kernargs_bo);
  };
  hal->graph_add_memcpy_node = [](void*, uint64_t g, uint64_t src,
                                  uint64_t dst, uint64_t size,
                                  int is_h2d) -> int {
    return sim_graph_add_memcpy_node(g, src, dst, size, is_h2d);
  };
  hal->graph_instantiate = [](void*, uint64_t g, uint64_t* out) -> int {
    return sim_graph_instantiate(g, out);
  };
  hal->graph_launch = [](void*, uint64_t exec, uint32_t stream_id,
                         uint64_t* gpfifo_out, uint32_t* count_out) -> int {
    return sim_graph_launch(exec, stream_id, gpfifo_out, count_out);
  };
  hal->graph_destroy_exec = [](void*, uint64_t exec) -> int {
    return sim_graph_destroy_exec(exec);
  };

  /* ── mem_pool lambdas (9 + export): delegate to sim_mem_pool_* ─── */
  hal->mem_pool_create = [](void*, const void* props, uint64_t* out) -> int {
    if (!props || !out) return -EINVAL;
    sim_mem_pool_props_t* sim_props = const_cast<sim_mem_pool_props_t*>(
        reinterpret_cast<const sim_mem_pool_props_t*>(props));
    return sim_mem_pool_create(sim_props, out);
  };
  hal->mem_pool_destroy = [](void*, uint64_t h) -> int {
    return sim_mem_pool_destroy(h);
  };
  hal->mem_pool_alloc = [](void*, uint64_t h, uint64_t size,
                           uint64_t* out) -> int {
    return sim_mem_pool_alloc(h, size, out);
  };
  hal->mem_pool_alloc_async = [](void*, uint64_t h, uint64_t size,
                                 int64_t* out) -> int {
    (void)h; (void)size;
    int64_t fence = sim_fence_id_alloc();
    if (fence < 0) return -ENOMEM;
    sim_fence_id_signal(static_cast<uint64_t>(fence));
    if (out) *out = fence;
    return 0;
  };
  hal->mem_pool_free = [](void*, uint64_t h, uint64_t va) -> int {
    (void)h;
    sim_mem_pool_free_async(va, 0);
    return 0;
  };
  hal->mem_pool_free_async = [](void*, uint64_t h, uint64_t va,
                                int64_t* out) -> int {
    (void)h;
    int64_t fence = sim_mem_pool_free_async(va, 0);
    if (out) *out = fence;
    return (fence < 0) ? static_cast<int>(fence) : 0;
  };
  hal->mem_pool_set_attr = [](void*, uint64_t h, uint32_t attr,
                              const void* value, uint64_t value_size) -> int {
    return sim_mem_pool_set_attr(h,
        static_cast<sim_mem_pool_attr_t>(attr), value,
        static_cast<size_t>(value_size));
  };
  hal->mem_pool_get_attr = [](void*, uint64_t h, uint32_t attr,
                              void* value_out, uint64_t value_size) -> int {
    return sim_mem_pool_get_attr(h,
        static_cast<sim_mem_pool_attr_t>(attr), value_out,
        static_cast<size_t>(value_size));
  };
  hal->mem_pool_trim = [](void*, uint64_t h, uint64_t min_bytes) -> int {
    return sim_mem_pool_trim(h, min_bytes);
  };
  hal->mem_pool_export_shareable = [](void*, uint64_t h, uint32_t handle_type,
                                     uint32_t flags, int32_t* fd_out) -> int {
    return sim_mem_pool_export_shareable(h, handle_type, flags, fd_out);
  };

  /* ── stream_capture lambdas (3): delegate to sim_stream_capture_* ── */
  hal->stream_capture_begin = [](void*, uint64_t stream_id,
                                 uint32_t mode) -> int {
    return sim_stream_capture_begin(static_cast<uint32_t>(stream_id), mode);
  };
  hal->stream_capture_end = [](void*, uint64_t stream_id,
                               uint64_t* out_graph) -> int {
    return sim_stream_capture_end(static_cast<uint32_t>(stream_id), out_graph);
  };
  hal->stream_capture_status = [](void*, uint64_t stream_id,
                                  uint32_t* out_status) -> int {
    /* sim_stream_capture_status uses sim_stream_capture_status_t (C++ enum).
     * For Phase 2 foundation, we pass-through as uint32_t* — the layout is
     * compatible (single uint32_t field). Real typed cast happens in
     * stream_capture removal change. */
    return sim_stream_capture_status(static_cast<uint32_t>(stream_id),
        reinterpret_cast<sim_stream_capture_status_t*>(out_status));
  };

  /* ── gpu_queue_emu lambdas (5): real GpuQueueEmu instances owned by
   * hal_user_context. drv/ sees only opaque hal_queue_handle_t. */
  hal->queue_create = [](void* ctx, uint32_t handle, uint32_t type,
                         uint32_t priority, uint32_t ring_size,
                         hal_queue_handle_t* out) -> int {
    if (!out) return -EINVAL;
    auto* hc = static_cast<struct hal_user_context*>(ctx);
    std::lock_guard<std::mutex> lock(hc->queue_lock);
    auto instance = std::make_shared<GpuQueueEmu>(handle, type, priority,
                                                  ring_size);
    hal_queue_handle_t qh = hc->next_queue_handle++;
    if (qh == 0) qh = hc->next_queue_handle++;  // skip 0 (NULL sentinel)
    hc->queues[qh] = std::move(instance);
    /* Cross-handle resolution for puller_register_queue. */
    hc->queue_ptrs[qh] = hc->queues[qh].get();
    *out = qh;
    return 0;
  };
  hal->queue_attach_shmem = [](void* ctx, hal_queue_handle_t q,
                                void* cpu_ptr, uint64_t size) -> int {
    if (!cpu_ptr) return -EINVAL;
    if (size == 0) return -EINVAL;
    auto* hc = static_cast<struct hal_user_context*>(ctx);
    std::lock_guard<std::mutex> lock(hc->queue_lock);
    auto it = hc->queues.find(q);
    if (it == hc->queues.end()) return -EINVAL;
    return it->second->attachSharedMemory(cpu_ptr, size);
  };
  hal->queue_submit = [](void* ctx, hal_queue_handle_t q,
                          uint64_t gpfifo_addr, uint32_t count,
                          int64_t* out_fence) -> int {
    auto* hc = static_cast<struct hal_user_context*>(ctx);
    std::lock_guard<std::mutex> lock(hc->queue_lock);
    auto it = hc->queues.find(q);
    if (it == hc->queues.end()) return -EINVAL;
    int64_t fence = sim_fence_id_alloc();
    if (fence < 0) return -ENOMEM;
    int ret = it->second->submit(gpfifo_addr, count,
                                    static_cast<uint64_t>(fence));
    if (ret != 0) return ret;
    if (out_fence) *out_fence = fence;
    return 0;
  };
  hal->queue_destroy = [](void* ctx, hal_queue_handle_t q) -> int {
    auto* hc = static_cast<struct hal_user_context*>(ctx);
    std::lock_guard<std::mutex> lock(hc->queue_lock);
    auto it = hc->queues.find(q);
    if (it == hc->queues.end()) return -EINVAL;
    hc->queue_ptrs.erase(q);
    hc->queues.erase(it);
    return 0;
  };
  hal->queue_register_puller = [](void* ctx, hal_queue_handle_t q,
                                  hal_puller_handle_t puller) -> int {
    if (puller == 0) return -EINVAL;
    auto* hc = static_cast<struct hal_user_context*>(ctx);
    std::scoped_lock lock(hc->puller_lock, hc->queue_lock);
    auto qit = hc->queue_ptrs.find(q);
    if (qit == hc->queue_ptrs.end()) return -EINVAL;
    auto pit = hc->pullers.find(puller);
    if (pit == hc->pullers.end()) return -EINVAL;
    qit->second->setPuller(pit->second.get());
    return 0;
  };

  /* ── hardware_puller_emu lambdas (5): real HardwarePullerEmu instances owned by
   * hal_user_context. drv/ sees only opaque hal_puller_handle_t. */
  hal->puller_create = [](void* ctx, void* doorbell, void* scheduler,
                          hal_puller_handle_t* out) -> int {
    if (!ctx || !doorbell || !scheduler || !out) return -EINVAL;
    auto* hc = static_cast<struct hal_user_context*>(ctx);
    auto* db = static_cast<DoorbellEmu*>(doorbell);
    auto* sched = static_cast<GlobalScheduler*>(scheduler);
    auto instance = std::make_shared<HardwarePullerEmu>(hc->hal_ops, db, sched);
    std::lock_guard<std::mutex> lock(hc->puller_lock);
    hal_puller_handle_t ph = hc->next_puller_handle++;
    if (ph == 0) ph = hc->next_puller_handle++;  // skip 0 (NULL sentinel)
    hc->pullers[ph] = std::move(instance);
    *out = ph;
    hc->pullers[ph]->start();
    return 0;
  };
  hal->puller_destroy = [](void* ctx, hal_puller_handle_t puller) -> int {
    if (puller == 0) return -EINVAL;
    auto* hc = static_cast<struct hal_user_context*>(ctx);
    std::shared_ptr<HardwarePullerEmu> instance;
    {
      std::lock_guard<std::mutex> lock(hc->puller_lock);
      auto it = hc->pullers.find(puller);
      if (it == hc->pullers.end()) return -EINVAL;
      instance = std::move(it->second);
      hc->pullers.erase(it);
    }
    instance->stop();
    return 0;
  };
  hal->puller_set_puller = [](void* ctx, hal_puller_handle_t puller,
                              uint64_t sim_puller_handle) -> int {
    if (puller == 0) return -EINVAL;
    auto* hc = static_cast<struct hal_user_context*>(ctx);
    std::lock_guard<std::mutex> lock(hc->puller_lock);
    auto it = hc->pullers.find(puller);
    if (it == hc->pullers.end()) return -EINVAL;
    return it->second->setSimPuller(sim_puller_handle);
  };
  hal->puller_register_queue = [](void* ctx, hal_puller_handle_t puller,
                                  hal_queue_handle_t queue) -> int {
    if (puller == 0 || queue == 0) return -EINVAL;
    auto* hc = static_cast<struct hal_user_context*>(ctx);
    std::scoped_lock lock(hc->puller_lock, hc->queue_lock);
    auto pit = hc->pullers.find(puller);
    if (pit == hc->pullers.end()) return -EINVAL;
    auto qit = hc->queue_ptrs.find(queue);
    if (qit == hc->queue_ptrs.end()) return -EINVAL;
    pit->second->registerQueue(qit->second);
    return 0;
  };
  hal->puller_unregister_queue = [](void* ctx, hal_puller_handle_t puller,
                                    uint32_t queue_id) -> int {
    if (puller == 0) return -EINVAL;
    auto* hc = static_cast<struct hal_user_context*>(ctx);
    std::lock_guard<std::mutex> lock(hc->puller_lock);
    auto it = hc->pullers.find(puller);
    if (it == hc->pullers.end()) return -EINVAL;
    it->second->unregisterQueue(queue_id);
    return 0;
  };
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
