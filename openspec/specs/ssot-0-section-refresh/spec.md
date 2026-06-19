# ssot-0-section-refresh Specification

## Purpose

Refresh SSOT `post-refactor-architecture.md` §0 self-description (5 stale lines) discovered after v0.1.7 SSOT approval and 4 P1 follow-up changes (commit `3e306d1` / `880eb4f` / `3faa3a7` / cleanup-orphan-spec-purpose) closed. The v0.1.6 audit (change `ssot-deep-audit`, commit `211b48c`) explicitly did not cover §0 (audit scope was §1.2 / §1.7 / §1.8 / Appendix A). This change closes that gap so SSOT §0 文档定位段 matches reality: AGENTS.md reverse link (L52), architecture.md v3.0 status (L53), self SSOT Approved status (L56), and the 关键事实 AGENTS.md 已升级 / 接管已达成 (L64, L67).

## Requirements

### Requirement: §0 line 52 AGENTS.md 状态更新

The `ssot-0-section-refresh` capability MUST update SSOT §0 line 52 to reflect that `AGENTS.md` is no longer the sole source of architecture truth — it now reverse-links to SSOT via the blockquote added in commit `3faa3a7` (change `fix-agents-md-ssot-link`).

#### Scenario: L52 描述对齐
- **WHEN** the change is committed
- **THEN** SSOT §0 L52 MUST read approximately `🟢 相对准确（已通过反向引用 SSOT 闭环，commit `3faa3a7`）` instead of the stale `唯一接近真相的文档，但定位为"开发指南"` wording

### Requirement: §0 line 53 architecture.md 状态更新

The `ssot-0-section-refresh` capability MUST update SSOT §0 line 53 to reflect `architecture.md` is at v3.0 (aligned with Phase 1.5 → 2, 830 lines, references SSOT) rather than the stale "🔴 严重过期（2026-03-23，旧布局）" claim.

#### Scenario: L53 描述对齐
- **WHEN** the change is committed
- **THEN** SSOT §0 L53 MUST read approximately `🟢 v3.0（2026-06-16 对齐 Phase 1.5 → 2；引用 SSOT）` instead of the stale "🔴 严重过期" wording

### Requirement: §0 line 56 self SSOT 状态更新

The `ssot-0-section-refresh` capability MUST update SSOT §0 line 56 to reflect that **the SSOT itself** is now `✅ Approved (v0.1.7)` (per the v0.1.7 commit `5fa1f71`), not `🔄 待评审`.

#### Scenario: L56 描述对齐
- **WHEN** the change is committed
- **THEN** SSOT §0 L56 MUST read `✅ Approved（v0.1.7）` instead of `🔄 待评审`

### Requirement: §0 关键事实段（line 64, 67）刷新

The `ssot-0-section-refresh` capability MUST update the §0 关键事实 bullets to reflect that:
- (L64) AGENTS.md has been formally cross-linked with SSOT via reverse-reference
- (L67) The SSOT 接管 AGENTS.md 的架构部分 goal has been achieved

#### Scenario: 关键事实段对齐
- **WHEN** the change is committed
- **THEN** SSOT §0 L64 MUST mention that AGENTS.md 反向引用 has been established (commit `3faa3a7`)
- **AND** SSOT §0 L67 MUST mention that the SSOT 接管 goal has been achieved with v0.1.7 ✅ Approved

### Requirement: Validation gates must pass

The `ssot-0-section-refresh` capability MUST pass 2 validation gates before merge.

#### Scenario: docs-audit.sh --strict 36/36 PASS
- **WHEN** the change is committed
- **THEN** `bash tools/docs-audit.sh --strict` MUST output `✅ Passed: 36 / ❌ Failed: 0`

#### Scenario: make -j4 -C build 100% pass
- **WHEN** the change is committed
- **THEN** `make -j4 -C build` MUST complete 100% (pure docs change — safety net)