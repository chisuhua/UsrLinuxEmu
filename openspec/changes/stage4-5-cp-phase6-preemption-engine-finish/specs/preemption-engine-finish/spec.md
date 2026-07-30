## ADDED Requirements

### Requirement: MQD state preempt transitions ACTIVE to PREEMPTED

The system SHALL transition a channel from ACTIVE to PREEMPTED state when `mqd_state_preempt()` is called, persisting `(gpfifo_addr, current_index, total_entries, pending_fence_id)` into the channel's `PreemptContext`.

#### Scenario: Preempt ACTIVE channel
- **WHEN** `mqd_state_preempt()` is called on a channel in ACTIVE state
- **THEN** the channel transitions to PREEMPTED and `PreemptContext` is populated with current gpfifo position

#### Scenario: Preempt IDLE channel is no-op
- **WHEN** `mqd_state_preempt()` is called on a channel in IDLE state
- **THEN** the call returns 0 and no state change occurs

#### Scenario: Double preempt is no-op
- **WHEN** `mqd_state_preempt()` is called on a channel already in PREEMPTED state
- **THEN** the call returns 0 and existing `PreemptContext` is preserved

### Requirement: MQD state resume transitions PREEMPTED to ACTIVE

The system SHALL transition a channel from PREEMPTED to ACTIVE state when `mqd_state_resume()` is called, restoring gpfifo position from `PreemptContext`.

#### Scenario: Resume PREEMPTED channel
- **WHEN** `mqd_state_resume()` is called on a channel in PREEMPTED state
- **THEN** the channel transitions to ACTIVE and Puller resumes execution from saved gpfifo position

#### Scenario: Resume non-PREEMPTED returns EINVAL
- **WHEN** `mqd_state_resume()` is called on a channel not in PREEMPTED state
- **THEN** the call returns -EINVAL

### Requirement: Fence not signaled during preempt-resume gap

The system SHALL NOT signal any fence associated with a preempted batch during the preempt→resume interval. Fence signal SHALL be bound to the resumed batch's completion.

#### Scenario: Preempted fence remains pending
- **WHEN** a batch is preempted mid-execution and its fence is queried
- **THEN** the fence is not signaled

#### Scenario: Fence signals on resumed batch completion
- **WHEN** the preempted batch is resumed and completes
- **THEN** the associated fence is signaled

### Requirement: Puller FSM preempt checkpoint invokes MQD state

The Puller FSM preempt checkpoint at batch boundary SHALL invoke `mqd_state_preempt()` on the preempted channel and `mqd_state_resume()` on the resumed channel.

#### Scenario: Batch boundary preempt checkpoint
- **WHEN** a HIGH priority batch is submitted and the current channel is mid-batch
- **THEN** the Puller FSM at the next batch boundary invokes `mqd_state_preempt()` on the current channel and switches execution to the HIGH priority channel

#### Scenario: Preempted channel resumes after high priority completes
- **WHEN** the HIGH priority batch completes
- **THEN** the Puller FSM invokes `mqd_state_resume()` on the previously preempted channel and resumes its execution

### Requirement: IB jump_stack preserved across preempt-resume

The system SHALL preserve the Puller FSM jump_stack across preempt→resume transitions so that chained Indirect Buffer jumps resume correctly.

#### Scenario: Chained IB jump survives preempt
- **WHEN** a channel executing chained IB jumps is preempted
- **THEN** upon resume, the chained jump continues from the saved jump_stack position