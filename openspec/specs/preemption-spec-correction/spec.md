# preemption-spec-correction Specification

## Purpose
TBD - created by archiving change stage4-5-cp-phase6-preemption-timeline-sem-gaps. Update Purpose after archive.
## Requirements
### Requirement: Preemption Saved State Field Constraints (Addendum)

The system SHALL NOT include `jump_stack_` in the saved MQD state during preemption, because `jump_stack_` is guaranteed empty at every valid preempt trigger point (boundary check + jump_stack empty pre-condition).

> **Reference**: This requirement adds a constraint on the field set of saved state that the canonical spec
> does not enumerate. The canonical spec lists the trigger conditions; this addendum specifies the saved-state
> field shape.

#### Scenario: Saved state includes expected fields
- **WHEN** `mqd_state_preempt(channel_id, *saved)` is called at a valid preempt point
- **THEN** saved state includes: `saved_gpfifo_addr`, `saved_index`, `saved_entries`, `ChannelSemaphoreState`
- **AND** these fields are byte-equivalent to the corresponding live fields at the moment of save

#### Scenario: Saved state excludes jump_stack
- **WHEN** `mqd_state_preempt(channel_id, *saved)` is called at a valid preempt point
- **THEN** saved state does NOT include `jump_stack_` (guaranteed empty at the trigger point by pre-condition)

### Requirement: Preemption Resume Trigger Conditions (Addendum)

The system SHALL trigger a pending preemption on the next `tick()` after `jump_stack_` becomes empty, if a HIGH-priority arrival was deferred while the IB chain was active.

> **Reference**: This requirement specifies the resume trigger that the canonical spec alludes to but does not
> formalize. It is the implementation contract for the "deferred until IB chain completes" guarantee.

#### Scenario: jump_stack pop allows subsequent preempt
- **WHEN** `set_jump_stack(channel_id, false)` is called (IB nested mode exit)
- **AND** `preempt_pending_` is still set from a prior HIGH-priority arrival
- **THEN** the next `tick()` checks `preempt_pending_` AND `jump_stack_` (now empty) — both true
- **AND** `mqd_state_preempt()` is invoked
- **AND** saved state is valid (since `jump_stack_` is now empty)

#### Scenario: Multiple deferred preempts coalesce
- **WHEN** multiple HIGH-priority arrivals occur while `jump_stack_` is non-empty
- **THEN** `preempt_pending_` remains set (boolean, not counter)
- **AND** the next `tick()` after `jump_stack_` empties triggers exactly one preempt

### Requirement: Defer Guard Mechanism (Addendum — Internal)

The preempt checkpoint at `tick()` SHALL implement the defer guard as an in-line check, not as a separate state machine state.

> **Reference**: This is an implementation hint, not a behavioral contract. Documented here so future maintainers
> understand why the FSM does not gain a new "DEFERRED" state.

#### Scenario: tick() in-line defer check
- **WHEN** `tick()` is called on the preempted channel
- **THEN** the function checks `jump_stack_active_` and `preempt_pending_` in a single conditional
- **AND** if both are true, `tick()` returns without invoking `mqd_state_preempt()`
- **AND** no state machine transition occurs (state remains UNCHANGED)

