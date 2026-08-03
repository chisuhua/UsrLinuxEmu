# Green Context Tests — Stage 4.6 closeout (P3-A3)

## ADDED Requirements

### Requirement: test_green_create_forces_low_priority (8.2)

The standalone test binary `test_green_context_standalone` SHALL include a test case verifying that queue creation with `context_type=GREEN` forces `ChannelState::priority = LOW` regardless of any explicit priority parameter passed.

#### Scenario: GREEN creation ignores HIGH priority override

- **GIVEN** a `gpu_create_queue` invocation with `context_type = GREEN` and explicit `priority = HIGH`
- **WHEN** the queue is created
- **THEN** `ChannelState.priority` SHALL be `LOW` (HIGH override rejected)
- **THEN** the explicit `priority` parameter SHALL be ignored
- **THEN** the executable `test_green_context_standalone` ctest entry SHALL PASS

#### Scenario: BROWN creation respects priority override

- **GIVEN** a `gpu_create_queue` invocation with `context_type = BROWN` and `priority = HIGH`
- **WHEN** the queue is created
- **THEN** `ChannelState.priority` SHALL be `HIGH` (BROWN respects priority)

### Requirement: test_brown_preempts_running_green (8.3)

The standalone test SHALL include a test case verifying that when a BROWN channel's submission makes it pending while a GREEN channel is running, the BROWN channel preempts the GREEN channel (per ADR-046 dispatch-level preemption flow).

#### Scenario: BROWN pending preempt running GREEN

- **GIVEN** GREEN channel G1 (priority=LOW, state=ACTIVE) is mid-batch
- **AND** BROWN channel B1 (priority=NORMAL) submits a new batch
- **WHEN** `GlobalScheduler::dispatch_next()` is invoked
- **THEN** G1 SHALL transition to `PREEMPTED` with `PreemptContext` saved
- **THEN** B1 SHALL be ACTIVE (dispatched immediately)

### Requirement: test_green_resumes_after_brown_completes (8.4)

The standalone test SHALL verify GREEN channel resumption after BROWN channel completion.

#### Scenario: GREEN resumes from saved gpfifo position

- **GIVEN** GREEN channel G1 has been preempted by BROWN channel B1
- **WHEN** B1 completes via `dispatch_next()` cycle
- **THEN** G1 SHALL resume to `ACTIVE` state
- **THEN** G1's `ChannelState.gpfifo_position` SHALL equal the saved position (resume from saved PC)

### Requirement: test_green_does_not_preempt_green (8.5)

The standalone test SHALL verify GREEN↛GREEN preempt exclusion (per ADR-056 + 4.6 archive §3 design: GREEN channels operate in FIFO ordering within the GREEN pool).

#### Scenario: 2 GREEN channels non-preempt FIFO

- **GIVEN** 2 GREEN channels G1 + G2 both LOW priority, submitted G1 < G2
- **WHEN** G1 completes (no GREEN preempt)
- **THEN** G2 SHALL be dispatched next WITHOUT triggering preempt path

### Requirement: test_three_greens_fifo_order (8.6)

The standalone test SHALL verify strict FIFO ordering among 3 same-priority GREEN channels.

#### Scenario: 3 GREEN channels strict FIFO submission order

- **GIVEN** 3 GREEN channels G1 + G2 + G3 same priority, submitted T0 < T1 < T2
- **WHEN** complete() + `dispatch_next()` cycles run 3 times
- **THEN** dispatch order SHALL be G1 → G2 → G3 (strict submission order)

### Requirement: test_hal_green_context_create_destroy (8.7)

The standalone test SHALL verify HAL inline wrapper `hal_green_context_create` + `hal_green_context_destroy` round-trip via `hal_mock.cpp` implementation.

#### Scenario: create + destroy round-trip

- **GIVEN** `gpu_hal_ops *hal = &kMockOps` (hal_mock.cpp registered GREEN fn-ptrs)
- **WHEN** `hal_green_context_create(hal, tsg_id, &handle)` invoked
- **THEN** return SHALL be 0 and `handle` SHALL be non-zero
- **WHEN** `hal_green_context_destroy(hal, handle)` invoked
- **THEN** return SHALL be 0

#### Scenario: double-destroy returns EINVAL

- **GIVEN** a destroyed green context handle
- **WHEN** `hal_green_context_destroy(hal, handle)` invoked a second time
- **THEN** return SHALL be `-EINVAL`

### Requirement: test binary build + ctest registration

The test binary SHALL be registered in `tests/CMakeLists.txt` and discoverable via `ctest -R green_context`.

#### Scenario: ctest discovers test_green_context_standalone

- **GIVEN** `tests/CMakeLists.txt` updated with `add_executable` + `add_test` blocks
- **WHEN** `cmake --build build --target test_green_context_standalone` runs
- **THEN** binary SHALL build with 0 errors
- **WHEN** `ctest -R green_context --output-on-failure` runs
- **THEN** the registered test SHALL be discovered and PASS
