# adr-placeholder-cleanup Specification

## Purpose
TBD - created by archiving change cleanup-adr-placeholders. Update Purpose after archive.
## Requirements
### Requirement: ADR-022 v1 decision on operator-level emulation

ADR-022 (GPU Compute Unit Emulation) MUST be advanced from "🔄 提议中 — 占位骨架" to "✅ 已接受 (v1)" with a concrete decision documented in the `## 决策` section: **operator-level emulation via 4 predefined kernel templates** (`add_vec4`, `mul_vec4`, `memcpy_h2d_via_pull`, `noop`). The 5 open questions from v0 MUST be answered (粒度 = operator-level / ISA = N/A for v1 / register file = minimal / perf model = N/A for v1 / debug interface = stderr per-template).

#### Scenario: ADR-022 file status updated
- **WHEN** the change `cleanup-adr-placeholders` is applied
- **THEN** `docs/00_adr/adr-022-gpu-compute-unit-emulation.md` status line is `✅ 已接受 (v1)`
- **AND** the `## 决策` section contains the 4-template list with rationale
- **AND** the v0 "决策待定" wording is removed
- **AND** a `## 修订记录` entry `2026-06-17 v1: 填入 operator-level emulation 决策（change cleanup-adr-placeholders）` is appended

#### Scenario: v0 placeholder history preserved
- **WHEN** ADR-022 is advanced to v1
- **THEN** the v0 5 open questions are preserved as `## 讨论历史 (v0 占位)` appendix (not deleted)

### Requirement: ADR-031 v1 decision on TTM wrapper architecture

ADR-031 (TTM Migration Priority) MUST be advanced from "🔄 提议中 — 占位骨架" to "✅ 已接受 (v1)" with the decision: **TTM as a thin wrapper layer over `libgpu_core/gpu_buddy`**. Buddy allocator is the page pool; TTM adds BO metadata + placement strategy. Full TTM (swapout) is explicitly out of v1 scope.

#### Scenario: ADR-031 file status updated
- **WHEN** the change is applied
- **THEN** `docs/00_adr/adr-031-ttm-migration-priority.md` status line is `✅ 已接受 (v1)`
- **AND** the `## 决策` section describes the wrapper architecture and the 5 v0 open questions are answered
- **AND** a `## 修订记录` entry `2026-06-17 v1: 填入 TTM wrapper 决策（change cleanup-adr-placeholders）` is appended

#### Scenario: ADR-031 cross-references updated
- **WHEN** ADR-031 v1 references its dependency
- **THEN** it MUST reference `libgpu_core/gpu_buddy.h` (the actual page pool) and `include/linux_compat/drm/` (the future TTM header location) explicitly

### Requirement: ADR-025-030 explicit deferral with Phase 3 trigger

ADR-025, ADR-026, ADR-028, ADR-029, ADR-030 (the 6 generic placeholders) MUST each be converted from "🔄 提议中 — 占位骨架" to "⏸️ 显式 Deferred — 待 Phase 3 触发" with:
1. Status line updated
2. v0 "候选项 A/B/C/D" content moved to `## 讨论历史 (v0 占位)` appendix
3. New `## Phase 3 触发条件` section added with concrete trigger criteria (commit events, issue numbers, or first-use signals)

#### Scenario: All 6 generic placeholders updated
- **WHEN** the change is applied
- **THEN** all 6 files have:
  - Status: `⏸️ 显式 Deferred — 待 Phase 3 触发`
  - Original candidates preserved as appendix
  - Trigger criteria specific to each ADR's topic (network plugin first commit for 028, third-party .so for 025, etc.)

#### Scenario: Trigger criteria are concrete
- **WHEN** a reviewer reads any of the 6 deferred ADRs
- **THEN** they can identify a specific future event (commit hash, issue number, or test case) that would re-open the ADR for active decision

### Requirement: SSOT and PRD reference consistency

`docs/02_architecture/post-refactor-architecture.md` §3.3 (ADR 治理) and `docs/PRD.md` MUST be updated to reflect the new state:
- ADR-022 / ADR-031 references change from "提议中" to "已接受 v1"
- ADR-025-030 references change from "提议中" to "显式 Deferred"

#### Scenario: SSOT ADR governance section updated
- **WHEN** the change is applied
- **THEN** `post-refactor-architecture.md` §3.3 has a new entry explaining the 8-placeholders cleanup result
- **AND** `docs/00_adr/README.md` §"编号 gap 治理" adds a sentence noting the 025-030 deferral policy

#### Scenario: PRD references aligned
- **WHEN** the change is applied
- **THEN** every PRD.md reference to ADR-022 / 025-030 / 031 is checked and either:
  - Removed (if the reference implied a decision that doesn't exist)
  - Updated to "ADR-XXX ✅ 已接受" or "ADR-XXX ⏸️ Deferred" (per the new state)

### Requirement: No code changes

The change MUST be governance + documentation only. It MUST NOT introduce new source code files, modify any `.cpp` / `.h` files, or change CMake configuration.

#### Scenario: Diff scope verification
- **WHEN** the change is applied
- **THEN** `git diff --stat` shows changes limited to `docs/00_adr/adr-*.md` (8 files) and reference updates in `docs/02_architecture/`, `docs/PRD.md`, `docs/00_adr/README.md`
- **AND** no changes to `src/`, `include/`, `plugins/`, `libgpu_core/`, `tests/`, `tools/`, or `CMakeLists.txt`

### Requirement: Build and audit still pass after change

The change MUST NOT regress existing build / test / audit gates.

#### Scenario: CI gates preserved
- **WHEN** the change is applied
- **THEN** `make -j4` builds 100%
- **AND** `ctest` passes 34/34 (no test changes)
- **AND** `bash tools/docs-audit.sh --strict` passes 36/36 (no audit check changes)

