# indirect-buffer Specification

## Purpose

定义 GPU Indirect Buffer (IB) / Command Chaining 能力。GPFIFO entry 新增 `IB_JUMP` 类型，Puller 遇到 JUMP 时保存当前 fetch 状态（`saved_pc`）并切换到目标 GPU VA 继续 FETCH。支持 `continue_flag` 链式 IB（nested depth 上限 4）和 target VA 合法性校验。语义对齐 NVIDIA `JUMP` method / AMD IB packet / Intel `MI_BATCH_BUFFER_START`，适用于 graph 条件节点、ring buffer overflow、multi-part command buffer 等场景。

> **来源**：ADR-050（Indirect Buffer / Command Chaining）
> **实现位置**：`plugins/gpu_driver/sim/hardware/hardware_puller_emu.{h,cpp}` + `gpu_ib_ref` 结构 + `MAX_IB_NEST=4` 嵌套限制
> **关联**：ADR-021（Puller FSM）| ADR-042（Pushbuffer Method Encoding）
> **Scope 限制**：本 ADR 仅定义 IB reference 数据结构和 Puller JUMP 行为，不实现 CALL/RETURN（嵌套调用栈）

## Requirements

### Requirement: IB_JUMP GPFIFO Entry Type

The GPFIFO entry enum SHALL include `IB_JUMP` as a new entry type. An `IB_JUMP` entry SHALL contain `target_gpu_va` (the destination pushbuffer address) and `continue_flag` (whether to continue the original batch after the JUMP target completes).

#### Scenario: continue_flag false terminates batch

- **GIVEN** a batch contains entries [OP_1, IB_JUMP(target=VA_X, continue=false), OP_2]
- **WHEN** the Puller processes OP_1
- **THEN** the Puller SHALL execute OP_1 first
- **WHEN** the Puller encounters IB_JUMP
- **THEN** the Puller SHALL switch FETCH to VA_X
- **THEN** after VA_X's pushbuffer completes, the Puller SHALL NOT return to OP_2 (batch terminates)

#### Scenario: continue_flag true enables chained IB

- **GIVEN** a batch contains entries [OP_1, IB_JUMP(target=VA_X, continue=true), OP_2]
- **WHEN** the Puller encounters IB_JUMP with continue_flag=true
- **THEN** the Puller SHALL save current PC
- **THEN** the Puller SHALL switch FETCH to VA_X
- **THEN** after VA_X's pushbuffer completes, the Puller SHALL restore saved PC
- **THEN** the Puller SHALL continue with OP_2 (chained IB)

### Requirement: Puller JUMP Behavior

When the Puller FETCH stage encounters an `IB_JUMP` entry, the system SHALL save the current fetch state (`saved_pc`), switch the FETCH pointer to `target_gpu_va`, and continue FETCH from the target address. The jump target SHALL be a valid GPU VA that is already mapped in the current VA Space; invalid addresses SHALL return `-EFAULT`. Nested JUMP depth SHALL be limited to 4 levels (via `MAX_IB_NEST=4`); depth overflow SHALL return `-E2BIG`.

#### Scenario: Valid JUMP target succeeds

- **GIVEN** GPU VA X is mapped in the current VA Space
- **WHEN** the Puller encounters IB_JUMP with target_gpu_va=X
- **THEN** the Puller SHALL save current PC and switch FETCH to X
- **THEN** FETCH from X SHALL continue normally

#### Scenario: Invalid JUMP target returns EFAULT

- **GIVEN** GPU VA X is NOT mapped in the current VA Space (or invalid)
- **WHEN** the Puller encounters IB_JUMP with target_gpu_va=X
- **THEN** the JUMP SHALL be rejected
- **THEN** `-EFAULT` SHALL be returned
- **THEN** the current batch SHALL be aborted

#### Scenario: Nested JUMP depth limit

- **GIVEN** the IB nest counter is currently 4 (max)
- **WHEN** the Puller encounters another `IB_JUMP` with continue_flag=true
- **THEN** `-E2BIG` SHALL be returned
- **THEN** the JUMP SHALL be rejected (current batch continues without JUMP)

#### Scenario: Nested JUMP within limit

- **GIVEN** the IB nest counter is currently 0
- **WHEN** the Puller encounters 4 chained `IB_JUMP` entries with continue_flag=true
- **THEN** each JUMP SHALL increment the nest counter
- **THEN** the 4th JUMP SHALL be allowed (depth 4 = max)
- **THEN** after returning from the deepest chain, the counter SHALL decrement correctly

### Requirement: IB Reference Management

The `submitBatch` function SHALL accept an optional `ib_refs` field pointing to an array of `gpu_ib_ref` structures. Each `gpu_ib_ref` SHALL contain `gpu_va`, `size`, and `flags` (e.g., read-only flag). IB references SHALL be automatically released upon batch completion.

#### Scenario: submitBatch with ib_refs

- **GIVEN** `submitBatch` is called with `entries=10, ib_refs=[VA_X(size=4KB, flags=RO), VA_Y(size=8KB, flags=RW)]`
- **WHEN** the batch is submitted
- **THEN** both VA_X and VA_Y SHALL be tracked as IB references
- **THEN** the references SHALL remain valid for the duration of the batch

#### Scenario: IB refs auto-released on batch completion

- **GIVEN** a batch was submitted with 2 ib_refs
- **WHEN** the batch completes (either successfully or via abort)
- **THEN** both ib_refs SHALL be released
- **THEN** no memory leaks SHALL be reported (validated by ASan/Valgrind)

#### Scenario: ib_refs flags respected

- **GIVEN** an ib_ref has `flags=READ_ONLY`
- **WHEN** the Puller FETCH stage reads from the IB target VA
- **THEN** reads SHALL be allowed
- **WHEN** the batch attempts to write to the read-only IB target
- **THEN** the write SHALL be rejected with `-EACCES`

### Requirement: Indirect Buffer Verifiability

A standalone test SHALL verify single JUMP, chained JUMP, illegal target rejection, nest overflow rejection, and no memory leaks from IB reference allocation.

#### Scenario: Single JUMP test

- **GIVEN** a batch with a single IB_JUMP to a valid VA
- **WHEN** the batch is processed
- **THEN** `test_indirect_buffer_standalone` SHALL verify the target pushbuffer executes correctly

#### Scenario: Chained JUMP test

- **GIVEN** a batch with 2 chained IB_JUMP entries (continue_flag=true)
- **WHEN** the batch is processed
- **THEN** `test_indirect_buffer_standalone` SHALL verify both targets execute in order and the post-JUMP entry executes last

#### Scenario: Illegal target test

- **GIVEN** an IB_JUMP targets an unmapped VA
- **WHEN** the batch is processed
- **THEN** `test_indirect_buffer_standalone` SHALL verify `-EFAULT` is returned
- **THEN** the batch SHALL be aborted cleanly

#### Scenario: Nest overflow test

- **GIVEN** a batch with 5 chained IB_JUMP entries (exceeding MAX_IB_NEST=4)
- **WHEN** the batch is processed
- **THEN** `test_indirect_buffer_standalone` SHALL verify `-E2BIG` is returned at the 5th JUMP

#### Scenario: No memory leak from IB refs

- **GIVEN** a batch is submitted with 3 ib_refs
- **WHEN** the batch completes (success, abort, or timeout)
- **THEN** ASan/Valgrind SHALL report zero leaks from IB reference allocation
