## Context

Stage 4 L2 portability gate (ADR-072 §L2) requires `plugins/gpu_driver/drv/` to compile against Linux 6.12 LTS kernel sources without modification. Re-audit (2026-08-03) found 12 B-class violations in drv/ (all interface calls or field access — 0 A-class per ADR-043 §D5 + ADR-072 §Decision 2 revision).

**Current `struct gpu_hal_ops` contents** (per `plugins/gpu_driver/hal/gpu_hal.h`):
- 14 fn-ptrs: register_read/write, mem_read/write, mem_alloc/free, fence_create/read, doorbell_ring, interrupt_raise/register, plus 4 ADR-061/062 additions (iommu_map/unmap, event_signal), plus 4 Stage 4.5/4.6 additions (sem_create/destroy/wait/query, green_context_create/destroy, pdl_launch/signal_completion)
- 14 inline wrappers in same file

**Missing fn-ptrs** (this change adds):
- `fence_id_alloc` (int64_t (*)(void *ctx) → int64_t fence_id) — B-class fix for `sim/fence_id.h`
- `fence_id_signal` (void (*)(void *ctx, uint64_t fence_id)) — B-class fix for `sim/fence_id.h`
- `fence_id_check` (int (*)(void *ctx, uint64_t fence_id, bool *signaled)) — B-class fix for `sim/fence_id.h`
- `method_codec_encode` (int (*)(void *ctx, ...)) — B-class fix for `sim/hardware/method_codec.h`
- `heap_ptr` (void *(*)(void *ctx, uint64_t gpu_va)) — B-class fix for `hal/hal_user.h` `hc->heap` field access

**Architectural basis**:
- **ADR-023 §Decision 4** (spec-driven "append-only" rule) — new fn-ptrs appended to `struct gpu_hal_ops` without modifying existing ones
- **ADR-072 §Decision 4 revised** (2026-08-03) — 1 foundation change + N removal changes pattern
- **ADR-043 §D5 revised** (2026-08-03) — 12 B-class violations to be fixed via this pattern

## Goals / Non-Goals

**Goals:**

- Add 5 new fn-ptrs to `struct gpu_hal_ops` (append-only)
- Add 5 new inline wrapper functions in `gpu_hal.h`
- Implement 5 fn-ptrs in `hal_user.cpp` (production path) — delegate to existing sim functions
- Implement 5 fn-ptrs in `hal_mock.cpp` (mock path) — return test-friendly defaults
- **0 functional change** to existing API, drv/, or sim/ code
- Set the pattern for 9 remaining B-class violations (to be addressed in subsequent foundation phases)
- Enable follow-up removal changes to migrate drv/ call sites to the new fn-ptrs

**Non-Goals:**

- ❌ Removing any sim/* or `hal/hal_user.h` includes from drv/ (separate removal changes)
- ❌ Modifying existing fn-ptrs in `struct gpu_hal_ops` (append-only per ADR-023 Decision 4)
- ❌ Changing drv/ behavior (this change is HAL extension only)
- ❌ Updating tests to use new fn-ptrs (existing tests continue to work)
- ❌ 9 remaining B-class violations (graph.h, mem_pool.h, gpu_queue_emu.h, stream_capture.h, hardware_puller_emu.h, plus additional B-class) — separate foundation phases

## Approach

### Step 1: Add 5 fn-ptrs to `struct gpu_hal_ops` (in `plugins/gpu_driver/hal/gpu_hal.h`)

Append after the last existing fn-ptr (`hal_pdl_signal_completion`):

```c
/* ── Stage 4.6 L2 foundation (ADR-072 §Decision 4) — B-class fix (Phase 1) ─ */

int64_t (*fence_id_alloc)(void *ctx);
void     (*fence_id_signal)(void *ctx, uint64_t fence_id);
int      (*fence_id_check)(void *ctx, uint64_t fence_id, bool *signaled);

int      (*method_codec_encode)(void *ctx, /* ... per existing signature ... */);

void*    (*heap_ptr)(void *ctx, uint64_t gpu_va);
```

### Step 2: Add 5 inline wrapper functions (same file)

```c
static inline int64_t hal_fence_id_alloc(struct gpu_hal_ops *hal) {
  return hal->fence_id_alloc(hal->ctx);
}

static inline void hal_fence_id_signal(struct gpu_hal_ops *hal, uint64_t fence_id) {
  hal->fence_id_signal(hal->ctx, fence_id);
}

