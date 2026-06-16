# ADR-026: Phase 3+ 议题占位 (Phase 3+ Topic Placeholder)

**状态**: 🔄 提议中 (Proposed) — 占位骨架

**日期**: 2026-06-16

**提案人**: UsrLinuxEmu Architecture Team

**评审者**: 待定

**关联 ADR**: ADR-005 (Ring Buffer), ADR-024 (用户态队列提交)

**修订记录**:
- 2026-06-16 v0: 占位骨架（来自 ADR 编号治理清理）

## 背景

PRD.md 引用 ADR-026 但**该 ADR 实际定义什么决策目前未定**。本文档占位以结束 docs-audit §3.2 的"ADR-026 missing"告警。

## 决策

待定。候选项：

- **候选 A — 多 GPU / 多设备拓扑**：当宿主有多个 GPU 时 UsrLinuxEmu 应该如何建模（设备树、P2P、NVLink 等）
- **候选 B — 设备热插拔仿真**：模拟设备在运行时插入/移除，验证 hot-plug 路径
- **候选 C — 自定义**：由 owner 决定

## 后续

详细设计待 Phase 3+ roadmap 启动后由对应 owner 填充。

## 相关文档

- `docs/02_architecture/post-refactor-architecture.md` §3.3
