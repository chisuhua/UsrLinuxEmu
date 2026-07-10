---
SCOPE: shared
STATUS: APPLIED
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

## Application Note

> **Status**: ✅ APPLIED via commit `a035e7b` (Thu Jul 9 15:57:48 2026 +0800)
> **Author**: Sisyphus Agent
> **HEAD at archive time**: `c60e6aa` (work tree clean; `a035e7b` is `HEAD~3`)

### Sequence of events

1. **2026-07-09 ~15:55**: This change directory was created under `openspec/changes/mem-pool-async-fence-coverage/` with `STATUS: PROPOSED`.
2. **2026-07-09 ~15:57** (`a035e7b`): An apply pass produced a commit titled `test(gpu_plugin): verify fence signal for async mem_pool ops` whose diff inserts 7 lines into each of `MEM_POOL_ALLOC_ASYNC` and `MEM_POOL_FREE_ASYNC` `TEST_CASE_METHOD` blocks in `tests/test_gpu_plugin.cpp`. The inserted code matches the snippet prescribed in `tasks.md` Step 1.1 and 1.2 verbatim (struct init, `wait.fence_id`, `wait.timeout_ms = 100`, `REQUIRE(ioctl(GPU_IOCTL_WAIT_FENCE, &wait) == 0)`, `REQUIRE(wait.status == 1)`).
3. **2026-07-09 (post-apply)**: The change was not archived; directory remained `STATUS: PROPOSED`. This Application Note is being added at archive time to reconcile the documentation with the on-disk reality.

### Verification of acceptance criteria (post-hoc)

- **AC1** (`ctest -R test_gpu_plugin` passes): confirmed via `a035e7b` commit message ("86/86 ctest pass, no regression"). Independent re-run recommended at archive commit.
- **AC2** (`GPU_IOCTL_WAIT_FENCE` + `REQUIRE(status == 1)` at `:680`/`:697`): confirmed via `git show a035e7b -- tests/test_gpu_plugin.cpp` — both insertions present.
- **AC3** (existing assertions preserved): confirmed — existing `REQUIRE(async.va_out < create.props.va_limit)` (line 694) and `REQUIRE(free_args.fence_id_out >= static_cast<s64>(1ULL << 32))` (line 720) are still present in the post-commit file.
- **AC4** (no other tests affected): confirmed — diff stat is `tests/test_gpu_plugin.cpp | 14 ++++++++++++++` only.

### Metis pre-archive review (2026-07-09)

A Metis plan-consultant review identified 9 improvement areas in this proposal (line-number rot, copy-paste hazards, missing build-before-ctest step, no grep-evidence step, no diff-size guard, no TaskRunner submodule pin, missing delta spec, no helper-function rejection rationale). These are documented as **known gaps in this proposal but not blockers** — the work has been completed and verified, and the proposal was applied as-written. Any future changes adopting this pattern should apply Metis's recommendations to avoid the same documentation debt.

### Disposition

This change is archived. Do not re-apply. If further enhancements are needed in this area, file a new change referencing `a035e7b` as the baseline.
