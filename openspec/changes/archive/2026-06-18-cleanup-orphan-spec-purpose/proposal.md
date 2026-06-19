# Change: cleanup-orphan-spec-purpose

> **状态**: 🔄 Proposed
> **创建**: 2026-06-17
> **来源**: v0.1.6 审计 A3 #3 + 范围扩大（v0.1.7 commit `5fa1f71` 后）
> **关联 SSOT**: `docs/02_architecture/post-refactor-architecture.md` (v0.1.7)
> **关联审计报告**: `docs/02_architecture/audit-reports/v0.1.6-audit.md`

## Why

v0.1.6 审计（change `ssot-deep-audit`）A3 #3 偏差：archived changes 的 spec.md `## Purpose` 字段全部是 OpenSpec archive 流程自动生成的 `TBD - created by archiving change <name>. Update Purpose after archive.` 模板占位，**违反 OpenSpec 工作流要求**（spec Purpose 必须是真实的 capability 描述）。

**当前状态（v0.1.7 后）**：`openspec/specs/` 下 5 个 spec 全部 TBD Purpose：

```
openspec/specs/
├── adr-placeholder-cleanup/spec.md                  → TBD (3 个原始之一)
├── gpu-pushbuffer-validation/spec.md                → TBD (3 个原始之一)
├── gpu-pushbuffer-validation-deployment/spec.md     → TBD (3 个原始之一)
├── ssot-deep-audit/spec.md                          → TBD (v0.1.6 archive 后新增)
└── ssot-v0-1-7-comprehensive-fix/spec.md            → TBD (v0.1.7 archive 后新增)
```

**范围扩大原因**：v0.1.6 + v0.1.7 两个 change 自己 archive 后又各新增 1 个 TBD Purpose spec，**净问题从 3 增长到 5**。本 change 需预防性解决：本次填入 5 个 Purpose 后，**本 change 自身 archive 时不应再产生新 TBD**——通过在 archive 前显式填入 Purpose 实现（v0.1.6/0.1.7 走的是直接 archive，没填 Purpose，所以是问题根源）。

**Why now**:
- OpenSpec 工作流完整性（spec Purpose 是 capability 的"权威描述"）
- v0.1.6 审计 #4 偏差已开放 1 周多
- v0.1.7 commit `5fa1f71` 的 commit msg 中明确"清理 orphan spec Purpose 由独立 follow-up change 处理"

## What Changes

**新能力**: `cleanup-orphan-spec-purpose` —— 一次性清理 5 个 archived spec 的 TBD Purpose

**实施**:
- 5 个 spec.md 文件，每个的 `## Purpose` 段从对应 `proposal.md` "Why" 段提炼
- 提取 1-3 句话描述 capability 的核心目标
- 移除 "TBD - created by archiving change..." 模板占位
- 完成后 `git add openspec/specs/` + 单 commit + archive

## Capabilities

### New Capabilities
- 无

### Modified Capabilities
- `adr-placeholder-cleanup`: Purpose 补全（3 个原始 spec 之一）
- `gpu-pushbuffer-validation`: Purpose 补全
- `gpu-pushbuffer-validation-deployment`: Purpose 补全
- `ssot-deep-audit`: Purpose 补全
- `ssot-v0-1-7-comprehensive-fix`: Purpose 补全

## Impact

| 影响项 | 范围 | 风险 |
|--------|------|------|
| 文档 | 5 个 spec.md 单字段替换 | 极低 |
| SSOT | 无 | 极低 |
| 代码 | 无 | 极低 |
| OpenSpec 工作流 | 提升 1 step | 极低 |

**不**影响：代码、SSOT、归档目录、其他 change。

## 关联 Changes

- 本 change 是 v0.1.6 审计 A3 #3 的修复，**与其他 4 个 P1 change 完全独立**
- 可与其他 4 个 P1 change **完全并行**（4 个改 4 个不同文件族，无冲突）
