## Context

B-class foundation change `2026-08-03-stage4-l2-foundation-hal-fence-method-heap` (merged 1b2cbac) added 1 new fn-ptr to `struct gpu_hal_ops`:
- `void* (*heap_ptr)(void *ctx, uint64_t gpu_va)` → wraps `hc->heap + (gpu_va - HAL_HEAP_BASE)`

Inline wrapper in `gpu_hal.h`:
- `hal_heap_ptr(hal, gpu_va)` → `hal->heap_ptr(hal->ctx, gpu_va)`

Two removal changes have already shipped (fence-id, method-codec). This is the final Phase-1 removal change.

**Current drv/ usage of `hal/hal_user.h`** (per `grep -rn 'hal_user' plugins/gpu_driver/drv/`):
- `gpgpu_device.cpp:24` — `#include "hal/hal_user.h"`
- `gpgpu_device.cpp:235` — `hc ? reinterpret_cast<void*>(hc->heap + (gpu_va - HAL_HEAP_BASE)) : nullptr}` (handleAllocBo)
- `gpgpu_device.cpp:232` — `auto hc = static_cast<struct hal_user_context*>(hal_ctx_);` (the cast is in scope)

Note: `gpu_drm_driver.cpp` does NOT include `hal/hal_user.h` directly (it accesses hal via `self->hal_`). Only `gpgpu_device.cpp` has the direct field access to `hc->heap`.

**Architectural basis**:
- **ADR-023 §Decision 4** (append-only HAL extension) — fn-ptr already in struct
- **ADR-072 §Decision 4 revised** — 1 foundation + N removal pattern; this is removal #3
- **ADR-043 §D5** — 12 B-class violations; this change removes 1

## Goals / Non-Goals

**Goals:**

- Remove `#include "hal/hal_user.h"` from `gpgpu_device.cpp`
- Migrate 1 call site: `hc->heap + (gpu_va - HAL_HEAP_BASE)` → `hal_heap_ptr(hal_, gpu_va)`
- L2 violation count: 9 → 8 (Phase 1 complete)
- 0 functional change (same behavior; different call path)
- 0 regression in existing tests

**Non-Goals:**

- ❌ Removal of `hal/hal_user.h` from sim/ itself (still needed by `hal_user.cpp` to implement the heap_ptr fn-ptr)
- ❌ 8 remaining B-class violations (graph.h, mem_pool.h, gpu_queue_emu.h, stream_capture.h, hardware_puller_emu.h, and 2 others)
- ❌ Phase 2 of B-class foundation (6 more fn-ptrs needed for remaining 6 headers)

## Approach

### Step 1: Read `gpgpu_device.cpp` to find exact call site

```bash
cd /workspace/project/UsrLinuxEmu/.rddf/wt/2026-08-03-stage4-l2-foundation-removal-hal-user
grep -n "hal_user\|hc->heap\|HAL_HEAP_BASE" plugins/gpu_driver/drv/gpgpu_device.cpp
```

### Step 2: Verify `hal_` is accessible in scope

`handleAllocBo` (where the call is) has access to `hal_` via the enclosing class (`GpgpuDevice::hal_`).

### Step 3: Migrate gpgpu_device.cpp

- Remove `#include "hal/hal_user.h"` (line 24)
- Replace line 235: `hc ? reinterpret_cast<void*>(hc->heap + (gpu_va - HAL_HEAP_BASE)) : nullptr}` → `hal_heap_ptr(hal_, gpu_va)`

Note: The `hc` variable (line 232) was only used for the `hc->heap` access. After migration, `hc` is no longer needed and can be removed. But this is a cosmetic cleanup; the main change is the function call replacement.

### Step 4: Verify

- `make kernel gpu_hal hal_mock gpu_hal_mock gpu_sim gpu_drv gpu_driver_plugin -j4` — clean build
- `make test_context_type_standalone test_pdl_standalone test_priority_sched_standalone -j4` — build + run, all PASS
- `grep '#include.*"hal/hal_user"' plugins/gpu_driver/drv/` — should return 0 matches
- `grep 'hc->heap' plugins/gpu_driver/drv/` — should return 0 matches
- L2 violation count: `grep -rn '#include.*"sim/\|#include.*"hal/hal_user' plugins/gpu_driver/drv/ | wc -l` — should be 8 (was 9)

## Risks

| Risk | Likelihood | Mitigation |
|------|-----------|------------|
| `hal_` is nullptr at call site | Very Low | Call is in `handleAllocBo` which uses `hal_` throughout |
| `hal_heap_ptr` returns nullptr for unmapped VA | Low | Same as current `hc->heap + offset` behavior; unchanged semantics |
| `HAL_HEAP_BASE` constant still accessible? | YES | `HAL_HEAP_BASE` is defined in `hal/hal_user.h` which we're removing. But `hal_heap_ptr` encapsulates the offset internally (added in the foundation). The drv/ side just passes `gpu_va` and gets the host pointer back. |
