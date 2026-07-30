## ADDED Requirements

### Requirement: Preemption Trigger by Priority

The system SHALL automatically trigger preemption when a HIGH priority batch arrives while a LOW priority channel is actively executing.

#### Scenario: HIGH priority batch arrives during LOW execution
- **WHEN** channel A (priority=LOW) is executing a batch AND channel C (priority=HIGH) submits a new batch
- **THEN** channel A's `preempt_pending_` flag is set immediately (trigger), and context save executes only at the current batch's completion boundary (effect)

#### Scenario: Preemption ignored during jump_stack state
- **WHEN** channel A is in IB nested state (jump_stack_active_ == true) AND `preempt_pending_` is set
- **THEN** preemption is deferred; save executes after jump_stack pops

### Requirement: Preemption Effect Only at Boundary

The system SHALL execute context save only at the entry/batch boundary (FETCH pre-check or DISPATCH post-check), NEVER mid-entry.

#### Scenario: Save at FETCH pre-check
- **WHEN** `HardwarePullerEmu::tick()` is called AND `preempt_pending_[current_channel]` is set AND state is at FETCH phase AND jump_stack is inactive
- **THEN** `mqd_state_preempt()` is called; saved state includes `saved_gpfifo_addr`, `saved_index`, `saved_entries`, and `ChannelSemaphoreState`; channel state transitions ACTIVE → PREEMPTED

#### Scenario: Save at DISPATCH post-check
- **WHEN** an entry has just been dispatched AND `preempt_pending_` is set AND state is at DISPATCH post-check AND jump_stack is inactive
- **THEN** context save executes at next tick; preempt_pending_ is cleared

#### Scenario: Mid-entry preemption forbidden
- **WHEN** state is mid-entry (between FETCH and DISPATCH) AND `preempt_pending_` is set
- **THEN** save does NOT execute; save is deferred to next boundary

### Requirement: Preemption Idempotence (Negative Path)

The system SHALL be idempotent: re-preempt of IDLE or PREEMPTED state SHALL return 0 (no-op) without side effects.

#### Scenario: IDLE state preempt
- **WHEN** `preempt_channel(channel_id)` is called AND channel has no ACTIVE batch (IDLE state)
- **THEN** return 0 (no-op); no state transition; no error

#### Scenario: PREEMPTED state preempt
- **WHEN** `preempt_channel(channel_id)` is called AND channel is already in PREEMPTED state
- **THEN** return 0 (no-op); preempt_pending_ is unchanged (already set or not relevant for PREEMPTED state)

### Requirement: MQD State Save/Restore Round-Trip

The system SHALL save and restore all channel execution state on preempt/resume, producing bit-identical execution results vs. non-preempted control.

#### Scenario: saved_gpfifo_addr/index/entries preserved
- **WHEN** `mqd_state_preempt(channel_id, &saved)` is called
- **THEN** `saved.saved_gpfifo_addr`, `saved.saved_index`, `saved.saved_entries` reflect the current execution position

#### Scenario: Resume restores saved position
- **WHEN** `mqd_state_resume(channel_id, &saved)` is called after preempt
- **THEN** channel state transitions PREEMPTED → ACTIVE; puller resumes from `saved.saved_gpfifo_addr[saved.saved_index]`

#### Scenario: Non-PREEMPTED resume returns error
- **WHEN** `mqd_state_resume` is called on a channel NOT in PREEMPTED state
- **THEN** return -EINVAL

#### Scenario: Reentrant preempt-resume cycle
- **WHEN** preempt → resume → preempt → resume is performed on same channel
- **THEN** second preempt saves state equivalent to first; second resume restores to second saved state; no leaks

### Requirement: ChannelSemaphoreState Survives Preempt

The system SHALL save and restore `ChannelSemaphoreState` (attached semaphores + pending waiter registrations) as part of `mqd_state_preempt/resume`.

#### Scenario: Attached semaphores preserved
- **WHEN** channel A has `attached_sems = {S1, S2}` AND preempt is triggered
- **THEN** `ChannelSemaphoreState.attached = {S1, S2}` is saved; on resume, attached set is restored

#### Scenario: Pending waiter registrations preserved
- **WHEN** channel A has entry in SEM_WAIT suspended state (registered with sem S1, callback cb)
- **THEN** on preempt: `ChannelSemaphoreState.pending_waits` records the waiter registration
- **AND** on resume: pending_waits is re-registered with sem S1 (callback fires when S1 signals)

### Requirement: Fence Completion Bound to Resumed Batch (Critical Correctness)

The system SHALL bind fence completion signaling to the actual completion of the resumed batch, NOT to the preemption event.

#### Scenario: Pre-empted batch fence not signaled before resume
- **WHEN** channel A is preempted with fence_id=F1 in `pending_fences_`
- **AND** channel A is in PREEMPTED state (not yet resumed)
- **THEN** reading F1's value returns 0 (no signal); `pending_fences_[A][F1]` is still present

#### Scenario: Resume + batch completion signals fence
- **WHEN** channel A resumes AND batch completes successfully
- **THEN** `sem_signal(sem_handle_of_F1, signal_value_of_F1)` is called
- **AND** F1's value becomes `signal_value_of_F1`
- **AND** `pending_fences_[A][F1]` is unregistered (entry removed)

#### Scenario: Preemption + fence combination scenario
- **WHEN** channel A (LOW) executing fence F1 is preempted by channel C (HIGH)
- **AND** channel C submits batch waiting on F1
- **THEN** channel C's batch is suspended (waiter registered with F1's sem)
- **AND** after channel A resumes and batch completes, F1 signals
- **AND** channel C's batch resumes and executes

### Requirement: IB jump_stack Safety

The system SHALL guarantee IB chain integrity across preempt/resume: puller PC equals saved jump target, and entire IB chain produces bit-identical results vs. non-preempted control.

#### Scenario: jump_stack preempt deferral
- **WHEN** channel is in `jump_stack_active_ == true` state AND `preempt_pending_` is set
- **THEN** preempt does NOT execute; `preempt_pending_` remains set

#### Scenario: jump_stack pop allows preempt
- **WHEN** `set_jump_stack(channel_id, false)` is called (exit IB nested mode) AND `preempt_pending_` is set
- **THEN** next tick triggers preempt; context save captures current PC (jump target address)

#### Scenario: PC restoration after resume
- **WHEN** channel resumes after preempt-during-jump_stack
- **THEN** puller PC == saved jump target address; entire IB chain execution bit-identical to non-preempted control