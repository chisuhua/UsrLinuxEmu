---
SCOPE: shared
STATUS: PROPOSED
---

## Why

Coverage audit of `test_gpu_plugin.cpp` (2026-07-09, after test-cu-graph-coverage-fixes merged) identified a **gap in async fence lifecycle verification** for memory pool operations:

- `test_gpu_plugin.cpp:680` `GPU_IOCTL_MEM_POOL_ALLOC_ASYNC` test verifies the ioctl returns success and `fence_id_out >= 1<<32` (sim-layer range), but does **not** call `GPU_IOCTL_WAIT_FENCE` to confirm the fence is actually signaled by the Puller's `handleComplete()`.
- `test_gpu_plugin.cpp:697` `GPU_IOCTL_MEM_POOL_FREE_ASYNC` has the **same gap** — fence_id range is checked but signal state is not.

For comparison, the graph launch path (`test_gpu_plugin.cpp:495-535`, "GPU_IOCTL_GRAPH_LAUNCH fence signaled after Puller completion (ADR-040)") **does** call `GPU_IOCTL_WAIT_FENCE` and asserts `status == 1`. The async memory pool path is inconsistent with this proven pattern.

This is the only async-fence path in `test_gpu_plugin.cpp` that does not validate the full Puller → `handleComplete()` → fence signal → wait round-trip.

## What Changes

Add `GPU_IOCTL_WAIT_FENCE` call to the **2 existing** `TEST_CASE_METHOD` blocks:

1. `GPU_IOCTL_MEM_POOL_ALLOC_ASYNC (0x63)` — after `REQUIRE(async.fence_id_out >= 1<<32)`, add wait_fence with 100ms timeout, REQUIRE status==1.
2. `GPU_IOCTL_MEM_POOL_FREE_ASYNC (0x64)` — after `REQUIRE(free_args.fence_id_out >= 1<<32)`, same pattern.

The pattern follows `test_gpu_plugin.cpp:529-534` exactly (no new helper, no new fixtures).

## Capabilities

### New Capabilities

- `mem-pool-async-fence-signal-coverage`: End-to-end fence signal verification for the async memory pool path (closes the only async-fence gap not covered by `test_gpu_plugin.cpp`).

### Modified Capabilities

(none)

## Impact

- **Single file modified**: `tests/test_gpu_plugin.cpp` (2 TEST_CASE blocks, ~12 lines net addition)
- **No production code changes** (UsrLinuxEmu or TaskRunner)
- **No new ioctls, no new fixtures, no new dependencies**
- **No submodule pointer bump** (change is entirely within UsrLinuxEmu)
- **Risk**: minimal — pure test addition that follows an already-validated pattern

## Acceptance Criteria

- `ctest -R test_gpu_plugin` from `build/` runs all existing test cases + the 2 enhanced cases, all pass
- `test_gpu_plugin.cpp:680` and `:697` each contain a `GPU_IOCTL_WAIT_FENCE` call with `REQUIRE(status == 1)`
- The 2 enhanced tests preserve their existing assertions (fence_id range, va_out range for ALLOC, etc.)
- No other test in the project is affected
