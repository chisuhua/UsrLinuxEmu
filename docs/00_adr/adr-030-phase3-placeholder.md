# ADR-030: Phase 3+ 议题占位 (Phase 3+ Topic Placeholder)

**状态**: ⏸️ 显式 Deferred — 待 Phase 3 触发

**日期**: 2026-06-16（v0 占位）→ 2026-06-17（v0.1 显式 deferred）

**提案人**: UsrLinuxEmu Architecture Team

**评审者**: 待定（Phase 3 触发后认领）

**关联 ADR**: ADR-013 (错误处理), ADR-018 (驱动/仿真分离)

**修订记录**:
- 2026-06-16 v0: 占位骨架（来自 ADR 编号治理清理）
- 2026-06-17 v0.1: 显式 deferred + Phase 3 触发条件（change cleanup-adr-placeholders）

## 背景

PRD.md 引用 ADR-030 但**该 ADR 实际定义什么决策目前未定**。本文档占位以结束 docs-audit §3.2 的告警。

## 决策

⏸️ **Deferred**。本 ADR 当前不进行决策，待 Phase 3 触发条件满足后由对应 owner 重新打开并填充决策章节。

候选方向（保留作为未来 owner 讨论起点，**不强制选择**）：

- **候选 A — 调试接口集成**：让 UsrLinuxEmu 进程可被 gdb/lldb attach，在 GpgpuDevice::ioctl 内部打断点
- **候选 B — 状态序列化/反序列化**：将 VA Space / Queue / Ring Buffer 状态 dump 到文件，便于离线分析
- **候选 C — 确定性执行 (Deterministic Replay)**：录制所有 ioctl + 内存读写，回放时保证字节级一致
- **候选 D — 自定义**：由 owner 决定

## Phase 3 触发条件

任一以下事件发生时，**本 ADR 必须被重新打开**并由认领 owner 填写决策章节：

1. **gdb/lldb 集成 issue 创建**（调试接口需求浮现）
2. **第一个 state-serialization 用例**（状态序列化需求浮现）

触发信号来源（可自动检测）：

- `gh issue list --label="gdb" --label="lldb" --label="debugger"`（gh CLI；或 grep 标题）
- `git log --all --grep="serialize"` 或 `git log --all --grep="state.dump"` 出现第一次匹配

## 后续

详细设计待 Phase 3 触发条件满足后由对应 owner 填充（候选 A/B/C/D 由 owner 选定或提出新方向）。

## 讨论历史 (v0 占位)

> 以下内容来自 2026-06-16 v0 占位骨架，保留作为 ADR 演进的历史记录。

### v0 决策原文

待定。候选项：

- **候选 A — 调试接口集成**：让 UsrLinuxEmu 进程可被 gdb/lldb attach，在 GpgpuDevice::ioctl 内部打断点
- **候选 B — 状态序列化/反序列化**：将 VA Space / Queue / Ring Buffer 状态 dump 到文件，便于离线分析
- **候选 C — 确定性执行 (Deterministic Replay)**：录制所有 ioctl + 内存读写，回放时保证字节级一致
- **候选 D — 自定义**：由 owner 决定

### v0 后续原文

详细设计待 Phase 3+ roadmap 启动后由对应 owner 填充。

> **v0.1 落地后**：v0 候选项已迁移到上方"决策"段与下方"讨论历史"附录；明确"⏸️ 显式 Deferred"状态 + Phase 3 触发条件。

## 相关文档

- `docs/02_architecture/post-refactor-architecture.md` §3.3
- `docs/openspec/changes/cleanup-adr-placeholders/`（本变更的设计与 spec）
