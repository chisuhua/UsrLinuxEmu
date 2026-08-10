# roadmap-unified-index Specification

## Purpose
Define the Unified Index contract for UsrLinuxEmu roadmap documentation: roadmap.md provides 4 navigation segments (派生建议 / 跨仓评审中 ADRs / 在途 changes / 已归档 changes), each stage doc follows the 4-quadrant template, and 4 documentation inconsistencies are guarded by ci-docs-audit cross-ref checks (10.1-10.4).
## Requirements
### Requirement: roadmap.md Unified Index Structure

`roadmap.md` SHALL provide a Unified Index view of all architectural work, comprising 4 mandatory segments in addition to existing narrative sections:

#### Scenario: 派生建议 segment present

- **WHEN** a reader inspects `roadmap.md`
- **THEN** a 派生建议 segment SHALL be present listing candidate improvements based on ADR trigger conditions (e.g., "ADR-049 Phase 6+ trigger not met → multi-engine Puller improvements 待 trigger")

#### Scenario: 跨仓评审中 ADRs segment present

- **WHEN** a cross-repo consumer-side ADR is in PROPOSED-state (e.g., adr-076)
- **THEN** a 跨仓评审中 ADRs segment SHALL list it with status "待 owner + consumer 双评审" and a link to the canonical ADR file

#### Scenario: 在途 OpenSpec changes segment present

- **WHEN** `openspec/changes/` contains non-archived changes
- **THEN** a 在途 OpenSpec changes segment SHALL list them (pointing to `openspec/changes/INDEX.md`)

#### Scenario: 已归档 OpenSpec changes segment present

- **WHEN** `openspec/changes/archive/` exists
- **THEN** a 已归档 OpenSpec changes segment SHALL list the 5 most recent archived changes (pointing to `openspec/changes/archive/`)

### Requirement: stage-X 4-Quadrant Template

Each file `docs/roadmap/stage-{0,1,2,3,4,5}-*.md` SHALL follow a 4-quadrant template:

#### Scenario: All 4 quadrants present

- **WHEN** a reader inspects any `stage-X-*.md` file
- **THEN** the file SHALL contain 4 mandatory quadrants: 对应 ADRs / 活跃 improvements / 在途 changes / 已归档 changes
- **AND** a 触发条件 segment referencing the relevant ADR §<trigger section>

### Requirement: Inconsistency Fixes

The 4 pre-existing documentation inconsistencies (A: roadmap.md:152 stale reference, B: proposal-approved.md mis-marked stage-5 items, C: adr-076 §R3 mis-references, D: adr-076 status symbol) SHALL be corrected such that the 4 corresponding `docs-audit.sh --section cross-ref` checks pass.

#### Scenario: roadmap.md:152 REPLACE applied

- **WHEN** `roadmap.md` is read
- **THEN** line ~152 SHALL NOT contain the text "含 5 个新增 entry"
- **AND** SHALL contain a 3-link paragraph pointing to proposal-suggestions.md / proposal-approved.md / openspec/changes/INDEX.md

#### Scenario: adr-076 §R3 references corrected

- **WHEN** `adr-076` is read
- **THEN** it SHALL NOT contain "ADR-035 §R3 cross-repo" in any of the 5 original lines (5, 77, 359, 370, 475)
- **AND** the corrected lines SHALL reference "ADR-035 §R5.1" or "§R6.3" instead

#### Scenario: adr-076 status symbol corrected

- **WHEN** `adr-076` frontmatter is read
- **THEN** the status SHALL use "🔄 Proposed" (not "📋 PROPOSED")

#### Scenario: proposal-approved.md 5-column alignment

- **WHEN** `proposal-approved.md` is read
- **THEN** the table header SHALL have 5 columns (提案 / 优先级 / 来源 / 批准日期 / 批准人)
- **AND** all body rows SHALL also have 5 columns

