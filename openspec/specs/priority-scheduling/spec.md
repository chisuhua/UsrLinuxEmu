# priority-scheduling Specification

## Purpose

定义 GPU `GlobalScheduler` 的优先级调度能力。`ChannelState` 新增 `priority` 字段（5 级枚举：IDLE / LOW / NORMAL / HIGH / REALTIME），Runlist 出队时按优先级降序选择，配合 starvation 保护（每 10 个 dispatch 周期强制至少 dispatch 1 个最低优先级 entry）和 priority inheritance（防止优先级反转）。语义对齐 CUDA stream priority / AMD MES priority levels / NVIDIA HyperQueue TSG priority class。

> **来源**：ADR-045（Priority Scheduling in GlobalScheduler）
> **实现位置**：`plugins/gpu_driver/sim/scheduler/{channel_state,global_scheduler}.{h,cpp}` + `std::multiset` 重排 + `kStarvationThreshold=10`
> **关联**：ADR-021（Puller FSM）、ADR-044（HyperQueue）、ADR-054（MQD/HQD）
> **优先级**：CUDA stream priority / AMD MES（IDLE/NORMAL/FOCUS/REALTIME）/ NVIDIA HyperQueue TSG

## Requirements

### Requirement: Channel Priority Field

`ChannelState` SHALL expose a `priority` field of type `ChannelPriority` enum with values `IDLE=0`, `LOW=1`, `NORMAL=2`, `HIGH=3`, `REALTIME=4`. The default priority SHALL be `NORMAL`. Priority SHALL be set at queue creation time and SHALL be immutable during the queue's lifetime.

#### Scenario: Default priority is NORMAL

- **GIVEN** a queue is created without explicit priority
- **WHEN** the queue is opened
- **THEN** `ChannelState.priority` SHALL be `NORMAL`

#### Scenario: Explicit priority assignment

- **GIVEN** a queue is created with `priority=HIGH`
- **WHEN** the queue is opened
- **THEN** `ChannelState.priority` SHALL be `HIGH`

#### Scenario: Priority immutable at runtime

- **GIVEN** a queue with priority `NORMAL` is currently active
- **WHEN** code attempts to mutate `ChannelState.priority`
- **THEN** the assignment SHALL be rejected and `ChannelState.priority` SHALL remain `NORMAL`

### Requirement: Runlist Reordering by Priority

`GlobalScheduler::dispatch_next()` SHALL select entries in descending priority order (REALTIME → HIGH → NORMAL → LOW). Within the same priority level, entries SHALL be dispatched in FIFO order.

#### Scenario: Higher priority dispatched first

- **GIVEN** queue Q_HIGH (priority=HIGH) and Q_LOW (priority=LOW) both have pending entries
- **WHEN** `dispatch_next()` is called repeatedly
- **THEN** Q_HIGH entries SHALL be dispatched before Q_LOW entries

#### Scenario: Same priority FIFO

- **GIVEN** queues Q1 and Q2 both have priority `NORMAL`, and Q1 submitted an entry before Q2
- **WHEN** `dispatch_next()` is called
- **THEN** Q1's entry SHALL be dispatched before Q2's entry

### Requirement: Starvation Protection

When HIGH or REALTIME entries continue to exist, LOW and NORMAL entries SHALL NOT be indefinitely postponed. The scheduler SHALL maintain a starvation counter; every 10 dispatch cycles, the scheduler SHALL force at least one LOW priority entry to be dispatched and reset the counter.

#### Scenario: Forced LOW dispatch after threshold

- **GIVEN** a sustained stream of HIGH priority submissions
- **WHEN** 10 HIGH entries have been dispatched without intervening LOW
- **THEN** the 11th dispatch SHALL be a LOW entry (if any LOW is pending)
- **THEN** the starvation counter SHALL be reset after the forced LOW dispatch

#### Scenario: LOW still dispatched when no HIGH pending

- **GIVEN** only LOW entries are pending
- **WHEN** `dispatch_next()` is called
- **THEN** LOW entries SHALL be dispatched in FIFO order

### Requirement: Priority Inheritance

When a REALTIME entry is blocked on a semaphore WAIT, and that semaphore is signalled by a LOW entry, the LOW entry SHALL be temporarily boosted to HIGH priority to prevent priority inversion.

#### Scenario: LOW entry boosted to HIGH on REALTIME waiter

- **GIVEN** REALTIME entry E1 is pending on semaphore S1 (wait_value > 0)
- **AND** LOW entry E2 is the next signal source for S1
- **WHEN** E2 reaches its signal point
- **THEN** E2 SHALL be temporarily boosted to HIGH priority
- **THEN** E2 SHALL complete ahead of any other LOW entries pending

#### Scenario: Inheritance releases after signal

- **GIVEN** LOW entry E2 has been boosted to HIGH due to priority inheritance
- **WHEN** E2 signals S1 (which unblocks REALTIME entry E1)
- **THEN** the inheritance boost SHALL be released
- **THEN** E2 (if still active) SHALL revert to LOW priority

### Requirement: Priority Scheduling Verifiability

A standalone test SHALL verify that priority scheduling behaves correctly across the four priority levels, FIFO within same priority, starvation protection, and priority inheritance.

#### Scenario: Multi-priority test coverage

- **GIVEN** 3 queues at priorities HIGH, NORMAL, LOW
- **WHEN** entries are submitted to all 3 queues concurrently
- **THEN** `test_priority_sched_standalone` SHALL verify HIGH completes first, NORMAL second, LOW third (subject to starvation counter)

#### Scenario: Starvation test coverage

- **GIVEN** sustained HIGH priority submissions over many cycles
- **WHEN** the starvation threshold is reached
- **THEN** `test_priority_sched_standalone` SHALL verify LOW entries are NOT indefinitely postponed
