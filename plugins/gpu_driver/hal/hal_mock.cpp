/*
 * hal_mock.cpp — HAL 测试 mock 实现
 *
 * 提供可控的 HAL 实现用于单元测试。
 * 每个函数记录调用参数，返回值由测试可控。
 */
#include "hal_mock.h"
#include <atomic>
#include <cerrno>
#include <cstring>
#include <condition_variable>
#include <mutex>
#include <functional>
#include <thread>
#include <pthread.h>
#include "kernel/thread/kernel_workqueue.h"
#include "sim/vram_store.h"

/* Forward declarations for C-12 B.3.4 + B.4.4 integration (Agent A's kfd_events + sim layer)
 * These are resolved at link time by kfd_events.c and sim_event.c respectively.
 */
extern "C" {
  /* kfd_events_get_workqueue — returns the kfd_events_thread_ workqueue.
   * Defined by Agent A in kfd_events.c (returns usr_linux_emu::kernel_workqueue* via C-linkage).  */
  void *kfd_events_get_workqueue(void);

  /* sim_signal_event — sim-layer event signal (see sim/sim_event.h) */
  int sim_signal_event(uint32_t pasid, uint32_t event_id, uint64_t events);

  /* sim_pm_* — page migration primitives (see sim/page_migration.h)
   * Phase B.3.3: hal_mock routes iommu_map/unmap through these. */
  struct sim_page_migration;
  struct sim_page_migration *sim_pm_create(unsigned long device_mem_size);
  void sim_pm_destroy(struct sim_page_migration *pm);
  int sim_pm_migrate_to_device(struct sim_page_migration *pm,
                                unsigned long offset,
                                const void *src, unsigned long size);
  int sim_pm_migrate_to_system(struct sim_page_migration *pm,
                                unsigned long offset,
                                void *dst, unsigned long size);
  unsigned long sim_pm_lookup_pfn(struct sim_page_migration *pm,
                                   unsigned long offset);
}

/* ── mock 回调函数 ────────────────────────────────── */

static int mock_reg_read(void *ctx, uint64_t offset, uint64_t *out_val) {
  auto *state = static_cast<struct hal_mock_state *>(ctx);
  state->register_read_count++;
  if (out_val)
    *out_val = state->register_read_out;
  return state->register_read_result;
}

static int mock_reg_write(void *ctx, uint64_t offset, uint64_t val) {
  auto *state = static_cast<struct hal_mock_state *>(ctx);
  state->register_write_count++;
  state->last_reg_write_val = val;
  return state->register_write_result;
}

static int mock_mem_read(void *ctx, uint64_t dev_addr, void *host_buf, uint64_t size) {
  auto *state = static_cast<struct hal_mock_state *>(ctx);
  state->mem_read_count++;
  return state->mem_read_result;
}

static int mock_mem_write(void *ctx, uint64_t dev_addr, const void *host_buf, uint64_t size) {
  auto *state = static_cast<struct hal_mock_state *>(ctx);
  state->mem_write_count++;
  state->last_mem_write_addr = dev_addr;
  return state->mem_write_result;
}

static int mock_mem_alloc(void *ctx, uint64_t size, uint64_t *out_dev_addr) {
  auto *state = static_cast<struct hal_mock_state *>(ctx);
  state->mem_alloc_count++;
  if (out_dev_addr)
    *out_dev_addr = state->mem_alloc_out_addr;
  return state->mem_alloc_result;
}

static int mock_mem_free(void *ctx, uint64_t dev_addr) {
  auto *state = static_cast<struct hal_mock_state *>(ctx);
  state->mem_free_count++;
  return state->mem_free_result;
}

static int mock_fence_create(void *ctx, uint64_t *out_fence_id) {
  auto *state = static_cast<struct hal_mock_state *>(ctx);
  state->fence_create_count++;
  if (out_fence_id)
    *out_fence_id = state->fence_create_out_id;
  return state->fence_create_result;
}

