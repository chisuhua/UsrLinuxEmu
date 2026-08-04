## Why

Phase 1 B-class L2 fix is complete (L2: 12 → 8, 4 violations removed). 8 L2 violations remain across 5 unique headers, blocking the L2 portability gate (ADR-072 §L2) from passing.

**This change (Phase 2 foundation)**:
- Adds ~24-27 new fn-ptrs to `struct gpu_hal_ops` (append-only per ADR-023 Decision 4)
- Enables the 5 Phase-2 removal changes to migrate drv/ from direct sim/ calls to HAL fn-ptr calls
- After this foundation: drv/ can be migrated header-by-header (1 removal change per header)

**Why this approach**:
- Establishes the fn-ptr signatures ONCE so all 5 removal changes follow the same pattern
- Each removal change becomes a simple "swap call + remove include" (~15-20 min each)
- Validates the patterns work before scaling

**Scope audited** (per `grep` on drv/):
| Header | Call sites | Unique fn-ptrs needed |
|--------|-----------:|---------------------:|
| sim/graph.h | 15 | ~8 (create/destroy/add_kernel_node/add_memcpy_node/instantiate/launch/destroy_exec/etc.) |
| sim/mem_pool.h | 27 | ~9 (create/destroy/alloc/alloc_async/free/free_async/set_attr/get_attr/trim) |
| sim/stream_capture.h | 8 | 3 (begin/end/status) |
| sim/gpu_queue_emu.h | 3 | ~5 (class methods: ctor + attachSharedMemory + submit + ...) |
| sim/hardware/hardware_puller_emu.h | 2 | ~3 (class methods: setPuller + registerQueue + ...) |
| **Total** | **55** | **~28** |

**Why now**:
- Phase 1 complete (L2: 12 → 8)
- Pattern established and validated (3 removal changes shipped, 0 regression)
- Remaining 8 violations need foundation before removal

**Why ~28 fn-ptrs is acceptable**:
- Append-only per ADR-023 Decision 4 (no breaking changes)
- struct gpu_hal_ops currently has 19 fn-ptrs; +28 = 47 fn-ptrs total
- Each removal change removes 1 include and updates 1-N call sites
- Total work: 1 foundation + 5 removals = 6 changes (vs. 28 individual changes if we skipped the foundation)

## What Changes

- `plugins/gpu_driver/hal/gpu_hal.h`:
  - Add ~28 new fn-ptr declarations to `struct gpu_hal_ops` (append-only at end of struct)
  - Add ~28 new inline wrapper functions (zero-overhead call forwarding)
- `plugins/gpu_driver/hal/hal_user.cpp`:
  - Add ~28 new lambda assignments that delegate to existing sim/ functions
  - Preserve existing ABI (no breaking change)
- `plugins/gpu_driver/hal/hal_mock.cpp`:
  - Add ~28 new mock fn-ptr impls (return test-friendly defaults: monotonic counter, true, 0, nullptr)
- (No drv/ changes in this change — separate removal changes will follow)

## Capabilities

### New Capabilities

(none — extends existing `l2-portability-foundation` capability from Phase 1 with more fn-ptrs)

## Impact

- `plugins/gpu_driver/hal/gpu_hal.h` (~28 fn-ptrs + ~28 inline wrappers, ~350 LOC)
- `plugins/gpu_driver/hal/hal_user.cpp` (~28 lambda impls, ~200 LOC)
- `plugins/gpu_driver/hal/hal_mock.cpp` (~28 mock impls, ~200 LOC)
- `struct gpu_hal_ops` total fn-ptr count: 19 → 47
- (Not in scope: drv/ changes — 5 separate removal changes will follow)

## Out of Scope

- ❌ drv/ call site migrations (5 separate removal changes: graph, mem_pool, stream_capture, gpu_queue_emu, hardware_puller_emu)
- ❌ Removal of sim/ headers from sim/ itself (still needed by hal_user.cpp to implement fn-ptrs)
- ❌ Class type exposure (e.g., `GpuQueueEmu` and `HardwarePullerEmu` are used as `shared_ptr<>` in drv/ — may need shared/ fwd declarations)

## Follow-up (5 removal changes after this foundation)

1. `2026-08-03-stage4-l2-foundation-removal-graph` (L2: 8 → 6)
2. `2026-08-03-stage4-l2-foundation-removal-mem-pool` (L2: 6 → 4)
3. `2026-08-03-stage4-l2-foundation-removal-stream-capture` (L2: 4 → 2)
4. `2026-08-03-stage4-l2-foundation-removal-gpu-queue-emu` (L2: 2 → 1)
5. `2026-08-03-stage4-l2-foundation-removal-hardware-puller-emu` (L2: 1 → 0 — **GATE PASSES**)
