## ADDED Requirements

### Requirement: Preemption trigger on HIGH priority arrival

When a HIGH priority batch arrives while a LOW priority channel is actively executing, the scheduler SHALL set a pending preemption flag (`pending_preempt_`). The actual context save SHALL NOT occur until the current batch boundary (DISPATCH complete or FETCH next entry).

#### Scenario: HIGH priority arrival triggers preemption flag

- **GIVEN** channel A (priority=LOW) is executing batch X
- **WHEN** channel C (priority=HIGH) submits batch Y
- **THEN** scheduler SHALL set `pending_preempt_` flag immediately
- **THEN** context save SHALL NOT occur before batch X completes its current entry

#### Scenario: Preemption trigger does not affect IDLE channels

- **GIVEN** channel A is in IDLE state (no ACTIVE batch)
- **WHEN** preemption is triggered
- **THEN** the operation SHALL return 0 (no-op) with no side effects

### Requirement: Context save on batch boundary

At the batch boundary (after DISPATCH completes or before FETCH of the next entry), the Puller SHALL check the preemption flag. If set, it SHALL call `mqd_state_preempt()` to save the current channel's context, then switch to the HIGH priority channel.

#### Scenario: Context save saves gpfifo_addr/index/entries

- **GIVEN** channel A is ACTIVE with `gpfifo_addr=0x1000`, `gpfifo_index=5`, `entries=42`
- **WHEN** `mqd_state_preempt()` is called
- **THEN** saved context SHALL contain `saved_gpfifo_addr=0x1000`, `saved_gpfifo_index=5`, `saved_entries=42`
- **THEN** channel A state SHALL transition from `ACTIVE` to `PREEMPTED`

#### Scenario: PREEMPTED state disallows second preempt

- **GIVEN** channel A is in `PREEMPTED` state
- **WHEN** preempt is triggered again
- **THEN** the operation SHALL return 0 (no-op) with no side effects

### Requirement: Context resume restores execution

When a PREEMPTED channel regains the scheduler, `mqd_state_resume()` SHALL restore its saved context and transition state back to ACTIVE. The puller PC SHALL resume from the saved gpfifo position.

#### Scenario: Resume restores execution from saved position

- **GIVEN** channel A is PREEMPTED with `saved_gpfifo_index=5`
- **WHEN** `mqd_state_resume()` is called
- **THEN** channel A state SHALL transition from `PREEMPTED` to `ACTIVE`
- **THEN** puller SHALL resume fetching from gpfifo entry 5

#### Scenario: Resume on non-PREEMPTED state returns error

- **GIVEN** channel A is in `ACTIVE` state (not PREEMPTED)
- **WHEN** `mqd_state_resume()` is called
- **THEN** the operation SHALL return `-EINVAL`

### Requirement: Preemption fence semantics

When a channel is preempted, the fence associated with the preempted batch SHALL NOT signal until the batch actually completes after resume. The fence value after completion SHALL match the resumed batch's submitted value.

#### Scenario: Fence not signaled during preemption

- **GIVEN** channel A is preempted with pending fence F1
- **WHEN** F1 is read between preemption and resume
- **THEN** F1 SHALL NOT be signaled (sem_query returns 0 or not > 0)

#### Scenario: Fence correctly signaled after resume completion

- **GIVEN** channel A was preempted, resumed, and its batch has completed
- **WHEN** the fence is read
- **THEN** fence value SHALL equal the resumed batch's submitted value
- **THEN** the per-channel pending fence table SHALL have F1 correctly cleaned up

### Requirement: IB jump_stack safety under preemption

Preemption SHALL NOT occur when the puller is in an IB (Indirect Buffer) jump stack (`jump_stack_` non-empty). Resume after preemption SHALL restore the jump stack and resume IB chain execution correctly.

#### Scenario: Preemption blocked during IB execution

- **GIVEN** the puller is executing an IB chain with `jump_stack_` non-empty
- **WHEN** a HIGH priority batch triggers preemption
- **THEN** preemption SHALL be deferred until IB chain completes

#### Scenario: IB chain resume produces identical results

- **GIVEN** an IB chain is preempted after returning to main batch
- **WHEN** the channel resumes
- **THEN** puller PC SHALL equal the saved jump target address
- **THEN** the entire IB chain execution result SHALL be byte-identical to a non-preempted control run
