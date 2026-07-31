## ADDED Requirements

> **CANONICAL REFERENCE**: The canonical semantics for "Preemption deferred during IB execution" are defined in
> [`openspec/changes/archive/2026-07-30-stage4-5-cp-phase6-preemption-engine-finish/specs/preemption-engine-finish/spec.md`](../archive/2026-07-30-stage4-5-cp-phase6-preemption-engine-finish/specs/preemption-engine-finish/spec.md)
> §"Requirement: Preemption deferred during IB execution". This change's spec is an **ADDENDUM** to that canonical
> spec — it adds 3 supplementary scenarios that capture additional implementation details not present in the
> canonical spec. The canonical spec is the authoritative source; conflicts SHALL be resolved in favor of the
> canonical spec.
>
> **Why an addendum, not a rewrite**: The IMPLEMENTATION_NOTES.md "归档 spec 不修改" policy forbids modifying
> archived specs. The canonical spec lives in an archived change, so it cannot be amended directly. This addendum
> is the documented mechanism for adding clarifications without disturbing the archive.
>
> **Drift governance**: Any future change that introduces new preemption semantics MUST either:
> 1. Add a scenario to this addendum (preferred), OR
> 2. Create a new addendum change that explicitly states "supersedes" — never silently overwrite

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

## References

- **Canonical spec**: `openspec/changes/archive/2026-07-30-stage4-5-cp-phase6-preemption-engine-finish/specs/preemption-engine-finish/spec.md` §"Requirement: Preemption deferred during IB execution"
- **Historical context**: `archive/2026-07-29-stage4-5-cp-phase6-preemption-timeline-sem/IMPLEMENTATION_NOTES.md` §"已知 spec/implementation 不一致"
- **ADR basis**: [ADR-046](../docs/00_adr/adr-046-preemption-context-switch.md) (D2 事件驱动模型)
- **Documentation links TO this addendum** (not to canonical spec, to avoid confusion):
  - `docs/02_architecture/post-refactor-architecture.md` §"Stage 4.5 GPU Compute Pipeline"
  - `roadmap.md` Stage 4.5 section
