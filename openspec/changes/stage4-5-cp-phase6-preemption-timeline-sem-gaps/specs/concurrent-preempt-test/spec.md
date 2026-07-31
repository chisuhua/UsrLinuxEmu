## ADDED Requirements

### Requirement: Concurrent Preempt Stress Test Exists

The system SHALL provide `test_concurrent_preempt` as a standalone Catch2 binary that exercises the preempt engine under concurrent submit load.

#### Scenario: Test binary registered in CMakeLists
- **WHEN** `tests/CMakeLists.txt` is inspected
- **THEN** a `test_concurrent_preempt_standalone` target exists, configured identically to other standalone tests

#### Scenario: Test compiles from source
- **WHEN** `cmake --build build --target test_concurrent_preempt` is run
- **THEN** the build succeeds and produces `build/bin/test_concurrent_preempt`

### Requirement: Concurrent Test Topology

The test SHALL spawn `kSubmitThreads = std::thread::hardware_concurrency()` worker threads, each running `kPreemptCycles` preempt/resume cycles. The cycle count SHALL be sanitizer-aware to keep CI runtime bounded.

#### Scenario: Default concurrency
- **WHEN** the test starts on a system with N hardware threads
- **THEN** it spawns N worker threads, each running 100 cycles (default build, no sanitizer)

#### Scenario: TSan-aware cycle reduction
- **WHEN** the test is compiled with ThreadSanitizer (detected via `__has_feature(thread_sanitizer)` or build flag)
- **THEN** `kPreemptCycles` is reduced from 100 to 20 (TSan typically causes 10-30x overhead)
- **AND** the reduced cycle count is logged at test start for CI visibility

#### Scenario: ASan+UBSan cycle count
- **WHEN** the test is compiled with AddressSanitizer + UndefinedBehaviorSanitizer (typical 2-3x overhead)
- **THEN** `kPreemptCycles` remains 100 (acceptable overhead)

#### Scenario: Single-thread fallback
- **WHEN** `hardware_concurrency() == 1`
- **THEN** the test runs as a single thread with 100 cycles (no parallel section)

### Requirement: Concurrent Test Invariants

The test SHALL verify that after all workers complete, no fences are lost and no worker deadlocked.

#### Scenario: No fence loss
- **WHEN** all worker threads have joined
- **THEN** `fences_submitted == fences_signaled + fences_canceled` (no fence dropped without accounting)
- **AND** `fences_canceled < fences_submitted * 0.01` (cancel ratio < 1%; accounts for legitimate channel-destroy races under TSan)

#### Scenario: No deadlock (timeout safety)
- **WHEN** any worker thread fails to join within 60 seconds
- **THEN** the test FAILS with diagnostic output (worker state, channel state, fence state)

#### Scenario: No state leak (implicit via process exit)
- **WHEN** test process exits cleanly
- **THEN** all channels / semaphores / fences are released (verified by sanitizer clean exit)

#### Scenario: Retry on transient flake
- **WHEN** the test fails on first run with error matching "flaky" pattern (cancel ratio > 1% but < 10%, or TSan-reported benign race)
- **THEN** the test is automatically retried up to 3 times
- **AND** if all 3 retries fail, the test is marked FAIL with diagnostic dump of the last attempt
- **AND** retry behavior is recorded in the test output (visible in CI log)

### Requirement: Concurrent Test Uses Backdoor Symbols

The test SHALL use v1's C-ABI backdoor symbols (`bd_preempt`, `bd_sem_*`, `bd_fence_read`, `bd_fence_create`) — NOT public GPU_IOCTL_* interface.

#### Scenario: Test calls backdoor directly
- **WHEN** the test code is inspected
- **THEN** it includes `plugins/gpu_driver/sim/backdoor/*.h` and calls backdoor functions directly

#### Scenario: Test does NOT call GPU_IOCTL_*
- **WHEN** `grep GPU_IOCTL tests/test_concurrent_preempt.cpp` is run
- **THEN** the output is empty (no public ioctl usage in stress test)