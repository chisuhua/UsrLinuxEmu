## Context

The `phase4-sim-graph-launch-real-impl` change (commits 7740a75 + f0f7a03, 2026-07-09) implemented ADR-040/041/043, shipping real async graph_launch. A post-merge coverage audit (`bg_01583a02`) identified 4 test gaps in the drv layer and Puller FSM that could trigger in production but are silently unverified.

Current state: 86/86 ctest PASS. After this change: 90-91/90-91 ctest PASS (additive, no production code touched).

## Goals / Non-Goals

**Goals:**
- Close 4 coverage gaps identified by `bg_01583a02` audit
- Purely additive test changes — no production code modifications
- Maintain 86/86 baseline (no regression) + add ~5 new test cases

**Non-Goals:**
- Adding new public APIs or ABIs
- Modifying existing production behavior
- Performance/benchmark coverage (separate concern)
- Cross-repo TaskRunner tests (handled in companion change)

## Decisions

### Decision 1: Add drv error path tests to `test_gpu_plugin.cpp` rather than `test_sim_graph_standalone.cpp`

**Rationale**: The two HIGH-priority gaps (P1 empty executable, P2 missing queue) are in the **drv layer** (`gpgpu_device.cpp::handleGraphLaunch`), not the sim layer. The sim layer for these scenarios is already covered (17 cases in `test_sim_graph_standalone.cpp`). Adding to `test_gpu_plugin.cpp` exercises the full ioctl → GpgpuDevice::ioctl → handleGraphLaunch → return path, which is the actual production code path.

**Alternatives considered:**
- New test binary `test_graph_launch_errors` — rejected, adds maintenance overhead for 2 tests
- Add to `test_gpu_driver_client_phase31_standalone.cpp` — rejected, that file is for cross-repo protocol testing, not drv error path

### Decision 2: Add multi-entry fence signal test to `test_hardware_puller_emu.cpp` (not `test_gpu_plugin.cpp`)

**Rationale**: The `current_index_+1 >= total_entries_` boundary in `handleComplete()` is a Puller FSM concern, not a drv concern. Testing at the Puller level gives precise control over entry count without needing to construct GPFIFO entries through the ioctl path. The mock_hal infrastructure already supports multiple entries per batch.

**Alternatives considered:**
- Integration test through `test_gpu_plugin.cpp` — rejected, harder to control entry count exactly
- Inline test in `test_sim_graph_standalone.cpp` — rejected, sim layer doesn't exercise Puller FSM

### Decision 3: Add null guard tests for `sim_graph_create` and `sim_graph_instantiate`

**Rationale**: The sim C API has null-pointer defensive checks (`if (!exec_handle_out) return -EINVAL;`) that are currently uncovered. While low risk (callers are protected), explicit tests document the contract and prevent future regressions if the checks are accidentally removed.

**Alternatives considered:**
- Skip these as "obviously correct" — rejected, defensive checks are the most common source of regressions
- Add to a single combined test — rejected, one assertion per test for clarity

## Risks / Trade-offs

[Risk 1] Tests may inadvertently exercise different code paths than intended (e.g., race conditions in Puller FSM tests) → Mitigation: Use the same `wait_for_state` drain pattern from existing `test_puller_fence_signal_on_completion` to ensure deterministic state observation

[Risk 2] Adding tests to `test_gpu_plugin.cpp` may increase its runtime → Mitigation: Each new test ~10-50ms; total budget increase <500ms (acceptable for the coverage gain)

[Risk 3] Multi-entry fence test may be flaky if Puller state transitions are not deterministic → Mitigation: Use 3 entries (small enough to not stress timing, large enough to exercise the boundary) and follow the established `wait_for_state(non-IDLE) → wait_for_state(IDLE)` drain pattern

[Risk 4] Null guard tests may be considered "noise" by reviewers → Mitigation: Include them with rationale (defensive contract documentation) rather than removing

## Migration Plan

1. Apply test additions to 3 test files (no production code changes)
2. Run `cmake --build build` to verify compilation
3. Run individual test binaries to verify new cases pass
4. Run full `ctest` to verify 86 baseline + new cases all pass
5. No rollback needed (purely additive)
6. No docs changes needed (test names are self-documenting)
7. No cross-repo coordination needed (UsrLinuxEmu-internal only)

## Open Questions

(none — all design decisions resolved)
