# ADR-025: Phase 3+ 议题占位 (Phase 3+ Topic Placeholder)

**状态**: 🔄 提议中 (Proposed) — 占位骨架

**日期**: 2026-06-16

**提案人**: UsrLinuxEmu Architecture Team

**评审者**: 待定

**关联 ADR**: ADR-024 (用户态队列提交), ADR-008 (Linux 兼容层)

**修订记录**:
- 2026-06-16 v0: 占位骨架（来自 ADR 编号治理清理）

## 背景

PRD.md 与 `docs/02_architecture/post-refactor-architecture.md` §3.3 引用 ADR-025 作为 Phase 3+ roadmap 的一部分，但**该 ADR 实际定义什么决策目前未定**。本文档占位以：

1. 结束 docs-audit §3.2 中"ADR-025 missing" 的告警
2. 为未来 owner 提供「接手点」（认领后更新本文档并改 status 为 ✅ 已接受）

## 决策

待定。候选项（待 Phase 3+ roadmap 启动后由 owner 选定）：

- **候选 A — 多进程隔离强化**：当前 `adr-011-multiprocess-support.md` 已提议但未实施；ADR-025 可作为多进程隔离在 GPU 仿真场景下的具体化
- **候选 B — 用户态驱动沙箱**：在 UsrLinuxEmu 中安全运行第三方 .so 插件的隔离机制
- **候选 C — 异步 I/O 仿真**：参考 Linux io_uring / Windows IOCP，为 GPU doorbell 路径提供高吞吐异步模型
- **候选 D — 自定义**：由 owner 在填充时决定

## 后续

详细设计待 Phase 3+ roadmap 启动后由对应 owner 填充。

## 相关文档

- `docs/02_architecture/post-refactor-architecture.md` §3.3（ADR 治理）
