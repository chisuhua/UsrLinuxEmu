# Change: ssot-0-section-refresh

> **状态**: ✅ Completed（2026-06-18，本 change 归档时即闭环）
> **创建**: 2026-06-17
> **来源**: v0.1.7 综合修复 + 4 P1 change 完成后发现的 SSOT §0 meta-stale（v0.1.6 审计未覆盖区域）
> **关联 SSOT**: `docs/02_architecture/post-refactor-architecture.md`
> **关联审计报告**: `docs/02_architecture/audit-reports/v0.1.6-audit.md`

## Why

v0.1.7 SSOT 状态升为 ✅ Approved，4 个 P1 change（cleanup-gtest-residue / fix-agents-md-ssot-link / fix-sim-hardware-layout / cleanup-orphan-spec-purpose）完成后：
- AGENTS.md 已反向引用 SSOT（fix-agents-md-ssot-link, commit `3faa3a7`）
- `architecture.md` 已是 v3.0（2026-06-16 对齐 Phase 1.5 → 2）

但 SSOT `post-refactor-architecture.md` §0 "文档定位" 段（lines 46-68）**未跟随上述进展刷新**，4 处描述已 meta-stale：

| Line | 当前描述 | 实际状态（v0.1.7 + P1 后）|
|------|----------|---------------------------|
| L52 | `AGENTS.md` "唯一接近真相的文档，但定位为'开发指南'" | 已通过反向引用成为完整 SSOT 引用方之一 |
| L53 | `architecture.md` "🔴 严重过期（2026-03-23，旧布局）" | 已是 v3.0（2026-06-16 对齐 Phase 1.5→2，830 行，引用 SSOT）|
| L56 | **本 SSOT** "🔄 待评审" | ✅ Approved（v0.1.7）|
| L64 | "AGENTS.md 没有正式升级为架构文档" | 已通过反向引用闭环 |
| L67 | "本文档目标是接管 AGENTS.md 的架构部分" | 已达成 |

**Why now**:
- v0.1.6 审计明确未审计 §0（4 个审计区域是 §1.2 / §1.7 / §1.8 / 附录 A）
- 新读者按 SSOT §0 自我描述会产生错误预期："SSOT 还在等评审"、"architecture.md 不能信"
- 与 v0.1.7 commit `5fa1f71` 显式状态升级矛盾

## What Changes

**新能力**: `ssot-0-section-refresh` —— SSOT §0 5 处描述刷新

**实施**: 单文件 `post-refactor-architecture.md` §0 5 处编辑（line 52, 53, 56, 64, 67）

## Capabilities

### New Capabilities
- 无

### Modified Capabilities
- 无

## Impact

| 影响项 | 范围 | 风险 |
|--------|------|------|
| 文档 | SSOT §0 5 处 1-行替换 | 极低 |
| 其他文档 | 无 | 极低 |
| 代码 | 无 | 极低 |

**不**影响：SSOT §1-5 章节、附录、变更记录、其他文档。

## 关联 Changes

- 跟随 v0.1.7 (commit `5fa1f71`) + 4 P1 changes（`3e306d1`, `880eb4f`, `3faa3a7`, 加上 cleanup-orphan-spec-purpose）后
- v0.1.6 审计未覆盖 §0（审计盲区）；本 change 闭环该盲区
- 与 `adr-024-status-upgrade` + `cleanup-shadow-dead-code` 完全独立（不同文件）