# ADR-025: Phase 3+ 议题占位 (Phase 3+ Topic Placeholder)

**状态**: ⏸️ 显式 Deferred — 待 Phase 3 触发

**日期**: 2026-06-16（v0 占位）→ 2026-06-17（v0.1 显式 deferred）

**提案人**: UsrLinuxEmu Architecture Team

**评审者**: 待定（Phase 3 触发后认领）

**关联 ADR**: ADR-024 (用户态队列提交), ADR-008 (Linux 兼容层)

**修订记录**:
- 2026-06-16 v0: 占位骨架（来自 ADR 编号治理清理）
- 2026-06-17 v0.1: 显式 deferred + Phase 3 触发条件（change cleanup-adr-placeholders）

## 背景

PRD.md 与 `docs/02_architecture/post-refactor-architecture.md` §3.3 引用 ADR-025 作为 Phase 3+ roadmap 的一部分，但**该 ADR 实际定义什么决策目前未定**。本文档占位以：

1. 结束 docs-audit §3.2 中"ADR-025 missing" 的告警
2. 为未来 owner 提供「接手点」（认领后更新本文档并改 status 为 ✅ 已接受）

## 决策

⏸️ **Deferred**。本 ADR 当前不进行决策，待 Phase 3 触发条件满足后由对应 owner 重新打开并填充决策章节。

候选方向（保留作为未来 owner 讨论起点，**不强制选择**）：

- **候选 A — 多进程隔离强化**：当前 `adr-011-multiprocess-support.md` 已提议但未实施；ADR-025 可作为多进程隔离在 GPU 仿真场景下的具体化
- **候选 B — 用户态驱动沙箱**：在 UsrLinuxEmu 中安全运行第三方 .so 插件的隔离机制
- **候选 C — 异步 I/O 仿真**：参考 Linux io_uring / Windows IOCP，为 GPU doorbell 路径提供高吞吐异步模型
- **候选 D — 自定义**：由 owner 在填充时决定

## Phase 3 触发条件

任一以下事件发生时，**本 ADR 必须被重新打开**并由认领 owner 填写决策章节：

1. **第一个第三方 .so 插件提交**（涉及 sandbox / 隔离 / 资源限制语义）
2. **Phase 3 网络设备原型 commit**（网络设备原型可能需要多进程 / 沙箱 / 异步 I/O 路径）

触发信号来源（可自动检测）：

- `git log --all --diff-filter=A --name-only -- 'plugins/*/external_*.so'`（第一个非自研 .so 出现）
- `git log --all --diff-filter=A --name-only -- 'drivers/network/'`（网络设备原型首文件）

## 后续

详细设计待 Phase 3 触发条件满足后由对应 owner 填充（候选 A/B/C/D 由 owner 选定或提出新方向）。

## 讨论历史 (v0 占位)

> 以下内容来自 2026-06-16 v0 占位骨架，保留作为 ADR 演进的历史记录。

### v0 决策原文

待定。候选项（待 Phase 3+ roadmap 启动后由 owner 选定）：

- **候选 A — 多进程隔离强化**：当前 `adr-011-multiprocess-support.md` 已提议但未实施；ADR-025 可作为多进程隔离在 GPU 仿真场景下的具体化
- **候选 B — 用户态驱动沙箱**：在 UsrLinuxEmu 中安全运行第三方 .so 插件的隔离机制
- **候选 C — 异步 I/O 仿真**：参考 Linux io_uring / Windows IOCP，为 GPU doorbell 路径提供高吞吐异步模型
- **候选 D — 自定义**：由 owner 在填充时决定

### v0 后续原文

详细设计待 Phase 3+ roadmap 启动后由对应 owner 填充。

> **v0.1 落地后**：v0 候选项已迁移到上方"决策"段与下方"讨论历史"附录；明确"⏸️ 显式 Deferred"状态 + Phase 3 触发条件。

## 相关文档

- `docs/02_architecture/post-refactor-architecture.md` §3.3（ADR 治理）
- `docs/openspec/changes/cleanup-adr-placeholders/`（本变更的设计与 spec）
