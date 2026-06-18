# ssot-deep-audit Specification

## Purpose

Deepen SSOT audit coverage to the 4 regions left uncovered by the v0.1.2 C audit (commit `9e5d5ea`): §1.2 hardware-simulation layer details, §1.7 test-framework "claimed vs actual" table, §1.8 authoritative-document gap, and Appendix A struct field definitions. Produces a v0.1.4 audit report at `docs/02_architecture/audit-reports/v0.1.4-audit.md` using 4 parallel explore agents and reports all deviations without opening follow-up changes.

## Requirements
### Requirement: §1.2 architecture diagram layer detail audit

The `ssot-deep-audit` capability MUST verify that all entities described in SSOT `post-refactor-architecture.md` §1.2 (architecture diagram) hardware simulation layer are real and accurately named. The audit MUST cover: `libgpu_core/` (file names `gpu_buddy.h` + `buddy.c` per v0.1.1 fix), `plugins/gpu_driver/sim/` subdirectory structure (scheduler/, hardware/, gpu_queue_emu), and any "shadow compile" references such as `sim/buddy_allocator.cpp` or `sim/fence_sim.cpp` (v0.1.1 noted them; audit MUST confirm whether they still exist or were removed).

#### Scenario: All §1.2 referenced files exist
- **WHEN** audit enumerates file paths named in §1.2 hardware simulation layer
- **THEN** every named file MUST be discoverable via `find` or `ls`
- **AND** filenames MUST match the SSOT description (case-sensitive)

#### Scenario: §1.2 subdirectory structure verified
- **WHEN** audit checks `plugins/gpu_driver/sim/` subdirectory contents
- **THEN** `scheduler/` and `hardware/` MUST exist
- **AND** `gpu_queue_emu.{h,cpp}` MUST be a sibling (not inside a subdirectory)

#### Scenario: Stale "shadow compile" references surfaced
- **WHEN** SSOT §1.2 mentions files outside the actual source tree (e.g. `sim/buddy_allocator.cpp`, `sim/fence_sim.cpp` with "(shadow 编译)" annotation)
- **THEN** the audit MUST report whether each file exists, was renamed, or was removed
- **AND** the audit MUST recommend either: keep reference with file existence proof, OR remove stale reference from §1.2

### Requirement: §1.7 test framework declaration audit

The `ssot-deep-audit` capability MUST verify that SSOT §1.7 "claimed vs actual" table (test framework declarations) is current. The audit MUST check at minimum: `README.md`, `AGENTS.md`, `.github/copilot-instructions.md`, `CONTRIBUTING.md`, `docs/01-quickstart/installation.md`, `docs/04-building/build_system.md`, `docs/04-building/testing_guide.md`, `docs/04-building/ci-cd.md`, `docs/03-development/adding-devices.md`, `docs/06-reference/glossary.md`, and `docs/00_adr/adr-010-gtest-migration.md` (11 sources per v0.1.2 §1.7 table). Each MUST be grepped for GTest / Catch2 / `libgtest-dev` / `find_package(GTest)`. The actual implementation MUST be confirmed via `tests/catch_amalgamated.{hpp,cpp}` presence.

#### Scenario: All §1.7 declaration sources checked
- **WHEN** audit enumerates the 11 declaration sources
- **THEN** each file's current test framework mention MUST be recorded (Catch2 / GTest / "未表态" / other)
- **AND** deviations from §1.7 table MUST be reported

#### Scenario: Actual implementation confirmed
- **WHEN** audit verifies test framework actually used by the project
- **THEN** `tests/catch_amalgamated.{hpp,cpp}` MUST exist (proves Catch2 vendored)
- **AND** no `find_package(GTest)` or `libgtest-dev` apt reference MUST exist in any CMakeLists.txt or CI workflow

#### Scenario: §1.7 status field is current
- **WHEN** audit checks SSOT §1.7 "ADR-010 status reflects proposed, not implemented" claim
- **THEN** the actual ADR-010 status MUST match (currently "🔄 提议中")
- **AND** if a v0.1.2/v0.1.3-era commit changed ADR-010 status, the §1.7 table MUST reflect that

### Requirement: §1.8 authoritative document gap audit

The `ssot-deep-audit` capability MUST verify whether the "authoritative document gap" identified in SSOT §1.8 (v0.1.2 stated "缺少一份'重构后的架构总览'文档") has been closed by the creation of `post-refactor-architecture.md` itself. The audit MUST also check whether any new authoritative doc gap has emerged since v0.1.2 (e.g. AGENTS.md may have become stale, or new orphan specs in `openspec/specs/` lack an `## Purpose` section).

