# timeline-semaphore Specification

## Purpose

定义 GPU **timeline semaphore**（单调递增信号量）作为跨引擎同步原语，与 Vulkan `VK_SEMAPHORE_TYPE_TIMELINE` / D3D12 `ID3D12Fence` 语义对齐。每个 semaphore 有单调递增 `value`；`signal(value)` 写入值，`wait(expected)` 注册回调直到 `value >= expected`。Timeline semaphore 取代 boolean sim fence 和 single-value hardware semaphore，作为 Compute / Copy / Graphics 引擎间的同步基础设施。

> **来源**：ADR-049（Cross-engine Synchronization，Stage 4.5 实施 D1 修订为 waiter 回调模式）
> **实现位置**：`plugins/gpu_driver/sim/semaphore_manager.{h,cpp}` + HAL `hal_sem_*` fn-ptrs
> **关联**：ADR-040（Puller Fence Completion，迁移为 timeline semaphore 触发源）| ADR-047（Hardware Semaphore，单 slot 退化）

## Requirements

### Requirement: sem_create allocates semaphore

The system SHALL provide `sem_create(initial_value)` that allocates a timeline semaphore with the given initial value and returns a unique handle. Semaphore handles SHALL be unique within a channel's lifetime.

#### Scenario: Create semaphore with zero initial value

- **GIVEN** no semaphore S1 exists
- **WHEN** user calls `HAL.sem_create(initial=0)`
- **THEN** a valid handle H1 SHALL be returned
- **THEN** `sem_query(H1)` SHALL return 0

#### Scenario: Create semaphore with non-zero initial value

- **GIVEN** no semaphore S1 exists
- **WHEN** user calls `HAL.sem_create(initial=5)`
- **THEN** a valid handle H1 SHALL be returned
- **THEN** `sem_query(H1)` SHALL return 5

### Requirement: sem_signal monotonic increment

The system SHALL provide `sem_signal(handle, value)` that signals the semaphore. The new value MUST be strictly greater than the current value (`new_value > current_value`). If `new_value <= current_value`, the operation SHALL return `-EINVAL`.

#### Scenario: Successful signal

- **GIVEN** semaphore S1 has current value 0
- **WHEN** `sem_signal(S1, 1)` is called
- **THEN** `sem_query(S1)` SHALL return 1

#### Scenario: Signal with equal value rejected

- **GIVEN** semaphore S1 has current value 5
- **WHEN** `sem_signal(S1, 5)` is called
- **THEN** the operation SHALL return `-EINVAL`
- **THEN** `sem_query(S1)` SHALL remain 5

#### Scenario: Signal with lower value rejected

- **GIVEN** semaphore S1 has current value 5
- **WHEN** `sem_signal(S1, 3)` is called
- **THEN** the operation SHALL return `-EINVAL`
- **THEN** `sem_query(S1)` SHALL remain 5

### Requirement: sem_wait registers waiter callback

The system SHALL provide `sem_wait(handle, expected_value, callback)` that registers a waiter callback. When the semaphore value reaches or exceeds `expected_value`, the callback SHALL be invoked. The implementation SHALL NOT block; waiters SHALL be stored in a FIFO queue and woken in registration order.

#### Scenario: Waiter callback fires when value condition met

- **GIVEN** semaphore S1 has current value 0
- **WHEN** `sem_wait(S1, 1, callback_fn)` is registered AND `sem_signal(S1, 1)` is called
- **THEN** `callback_fn` SHALL be invoked
- **THEN** callback SHALL be invoked exactly once

#### Scenario: FIFO waiter ordering

- **GIVEN** `sem_wait(S1, 2, callback_A)` and `sem_wait(S1, 2, callback_B)` are registered in sequence, and S1 has current value 0
- **WHEN** `sem_signal(S1, 2)` is called
- **THEN** `callback_A` SHALL be invoked before `callback_B`

#### Scenario: Multiple waiters for same semaphore

- **GIVEN** `sem_wait(S1, 1, callback_A)` and `sem_wait(S1, 2, callback_B)` are registered, S1 has current value 0
- **WHEN** `sem_signal(S1, 1)` is called
- **THEN** `callback_A` SHALL be invoked
- **THEN** `callback_B` SHALL NOT be invoked
- **WHEN** `sem_signal(S1, 2)` is called
- **THEN** `callback_B` SHALL be invoked

### Requirement: sem_query returns current value

The system SHALL provide `sem_query(handle)` that returns the current semaphore value with acquire semantics (observing all prior signals).

#### Scenario: Query after signal

- **GIVEN** semaphore S1 has current value 0
- **WHEN** `sem_signal(S1, 3)` is called, then `sem_query(S1)`
- **THEN** `sem_query(S1)` SHALL return 3

#### Scenario: Query invalid handle

- **GIVEN** handle H1 is invalid (never created, or destroyed)
- **WHEN** `sem_query(H1)` is called
- **THEN** the operation SHALL return `-EINVAL`

### Requirement: sem_destroy cleans up semaphore

The system SHALL provide `sem_destroy(handle)` that destroys the semaphore. If there are registered waiters, they SHALL be woken up with an error status. Channel destruction SHALL clean up all associated semaphores.

#### Scenario: Destroy with registered waiters

- **GIVEN** semaphore S1 has 2 registered waiters
- **WHEN** `sem_destroy(S1)` is called
- **THEN** both waiters SHALL be woken up with error status
- **THEN** the operation SHALL return the waiter error code

#### Scenario: Double destroy returns error

- **GIVEN** semaphore S1 has been destroyed
- **WHEN** `sem_destroy(S1)` is called again
- **THEN** the operation SHALL return `-EINVAL`

### Requirement: gpfifo_entry.timeline field

The `gpfifo_entry` structure SHALL contain a `timeline` field with `handle`, `signal_value`, and `wait_value` sub-fields. When a batch completes, the Puller SHALL automatically call `sem_signal(handle, signal_value)`. Before dispatching, if `wait_value > 0`, the Puller SHALL register a waiter and suspend until the condition is met.

#### Scenario: Batch completion triggers sem_signal

- **GIVEN** gpfifo_entry has `timeline={handle=S1, signal_value=1, wait_value=0}`
- **WHEN** the batch completes successfully
- **THEN** `sem_signal(S1, 1)` SHALL be called automatically

#### Scenario: Batch suspended on wait_value

- **GIVEN** semaphore S1 has current value 0, and gpfifo_entry has `timeline={handle=S1, signal_value=0, wait_value=1}`
- **WHEN** the Puller reaches DISPATCH stage
- **THEN** the batch SHALL be suspended until S1 reaches >= 1

### Requirement: fence_create/fence_read as semaphore wrappers

`fence_create()` SHALL be a thin wrapper that calls `sem_create(0)`. `fence_read()` SHALL be a thin wrapper that calls `sem_query() > 0`. Fence signal SHALL be triggered by Puller completion callback calling `sem_signal(fence_sem, 1)`.

#### Scenario: fence_create creates semaphore

- **GIVEN** no fence F1 exists
- **WHEN** `fence_create(&F1)` is called
- **THEN** F1 internally SHALL have a semaphore with handle H1 and initial value 0
- **THEN** `sem_query(H1)` SHALL return 0

#### Scenario: fence_read checks completion

- **GIVEN** fence F1 has associated semaphore with value 0
- **WHEN** `fence_read(F1)` is called
- **THEN** it SHALL return false (not completed)
- **WHEN** `sem_signal(sem_of_F1, 1)` is called
- **THEN** `fence_read(F1)` SHALL return true (completed)
