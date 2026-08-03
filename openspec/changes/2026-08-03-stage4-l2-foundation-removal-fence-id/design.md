## Context

B-class foundation change `2026-08-03-stage4-l2-foundation-hal-fence-method-heap` (merged 1b2cbac) added 3 new fn-ptrs to `struct gpu_hal_ops` for fence_id lifecycle:
- `int64_t (*fence_id_alloc)(void *ctx)` → wraps `sim_fence_id_alloc()`
- `void (*fence_id_signal)(void *ctx, uint64_t fence_id)` → wraps `sim_fence_id_signal()`
- `int (*fence_id_check)(void *ctx, uint64_t fence_id, bool *signaled)` → wraps `sim_fence_id_check()`

Inline wrappers in `gpu_hal.h`:
- `hal_fence_id_alloc(hal)` → `hal->fence_id_alloc(hal->ctx)`
- `hal_fence_id_signal(hal, fence_id)` → `hal->fence_id_signal(hal->ctx, fence_id)`
- `hal_fence_id_check(hal, fence_id, signaled)` → `hal->fence_id_check(hal->ctx, fence_id, signaled)`

**Current drv/ usage of `sim_fence_id_*` functions** (per `grep -rn 'sim_fence_id_' plugins/gpu_driver/drv/`):

- `gpgpu_device.cpp:380` — `int64_t sim_fence = sim_fence_id_alloc();` (handlePushbufferSubmitBatch)
- `gpgpu_device.cpp:382` — `std::cerr << "[GpgpuDevice] PUSHBUFFER: sim_fence_id_alloc failed\n";` (error log)
- `gpgpu_device.cpp:487` — `int ret = sim_fence_id_check(fence_id, &sim_signaled);` (handleWaitFence)
- `gpgpu_device.cpp:969` — `int64_t sim_fence = sim_fence_id_alloc();` (handleGraphLaunch)
- `gpgpu_device.cpp:971` — `std::cerr << "[GpgpuDevice] GRAPH_LAUNCH: sim_fence_id_alloc failed\n";` (error log)
- `gpu_drm_driver.cpp:287` — `ret = sim_fence_id_check(fence_id, &sim_signaled);` (DRM fence wait)

Total: 4 sim_fence_id_* call sites in drv/ across 2 files.

**Architectural basis**:
- **ADR-023 §Decision 4** (append-only HAL extension) — fn-ptrs already in struct
- **ADR-072 §Decision 4 revised** — 1 foundation + N removal pattern; this is removal #1
- **ADR-043 §D5** — 12 B-class violations; this change removes 2

## Goals / Non-Goals

**Goals:**

- Remove `#include "sim/fence_id.h"` from `gpgpu_device.cpp` and `gpu_drm_driver.cpp`
- Migrate 4 call sites:
  - `gpgpu_device.cpp:380` — `sim_fence_id_alloc()` → `hal_fence_id_alloc(hal_)`
  - `gpgpu_device.cpp:487` — `sim_fence_id_check(fence_id, &sim_signaled)` → `hal_fence_id_check(hal_, fence_id, &sim_signaled)`
  - `gpgpu_device.cpp:969` — `sim_fence_id_alloc()` → `hal_fence_id_alloc(hal_)`
  - `gpu_drm_driver.cpp:287` — `sim_fence_id_check(fence_id, &sim_signaled)` → `hal_fence_id_check(hal_, fence_id, &sim_signaled)`
- L2 violation count: 12 → 10 (2 violations removed)
- 0 functional change (same behavior; different call path)
- 0 regression in existing tests

**Non-Goals:**

- ❌ `sim_fence_id_signal()` migration (not called by drv/; only by sim layer internals)
- ❌ `sim/fence_id.h` removal from sim/ itself (still needed by `hal_user.cpp` to implement fn-ptrs)
- ❌ `method_codec_encode()` migration (separate change: `removal-method-codec`)
- ❌ `hc->heap` field access migration (separate change: `removal-hal-user`)
- ❌ 10 remaining B-class violations (subsequent changes)

## Approach

### Step 1: Read `gpgpu_device.cpp` and `gpu_drm_driver.cpp` to find exact call sites

```bash
cd /workspace/project/UsrLinuxEmu/.rddf/wt/2026-08-03-stage4-l2-foundation-removal-fence-id
grep -n "sim_fence_id_" plugins/gpu_driver/drv/gpgpu_device.cpp
grep -n "sim_fence_id_" plugins/gpu_driver/drv/gpu_drm_driver.cpp
```

### Step 2: Verify `hal_` is accessible in scope

Both files already use `hal_` extensively (e.g., `hal_->register_read`, `hal_->fence_create`). The new inline wrappers (`hal_fence_id_alloc`, `hal_fence_id_check`) are defined in `gpu_hal.h` which is already included via `hal/gpu_hal.h`.

### Step 3: Migrate gpgpu_device.cpp

- Remove `#include "sim/fence_id.h"` (line 18)
- Replace `sim_fence_id_alloc()` → `hal_fence_id_alloc(hal_)` (2 call sites: lines 380, 969)
- Replace `sim_fence_id_check(fence_id, &sim_signaled)` → `hal_fence_id_check(hal_, fence_id, &sim_signaled)` (line 487)
- Error log messages can be updated to reflect the new function name (cosmetic)

### Step 4: Migrate gpu_drm_driver.cpp

- Remove `#include "sim/fence_id.h"` (line 26)
- Replace `sim_fence_id_check(fence_id, &sim_signaled)` → `hal_fence_id_check(hal_, fence_id, &sim_signaled)` (line 287)

### Step 5: Verify

- `make kernel gpu_hal hal_mock gpu_hal_mock gpu_sim gpu_drv gpu_driver_plugin -j4` — clean build
- `make test_context_type_standalone test_pdl_standalone test_priority_sched_standalone -j4` — build + run
- `make test` — all existing tests still PASS (0 regression)
- `grep '#include.*"sim/fence_id"' plugins/gpu_driver/drv/` — should return 0 matches
- L2 violation count: `grep -rn '#include.*"sim/\|#include.*"hal/hal_user' plugins/gpu_driver/drv/ | wc -l` — should be 10 (was 12)

## Risks

| Risk | Likelihood | Mitigation |
|------|-----------|------------|
| `hal_` is nullptr at call site | Low | Both files already call `hal_->xxx` in the same function bodies; pattern is established |
| Signature mismatch between inline wrapper and fn-ptr | Low | Both defined in same header file; compile error would catch immediately |
| `bool*` parameter in `hal_fence_id_check` | Low | Inline wrapper signature matches; just forwards |
| Error log message cosmetic update | None | Log strings are user-facing; minor update is acceptable |
