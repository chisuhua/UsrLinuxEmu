## Why

The recently-merged `phase4-sim-graph-launch-real-impl` change (ADR-040/041/043, commits 7740a75 + f0f7a03) shipped with **86/86 ctest PASS** but a post-implementation coverage audit (`bg_01583a02`) identified **4 high/medium-priority test gaps** in the drv layer error paths and Puller fence signal logic:

1. **P1/C1 (HIGH)**: `handleGraphLaunch` returns -EINVAL when `entry_count==0` (empty executable) — no test covers this drv-layer path (sim layer is covered)
2. **P2/C2 (HIGH)**: `handleGraphLaunch` returns -ENOENT when `getQueue(stream_id)` fails (no queue mapped) — no test covers this
3. **H1 (MEDIUM)**: `HardwarePullerEmu::handleComplete()` fence signal logic tested only with `total_entries_=1`; the `current_index_+1 >= total_entries_` boundary condition is untested for multi-entry batches
4. **P5/P6/P7 (LOW)**: error injection paths for `sim_fence_id_alloc` OOM, `q->submit` failure, and `hal_=nullptr` doorbell path

These gaps represent real code paths that could trigger in production but are silently unverified.

## What Changes

- **New test cases** in `tests/test_gpu_plugin.cpp`:
  - `GPU_IOCTL_GRAPH_LAUNCH empty executable returns -EINVAL` (instantiate empty graph, launch, expect -EINVAL)
  - `GPU_IOCTL_GRAPH_LAUNCH missing queue returns -ENOENT` (create valid exec but no queue, launch, expect -ENOENT)
- **New test case** in `tests/test_hardware_puller_emu.cpp`:
  - `test_puller_fence_signal_multi_entry` — submit 3-entry batch with `fence_id≠0`, verify fence signaled only after all 3 entries processed
- **New test cases** in `tests/test_sim_graph_standalone.cpp`:
  - `graph — create with NULL handle returns -EINVAL` (defensive null check coverage)
  - `graph — instantiate with NULL exec handle returns -EINVAL` (defensive null check coverage)
- **No production code changes** — pure test additions, no implementation changes
- **No ABI/interface changes** — purely additive test coverage

## Capabilities

### New Capabilities

- `sim-graph-test-coverage`: Test coverage completeness for the sim_graph_launch C ABI (null/edge case handling) and the drv-side `handleGraphLaunch` error paths
- `puller-fence-multi-entry`: Test coverage for `HardwarePullerEmu::handleComplete()` fence signal boundary with multi-entry batches

### Modified Capabilities

(none — no requirement-level changes to existing specs)

## Impact

- **Affected test files** (purely additive, no production code touched):
  - `tests/test_gpu_plugin.cpp` (+2 cases for drv error paths)
  - `tests/test_hardware_puller_emu.cpp` (+1 case for multi-entry fence)
  - `tests/test_sim_graph_standalone.cpp` (+2 cases for null guards)
- **No API/ABI changes** — all new tests are consumer-side
- **No cross-repo impact** — purely UsrLinuxEmu-internal test coverage improvement
- **No docs changes** — the existing test cases document the behavior
- **CI impact**: test count increases from 86 → 90-91 cases
