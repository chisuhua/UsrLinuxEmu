# ADR-028: Phase 3+ 议题占位 (Phase 3+ Topic Placeholder)

**状态**: 🔄 提议中 (Proposed) — 占位骨架

**日期**: 2026-06-16

**提案人**: UsrLinuxEmu Architecture Team

**评审者**: 待定

**关联 ADR**: ADR-007 (CMake), ADR-018 (驱动/仿真分离)

**修订记录**:
- 2026-06-16 v0: 占位骨架（来自 ADR 编号治理清理）

## 背景

PRD.md 引用 ADR-028 但**该 ADR 实际定义什么决策目前未定**。本文档占位以结束 docs-audit §3.2 的告警。

## 决策

待定。候选项：

- **候选 A — 网络设备插件架构**：参考 `drivers/sample_*` 模式为 NIC 设计设备插件接口（需不需要 DMA scatter-gather 仿真？RSS？多队列？）
- **候选 B — 存储设备插件架构**：NVMe / virtio-blk 仿真
- **候选 C — 跨主机 IPC 仿真**：让两个 UsrLinuxEmu 实例互通，验证多机驱动栈
- **候选 D — 自定义**：由 owner 决定

## 后续

详细设计待 Phase 3+ roadmap 启动后由对应 owner 填充。

## 相关文档

- `docs/02_architecture/post-refactor-architecture.md` §3.3
