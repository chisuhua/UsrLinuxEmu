# Change: adr-024-status-upgrade

> **状态**: ✅ Completed（2026-06-18，本 change 归档时即闭环）
> **创建**: 2026-06-17
> **来源**: v0.1.7 + 4 P1 change 完成后 ADR 治理审计（v0.1.6 未审计）
> **关联 SSOT**: `docs/02_architecture/post-refactor-architecture.md`
> **关联 ADR 索引**: `docs/00_adr/README.md`

## Why

ADR-024（用户态队列命令提交架构）的状态与现实矛盾：

| 字段 | 当前声明 | 实际 |
|------|----------|------|
| ADR 文件状态 (line 3) | `提议中 (Proposed)` | 已实施 |
| ADR 索引表 (README.md) | `🔄 提议中 \| 2026-05` | 已实施 |

**实际实施证据**（SSOT §1.5 Phase 2 时间轴）:
- `7dc5cb2` Ring Buffer + `GpuQueueEmu`
- `5e0258e` 多队列 fetch
- `b78edc9` LAUNCH_CB 删除
- `85b2e5b` 队列 ioctl 接线
- ADR-024 自身：5a25099（ADR 落地）
- `38de565` GlobalScheduler 回调链

SSOT §1.5 line 80 明确标注：
> **Phase 2** | 2026-05-13 | ring buffer + GpuQueueEmu + 多队列 fetch + doorbell 修复 + **LAUNCH_CB 删除** + 队列 ioctl 接线 + **ADR-024** + ...

**Why now**:
- ADR 状态与代码/SSOT 矛盾（治理盲区）
- 类似的占位 ADR（022/031）已在 v0.1.5 升 ✅ v1
- ADR 024 是仅剩的 Phase 2 已实施但仍 🔄 的 ADR

## What Changes

**新能力**: `adr-024-status-upgrade` —— ADR-024 状态从 🔄 提议中 升为 ✅ 已接受 v1

**实施**:
- `docs/00_adr/adr-024-user-mode-queue-submission.md` line 3: `提议中 (Proposed)` → `✅ 已接受 (v1)` + 修订记录追加 v1 行
- `docs/00_adr/README.md` 索引表 + 关系图：ADR-024 行状态更新

## Capabilities

### New Capabilities
- 无

### Modified Capabilities
- 无

## Impact

| 影响项 | 范围 | 风险 |
|--------|------|------|
| 文档 | 2 文件 3 处编辑（ADR 文件 + README 索引）| 极低 |
| SSOT | 无 | 极低 |
| 代码 | 无 | 极低 |

## 关联 Changes

- 跟随 v0.1.5 cleanup-adr-placeholders（commit `a75a23f`/`4c5874c`/`10adfcc`）模式
- ADR-024 是仅剩的"🔄 提议中但已实施"ADR（其他 4 个 011/012/013/014 未实施）
- 与 `ssot-0-section-refresh` + `cleanup-shadow-dead-code` 完全独立（不同文件）