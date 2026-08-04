## Context

Phase 1 B-class L2 fix is complete. B-class foundation change `2026-08-03-stage4-l2-foundation-hal-fence-method-heap` (commit 1b2cbac) added 5 fn-ptrs (14 → 19 total). 3 removal changes shipped (fence-id, method-codec, hal-user) cleared 4 of 12 L2 violations (12 → 8). 8 violations remain across 5 unique headers.

This change is **Phase 2 of the B-class foundation**: adding ~28 new fn-ptrs to `struct gpu_hal_ops` to enable the 5 remaining removal changes.

**Current `struct gpu_hal_ops`** (19 fn-ptrs after Phase 1):
- 4 ADR-061/062 additions (iommu_map/unmap, event_signal, event_wait, event_notify)
- 5 Stage 4.5 (hal_preempt, hal_resume, hal_sem_create/signal/wait/query/destroy)
- 4 Stage 4.6 (hal_green_context_create/destroy, hal_pdl_launch/signal_completion)
- 4 misc (register_read/write, mem_read/write, mem_alloc/free, etc.)
- 2 sim_fence_id (fence_id_alloc/signal/check) — Phase 1 #1
- 1 sim_fence_id_check (fence_id_check) — Phase 1 #1
- 1 sim_method_codec (method_codec_encode) — Phase 1 #2
- 1 hal_heap (heap_ptr) — Phase 1 #3

**Required Phase 2 fn-ptrs** (per `grep` on drv/ call sites):

### sim/graph.h (8 fn-ptrs, 15 call sites)
```c
int (*graph_create)(void *ctx, uint64_t* out_handle);
int (*graph_destroy)(void *ctx, uint64_t handle);
int (*graph_add_kernel_node)(void *ctx, uint64_t graph, uint32_t kernel_idx, ...);
int (*graph_add_memcpy_node)(void *ctx, uint64_t graph, uint64_t src_va, uint64_t dst_va, uint64_t size);
int (*graph_instantiate)(void *ctx, uint64_t graph, uint64_t* out_exec);
int (*graph_launch)(void *ctx, uint64_t exec, uint32_t stream);
int (*graph_destroy_exec)(void *ctx, uint64_t exec);
// (1 more, TBD per full audit of sim_graph_* signatures)
```

### sim/mem_pool.h (9 fn-ptrs, 27 call sites)
```c
int (*mem_pool_create)(void *ctx, const sim_mem_pool_props_t* props, uint64_t* out_handle);
int (*mem_pool_destroy)(void *ctx, uint64_t handle);
int (*mem_pool_alloc)(void *ctx, uint64_t handle, uint64_t size, uint64_t* out_va);
int (*mem_pool_alloc_async)(void *ctx, uint64_t handle, uint64_t size, int64_t* out_fence);
int (*mem_pool_free)(void *ctx, uint64_t handle, uint64_t va);
int (*mem_pool_free_async)(void *ctx, uint64_t handle, uint64_t va, int64_t* out_fence);
int (*mem_pool_set_attr)(void *ctx, uint64_t handle, uint32_t attr, uint64_t val);
int (*mem_pool_get_attr)(void *ctx, uint64_t handle, uint32_t attr, uint64_t* out_val);
int (*mem_pool_trim)(void *ctx, uint64_t handle, uint64_t va, uint64_t size);
```

### sim/stream_capture.h (3 fn-ptrs, 8 call sites)
```c
int (*stream_capture_begin)(void *ctx, uint64_t stream_id, uint32_t mode);
int (*stream_capture_end)(void *ctx, uint64_t stream_id, uint64_t* out_graph);
int (*stream_capture_status)(void *ctx, uint64_t stream_id, uint32_t* out_status);
```

### sim/gpu_queue_emu.h (5 fn-ptrs, 3 call sites + class usage)
```c
// GpuQueueEmu is a class — drv/ uses shared_ptr<GpuQueueEmu>.
// fn-ptrs needed for the methods drv/ actually calls:
int (*queue_create)(void *ctx, uint32_t handle, uint32_t type, uint32_t priority,
                   uint32_t ring_size, uint64_t* out_q);
int (*queue_attach_shmem)(void *ctx, uint64_t q, void* cpu_ptr, uint64_t size);
int (*queue_submit)(void *ctx, uint64_t q, uint64_t gpfifo_addr, uint32_t count, int64_t* out_fence);
// (2 more: registerQueue, etc.)
```