static int mock_fence_read(void *ctx, uint64_t fence_id, uint64_t *out_val) {
  auto *state = static_cast<struct hal_mock_state *>(ctx);
  state->fence_read_count++;
  if (out_val)
    *out_val = state->fence_read_out_val;
  return state->fence_read_result;
}

static void mock_doorbell_ring(void *ctx, uint32_t queue_id) {
  auto *state = static_cast<struct hal_mock_state *>(ctx);
  state->doorbell_ring_count++;
  state->last_doorbell_queue = queue_id;
}

static void mock_interrupt_raise(void *ctx, uint32_t vector) {
  auto *state = static_cast<struct hal_mock_state *>(ctx);
  state->interrupt_raise_count++;
  state->last_interrupt_vector = vector;
}

static int mock_interrupt_register(void *ctx, uint32_t vector,
                                    void (*handler)(uint64_t user_data)) {
  auto *state = static_cast<struct hal_mock_state *>(ctx);
  if (vector >= 4) return -1;
  state->interrupt_handlers[vector] = handler;
  state->interrupt_register_count++;
  state->last_register_vector = static_cast<int>(vector);
  return 0;
}

static void mock_interrupt_raise_ex(void *ctx, uint32_t vector,
                                    uint64_t user_data) {
  auto *state = static_cast<struct hal_mock_state *>(ctx);
  if (vector >= 4) return;
  state->interrupt_raise_ex_count++;
  state->last_raise_ex_vector = static_cast<int>(vector);
  state->last_raise_ex_data = user_data;
  auto handler = state->interrupt_handlers[vector];
  if (handler) {
    std::thread([handler, user_data]() { handler(user_data); }).detach();
  }
}

static void mock_time_wait(void *ctx, uint64_t us) {
  auto *state = static_cast<struct hal_mock_state *>(ctx);
  state->time_wait_count++;
  state->last_time_wait_us = us;
}

/* ── ADR-061/062 扩展 mock（C-12 KFD） ────────────────────── */

/* Phase B.3.3 (C-12): sim_pm_* integration replaces Day-1 counter stub.
 * Per ADR-061: mock_iommu_map/unmap now route through real sim_pm_* primitives.
 * The sim_page_migration singleton is lazily initialized on first iommu_map call. */
static struct sim_page_migration *mock_pm_ = nullptr;
static pthread_mutex_t mock_pm_lock_ = PTHREAD_MUTEX_INITIALIZER;

static int mock_pm_init_if_needed() {
  pthread_mutex_lock(&mock_pm_lock_);
  if (!mock_pm_) {
    mock_pm_ = sim_pm_create(16UL * 1024 * 1024);  /* 16 MB device memory */
  }
  pthread_mutex_unlock(&mock_pm_lock_);
  return mock_pm_ ? 0 : -12;  /* -ENOMEM */
}

static int mock_iommu_map(void *ctx, uint64_t va, uint64_t size, uint32_t domain_id) {
  (void)ctx;
  (void)domain_id;
  if (mock_pm_init_if_needed() != 0) return -12; /* -ENOMEM */

  /* va → device-memory offset; src → zero-init (real impl migrates CPU pages) */
  unsigned char src[4096] = {};
  unsigned long offset = (unsigned long)(va & 0xFFFFFFFF);
  if (offset + size > 16UL * 1024 * 1024) return -22; /* -EINVAL */
  return sim_pm_migrate_to_device(mock_pm_, offset, src, (unsigned long)size);
}

static int mock_iommu_unmap(void *ctx, uint64_t va, uint64_t size) {
  (void)ctx;
  if (!mock_pm_) return 0;  /* idempotent: nothing to unmap */

  unsigned long offset = (unsigned long)(va & 0xFFFFFFFF);
  if (offset + size > 16UL * 1024 * 1024) return -22; /* -EINVAL */
  /* migrate_to_system reads device memory to caller buffer;
   * we discard the result since no caller buffer exists here. */
  unsigned char dst[4096];
  return sim_pm_migrate_to_system(mock_pm_, offset, dst, (unsigned long)size);
}

