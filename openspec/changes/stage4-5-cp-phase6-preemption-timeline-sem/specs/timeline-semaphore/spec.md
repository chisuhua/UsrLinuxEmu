## ADDED Requirements

### Requirement: Timeline Semaphore Create

The system SHALL provide `sem_create(initial)` returning a unique handle with the given initial value.

#### Scenario: Create with initial=0
- **WHEN** `sem_create(0, &handle)` is called
- **THEN** handle is a valid monotonic-unique identifier; `sem_query(handle)` returns 0

#### Scenario: Create with initial>0
- **WHEN** `sem_create(N, &handle)` is called where N > 0
- **THEN** handle is valid; `sem_query(handle)` returns N immediately

### Requirement: Timeline Semaphore Signal Strict Monotonicity

The system SHALL enforce strict monotonic signal: `sem_signal(value)` accepts only `value > current`, otherwise returns -EINVAL.

#### Scenario: Signal with value > current
- **WHEN** `sem_query(handle)` returns 5 AND `sem_signal(handle, 7)` is called
- **THEN** returns 0; `sem_query(handle)` returns 7

#### Scenario: Signal with value == current
- **WHEN** `sem_query(handle)` returns 5 AND `sem_signal(handle, 5)` is called
- **THEN** returns -EINVAL; `sem_query(handle)` still returns 5

#### Scenario: Signal with value < current
- **WHEN** `sem_query(handle)` returns 5 AND `sem_signal(handle, 3)` is called
- **THEN** returns -EINVAL; `sem_query(handle)` still returns 5

### Requirement: Timeline Semaphore Wait via Callback (Non-Blocking)

The system SHALL provide `sem_wait(handle, callback)` that registers a callback without blocking. The callback SHALL fire when the sem value reaches or exceeds `wait_value`.

> **D1 修订** (ADR-049 显式修订)：wait 语义由阻塞改为 waiter 回调注册。Puller 线程禁止 blocking wait semaphore。

#### Scenario: Waiter callback fires when condition met
- **WHEN** `sem_wait(S1, callback)` is registered with `wait_value=5` AND `sem_query(S1) == 3`
- **THEN** callback does NOT fire immediately
- **AND WHEN** `sem_signal(S1, 7)` is later called
- **THEN** callback fires (S1 value 7 >= wait_value 5)

#### Scenario: Waiter callback fires immediately if already satisfied
- **WHEN** `sem_wait(S1, callback)` is registered with `wait_value=5` AND `sem_query(S1) == 10`
- **THEN** callback fires immediately (condition already met)

#### Scenario: Multiple waiters FIFO
- **WHEN** waiter W1 is registered with sem S1 (wait_value=3) THEN waiter W2 is registered (wait_value=3)
- **AND WHEN** `sem_signal(S1, 5)` is called
- **THEN** W1 fires first (FIFO), then W2 fires

### Requirement: Timeline Semaphore Query (Acquire Semantics)

The system SHALL provide `sem_query(handle)` returning the current value with acquire semantics (cross-thread safe).

#### Scenario: Query reads current value
- **WHEN** `sem_query(S1)` is called
- **THEN** returns current value of S1

#### Scenario: Cross-thread signal-then-query visibility
- **WHEN** thread T1 calls `sem_signal(S1, 10)` with release semantics
- **AND WHEN** thread T2 subsequently calls `sem_query(S1)`
- **THEN** T2's query returns at least 10 (acquire semantics)

### Requirement: Timeline Semaphore Destroy with Waiter Wakeup

The system SHALL provide `sem_destroy(handle)` that wakes all pending waiters with -ECANCELED and removes the handle from the registry.

#### Scenario: Destroy with no waiters
- **WHEN** `sem_destroy(S1)` is called AND no waiters registered
- **THEN** returns 0; handle is removed from registry; subsequent operations on S1 return -EINVAL

#### Scenario: Destroy with pending waiters
- **WHEN** `sem_destroy(S1)` is called AND 3 waiters are registered
- **THEN** all 3 waiter callbacks fire with error code -ECANCELED
- **AND** handle is removed from registry

#### Scenario: Double-destroy returns error
- **WHEN** `sem_destroy(S1)` is called twice
- **THEN** second call returns -EINVAL (handle no longer in registry)

