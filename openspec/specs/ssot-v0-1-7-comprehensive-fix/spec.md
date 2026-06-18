# ssot-v0-1-7-comprehensive-fix Specification

## Purpose

Address 17 SSOT-side deviations discovered by the v0.1.6 audit (change `ssot-deep-audit`, commit `211b48c`). Covers Appendix A struct completeness (9 items, including the A4 #1 P0必修 `u64 va_space_handle` field on `gpu_pushbuffer_args`), §1.7 test-framework table refresh (3 items: ADR-010 ✅, 3 new rows, AGENTS.md row), §1.8 self-referential closure (3 items: §1.8.1 闭环证据 subsection, status升级 ✅ Approved, 自指 sentence past-tense), and 2 cross-file updates (`docs/README.md` ADR-022 status, §1.5 `src/kernel/` cpp count 14→12). The remaining 8 deviations are covered by 5 parallel follow-up changes.

## Requirements
### Requirement: SSOT Appendix A struct field completeness

The `ssot-v0-1-7-comprehensive-fix` capability MUST ensure that `docs/02_architecture/post-refactor-architecture.md` 附录 A contains complete field definitions for ALL 13 struct types referenced in §1.6 IOCTL 体系表. The audit (v0.1.6, commit `211b48c`) found that only 4 structs were fully documented; the remaining 9 (1 P0 必修 + 8 P2) MUST be added in this change. All struct field definitions MUST be copied verbatim from source headers (no design changes).

#### Scenario: A4 #1 P0 必修 - `gpu_pushbuffer_args.va_space_handle` field added

- **WHEN** the change is committed
- **THEN** SSOT 附录 A's `struct gpu_pushbuffer_args` definition MUST contain a `u64 va_space_handle;` field as the last member
- **AND** the field MUST be accompanied by a comment explaining: (a) it is the Phase 2 校验字段, (b) `0` is a sentinel meaning "skip validation" (向后兼容), (c) it was added in H-1 commit `0272970`

#### Scenario: A4 #2-#6 - 5 IOCTL companion structs added

- **WHEN** the change is committed
- **THEN** SSOT 附录 A MUST contain complete field definitions for: `gpu_mmu_event_cb_args` (IOCTL 0x02), `gpu_firmware_cb_args` (IOCTL 0x03), `gpu_map_bo_args` (IOCTL 0x12), `gpu_wait_fence_args` (IOCTL 0x13), `gpu_register_gpu_args` (IOCTL 0x32)
- **AND** each struct's field names, types, and order MUST match the source `plugins/gpu_driver/shared/gpu_ioctl.h` verbatim
- **AND** the structs MUST be ordered by IOCTL 编号 (ascending)

#### Scenario: A4 #7-#8 - 2 gpu_queue.h structs added

- **WHEN** the change is committed
- **THEN** SSOT 附录 A MUST contain complete field definitions for `gpu_queue_map_ring_args` (IOCTL 0x42) and `gpu_queue_info_args` (IOCTL 0x43)
- **AND** the field names, types, and order MUST match the source `plugins/gpu_driver/shared/gpu_queue.h` verbatim
- **AND** the structs MUST be ordered by IOCTL 编号 (ascending) — placed AFTER `gpu_queue_args`

#### Scenario: A4 #9 - `gpu_ring_header` struct added (with volatile + reserved[32])

- **WHEN** the change is committed
- **THEN** SSOT 附录 A MUST contain a complete `struct gpu_ring_header` definition
- **AND** the definition MUST include all 6 fields: `write_idx` (volatile uint32), `read_idx` (volatile uint32), `capacity` (uint32), `flags` (uint32), `fence_value` (uint64), `reserved[32]` (uint8)
- **AND** the `volatile` qualifier MUST be preserved on `write_idx` and `read_idx` (per source)

#### Scenario: Existing 4 struct definitions preserved unchanged

- **WHEN** the change is committed
- **THEN** the existing 4 struct definitions in 附录 A (`gpu_alloc_bo_args`, `gpu_device_info`, `gpu_va_space_args`, `gpu_queue_args`) MUST remain unchanged in field order, names, and types
- **AND** `gpu_pushbuffer_args` (after A4 #1) MUST have the 5 original fields preserved at the same positions, with `va_space_handle` appended at the end

### Requirement: SSOT §1.7 test framework table refresh

The `ssot-v0-1-7-comprehensive-fix` capability MUST refresh SSOT §1.7 "测试框架（声称 vs 实际）" table to reflect the current Catch2 project state, addressing 3 P3 deviations (A2 #3, #4, #5) identified in v0.1.6 audit.

#### Scenario: A2 #3 - ADR-010 row updated to ✅ Accepted Catch2

- **WHEN** the change is committed
- **THEN** SSOT §1.7 table row 9 (`docs/00_adr/adr-010-gtest-migration.md`) MUST be updated from "声明: 提议\"迁移到 GTest\"" / "实际: **未实施**" to "声明: **✅ 已接受 Catch2（最终决策）**" / "实际: **Catch2**（ADR-010 自身已对齐实际）"
- **AND** the §1.7 conclusion line MUST be updated to reflect that ADR-010 is now formally Accepted (not pending)

#### Scenario: A2 #4 - 3 missing source rows added

- **WHEN** the change is committed
- **THEN** SSOT §1.7 table MUST be extended with 3 new rows: `CONTRIBUTING.md` (声明: Catch2, 实际: Catch2), `docs/03-development/adding-devices.md` (声明: Catch2, 实际: Catch2), `docs/06-reference/glossary.md` (声明: Catch2, 实际: Catch2)
- **AND** the table MUST now have 12 rows total (1 baseline + 8 original + 3 new)
- **AND** the 3 new rows MUST follow the same 3-column format as existing rows

#### Scenario: A2 #5 - AGENTS.md row updated from "未表态" to "明确表态"

- **WHEN** the change is committed
- **THEN** SSOT §1.7 table row 3 (`AGENTS.md`) MUST be updated from "声明: **—（未表态）**" to "声明: **Catch2（明确反对 GTest）**"
- **AND** the "实际" column MUST remain "Catch2"

### Requirement: SSOT §1.8 self-referential closure

The `ssot-v0-1-7-comprehensive-fix` capability MUST close the §1.8 self-referential "gap" by adding a 闭环证据 subsection (A3 #1), updating the status (A3 #4), and rewording the self-referential sentence (A3 #6), addressing 3 P2/P3 deviations identified in v0.1.6 audit.

#### Scenario: A3 #1 - §1.8.1 闭环证据 subsection added

- **WHEN** the change is committed
- **THEN** SSOT §1.8 MUST contain a new subsection `#### §1.8.1 闭环证据（v0.1.6 审计确认）` immediately after the existing §1.8 content
- **AND** the subsection MUST contain: (a) v0.1.6 审计 reference (change `ssot-deep-audit`, commit `211b48c`), (b) file existence stats (47,232 字节, 691 行), (c) cross-reference stats (28 docs/AGENTS.md/README.md + 42 project-wide), (d) total deviation count (25 = 🔴 1 / 🟠 4 / 🟡 14 / 🟢 6), (e) 4-region coverage list
- **AND** the subsection MUST link to `docs/02_architecture/audit-reports/v0.1.6-audit.md`

#### Scenario: A3 #4 - SSOT status upgraded to ✅ Approved

- **WHEN** the change is committed
- **THEN** SSOT header "状态" field MUST be updated from "🔄 待评审" to "✅ Approved（v0.1.7）"
- **AND** the bottom-of-file status line MUST be updated to reflect v0.1.7 approval
- **AND** the 变更记录 table MUST contain a new v0.1.7 row (per A3 #4 also covers changelog)

#### Scenario: A3 #6 - Self-referential sentence reworded to past tense

- **WHEN** the change is committed
- **THEN** SSOT §1.8 line "**缺少一份'重构后的架构总览'文档** —— 本文正是为此而写" MUST be updated to past tense
- **AND** the new wording MUST acknowledge the gap is closed, e.g. "已于 v0.1.5 创建本文闭环此 gap（v0.1.6 审计确认闭环证据见 §1.8.1）"
- **AND** the "缺少" wording MUST be removed (the gap is no longer present)

### Requirement: Cross-file & cross-section updates

The `ssot-v0-1-7-comprehensive-fix` capability MUST update 2 cross-file/cross-section items: `docs/README.md` ADR-022 status (A3 #5) and SSOT §1.5 src/kernel cpp count (A1 #5).

#### Scenario: A3 #5 - docs/README.md ADR-022 status updated

- **WHEN** the change is committed
- **THEN** `docs/README.md` line 222 "❌ **未启动**：022（\"GPU 计算单元仿真\"） — 占位编号" MUST be updated to "✅ v1（operator-level emulation，ADR-022）"
- **AND** the ADR-022 entry in the ADR list (around line 80-100) MUST be marked as ✅ Accepted
- **AND** the change MUST be consistent with `docs/00_adr/adr-022-gpu-compute-unit-emulation.md` status (already ✅ v1 per v0.1.5 closeout)

#### Scenario: A1 #5 - src/kernel cpp count recalibrated from 14 to 12

- **WHEN** the change is committed
- **THEN** SSOT §1.5 "src/" description (line 204) `(kernel SHARED lib, 14 cpp)` MUST be updated to `(kernel SHARED lib, 12 cpp)`
- **AND** the v0.1.1 changelog entry MUST NOT be modified (the historical claim is preserved; only the current state in §1.5 is corrected)

### Requirement: Audit report format and location (already satisfied)

The `ssot-v0-1-7-comprehensive-fix` capability inherits the report format requirement from `ssot-deep-audit` capability: audit reports MUST be stored in `docs/02_architecture/audit-reports/`. This change MUST NOT create a new audit report; it acts on v0.1.6's findings.

#### Scenario: No new audit report created

- **WHEN** the change is committed
- **THEN** no new file MUST be created in `docs/02_architecture/audit-reports/`
- **AND** the v0.1.6 audit report (`v0.1.6-audit.md`) MUST remain unchanged

### Requirement: Validation gates must pass

The `ssot-v0-1-7-comprehensive-fix` capability MUST pass 2 validation gates before merge.

#### Scenario: docs-audit.sh --strict still 36/36 PASS

- **WHEN** the change is committed (pre-commit hook runs `bash tools/docs-audit.sh --strict`)
- **THEN** the script MUST output `✅ Passed: 36 / ❌ Failed: 0`
- **AND** if any check fails, the commit MUST be aborted and the failing check MUST be addressed before re-commit

#### Scenario: make -j4 -C build still 100% pass

- **WHEN** the change is committed
- **THEN** `make -j4 -C build` MUST complete with `[100%] Built target ...` for all targets
- **AND** no compilation warnings or errors MUST be introduced
- **NOTE**: since this is a doc-only change, the build MUST be unchanged; this check is a safety net per task 6.2

### Requirement: Out-of-scope deviations (covered by other changes)

The `ssot-v0-1-7-comprehensive-fix` capability MUST NOT fix the 8 deviations covered by other follow-up changes. This change's tasks.md MUST explicitly mark these as "covered by other change" and not attempt to fix them.

#### Scenario: Deviations NOT fixed in this change

- **WHEN** the change is committed
- **THEN** the following deviations MUST remain unfixed (covered by other changes):
  - A1 #1, #3, #4 (sim/scheduler/translator 嵌套、doorbell 空壳、shadow 死代码) → `cleanup-shadow-dead-code`
  - A1 #2 (sim/hardware/ 布局分裂) → `fix-sim-hardware-layout`
  - A2 #1 (.github/copilot-instructions.md GTest) → `cleanup-gtest-residue`
  - A2 #2 (CONTRIBUTING.md libgtest-dev) → `cleanup-gtest-residue`
  - A3 #2 (AGENTS.md 反向引用 SSOT) → `fix-agents-md-ssot-link`
  - A3 #3 (3 openspec/specs/*.md TBD Purpose) → `cleanup-orphan-spec-purpose`
- **AND** these deviations MUST be listed in tasks.md as "not in scope" with cross-references to their respective changes

#### Scenario: tasks.md explicit out-of-scope section

- **WHEN** the change is committed
- **THEN** tasks.md MUST contain a section "## Out of Scope" listing all 8 deviations NOT fixed in this change
- **AND** each item MUST include: (a) deviation ID, (b) brief description, (c) target change name