static int mock_event_signal(void *ctx, uint32_t pasid, uint32_t event_id, uint64_t events) {
  auto *state = static_cast<struct hal_mock_state *>(ctx);
  state->event_signal_count++;
  state->last_event_id = event_id;
  state->last_event_events = events;
  (void)pasid;

  if (event_id >= 256) return -22;
  if (events == 0) return -22;
  if (!state->event_state) return -12;

  {
    std::lock_guard<std::mutex> lock(state->event_state->mtx);
    state->event_state->signaled[event_id] = true;
  }
  state->event_state->cv.notify_one();
  return 0;
}

static int mock_event_wait(void *ctx, uint32_t event_id, uint64_t timeout_us) {
  auto *state = static_cast<struct hal_mock_state *>(ctx);
  state->event_wait_count++;

  if (event_id >= 256) return -22;
  if (!state->event_state) return -12;

  std::unique_lock<std::mutex> lock(state->event_state->mtx);

  if (timeout_us == 0) {
    if (state->event_state->signaled[event_id]) {
      return 0;
    }
    return -110;
  }

  if (timeout_us == UINT64_MAX) {
    state->event_state->cv.wait(lock, [&]() {
      return state->event_state->signaled[event_id];
    });
    return 0;
  }

  bool signaled = state->event_state->cv.wait_for(lock,
      std::chrono::microseconds(timeout_us), [&]() {
        return state->event_state->signaled[event_id];
      });
  if (signaled) {
    return 0;
  }
  return -110;
}

static int mock_event_notify(void *ctx, uint32_t event_id) {
  auto *state = static_cast<struct hal_mock_state *>(ctx);
  state->event_notify_count++;
  state->last_event_id = event_id;

  if (event_id >= 256) return -22;
  if (!state->event_state) return -12;

  {
    std::lock_guard<std::mutex> lock(state->event_state->mtx);
    state->event_state->signaled[event_id] = true;
  }
  state->event_state->cv.notify_all();
  return 0;
}

/* ── Stage 4.1: BAR2 VRAM mmap (ADR-064 D2, ADR-069 D4) ──── */

static int mock_mem_map_bo(struct gpgpu_device* dev, uint64_t bo_offset,
                           size_t size, void** user_map) {
    (void)dev;
    (void)size;
    auto* store = &usr_linux_emu::g_vram_store;
    if (!store->initialized) return -ENODEV;

    *user_map = static_cast<uint8_t*>(store->pool_backing) + bo_offset;
    return 0;
}

/* ── 公开初始化函数 ────────────────────────────────── */

