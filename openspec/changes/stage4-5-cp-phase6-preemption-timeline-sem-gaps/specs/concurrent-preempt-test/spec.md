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

The test SHALL spawn `kSubmitThreads = std::thread::hardware_concurrency()` worker threads, each running `kPreemptCycles = 100` preempt/resume cycles.

#### Scenario: Default concurrency
- **WHEN** the test starts on a system with N hardware threads
- **THEN** it spawns N worker threads, each running 100 cycles

#### Scenario: Single-thread fallback
- **WHEN** `hardware_concurrency() == 1`
- **THEN** the test runs as a single thread with 100 cycles (no parallel section)

### Requirement: Concurrent Test Invariants

The test SHALL verify that after all workers complete, no fences are lost and no worker deadlocked.

#### Scenario: No fence loss
- **WHEN** all worker threads have joined
- **THEN** `fences_submitted == fences_signaled + fences_canceled`
- **AND** `fences_canceled == 0` (no deadlock forced cancellation)

#### Scenario: No deadlock (timeout safety)
- **WHEN** any worker thread fails to join within 30 seconds
- **THEN** the test FAILS with diagnostic output (worker state, channel state, fence state)

#### Scenario: No state leak (implicit via process exit)
- **WHEN** test process exits cleanly
- **THEN** all channels / semaphores / fences are released (verified by sanitizer clean exit)

### Requirement: Concurrent Test Uses Backdoor Symbols

The test SHALL use v1's C-ABI backdoor symbols (`bd_preempt`, `bd_sem_*`, `bd_fence_read`, `bd_fence_create`) — NOT public GPU_IOCTL_* interface.

#### Scenario: Test calls backdoor directly
- **WHEN** the test code is inspected
- **THEN** it includes `plugins/gpu_driver/sim/backdoor/*.h` and calls backdoor functions directly

#### Scenario: Test does NOT call GPU_IOCTL_*
- **WHEN** `grep GPU_IOCTL tests/test_concurrent_preempt.cpp` is run
- **THEN** the output is empty (no public ioctl usage in stress test)