#### Scenario: §1.8 original gap closed
- **WHEN** audit checks whether `post-refactor-architecture.md` exists and is widely cross-referenced
- **THEN** the file MUST exist
- **AND** at least 3 other docs MUST link to it (proves it functions as SSOT)
- **AND** if both conditions hold, §1.8 "gap" paragraph SHOULD be updated to "closed (v0.1.2 itself closed this gap)"

#### Scenario: AGENTS.md drift check
- **WHEN** audit compares AGENTS.md "唯一非正式权威架构说明" claim against current reality
- **THEN** the audit MUST report whether AGENTS.md has been updated since v0.1.2
- **AND** if it was updated, the SSOT description of AGENTS.md MUST be reconciled

#### Scenario: Orphan spec Purpose fields
- **WHEN** audit lists all specs in `openspec/specs/` (excluding the current change's spec)
- **THEN** every spec MUST have a non-`TBD` `## Purpose` section
- **AND** any spec with `TBD` Purpose MUST be flagged for the owner to complete

### Requirement: Appendix A struct field audit

The `ssot-deep-audit` capability MUST verify that all struct definitions referenced in SSOT Appendix A (13 structs across `plugins/gpu_driver/shared/gpu_ioctl.h` and `plugins/gpu_driver/shared/gpu_queue.h`) match the actual field declarations. The audit MUST check field names, types, and order for: `gpu_pushbuffer_args`, `gpu_mmu_event_cb_args`, `gpu_firmware_cb_args`, `gpu_alloc_bo_args`, `gpu_map_bo_args`, `gpu_wait_fence_args`, `gpu_va_space_args`, `gpu_register_gpu_args`, `gpu_queue_args`, `gpu_queue_map_ring_args`, `gpu_queue_info_args`, `gpu_device_info` (12 from gpu_ioctl.h; `gpu_ring_header` from gpu_queue.h). The v0.1.2 audit only verified macro numbers (0x01-0x43) — struct field audit is the next layer.

#### Scenario: All 12 struct fields verified
- **WHEN** audit compares each struct's actual field declarations against the description in SSOT §1.3 or Appendix A
- **THEN** every field name MUST match (case-sensitive)
- **AND** every field type MUST match (allowing `u32` ↔ `uint32_t` alias equivalence)
- **AND** field order deviations MUST be reported (does not necessarily mean bug, but worth noting)

#### Scenario: gpu_pushbuffer_args va_space_handle field
- **WHEN** audit checks `gpu_pushbuffer_args` after the H-1 change (commit `0272970`)
- **THEN** the struct MUST contain a `u64 va_space_handle` field
- **AND** the field MUST be appended at the end (preserving earlier field offsets)

#### Scenario: gpu_queue_args doorbell_pgoff field
- **WHEN** audit checks `gpu_queue_args` for the `doorbell_pgoff` field per SSOT §1.4
- **THEN** the field MUST exist with `u64` type
- **AND** its purpose comment MUST match SSOT §1.4 (mmap offset for doorbell)

### Requirement: Audit report format and location

The `ssot-deep-audit` capability MUST produce a markdown audit report at `docs/02_architecture/audit-reports/v0.1.4-audit.md` (creating the `audit-reports/` directory if absent). The report MUST contain: an executive summary table (deviation count by severity), one section per audit region (A1 §1.2, A2 §1.7, A3 §1.8, A4 Appendix A structs), each section using the unified deviation table format (| 偏差 | SSOT 描述 | 实际状态 | 严重度 | 证据 | 建议 |). The SSOT MUST gain a one-line reference `> 历史审计报告：docs/02_architecture/audit-reports/` near the top of `post-refactor-architecture.md`.

#### Scenario: Report file created
- **WHEN** audit completes
- **THEN** `docs/02_architecture/audit-reports/v0.1.4-audit.md` MUST exist
- **AND** it MUST contain a top-level summary table
- **AND** it MUST contain 4 region sections (A1-A4)

#### Scenario: SSOT cross-reference added
- **WHEN** audit completes
- **THEN** SSOT `post-refactor-architecture.md` MUST have a one-line reference to `audit-reports/` directory near the top
- **AND** the line MUST follow the format `> 历史审计报告：docs/02_architecture/audit-reports/`

#### Scenario: Deviation table format consistent
- **WHEN** any agent reports a deviation
- **THEN** the deviation row MUST use the 6-column table format: 偏差 / SSOT 描述 / 实际状态 / 严重度 (🔴/🟠/🟡/🟢) / 证据 / 建议
- **AND** 严重度 MUST map to SSOT §3 P0/P1/P2/P3 (🔴=P0, 🟠=P1, 🟡=P2, 🟢=P3)

