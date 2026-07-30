## ADDED Requirements

### Requirement: Preemption Deferred During IB Nested Execution (Canonical)

The system SHALL defer preemption when the channel is in IB nested execution state. The `jump_stack_` field is NOT included in saved MQD state because it is guaranteed empty at any preempt trigger point.

> **Reference**: This is the canonical semantics. The archived spec
> (`openspec/changes/archive/2026-07-29-stage4-5-cp-phase6-preemption-timeline-sem/specs/preemption-engine/spec.md`)
> describes an older "save/restore jump_stack" model that was not implemented.
> See `IMPLEMENTATION_NOTES.md` §"已知 spec/implementation 不一致" for context.
> This spec is the authoritative source going forward.

#### Scenario: Preemption check skips during IB nested
- **WHEN** `preempt_pending_[channel_id]` is set
- **AND** the channel's `jump_stack_` is non-empty at the preempt check point
- **THEN** preemption is deferred (no `mqd_state_preempt()` call)
- **AND** `preempt_pending_` remains set for the next check

#### Scenario: Saved state excludes jump_stack
- **WHEN** `mqd_state_preempt(channel_id, *saved)` is called at a valid preempt point (boundary check + jump_stack empty)
- **THEN** saved state includes: `saved_gpfifo_addr`, `saved_index`, `saved_entries`, `ChannelSemaphoreState`
- **AND** saved state does NOT include `jump_stack_` (guaranteed empty at this point)

#### Scenario: jump_stack pop allows subsequent preempt
- **WHEN** `set_jump_stack(channel_id, false)` is called (IB nested mode exit)
- **AND** `preempt_pending_` is still set from a prior HIGH-priority arrival
- **THEN** the next tick's preempt check triggers `mqd_state_preempt()`
- **AND** saved state is valid (since `jump_stack_` is now empty)

#### Scenario: PC restoration correctness
- **WHEN** channel resumes after preempt that was deferred during jump_stack
- **THEN** puller PC equals the saved jump target address
- **AND** the entire IB chain executes with bit-identical results vs non-preempted control

### Requirement: Documentation Links to Canonical Spec

The architecture and roadmap documentation SHALL link to this canonical spec, not the archived older spec.

#### Scenario: post-refactor-architecture.md has link
- **WHEN** `docs/02_architecture/post-refactor-architecture.md` §"Stage 4.5 GPU Compute Pipeline" is inspected
- **THEN** it contains a link to `openspec/changes/stage4-5-cp-phase6-preemption-timeline-sem-gaps/specs/preemption-engine/spec.md`
- **AND** a note explaining that the archived spec is superseded by this canonical spec

#### Scenario: roadmap.md has link
- **WHEN** `roadmap.md` Stage 4.5 section is inspected
- **THEN** it contains a link to this canonical spec
- **AND** no link to the archived spec (to avoid confusion)

### Requirement: Archive Spec Untouched

The archived spec.md SHALL NOT be modified by this change.

#### Scenario: Archive spec.md unchanged
- **WHEN** `git diff openspec/changes/archive/2026-07-29-stage4-5-cp-phase6-preemption-timeline-sem/specs/preemption-engine/spec.md` is run
- **THEN** the diff is empty

#### Scenario: Archive IMPLEMENTATION_NOTES.md unchanged
- **WHEN** `git diff openspec/changes/archive/2026-07-29-stage4-5-cp-phase6-preemption-timeline-sem/IMPLEMENTATION_NOTES.md` is run
- **THEN** the diff is empty (notes preserved as historical record)