## ADDED Requirements

### Requirement: save_context() API

The system SHALL provide `mqd_state_preempt()` API that saves the current channel's MQD/HQD state to a preemption context structure. Saved state SHALL include: gpfifo_addr, gpfifo_index, entries count, puller PC, jump_stack state, ChannelSemaphoreState.

#### Scenario: Complete context save

- **GIVEN** channel A is ACTIVE with non-trivial puller state
- **WHEN** `mqd_state_preempt()` is called
- **THEN** saved context SHALL capture gpfifo_addr, gpfifo_index, entries count, puller PC, jump_stack, semaphore state
- **THEN** channel A state SHALL transition to PREEMPTED

#### Scenario: IDLE channel save returns no-op

- **GIVEN** channel A is in IDLE state
- **WHEN** `mqd_state_preempt()` is called
- **THEN** the operation SHALL return 0 with no side effects

### Requirement: restore_context() API

The system SHALL provide `mqd_state_resume()` API that restores a previously saved MQD/HQD state to its channel. After restore, the puller SHALL continue execution from the saved position.

#### Scenario: Complete context restore

- **GIVEN** channel A is PREEMPTED with saved context C
- **WHEN** `mqd_state_resume()` is called
- **THEN** channel A state SHALL transition to ACTIVE
- **THEN** puller PC SHALL be set to saved context C's gpfifo position
- **THEN** gpfifo_addr, gpfifo_index, entries SHALL match saved context C

#### Scenario: Resume ACTIVE channel returns error

- **GIVEN** channel A is ACTIVE (not PREEMPTED)
- **WHEN** `mqd_state_resume()` is called
- **THEN** the operation SHALL return `-EINVAL`

### Requirement: ChannelSemaphoreState saved with context

When a channel has pending semaphore waits (SEM_WAIT suspended state), the `ChannelSemaphoreState` SHALL be saved and restored as part of the preemption context. After resume, semaphore wait state SHALL be consistent.

#### Scenario: SEM_WAIT state survives preempt/resume cycle

- **GIVEN** channel A has an entry waiting on semaphore S1 (SEM_WAIT state)
- **WHEN** channel A is preempted and then resumed
- **THEN** the ChannelSemaphoreState SHALL be identical before and after the preempt/resume cycle
- **THEN** when S1 reaches the required value, the waiter SHALL be correctly triggered
