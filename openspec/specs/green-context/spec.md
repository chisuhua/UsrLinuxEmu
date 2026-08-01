# green-context Specification

## Purpose
TBD - created by archiving change stage4-6-cp-phase7-green-context-pdl. Update Purpose after archive.
## Requirements
### Requirement: MQD context_type field

The `MQD` structure (per ADR-054) SHALL include a `context_type` field of type `ContextType` enum with values `BROWN=0` (normal priority, not preemptable by other BROWN contexts) and `GREEN=1` (low priority, preemptable by BROWN contexts). The default value SHALL be `BROWN`. The field SHALL be set at queue creation time and SHALL be immutable during the queue's lifetime.

#### Scenario: Default context type is BROWN

- **GIVEN** a queue is created without explicit context_type
- **WHEN** the queue is opened
- **THEN** `MQD.context_type` SHALL be `BROWN`

#### Scenario: Explicit GREEN context creation

- **GIVEN** a queue is created with `context_type=GREEN`
- **WHEN** the queue is opened
- **THEN** `MQD.context_type` SHALL be `GREEN`
- **THEN** the queue's `ChannelPriority` SHALL be forced to `LOW`

#### Scenario: context_type immutable at runtime

- **GIVEN** a queue with context_type=BROWN is currently active
- **WHEN** code attempts to mutate `MQD.context_type`
- **THEN** the assignment SHALL be rejected and `MQD.context_type` SHALL remain `BROWN`

### Requirement: GREEN channel priority forced to LOW

When a queue is created with `context_type=GREEN`, the system SHALL force its `ChannelPriority` to `LOW` regardless of any priority parameter passed to the queue creation API. This enforces that all GREEN contexts are eligible to be preempted by higher-priority BROWN contexts.

#### Scenario: GREEN priority forced to LOW

- **GIVEN** a queue is created with `context_type=GREEN` and `priority=HIGH` (attempted override)
- **WHEN** the queue is opened
- **THEN** `ChannelState.priority` SHALL be `LOW` (HIGH override rejected)
- **THEN** any explicit `priority` parameter SHALL be ignored

#### Scenario: BROWN priority respected

- **GIVEN** a queue is created with `context_type=BROWN` and `priority=HIGH`
- **WHEN** the queue is opened
- **THEN** `ChannelState.priority` SHALL be `HIGH` (BROWN respects priority)

### Requirement: BROWN preempt GREEN

When a BROWN channel needs to be dispatched and the scheduler is busy executing a GREEN channel, the scheduler SHALL preempt the GREEN channel using the dispatch-level preemption flow (per ADR-046 `mqd_state_preempt`). The preempted GREEN channel's `PreemptContext` SHALL be saved, allowing resume after the BROWN channel completes.

#### Scenario: BROWN preempts running GREEN

- **GIVEN** GREEN channel G1 is currently executing a long batch
- **AND** BROWN channel B1 (priority=NORMAL) submits a new batch
- **WHEN** the scheduler detects B1 is pending
- **THEN** the scheduler SHALL call `mqd_state_preempt()` on G1
- **THEN** G1's `PreemptContext` SHALL be saved (gpfifo_addr, current_index, total_entries)
- **THEN** G1 SHALL transition to PREEMPTED state
- **THEN** B1 SHALL be dispatched

#### Scenario: GREEN resumes after BROWN completes

- **GIVEN** GREEN channel G1 was preempted by BROWN B1
- **WHEN** B1 completes its batch
- **THEN** the scheduler SHALL call `mqd_state_resume()` on G1
- **THEN** G1 SHALL transition from PREEMPTED to ACTIVE
- **THEN** G1's `PreemptContext` SHALL be used to restore gpfifo position
- **THEN** G1 SHALL continue executing from the saved position

### Requirement: GREEN channels do not preempt each other

When two GREEN channels are both ready to dispatch, the scheduler SHALL NOT preempt one GREEN channel to dispatch another. GREEN channels SHALL follow normal priority/FIFO ordering. This prevents GREEN contexts from interfering with each other while remaining preemptable by BROWN.

#### Scenario: GREEN channel yields normally to higher priority GREEN

- **GIVEN** GREEN channel G1 (priority=LOW) is executing
- **AND** GREEN channel G2 (priority=LOW) submits a new batch
- **WHEN** the scheduler processes G2's submission
- **THEN** G1 SHALL NOT be preempted (both are GREEN)
- **THEN** G2 SHALL wait until G1 naturally yields (batch completion)

#### Scenario: Multiple GREEN channels dispatched in priority order

- **GIVEN** 3 GREEN channels G1, G2, G3 with priorities LOW, LOW, LOW (all same)
- **WHEN** all 3 submit batches concurrently
- **THEN** the scheduler SHALL dispatch them in FIFO order (submission order)
- **THEN** no preemptions SHALL occur between GREEN channels

### Requirement: Green Context HAL operations

The HAL SHALL expose two function pointers for Green Context lifecycle: `hal_green_context_create` (allocates a new green context within a TSG) and `hal_green_context_destroy` (frees the context). Both operations SHALL respect the MQD state machine and SHALL be thread-safe.

#### Scenario: hal_green_context_create succeeds

- **GIVEN** no GREEN context exists in TSG T1
- **WHEN** `HAL.hal_green_context_create(ctx, tsg_id=T1, &out_handle)` is called
- **THEN** a valid handle SHALL be returned in `out_handle`
- **THEN** the context SHALL be associated with TSG T1

#### Scenario: hal_green_context_destroy succeeds

- **GIVEN** GREEN context G1 with handle H1 exists
- **WHEN** `HAL.hal_green_context_destroy(ctx, H1)` is called
- **THEN** the context SHALL be freed
- **THEN** subsequent operations on H1 SHALL return `-EINVAL`

#### Scenario: double destroy returns error

- **GIVEN** GREEN context G1 has been destroyed
- **WHEN** `HAL.hal_green_context_destroy(ctx, H1)` is called again
- **THEN** the operation SHALL return `-EINVAL`

### Requirement: Green Context Verifiability

A standalone test SHALL verify Green Context creation with forced LOW priority, BROWN preemption of GREEN, GREEN non-preemption of GREEN, and HAL operations correctness.

#### Scenario: Multi-scenario test coverage

- **GIVEN** mixed BROWN and GREEN channels
- **WHEN** various preempt/submit sequences are exercised
- **THEN** `test_green_context_standalone` SHALL verify:
  - GREEN creation forces priority to LOW
  - BROWN can preempt running GREEN
  - GREEN cannot preempt running GREEN
  - PreemptContext is correctly saved and restored across preempt/resume cycles
  - HAL operations succeed and return correct error codes for invalid inputs

