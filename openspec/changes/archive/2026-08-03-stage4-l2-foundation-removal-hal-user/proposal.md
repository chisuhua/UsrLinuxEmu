## Why

L2 portability gate (ADR-072 §L2) is blocked by 12 B-class violations. B-class foundation change `2026-08-03-stage4-l2-foundation-hal-fence-method-heap` added 5 new fn-ptrs to `struct gpu_hal_ops`. Two removal changes have already shipped:
- Change #1 (fence-id): 12 → 10
- Change #2 (method-codec): 10 → 9

This is the **final Phase-1 removal change**:
- Removes 1 of 9 remaining L2 violations: `gpgpu_device.cpp:24` (`#include "hal/hal_user.h"`)
- Migrates 1 call site: `hc->heap + (gpu_va - HAL_HEAP_BASE)` → `hal_heap_ptr(hal_, gpu_va)`
- L2 violation count: 9 → 8 (Phase 1 complete)

**Why this change**:
- `hal/hal_user.h`: 1 violation, 1 call site (field access to `hc->heap`)
- The foundation change added `heap_ptr` fn-ptr that wraps the `hc->heap + offset` calculation
- Completes Phase 1 of B-class fix path (after this, 8 violations remain for Phase 2)

**Why now**:
- Fence-id removal shipped (commit dc5b9a1)
- Method-codec removal shipped (commit 9047745)
- Pattern is established and validated (tests pass, 0 regression)
- L2: 9 → 8; Phase 1 complete; next is Phase 2 (6 more fn-ptrs + 6 removals)

**Architectural basis**:
- **ADR-023 §Decision 4** (append-only HAL extension) — fn-ptr added in foundation
- **ADR-072 §Decision 4 revised** (1 foundation + N removal pattern) — this is removal #3
- **ADR-043 §D5** (12 B-class violations) — 1 more removed by this change

## What Changes

- `plugins/gpu_driver/drv/gpgpu_device.cpp`:
  - Remove `#include "hal/hal_user.h"` (line 24)
  - Replace `hc ? reinterpret_cast<void*>(hc->heap + (gpu_va - HAL_HEAP_BASE)) : nullptr}` with `hal_heap_ptr(hal_, gpu_va)` (line 235)

## Capabilities

### New Capabilities

(none — uses capability from B-class foundation change)

## Impact

- `plugins/gpu_driver/drv/gpgpu_device.cpp` (1 include removed, 1 call site migrated)
- L2 violation count: 9 → 8 (Phase 1 complete)
- (Not in scope: 8 remaining B-class violations — Phase 2 + final removals)

## Out of Scope

- ❌ Removal of `hal/hal_user.h` from sim/ itself (still needed by `hal_user.cpp` to implement the heap_ptr fn-ptr)
- ❌ 8 remaining B-class violations (graph.h, mem_pool.h, gpu_queue_emu.h, stream_capture.h, hardware_puller_emu.h, and 2 others)
- ❌ Phase 2 of B-class foundation (6 more fn-ptrs needed for remaining 6 headers)
