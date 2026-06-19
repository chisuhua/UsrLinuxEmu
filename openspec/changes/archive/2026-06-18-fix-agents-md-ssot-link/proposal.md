# Change: fix-agents-md-ssot-link

> **状态**: 🔄 Proposed
> **创建**: 2026-06-17
> **来源**: v0.1.6 审计 A3 #2（AGENTS.md 0 处反向引用 SSOT）
> **关联 SSOT**: `docs/02_architecture/post-refactor-architecture.md` (v0.1.7)
> **关联审计报告**: `docs/02_architecture/audit-reports/v0.1.6-audit.md`

## Why

v0.1.6 审计 A3 #2 偏差：`AGENTS.md` 中 **0 处**提及 `post-refactor-architecture.md`（SSOT）。所有其他顶层文档（README.md / docs/README.md / docs/CHANGELOG.md / docs/04-building/build_system.md / docs/04-building/testing_guide.md）均已同时引用 AGENTS.md + SSOT，**唯独 AGENTS.md 自身未反向指向 SSOT**。

**SSOT 自身已明确预期此引用**：
- §0 line 64-66: "AGENTS.md 是事实上的'权威架构说明'，但**没有正式升级为架构文档**" / "修复完成后，AGENTS.md 的架构部分应**反向引用本文**而非重复内容"
- §4.1 line 525-528: "AGENTS.md 应作为**事实上的权威**（虽非正式）" / "SSOT 的更新责任: 每次重大重构（Phase 边界），必须先更新 SSOT"

**实际状态（验证）**：
```bash
$ grep -c "post-refactor-architecture" AGENTS.md
0
$ stat -c '%y' AGENTS.md
2026-06-16 16:34:18  # 早于 SSOT mtime 2026-06-17 18:22
```

**Why now**: A3 #2 是 P1 高优偏差（影响 SSOT 治理一致性）；v0.1.7 commit `5fa1f71` 明确将本修复作为 follow-up change。

## What Changes

**新能力**: `fix-agents-md-ssot-link` —— AGENTS.md 反向引用 SSOT

**实施**（单文件 3-5 行新增）:
- 在 `AGENTS.md` 头部"## 项目概述"段后或"权威架构说明"相关位置，加 1-2 行反向引用
- 引用格式与 `README.md` line 12 和 `docs/README.md` line 5 对齐
- 不删除 AGENTS.md 现有的架构内容（SSOT §0 line 66 是"反向引用而非重复"，意味着 AGENTS.md 保留但加链接）

## Capabilities

### New Capabilities
- 无

### Modified Capabilities
- 无（AGENTS.md 不是 OpenSpec capability）

## Impact

| 影响项 | 范围 | 风险 |
|--------|------|------|
| 文档 | `AGENTS.md` 单文件 +3 行 | 极低 |
| SSOT | 无 | 极低 |
| 代码 | 无 | 极低 |
| 其他 | 无 | 极低 |

**不**影响：SSOT、代码、归档、其他文件。

## 关联 Changes

- 本 change 是 v0.1.6 审计 A3 #2 的修复，**与其他 4 个 P1 change 完全独立**
- 可与其他 4 个 P1 change **完全并行**（改不同文件，无冲突）
