# adr-024-status-upgrade Specification

## Purpose

Upgrade ADR-024 (用户态队列命令提交架构) status from 🔄 提议中 to ✅ 已接受 (v1), closing the only remaining "🔄 提议中 but already implemented" gap in ADR governance. The Phase 2 implementation is complete per SSOT §1.5 timeline (commits `7dc5cb2` / `5e0258e` / `b78edc9` / `85b2e5b` / `5a25099` / `38de565`). The other 4 🔄 提议中 ADRs (011/012/013/014) are genuinely pending implementation; ADR-024 stands out as the only governance inconsistency in the 提议中 status cohort.

## Requirements

### Requirement: ADR-024 文件状态更新

The `adr-024-status-upgrade` capability MUST update `docs/00_adr/adr-024-user-mode-queue-submission.md` line 3 from `**状态**: 提议中 (Proposed)` to `**状态**: ✅ 已接受 (v1)`, and append a v1-落地 revision line to the 修订记录 section citing the Phase 2 implementation commits.

#### Scenario: ADR 文件状态升 v1
- **WHEN** the change is committed
- **THEN** `docs/00_adr/adr-024-user-mode-queue-submission.md` line 3 MUST read `**状态**: ✅ 已接受 (v1)`
- **AND** the 修订记录 section MUST contain a new line `2026-06-17 v1 落地: 经 SSOT v0.1.5 复审，Phase 2 实施完整（commit `7dc5cb2`/`5e0258e`/`b78edc9`/`85b2e5b`/`5a25099`/`38de565`），状态升 ✅ 已接受`

### Requirement: ADR 索引表更新

The `adr-024-status-upgrade` capability MUST update the ADR 索引 table row for adr-024 in `docs/00_adr/README.md` from `🔄 提议中 | 2026-05` to `✅ 已接受 (v1) | 2026-06`.

#### Scenario: README 索引表 ADR-024 状态对齐
- **WHEN** the change is committed
- **THEN** `docs/00_adr/README.md` ADR 索引 table row for adr-024 MUST read `✅ 已接受 (v1) | 2026-06`

### Requirement: ADR 关系图更新

The `adr-024-status-upgrade` capability MUST update the 关系图 in `docs/00_adr/README.md` to reflect ADR-024 as 已接受 v1 (consistent with other 已接受 ADRs like adr-022 / adr-031).

#### Scenario: README 关系图 ADR-024 标注对齐
- **WHEN** the change is committed
- **THEN** `docs/00_adr/README.md` 关系图 ADR-024 entry MUST NOT contain the `已提议` suffix
- **AND** it MUST be grouped with other v1 accepted ADRs (022 / 031)

### Requirement: Validation gates must pass

The `adr-024-status-upgrade` capability MUST pass 2 validation gates before merge.

#### Scenario: docs-audit.sh --strict 36/36 PASS
- **WHEN** the change is committed
- **THEN** `bash tools/docs-audit.sh --strict` MUST output `✅ Passed: 36 / ❌ Failed: 0`

#### Scenario: make -j4 -C build 100% pass
- **WHEN** the change is committed
- **THEN** `make -j4 -C build` MUST complete 100% (pure docs change — safety net)