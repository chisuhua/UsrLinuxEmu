## Why

Stage 4 L2 portability gate (ADR-072 §L2) is blocked by **12 B-class violations** in `plugins/gpu_driver/drv/` (per ADR-043 §D5 + ADR-072 §Decision 2/4, 2026-08-03 revision). All 12 are interface calls or field access — **0 are pure A-class** (the original ADR-072 §Decision 2 example `fence_id.h` is actually B-class because drv/ calls `sim_fence_id_alloc()`).

**Fix path** (per ADR-072 §Decision 4 revised):
1. **Foundation change** (this proposal) — extend `struct gpu_hal_ops` with fn-ptrs for the 3 lowest-risk B-class headers, establishing the pattern per ADR-023 Decision 4 spec-driven "append-only" rule
2. **N removal changes** (follow-ups) — each removes one sim/* or `hal/hal_user.h` include by routing through the new fn-ptrs

**Why now**:
- L2 gate is the Stage 4 §② acceptance criterion (per `docs/roadmap/stage-4-bar-ioremap.md`)
- Current state: `tools/ci/l2-portability/build-drv-against-linux-6.12.sh` will FAIL until 0 violations remain
- Building the foundation first ensures the pattern is correct before scaling to 8+ removal changes

**Scope of this change** (Phase 1 of B-class foundation):
- 3 lowest-risk B-class headers → 5 new HAL fn-ptrs
  - `sim/fence_id.h` → `hal_fence_id_alloc`, `hal_fence_id_signal`, `hal_fence_id_check` (3 fn-ptrs)
  - `sim/hardware/method_codec.h` → `hal_method_codec_encode` (1 fn-ptr)
  - `hal/hal_user.h` (field access) → `hal_heap_ptr` (1 fn-ptr — replaces `hc->heap` direct access)
- NO removal of sim includes in this change (that's the follow-up removal changes)
- Pattern demonstration for 9 remaining B-class headers

**Why these 3 first**:
- `fence_id.h`: cleanest C-ABI functions (no class state), 2 drv/ files use it
- `method_codec.h`: single function, minimal interface
- `hal_user.h` heap accessor: addresses the A-class-attempt failure (drv/ accesses `hc->heap` field)

## What Changes

- `plugins/gpu_driver/hal/gpu_hal.h`:
  - Add 5 new fn-ptr signatures to `struct gpu_hal_ops` (append-only, no existing fn-ptr modified — per ADR-023 Decision 4)
  - Add 5 new inline wrapper functions (zero-overhead call forwarding)
- `plugins/gpu_driver/hal/hal_user.cpp`:
  - Implement 5 fn-ptrs using existing sim functions
  - Preserve existing ABI (no breaking change)
- `plugins/gpu_driver/hal/hal_mock.cpp`:
  - Implement 5 fn-ptrs for unit test path (mock backing)
- (Not in scope: removing sim includes from drv/ — separate removal changes)

## Capabilities

### New Capabilities

- `l2-portability-foundation`: HAL exposes fn-ptrs for drv/ to call sim-layer functions without including sim/ headers, enabling the L2 portability gate

## Impact

- `plugins/gpu_driver/hal/gpu_hal.h` (5 fn-ptrs + 5 inline wrappers, ~50 LOC)
- `plugins/gpu_driver/hal/hal_user.cpp` (5 fn-ptr impls, ~40 LOC)
- `plugins/gpu_driver/hal/hal_mock.cpp` (5 mock fn-ptr impls, ~40 LOC)
- `plugins/gpu_driver/sim/fence_id.h` — UNCHANGED (still used by sim/ itself + hal_user.cpp)
- `plugins/gpu_driver/sim/hardware/method_codec.h` — UNCHANGED (still used by sim/ itself + hal_user.cpp)
- `plugins/gpu_driver/hal/hal_user.h` — UNCHANGED struct, but heap field access will be deprecated for drv/ use
- (Not in scope: drv/ code changes — this change is HAL extension only; removal is separate)

## Out of Scope

- ❌ Removing `sim/fence_id.h` from drv/ → follow-up change `stage4-l2-foundation-removal-fence-id`
- ❌ Removing `sim/hardware/method_codec.h` from drv/ → follow-up change `stage4-l2-foundation-removal-method-codec`
- ❌ Removing `hal/hal_user.h` from drv/ → follow-up change `stage4-l2-foundation-removal-hal-user`
- ❌ 9 remaining B-class headers (graph.h, mem_pool.h, gpu_queue_emu.h, stream_capture.h, hardware_puller_emu.h) — separate foundation phases
- ❌ Updating tests to use new fn-ptrs (tests can continue using existing API)
- ❌ Changing HAL function signatures (append-only per ADR-023 Decision 4)