void hal_mock_init(struct gpu_hal_ops *hal, struct hal_mock_state *state) {
  *state = {};

  state->event_state = new hal_mock_event_state{};
  std::memset(state->event_state->signaled, 0, sizeof(state->event_state->signaled));

  state->register_read_result = 0;
  state->register_write_result = 0;
  state->mem_read_result = 0;
  state->mem_write_result = 0;
  state->mem_alloc_result = 0;
  state->mem_free_result = 0;
  state->fence_create_result = 0;
  state->fence_read_result = 0;
  state->event_wait_result = 0;
  state->event_notify_result = 0;

  hal->ctx = state;
  hal->register_read = mock_reg_read;
  hal->register_write = mock_reg_write;
  hal->mem_read = mock_mem_read;
  hal->mem_write = mock_mem_write;
  hal->mem_alloc = mock_mem_alloc;
  hal->mem_free = mock_mem_free;
  hal->fence_create = mock_fence_create;
  hal->fence_read = mock_fence_read;
  hal->doorbell_ring = mock_doorbell_ring;
  hal->interrupt_raise = mock_interrupt_raise;
  hal->interrupt_register = mock_interrupt_register;
  hal->interrupt_raise_ex = mock_interrupt_raise_ex;
  hal->time_wait = mock_time_wait;
  hal->iommu_map = mock_iommu_map;
  hal->iommu_unmap = mock_iommu_unmap;
  hal->event_signal = mock_event_signal;
  hal->event_wait = mock_event_wait;
  hal->event_notify = mock_event_notify;
  hal->mem_map_bo = mock_mem_map_bo;
  hal->hal_preempt = [](void*, uint32_t) -> int { return 0; };
  hal->hal_resume = [](void*, uint32_t) -> int { return 0; };
  hal->hal_sem_create = [](void*, uint64_t init, uint64_t* out) -> int {
    static uint64_t next_handle = 1;
    *out = next_handle++;
    return 0;
  };
  hal->hal_sem_signal = [](void*, uint64_t, uint64_t) -> int { return 0; };
  hal->hal_sem_wait = [](void*, uint64_t, uint64_t,
                          void (*)(uint64_t, uint64_t), uint64_t) -> int { return 0; };
  hal->hal_sem_query = [](void*, uint64_t, uint64_t* out) -> int {
    *out = 0;
    return 0;
  };
  hal->hal_sem_destroy = [](void*, uint64_t) -> int { return 0; };
  hal->hal_green_context_create = [](void*, uint64_t tsg_id, uint64_t* out) -> int {
    static uint64_t next_handle = 0x1000;
    *out = ++next_handle;  // mock: monotonic handle, no real binding
    return 0;
  };
  hal->hal_green_context_destroy = [](void*, uint64_t handle) -> int {
    if (handle == 0) return -EINVAL;
    return 0;
  };
  hal->hal_pdl_launch = [](void*, uint64_t, uint64_t, uint32_t, uint32_t,
                           uint64_t* out) -> int {
    static uint64_t next_sem = 0x2000;
    *out = ++next_sem;  // mock: monotonic semaphore handle, no real PDL dispatch
    return 0;
  };
  hal->hal_pdl_signal_completion = [](void*, uint64_t, uint64_t) -> int {
    return 0;  // mock: no real timeline-sem signal
  };

  /* ── Stage 4.6 L2 foundation (ADR-072 §Decision 4) — B-class fix (Phase 1) ─
   * 5 new fn-ptrs enabling drv/ to call sim-layer functions without
   * #including sim/ headers. Mock impl returns test-friendly defaults
   * (no real sim-layer state, deterministic for test reproducibility). */
  hal->fence_id_alloc = [](void*) -> int64_t {
    static std::atomic<uint64_t> next{0x1000};
    return ++next;  // mock: monotonic counter, no real sim state
  };
  hal->fence_id_signal = [](void*, uint64_t) -> void {
    // mock: no real sim signal, tests verify via other fn-ptrs
  };
  hal->fence_id_check = [](void*, uint64_t, bool* signaled) -> int {
    if (signaled) *signaled = true;  // mock: always signaled
    return 0;
  };
  hal->method_codec_encode = [](void*, const gpu_method_packet*,
                              const uint32_t*) -> int {
    return 0;  // mock: no real encoding
  };
  hal->heap_ptr = [](void*, uint64_t) -> void* {
    return nullptr;  // mock: no real heap backing
  };

  /* ── Stage 4.6 L2 foundation (ADR-072 §Decision 4) — Phase 2 ─── */
  /* Mock fn-ptrs: test-friendly defaults (monotonic counter, true, 0,
   * nullptr). No real sim state, deterministic for test reproducibility. */

  /* ── graph mocks (7) ────────────────────────────────────────── */
  hal->graph_create = [](void*, uint64_t* out) -> int {
    static std::atomic<uint64_t> next{0x2000};
    if (out) *out = ++next;
    return 0;
  };
  hal->graph_destroy = [](void*, uint64_t) -> int { return 0; };
  hal->graph_add_kernel_node = [](void*, uint64_t, uint32_t,
                                  uint32_t, uint32_t, uint32_t,
                                  uint32_t, uint32_t, uint32_t,
                                  uint64_t*) -> int {
    return 0;
  };
  hal->graph_add_memcpy_node = [](void*, uint64_t, uint64_t, uint64_t,
                                  uint64_t, int) -> int { return 0; };
  hal->graph_instantiate = [](void*, uint64_t, uint64_t* out) -> int {
    static std::atomic<uint64_t> next{0x3000};
    if (out) *out = ++next;
    return 0;
  };
  hal->graph_launch = [](void*, uint64_t, uint32_t, uint64_t*,
                         uint32_t*) -> int { return 0; };
  hal->graph_destroy_exec = [](void*, uint64_t) -> int { return 0; };

  /* ── mem_pool mocks (9) ──────────────────────────────────────── */
  hal->mem_pool_create = [](void*, const void*, uint64_t* out) -> int {
    static std::atomic<uint64_t> next{0x4000};
    if (out) *out = ++next;
    return 0;
  };
  hal->mem_pool_destroy = [](void*, uint64_t) -> int { return 0; };
  hal->mem_pool_alloc = [](void*, uint64_t, uint64_t, uint64_t* out) -> int {
    static std::atomic<uint64_t> next{0x5000};
    if (out) *out = ++next;
    return 0;
  };
  hal->mem_pool_alloc_async = [](void*, uint64_t, uint64_t,
                                 int64_t* out) -> int {
    if (out) *out = 0;
    return 0;
  };
  hal->mem_pool_free = [](void*, uint64_t, uint64_t) -> int { return 0; };
  hal->mem_pool_free_async = [](void*, uint64_t, uint64_t,
                                int64_t* out) -> int {
    if (out) *out = 0;
    return 0;
  };
  hal->mem_pool_set_attr = [](void*, uint64_t, uint32_t, const void*,
                              uint64_t) -> int { return 0; };
  hal->mem_pool_get_attr = [](void*, uint64_t, uint32_t, void*,
                              uint64_t) -> int { return 0; };
  hal->mem_pool_trim = [](void*, uint64_t, uint64_t) -> int { return 0; };
  hal->mem_pool_export_shareable = [](void*, uint64_t, uint32_t, uint32_t,
                                       int32_t* fd_out) -> int {
    if (fd_out) *fd_out = -1;
    return 0;
  };

  /* ── stream_capture mocks (3) ────────────────────────────────── */
  hal->stream_capture_begin = [](void*, uint64_t, uint32_t) -> int {
    return 0;
  };
  hal->stream_capture_end = [](void*, uint64_t, uint64_t* out) -> int {
    if (out) *out = 0;
    return 0;
  };
  hal->stream_capture_status = [](void*, uint64_t, uint32_t* out) -> int {
    if (out) *out = 0;  // mock: never capturing
    return 0;
  };

  /* ── gpu_queue_emu mocks (5) ─────────────────────────────────── */
  hal->queue_create = [](void*, uint32_t, uint32_t, uint32_t, uint32_t,
                         hal_queue_handle_t* out) -> int {
    static std::atomic<uint64_t> next{0x6000};
    if (out) *out = ++next;
    return 0;
  };
  hal->queue_attach_shmem = [](void*, hal_queue_handle_t, void*,
                               uint64_t) -> int { return 0; };
  hal->queue_submit = [](void*, hal_queue_handle_t, uint64_t, uint32_t,
                         int64_t* out) -> int {
    if (out) *out = 0;
    return 0;
  };
  hal->queue_destroy = [](void*, hal_queue_handle_t) -> int { return 0; };
  hal->queue_register_puller = [](void*, hal_queue_handle_t,
                                  hal_puller_handle_t) -> int { return 0; };

  /* ── hardware_puller_emu mocks (5) ───────────────────────────── */
  hal->puller_create = [](void*, void*, void*, hal_puller_handle_t* out) -> int {
    static std::atomic<uint64_t> next{0x7000};
    if (out) *out = ++next;
    return 0;  // mock: no real sim instance
  };
  hal->puller_destroy = [](void*, hal_puller_handle_t) -> int { return 0; };
  hal->puller_set_puller = [](void*, hal_puller_handle_t, uint64_t) -> int {
    return 0;
  };
  hal->puller_register_queue = [](void*, hal_puller_handle_t,
                                  hal_queue_handle_t) -> int { return 0; };
  hal->puller_unregister_queue = [](void*, hal_puller_handle_t,
                                    uint32_t) -> int { return 0; };
}

void hal_mock_destroy(struct hal_mock_state *state) {
  if (state->event_state) {
    delete state->event_state;
    state->event_state = nullptr;
  }
}