static inline int hal_fence_id_check(struct gpu_hal_ops *hal, uint64_t fence_id, bool *signaled) {
  return hal->fence_id_check(hal->ctx, fence_id, signaled);
}

static inline int hal_method_codec_encode(struct gpu_hal_ops *hal /*, ...args... */) {
  return hal->method_codec_encode(hal->ctx /*, ...args... */);
}

static inline void* hal_heap_ptr(struct gpu_hal_ops *hal, uint64_t gpu_va) {
  return hal->heap_ptr(hal->ctx, gpu_va);
}
```

### Step 3: Implement 5 fn-ptrs in `hal_user.cpp` (production path)

```cpp
// In hal_user_init() or similar, after existing fn-ptr assignments:
hal->fence_id_alloc = [](void *ctx) -> int64_t {
  return sim_fence_id_alloc();
};
hal->fence_id_signal = [](void *ctx, uint64_t fence_id) -> void {
  sim_fence_id_signal(fence_id);
};
hal->fence_id_check = [](void *ctx, uint64_t fence_id, bool *signaled) -> int {
  return sim_fence_id_check(fence_id, signaled);
};

hal->method_codec_encode = [](void *ctx, /* ... */) -> int {
  return method_codec_encode(/* ... */);
};

hal->heap_ptr = [](void *ctx, uint64_t gpu_va) -> void* {
  auto* hc = static_cast<hal_user_context*>(ctx);
  return hc->heap + (gpu_va - HAL_HEAP_BASE);
};
```

### Step 4: Implement 5 fn-ptrs in `hal_mock.cpp` (test path)

```cpp
// In hal_mock_init() or similar, after existing fn-ptr assignments:
hal->fence_id_alloc = [](void *ctx) -> int64_t {
  static std::atomic<uint64_t> next{0x1000};
  return ++next;
};
hal->fence_id_signal = [](void*, uint64_t) -> void { /* mock: no real signal */ };
hal->fence_id_check = [](void*, uint64_t, bool *signaled) -> int {
  if (signaled) *signaled = true;
  return 0;
};
hal->method_codec_encode = [](void*, /* ... */) -> int { return 0; };
hal->heap_ptr = [](void*, uint64_t) -> void* { return nullptr; };
```

### Step 5: Verify

- `make kernel gpu_hal hal_mock hal_user gpu_sim plugin_gpu_driver` — clean build
- `make test` — all existing tests still PASS (0 regression)
- New fn-ptrs in `struct gpu_hal_ops` are all initialized to non-null in both `hal_user_init()` and `hal_mock_init()`
- L2 violation count: still 12 (this change doesn't remove any — that's follow-up work)

## Risks

| Risk | Likelihood | Mitigation |
|------|-----------|------------|
| Naming conflicts with existing fn-ptrs | Very Low | All 5 names are new (no existing fence_id_*, method_codec_*, heap_ptr) |
| Signature mismatch with sim functions | Low | Each fn-ptr signature mirrors the sim function it delegates to; verified by inspection |
| ABI change for existing drv/ callers | None | New fn-ptrs appended; existing fn-ptrs unchanged (append-only per ADR-023 Decision 4) |
| Mock impl returns wrong type for tests | Low | Each mock returns test-friendly default (monotonic counter, true, 0, nullptr) |
| Inline wrapper signature errors | Low | Each wrapper is 1-3 lines forwarding args 1:1 to fn-ptr |

## Follow-up (Out of Scope)

After this foundation ships, the following removal changes are needed to reduce L2 violation count:

1. `2026-08-03-stage4-l2-foundation-removal-fence-id` — migrate drv/ `sim_fence_id_alloc` calls to `hal_fence_id_alloc`
2. `2026-08-03-stage4-l2-foundation-removal-method-codec` — migrate drv/ `method_codec_encode` call to `hal_method_codec_encode`
3. `2026-08-03-stage4-l2-foundation-removal-hal-user` — migrate drv/ `hc->heap` access to `hal_heap_ptr`

After 3 removals: violation count 12 → 9 (remaining: graph.h, mem_pool.h, gpu_queue_emu.h, stream_capture.h, hardware_puller_emu.h, and possibly method_codec.h if missed).

Then Phase 2 of foundation: add fn-ptrs for the 6 remaining B-class headers, then 6 more removal changes.

Total: 2 foundation phases + 11 removal changes to clear all 12 violations.
