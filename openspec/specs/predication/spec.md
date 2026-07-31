# predication Specification

## Purpose
TBD - created by archiving change stage4-5-cp-phase6-predication-aql. Update Purpose after archive.
## Requirements
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

