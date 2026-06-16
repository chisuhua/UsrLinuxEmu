# ADR-030: Phase 3+ 议题占位 (Phase 3+ Topic Placeholder)

**状态**: 🔄 提议中 (Proposed) — 占位骨架

**日期**: 2026-06-16

**提案人**: UsrLinuxEmu Architecture Team

**评审者**: 待定

**关联 ADR**: ADR-013 (错误处理), ADR-018 (驱动/仿真分离)

**修订记录**:
- 2026-06-16 v0: 占位骨架（来自 ADR 编号治理清理）

## 背景

PRD.md 引用 ADR-030 但**该 ADR 实际定义什么决策目前未定**。本文档占位以结束 docs-audit §3.2 的告警。

## 决策

待定。候选项：

- **候选 A — 调试接口集成**：让 UsrLinuxEmu 进程可被 gdb/lldb attach，在 GpgpuDevice::ioctl 内部打断点
- **候选 B — 状态序列化/反序列化**：将 VA Space / Queue / Ring Buffer 状态 dump 到文件，便于离线分析
- **候选 C — 确定性执行 (Deterministic Replay)**：录制所有 ioctl + 内存读写，回放时保证字节级一致
- **候选 D — 自定义**：由 owner 决定

## 后续

详细设计待 Phase 3+ roadmap 启动后由对应 owner 填充。

## 相关文档

- `docs/02_architecture/post-refactor-architecture.md` §3.3
