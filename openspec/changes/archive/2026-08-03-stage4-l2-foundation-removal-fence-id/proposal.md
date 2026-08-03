## Why

L2 portability gate (ADR-072 §L2) is blocked by 12 B-class violations. The B-class foundation change `2026-08-03-stage4-l2-foundation-hal-fence-method-heap` added 5 new fn-ptrs to `struct gpu_hal_ops` (fence_id_alloc, fence_id_signal, fence_id_check, method_codec_encode, heap_ptr). This change is the first of 3 Phase-1 removal changes that use those fn-ptrs to clear actual L2 violations.

**This change (Change 1 of 3)**:
- Removes 2 of 12 L2 violations: `gpgpu_device.cpp:18` + `gpu_drm_driver.cpp:26` (both `#include "sim/fence_id.h"`)
- Migrates 4 call sites (3 in gpgpu_device.cpp, 1 in gpu_drm_driver.cpp) from `sim_fence_id_alloc/signal/check()` to `hal_fence_id_alloc/signal/check()`
- L2 violation count: 12 → 10

**Why now**:
- B-class foundation is merged (commit 1b2cbac); fn-ptrs are available
- Without this change, the foundation is dormant (fn-ptrs defined but unused)
- L2 gate remains blocked until 12 → 0 violations

**Why these 3 first** (out of 12 total):
- `fence_id.h`: 2 violations, 4 call sites — cleanest C-ABI function calls
- `method_codec.h`: 1 violation, 1 call site — single function
- `hal/hal_user.h`: 1 violation, 1 call site — single field access (heap)

After Phase 1 (3 changes): 12 → 9 violations. Then Phase 2 (6 fn-ptrs + 6 removals): 9 → 3. Then final 3 removals: 3 → 0.

**Architectural basis**:
- **ADR-023 §Decision 4** (append-only HAL extension) — fn-ptrs added in foundation
- **ADR-072 §Decision 4 revised** (1 foundation + N removal pattern) — this is removal #1
- **ADR-043 §D5** (12 B-class violations) — first 2 removed by this change

## What Changes

- `plugins/gpu_driver/drv/gpgpu_device.cpp`:
  - Remove `#include "sim/fence_id.h"` (line 18)
  - Replace `sim_fence_id_alloc()` with `hal_fence_id_alloc(hal_)` (3 call sites: lines 380, 969, ~976)
  - Replace `sim_fence_id_check()` with `hal_fence_id_check(hal_, fence_id, &sim_signaled)` (line 487)
  - Note: `sim_fence_id_signal()` is NOT called by drv/ (only by sim layer) — no migration needed
- `plugins/gpu_driver/drv/gpu_drm_driver.cpp`:
  - Remove `#include "sim/fence_id.h"` (line 26)
  - Replace `sim_fence_id_check()` with `hal_fence_id_check(hal_, fence_id, &sim_signaled)` (line 287)

## Capabilities

### New Capabilities

(none — this change uses capabilities from the B-class foundation change)

## Impact

- `plugins/gpu_driver/drv/gpgpu_device.cpp` (1 include removed, 4 call sites migrated)
- `plugins/gpu_driver/drv/gpu_drm_driver.cpp` (1 include removed, 1 call site migrated)
- L2 violation count: 12 → 10 (2 violations removed)
- (Not in scope: 10 remaining B-class violations — 2 more Phase-1 removals + Phase-2 foundation + 6 removals + final 3 removals)

## Out of Scope

- ❌ Migration of `sim_fence_id_signal()` in drv/ (not called by drv/; only by sim layer)
- ❌ Removal of `sim/fence_id.h` from sim/ itself (still needed by `hal_user.cpp` to implement the fn-ptrs)
- ❌ Migration of `method_codec_encode()` (separate change: `removal-method-codec`)
- ❌ Migration of `hc->heap` field access (separate change: `removal-hal-user`)
- ❌ 10 remaining B-class violations
