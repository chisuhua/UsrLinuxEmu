---
SCOPE: shared
STATUS: PROPOSED
---

## Context

`test_gpu_plugin.cpp` validates the GPU plugin via real ioctl calls. For async operations, the test pattern is:

1. Submit async op → ioctl returns `fence_id_out`
2. Assert `fence_id_out >= 1<<32` (sim-layer range)
3. **Call `GPU_IOCTL_WAIT_FENCE` with timeout and assert `status == 1`** — proves Puller processed the batch and signaled

Step 3 is missing for `MEM_POOL_ALLOC_ASYNC` (line 680) and `MEM_POOL_FREE_ASYNC` (line 697), but present for `GRAPH_LAUNCH` (line 495-535). This change closes the inconsistency.

## Goals / Non-Goals

**Goals:**
- Add `GPU_IOCTL_WAIT_FENCE` validation to `MEM_POOL_ALLOC_ASYNC` test
- Add `GPU_IOCTL_WAIT_FENCE` validation to `MEM_POOL_FREE_ASYNC` test
- Both follow the proven pattern at `test_gpu_plugin.cpp:529-534`

**Non-Goals:**
- No new test cases
- No new test fixtures
- No production code changes
- No new ioctls

## Decisions

### Decision 1: Follow existing pattern exactly (no new helper)

The pattern at `:529-534` is:

```cpp
struct gpu_wait_fence_args wait = {};
wait.fence_id = launch.fence_id_out;
wait.timeout_ms = 100;
REQUIRE(ioctl(GPU_IOCTL_WAIT_FENCE, &wait) == 0);
REQUIRE(wait.status == 1);
```

We copy this verbatim into each of the 2 enhanced test cases. No helper function, no shared utility — keeping the change minimal and self-evident.

**Rationale**: 2 test cases × 5 lines = 10 lines net. Introducing a helper would be over-engineering for this scope.

### Decision 2: 100ms timeout

The graph launch test (`:532`) uses 100ms. We use the same value for consistency. Puller's `handleComplete()` is documented to signal within ~1ms in normal operation; 100ms gives a 100x safety margin while still failing fast in regression scenarios.

### Decision 3: No new tags, no metadata changes

The 2 enhanced test cases keep their existing tags (`[gpu][ioctl][phase32][mem_pool]`). The change is a pure test enhancement within existing test boundaries.

## Risks / Trade-offs

| # | Risk | Mitigation |
|---|------|------------|
| 1 | Puller timing — async mem_pool op might take longer than 100ms to signal | Existing :532 uses 100ms for graph launch and passes consistently. Puller state machine is identical regardless of op type (graph/alloc/free). |
| 2 | Test becomes flaky under heavy CI load | If Puller has no async deadline, the test could time out. We will monitor; can extend to 500ms if needed (low priority follow-up). |
| 3 | Other tests using same fixture get affected | No — the change is in the body of 2 test cases, not in the fixture setup. |

## Verification

```bash
cd /workspace/project/UsrLinuxEmu/build
ctest -R test_gpu_plugin --output-on-failure
```

Expected: all test cases pass, including the 2 enhanced ones.

## Open Questions

(none — exact change scope defined)
