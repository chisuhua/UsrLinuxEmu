# hal-preempt-resume-semaphore Specification

## Purpose
TBD - created by archiving change implement-hal-preempt-resume-semaphore. Update Purpose after archive.
## Requirements
### Requirement: HAL user preemption and timeline semaphore wiring

`plugins/gpu_driver/hal/hal_user.cpp` SHALL replace the seven no-op Stage 4.5/4.6 HAL fn-pointer lambdas (`hal_preempt`, `hal_resume`, `hal_sem_create`, `hal_sem_signal`, `hal_sem_wait`, `hal_sem_query`, `hal_sem_destroy`, currently lines 293-307) with real implementations that delegate to the existing `sim::SemaphoreManager` (per `hal_user.cpp:14` include) and to the sim scheduler's preempt/resume API. `struct gpu_hal_ops` SHALL NOT change signature, `hal_mock.cpp` SHALL NOT be modified, and `struct hal_user_context` SHALL remain thread-safe (mirroring the `fence_lock` pattern). The `SemaphoreManager::create` / `signal` / `wait` / `query` / `destroy` methods SHALL be exercised end-to-end so that semaphore lifecycle (`create → signal → wait-callback → destroy`) is observable from the drv/ layer.

#### Scenario: hal_sem_create returns a real SemaphoreManager handle

- **GIVEN** drv/ invokes `hal->hal_sem_create(ctx, initial, &handle)`
- **WHEN** the call returns
- **THEN** `handle` SHALL be a real `SemaphoreManager` instance handle stored in `hc->sem_handles` (mirroring the `fence_create` pattern at `hal_user.cpp:90-109`)
- **AND** subsequent `hal_sem_signal` / `hal_sem_wait` / `hal_sem_query` / `hal_sem_destroy` on that handle SHALL operate on the same `SemaphoreManager` instance
- **AND** the function SHALL return `0` on success

#### Scenario: hal_sem_wait callback fires after signal reaches expected value

- **GIVEN** drv/ has called `hal_sem_create(ctx, 0, &h)` to obtain `h`
- **AND** drv/ has registered a waiter via `hal_sem_wait(ctx, h, expected=3, cb, ud)`
- **WHEN** drv/ calls `hal_sem_signal(ctx, h, 5)` (or any value `>= expected`)
- **THEN** `cb(ud)` SHALL be invoked asynchronously with the `user_data` passed at registration time
- **AND** the function SHALL NOT block the caller waiting for the value to reach `expected`

#### Scenario: hal_sem_query reflects current SemaphoreManager state

- **GIVEN** drv/ has called `hal_sem_create(ctx, 7, &h)`
- **WHEN** drv/ calls `hal_sem_query(ctx, h, &out)`
- **THEN** `out` SHALL equal `7` (the initial value, since no signal has occurred)
- **AND** after `hal_sem_signal(ctx, h, 4)`, `hal_sem_query(ctx, h, &out)` SHALL yield `out >= 11` (monotonic semantics)

#### Scenario: hal_sem_destroy releases the SemaphoreManager instance

- **GIVEN** drv/ has a live handle `h` from `hal_sem_create`
- **WHEN** drv/ calls `hal_sem_destroy(ctx, h)`
- **THEN** the underlying `SemaphoreManager` instance SHALL be destroyed
- **AND** subsequent `hal_sem_signal` / `hal_sem_wait` / `hal_sem_query` on `h` SHALL return `-EINVAL` (handle no longer exists)
- **AND** `hc->sem_handles` SHALL no longer reference `h`

#### Scenario: hal_preempt marks channel as PREEMPTED in sim scheduler

- **GIVEN** drv/ invokes `hal->hal_preempt(ctx, channel_id)` while a channel is executing
- **WHEN** the call returns
- **THEN** the sim scheduler SHALL mark `channel_id` as PREEMPTED
- **AND** the function SHALL return `0`
- **AND** the call SHALL NOT crash the process

#### Scenario: hal_resume restores a preempted channel in sim scheduler

- **GIVEN** `channel_id` was previously preempted via `hal_preempt`
- **WHEN** drv/ invokes `hal->hal_resume(ctx, channel_id)`
- **THEN** the sim scheduler SHALL restore `channel_id` to an executing state
- **AND** the function SHALL return `0`

#### Scenario: handle not found returns -EINVAL

- **GIVEN** drv/ invokes `hal_sem_signal` / `hal_sem_wait` / `hal_sem_query` / `hal_sem_destroy` with a handle that was never produced by `hal_sem_create` (or was destroyed)
- **WHEN** the call is evaluated
- **THEN** the function SHALL return `-EINVAL`
- **AND** the call SHALL NOT crash the process

#### Scenario: struct gpu_hal_ops signature unchanged

- **GIVEN** `struct gpu_hal_ops` declares the seven fn-pointers in `plugins/gpu_driver/hal/gpu_hal.h`
- **WHEN** the user HAL implementation is upgraded
- **THEN** the seven fn-pointer signatures SHALL remain identical (no ABI break)
- **AND** all existing callers SHALL continue to compile without source changes

#### Scenario: hal_mock implementation unchanged

- **GIVEN** `hal_mock.cpp` currently implements the seven fn-pointers as no-ops / counters
- **WHEN** the user HAL fix is applied
- **THEN** `hal_mock.cpp` SHALL NOT be modified
- **AND** existing mock-based tests SHALL continue to pass without changes

#### Scenario: thread safety preserves SemaphoreManager state

- **GIVEN** two drv/ threads concurrently call `hal_sem_signal` and `hal_sem_query` on the same handle
- **WHEN** the operations interleave
- **THEN** `SemaphoreManager`'s monotonic semantics SHALL be preserved
- **AND** no data race or undefined behavior SHALL occur (mirroring `fence_lock` pattern)

#### Scenario: Build and ctest verification

- **GIVEN** the seven lambdas have been replaced with real implementations
- **WHEN** the developer runs `make -j4` and `ctest --output-on-failure`
- **THEN** the build SHALL compile with zero warnings
- **AND** `ctest` SHALL report the baseline 130/130 tests PASS with no regressions
- **AND** new unit tests `test_sem_create_signal_query_destroy`, `test_sem_wait_callback_triggered`, `test_preempt_resume_basic` SHALL be added and PASS

#### Scenario: lsp_diagnostics and sanitizer runs clean

- **GIVEN** the modified `hal_user.cpp`, `hal_user.h`, and any new test files
- **WHEN** `lsp_diagnostics` is run on the changed files AND a sanitizer build (`SANITIZER=asan-ubsan ./build.sh test`) is executed
- **THEN** `lsp_diagnostics` SHALL report no errors and no warnings on modified lines
- **AND** the sanitizer run SHALL report zero failures