### sim/hardware/hardware_puller_emu.h (3 fn-ptrs, 2 call sites + class usage)
```c
// HardwarePullerEmu is a class — drv/ uses shared_ptr<HardwarePullerEmu>.
int (*set_puller)(void *ctx, uint64_t sim_puller_handle);
int (*register_queue)(void *ctx, void* sim_queue_handle);
// (1 more: unregister_queue, etc.)
```

**Architectural basis**:
- **ADR-023 §Decision 4** (append-only HAL extension) — all new fn-ptrs appended, no existing changed
- **ADR-072 §Decision 4 revised** — Phase 2 foundation (1 foundation + 5 removals)
- **ADR-043 §D5** — 8 remaining B-class violations to be cleared

## Goals / Non-Goals

**Goals:**

- Add ~28 new fn-ptrs to `struct gpu_hal_ops` (append-only)
- Add ~28 new inline wrapper functions in `gpu_hal.h`
- Implement ~28 new lambda assignments in `hal_user_init()` (delegate to sim functions)
- Implement ~28 new mock fn-ptrs in `hal_mock_init()` (test-friendly defaults)
- 0 functional change to existing 19 fn-ptrs
- 0 regression in existing tests
- 0 drv/ changes (out of scope; 5 separate removal changes will follow)

**Non-Goals:**

- ❌ drv/ call site migrations (5 separate removal changes will follow)
- ❌ Removal of sim/ headers from sim/ itself (still needed by hal_user.cpp to implement fn-ptrs)
- ❌ Changes to existing 19 fn-ptrs (append-only per ADR-023 Decision 4)
- ❌ New types in shared/ (class types like GpuQueueEmu are used as shared_ptr in drv/ — may need fwd decls but that's part of removal changes)

## Approach

### Step 1: Add ~28 fn-ptr declarations to `struct gpu_hal_ops` (in `plugins/gpu_driver/hal/gpu_hal.h`)

Append after the last existing fn-ptr (`heap_ptr` from Phase 1). Each fn-ptr uses C-compatible types (no std::vector, no C++ classes in the signature).

### Step 2: Add ~28 inline wrapper functions in `gpu_hal.h`

Each wrapper is 1-3 lines forwarding args 1:1 to the fn-ptr. Pattern matches Phase 1 wrappers.

### Step 3: Implement ~28 new lambda assignments in `hal_user_init()` (in `plugins/gpu_driver/hal/hal_user.cpp`)

Each lambda delegates to the existing sim/ function. For class types (GpuQueueEmu, HardwarePullerEmu), the lambda accesses the sim instance through a lookup (e.g., g_puller_map) or creates the object directly.

### Step 4: Implement ~28 new mock fn-ptrs in `hal_mock_init()` (in `plugins/gpu_driver/hal/hal_mock.cpp`)

Each mock returns a test-friendly default: monotonic counter, true, 0, nullptr.

### Step 5: Verify

- `make kernel gpu_hal hal_mock gpu_hal_mock gpu_sim gpu_drv gpu_driver_plugin -j4` — clean build
- `make test_context_type_standalone test_pdl_standalone test_priority_sched_standalone -j4` — build + run, all PASS
- `make test` — full ctest suite, all PASS (0 regression)
- L2 violation count: still 8 (this change is foundation only; no drv/ changes)

## Risks

| Risk | Likelihood | Mitigation |
|------|-----------|------------|
| `struct gpu_hal_ops` grows too large (47 fn-ptrs) | Low | Append-only is fine; 47 is manageable |
| Class types (GpuQueueEmu, HardwarePullerEmu) in fn-ptr signatures | Medium | Use opaque pointer type or handle-based dispatch; details TBD during implementation |
| New fn-ptrs break existing 19 fn-ptrs | Very Low | Strictly append-only; compile will catch any regression |
| Build system can't handle large static lib | Low | Same pattern as Phase 1 (worked fine with 5 new fn-ptrs) |
