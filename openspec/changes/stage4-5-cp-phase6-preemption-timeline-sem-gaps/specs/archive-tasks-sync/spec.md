## ADDED Requirements

### Requirement: Archive Tasks.md Checkbox State Synchronized

The archived `tasks.md` SHALL reflect the actual implementation state via checkbox accuracy. Six tasks currently marked `[ ]` but actually completed per commit history SHALL be updated to `[x]`.

#### Scenario: Task 2.4 checkbox synced
- **WHEN** `archive/2026-07-29-stage4-5-cp-phase6-preemption-timeline-sem/tasks.md` task 2.4 is inspected
- **THEN** checkbox shows `- [x] 2.4 Wire mqd_state_preempt() into preemption flow` (was `[ ]`)
- **AND** git blame shows commit implementing this task is `d1f569b`

#### Scenario: Task 2.5 checkbox synced
- **WHEN** archived tasks.md task 2.5 is inspected
- **THEN** checkbox shows `- [x] 2.5 Wire mqd_state_resume() into resume flow` (was `[ ]`)
- **AND** implementing commit is `de620b5`

#### Scenario: Task 2.6 checkbox synced
- **WHEN** archived tasks.md task 2.6 is inspected
- **THEN** checkbox shows `- [x] 2.6 Handle preempt on IDLE channel (no-op), double-preempt on PREEMPTED (no-op), resume on non-PREEMPTED (-EINVAL)` (was `[ ]`)
- **AND** implementing commit is `d9728e8`

#### Scenario: Task 2.7 checkbox synced
- **WHEN** archived tasks.md task 2.7 is inspected
- **THEN** checkbox shows `- [x] 2.7 Implement per-channel pending fence table` (was `[ ]`)
- **AND** implementing commit is `91b1fbf`

#### Scenario: Task 2.8 checkbox synced
- **WHEN** archived tasks.md task 2.8 is inspected
- **THEN** checkbox shows `- [x] 2.8 Ensure fence NOT signaled during preempt→resume gap` (was `[ ]`)
- **AND** implementing commit is `d1f569b` (same commit as 2.4, includes fence freeze)

#### Scenario: Task 2.9 checkbox synced
- **WHEN** archived tasks.md task 2.9 is inspected
- **THEN** checkbox shows `- [x] 2.9 Write test_preemption_standalone — all state transitions, fence semantics, IB jump_stack safety` (was `[ ]`)
- **AND** implementing commit is `cbe5bf7`
- **AND** current test status is PASS (477 assertions in 17 cases)

### Requirement: Archive Spec.md Untouched

The archived `spec.md` files SHALL NOT be modified by this checkbox sync.

#### Scenario: Spec diff is empty
- **WHEN** `git diff archive/2026-07-29-stage4-5-cp-phase6-preemption-timeline-sem/specs/preemption-engine/spec.md` is run
- **THEN** the diff is empty

#### Scenario: Spec diff is empty (mqd-hqd)
- **WHEN** `git diff archive/2026-07-29-stage4-5-cp-phase6-preemption-timeline-sem/specs/mqd-hqd-state-ops/spec.md` is run
- **THEN** the diff is empty

### Requirement: Archive IMPLEMENTATION_NOTES.md Untouched

The archived `IMPLEMENTATION_NOTES.md` SHALL NOT be modified by this checkbox sync.

#### Scenario: Notes diff is empty
- **WHEN** `git diff archive/2026-07-29-stage4-5-cp-phase6-preemption-timeline-sem/IMPLEMENTATION_NOTES.md` is run
- **THEN** the diff is empty (notes preserved as historical record per archive policy)

### Requirement: Hygiene Policy Documented

The repository SHALL document an explicit policy that archive tasks.md checkboxes may be updated to reflect actual implementation state, separate from the "archive spec not modified" policy.

#### Scenario: Hygiene ADR or section exists
- **WHEN** `docs/00_adr/` or `docs/02_architecture/post-refactor-architecture.md` is inspected
- **THEN** a section exists stating "Archive tasks.md checkbox state may be updated to reflect actual implementation commits; this is distinct from the 'archive spec.md not modified' policy"
- **OR** a new ADR (e.g., ADR-058) is created documenting this hygiene policy