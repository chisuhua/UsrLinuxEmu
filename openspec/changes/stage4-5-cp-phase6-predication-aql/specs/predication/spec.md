## ADDED Requirements

### Requirement: Predicate register SET operation

The system SHALL support `GPU_OP_SET_PREDICATE` entry with SET operation that directly assigns the predicate value.

#### Scenario: SET_PREDICATE with SET operation
- **WHEN** a `GPU_OP_SET_PREDICATE` entry with operation=SET and value=0 is processed
- **THEN** the predicate register's value becomes 0 and `enabled` becomes false

#### Scenario: SET_PREDICATE with SET operation non-zero
- **WHEN** a `GPU_OP_SET_PREDICATE` entry with operation=SET and value=1 is processed
- **THEN** the predicate register's value becomes 1 and `enabled` becomes true

### Requirement: Predicate register compound operations

The system SHALL support `GPU_OP_SET_PREDICATE` entry with AND, OR, XOR compound operations.

#### Scenario: AND operation
- **WHEN** predicate is 1 and `GPU_OP_SET_PREDICATE` with AND and value=1 is processed
- **THEN** predicate remains 1

#### Scenario: OR operation
- **WHEN** predicate is 0 and `GPU_OP_SET_PREDICATE` with OR and value=1 is processed
- **THEN** predicate becomes 1

#### Scenario: XOR operation
- **WHEN** predicate is 1 and `GPU_OP_SET_PREDICATE` with XOR and value=1 is processed
- **THEN** predicate becomes 0

### Requirement: Puller DECODE predicate skip

The Puller FSM SHALL skip entry dispatch when the entry has a predicate flag and `predicate_.enabled == false`.

#### Scenario: Skip entry when predicate disabled
- **WHEN** predicate is 0 (disabled) and an entry with predicate flag is processed
- **THEN** the entry is skipped (not dispatched) and the next entry is FETCHed

#### Scenario: Dispatch entry when predicate enabled
- **WHEN** predicate is 1 (enabled) and an entry with predicate flag is processed
- **THEN** the entry is dispatched normally

### Requirement: Predicate state preserved across context switch

The system SHALL persist the predicate register state in `ChannelState` so that context switches (preempt/resume) preserve predicate state.

#### Scenario: Predicate state survives preempt
- **WHEN** a channel is preempted with predicate value=1
- **THEN** upon resume, the predicate register is restored to value=1

## ADDED Requirements (AQL Support)

### Requirement: gpu_gpfifo_entry format field

The `gpu_gpfifo_entry` SHALL include a `format` field (1 byte) indicating the encoding format: 0=UsrNative, 1=AQL, 2=PM4.

#### Scenario: Default format is UsrNative
- **WHEN** a `gpu_gpfifo_entry` is created without explicit format
- **THEN** format=0 (UsrNative) is used

#### Scenario: AQL format selection
- **WHEN** a `gpu_gpfifo_entry` is created with format=1
- **THEN** the translator dispatches to AQL packet parser

### Requirement: AQL packet parsing

The system SHALL parse AQL `hsa_kernel_dispatch_packet_t` (64 bytes) from `gpu_gpfifo_entry` and produce `LaunchParams` with: kernel_addr (from kernel_object), kernargs (from kernarg_address), grid/block dimensions.

#### Scenario: AQL packet parsed into LaunchParams
- **WHEN** a `gpu_gpfifo_entry` with format=AQL is processed by `GpfifoToLaunchParamsTranslator`
- **THEN** LaunchParams is populated with kernel_addr, kernargs, grid_x, block_x, etc. from the AQL packet fields

### Requirement: AQL completion_signal bridges to Timeline Semaphore

The system SHALL map AQL `completion_signal` field to a Timeline Semaphore (per ADR-049) and signal it on batch completion.

#### Scenario: AQL batch completion signals semaphore
- **WHEN** an AQL batch completes and its completion_signal handle is set
- **THEN** the corresponding Timeline Semaphore is signaled

#### Scenario: AQL completion_signal none is no-op
- **WHEN** an AQL batch completes and its completion_signal handle is zero/none
- **THEN** no semaphore is signaled

### Requirement: PM4 format returns ENOSYS

The system SHALL return -ENOSYS when encountering a `gpu_gpfifo_entry` with format=2 (PM4).

#### Scenario: PM4 entry rejected
- **WHEN** a `gpu_gpfifo_entry` with format=2 is processed
- **THEN** the system returns -ENOSYS