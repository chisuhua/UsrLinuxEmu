# semaphore-barrier Specification

## Purpose

定义 GPU Puller FSM 的 hardware semaphore WAIT/RELEASE 与 barrier AND/OR 同步原语（per-entry 级别，非 timeline semaphore 跨引擎同步）。`SEM_WAIT` / `SEM_RELEASE` / `BARRIER_AND` / `BARRIER_OR` 四种 GPFIFO entry 类型扩展 Puller FSM 的 FETCH / COMPLETE 阶段。WAIT 模式下 entry 移入 pending 队列，Puller 主循环定期重检；RELEASE 完成后立即继续；Barrier 通过计数器/首达放行机制实现跨 stream 同步。

> **来源**：ADR-047（Hardware Semaphore & Barrier Model，扩展 ADR-021 §决策 3 简单 semaphore）
> **实现位置**：`plugins/gpu_driver/sim/hardware/hardware_puller_emu.{h,cpp}` + `ChannelState::pending_queue`
> **关联**：ADR-021（Puller FSM §决策 3）| ADR-040（Puller Fence Completion — completion token）| ADR-049（Timeline Semaphore — 跨引擎）
> **注**：本 spec 描述 entry-level semaphore/barrier，与 timeline-semaphore spec（ADR-049 跨引擎）是不同层次原语

## Requirements

### Requirement: Semaphore WAIT in Puller FETCH

When the Puller FSM FETCH stage encounters a `SEM_WAIT` type entry, the system SHALL block that entry until `mem_read(semaphore_va) >= semaphore_value`. The blocking SHALL be implemented by moving the entry to a pending queue; the Puller SHALL re-check pending queue conditions on every FETCH cycle.

#### Scenario: WAIT satisfied

- **GIVEN** semaphore VA=0x1000 currently holds value 5
- **WHEN** the Puller encounters a `SEM_WAIT` entry with `semaphore_va=0x1000`, `semaphore_value=3`
- **THEN** the entry SHALL proceed immediately (5 >= 3)

#### Scenario: WAIT blocked

- **GIVEN** semaphore VA=0x1000 currently holds value 2
- **WHEN** the Puller encounters a `SEM_WAIT` entry with `semaphore_va=0x1000`, `semaphore_value=5`
- **THEN** the entry SHALL be moved to `ChannelState::pending_queue`
- **THEN** the Puller SHALL continue dispatching other entries

#### Scenario: WAIT released by subsequent signal

- **GIVEN** a `SEM_WAIT` entry is in the pending queue waiting for VA=0x1000 >= 5
- **AND** semaphore VA=0x1000 currently holds value 3
- **WHEN** another entry executes `SEM_RELEASE` with VA=0x1000, value=5
- **THEN** on the next Puller FETCH cycle, the pending entry SHALL be re-evaluated
- **THEN** the condition (5 >= 5) SHALL be satisfied and the entry SHALL proceed

### Requirement: Semaphore RELEASE in Puller COMPLETE

In the Puller FSM COMPLETE stage, after batch completion, the system SHALL execute `mem_write(semaphore_va, semaphore_value)`. RELEASE SHALL NOT block; the Puller SHALL continue immediately after the write completes. If a batch contains both RELEASE and other entries, the RELEASE SHALL be the last entry executed in the batch.

#### Scenario: RELEASE writes semaphore value

- **GIVEN** semaphore VA=0x2000 currently holds value 0
- **WHEN** the Puller completes a batch containing `SEM_RELEASE` with VA=0x2000, value=7
- **THEN** `mem_write(0x2000, 7)` SHALL be executed
- **THEN** the Puller SHALL continue to the next entry without blocking

#### Scenario: RELEASE is the last entry in batch

- **GIVEN** a batch contains entries [NORMAL_OP_1, NORMAL_OP_2, SEM_RELEASE, NORMAL_OP_3]
- **WHEN** the Puller processes the batch
- **THEN** NORMAL_OP_1 SHALL execute first
- **THEN** NORMAL_OP_2 SHALL execute second
- **THEN** SEM_RELEASE SHALL execute third (immediately before NORMAL_OP_3)
- **THEN** NORMAL_OP_3 SHALL execute after the RELEASE write

### Requirement: Barrier AND Synchronization

A `BARRIER_AND` type entry SHALL wait until all specified streams reach the barrier point before allowing the entry to proceed. The barrier counter SHALL be incremented by each stream's signal and SHALL release all waiting entries when the counter reaches the expected count.

#### Scenario: Barrier AND releases after all streams

- **GIVEN** streams S1, S2, S3 all need to reach barrier B
- **WHEN** S1 signals barrier B, then S2 signals barrier B, then S3 signals barrier B
- **THEN** after the 3rd signal, all entries waiting at barrier B SHALL proceed

#### Scenario: Barrier AND partial signal

- **GIVEN** streams S1, S2, S3 all need to reach barrier B
- **WHEN** only S1 and S2 have signaled (S3 not yet signaled)
- **THEN** entries waiting at barrier B SHALL remain blocked

### Requirement: Barrier OR Synchronization

A `BARRIER_OR` type entry SHALL release as soon as ANY specified stream reaches the barrier point. The first stream signal SHALL immediately release the waiting entry; subsequent signals SHALL be ignored.

#### Scenario: Barrier OR releases on first signal

- **GIVEN** streams S1, S2, S3 are candidates for barrier B (OR semantics)
- **WHEN** S1 signals barrier B
- **THEN** entries waiting at barrier B SHALL proceed immediately
- **THEN** subsequent signals from S2 or S3 SHALL be ignored (no double-release)

### Requirement: Hardware Semaphore & Barrier Verifiability

A standalone test SHALL verify semaphore WAIT/RELEASE serialization, Barrier AND multi-stream synchronization, and Barrier OR first-signal semantics, including infinite-WAIT non-crash behavior.

#### Scenario: WAIT/RELEASE sequence test

- **GIVEN** two batches B1 and B2 are submitted to the same channel
- **AND** B1 ends with `SEM_RELEASE(VA=X, value=1)` and B2 starts with `SEM_WAIT(VA=X, value=1)`
- **WHEN** both batches are processed
- **THEN** `test_semaphore_barrier_standalone` SHALL verify B1 completes before B2 starts

#### Scenario: Barrier AND test

- **GIVEN** streams S1, S2, S3 each submit a batch ending with `BARRIER_AND(B)`
- **WHEN** all three batches complete
- **THEN** `test_semaphore_barrier_standalone` SHALL verify the barrier releases only after all 3 signals

#### Scenario: Barrier OR test

- **GIVEN** streams S1, S2 are candidates for `BARRIER_OR(B)` on entry E
- **WHEN** S1 signals before S2
- **THEN** `test_semaphore_barrier_standalone` SHALL verify E proceeds on S1's signal
- **THEN** S2's subsequent signal SHALL be a no-op

#### Scenario: Infinite WAIT non-crash

- **GIVEN** a `SEM_WAIT` entry references a semaphore VA that NEVER receives a signal
- **WHEN** the batch is processed
- **THEN** the entry SHALL remain in the pending queue (no crash)
- **THEN** the Puller SHALL continue processing other channels (no global stall)

#### Scenario: Pending entry does not block other channels

- **GIVEN** channel C1 has a `SEM_WAIT` entry pending indefinitely
- **AND** channel C2 has a normal batch
- **WHEN** the Puller dispatches entries
- **THEN** C2's batch SHALL proceed normally