### Requirement: Invalid Handle Handling (Negative Path)

The system SHALL return -EINVAL for any operation on an invalid or destroyed handle.

#### Scenario: Signal on destroyed handle
- **WHEN** handle S1 was destroyed AND `sem_signal(S1, 5)` is called
- **THEN** returns -EINVAL

#### Scenario: Wait on invalid handle
- **WHEN** handle 99999 does not exist AND `sem_wait(99999, callback)` is called
- **THEN** returns -EINVAL

#### Scenario: Query on invalid handle
- **WHEN** handle 99999 does not exist AND `sem_query(99999)` is called
- **THEN** returns -EINVAL

### Requirement: gpfifo_entry Timeline Field Consumption

The system SHALL consume the `gpu_gpfifo_entry.timeline` field on batch submission: signal on completion, wait before DISPATCH if wait_value not met.

#### Scenario: Batch completion triggers signal
- **WHEN** batch B1 with `timeline = {signal_handle=S1, signal_value=3}` completes
- **THEN** `sem_signal(S1, 3)` is called by Puller complete callback

#### Scenario: Batch waits on SEM_WAIT before DISPATCH
- **WHEN** batch B2 with `timeline = {wait_handle=S1, wait_value=5}` is fetched
- **AND** `sem_query(S1) == 3` (not satisfied)
- **THEN** DISPATCH is deferred; waiter callback registered with S1 (wait_value=5)
- **AND WHEN** later `sem_signal(S1, 5)` is called
- **THEN** callback fires; DISPATCH resumes for B2

#### Scenario: Batch with both signal and wait
- **WHEN** batch B3 has `timeline = {signal_handle=S1, signal_value=1, wait_handle=S2, wait_value=2}`
- **THEN** DISPATCH waits on S2 (if not satisfied); on completion, signals S1

### Requirement: Fence Thin Wrapper over Timeline Semaphore

The system SHALL implement `fence_create/fence_read` as thin wrappers over Timeline Semaphore.

> **ADR-040 迁移**：`sim_fence_id_signal` 路径迁移到 timeline sem（作为 `sem_signal` 触发源之一），删除双实现。

#### Scenario: fence_create thin wrapper
- **WHEN** `fence_create(&fence_id)` is called
- **THEN** internally calls `sem_create(0, &handle)`; fence_id is mapped to sem handle in `pending_fences_[channel_id]`

#### Scenario: fence_read thin wrapper
- **WHEN** `fence_read(fence_id)` is called
- **THEN** internally calls `sem_query(handle_of_fence_id)`; returns `value > 0` as boolean

#### Scenario: Puller complete callback triggers fence signal
- **WHEN** batch with fence F1 completes
- **THEN** Puller calls `sem_signal(handle_of_F1, 1)`; F1's value becomes 1; fence_read returns true

#### Scenario: No dual implementation
- **WHEN** `grep sim/fence_id.* sim_fence_id_signal` is run on the codebase
- **THEN** output is empty (no dual implementation)

### Requirement: Waiter Queue Channel Cleanup

The system SHALL clean up waiter registrations when a channel is destroyed, preventing dangling callbacks.

#### Scenario: Channel destroy cleans up attached sems
- **WHEN** channel C is destroyed AND C has attached sems {S1, S2}
- **THEN** all waiters registered by C are woken with -ECANCELED
- **AND** S1, S2 are removed from C's attached set (semaphores may persist if shared with other channels)

#### Scenario: No dangling waiter callbacks
- **WHEN** channel C is destroyed
- **THEN** no callbacks referencing C's destroyed state can fire afterward

### Requirement: Sem Value Atomic Guarantee

The system SHALL guarantee atomic visibility of sem value across signal (release semantics) and query/waiter-observation (acquire semantics).

#### Scenario: Atomic value read-write
- **WHEN** thread T1 calls `sem_signal(S1, N)` (release)
- **AND WHEN** thread T2 calls `sem_query(S1)` (acquire)
- **THEN** T2 observes at least N; no torn reads

#### Scenario: Waiter queue mutex protection
- **WHEN** multiple threads concurrently register waiters on sem S1
- **THEN** waiter queue integrity is maintained (no race); all waiters eventually fire in correct FIFO order