# ADR-029: Phase 3+ 议题占位 (Phase 3+ Topic Placeholder)

**状态**: 🔄 提议中 (Proposed) — 占位骨架

**日期**: 2026-06-16

**提案人**: UsrLinuxEmu Architecture Team

**评审者**: 待定

**关联 ADR**: ADR-013 (错误处理), ADR-014 (日志)

**修订记录**:
- 2026-06-16 v0: 占位骨架（来自 ADR 编号治理清理）

## 背景

PRD.md 引用 ADR-029 但**该 ADR 实际定义什么决策目前未定**。本文档占位以结束 docs-audit §3.2 的告警。

## 决策

待定。候选项：

- **候选 A — 性能基准与回归检测**：建立 micro-benchmark 套件（ioctl latency, pushbuffer throughput, doorbell 延迟），每次 CI 自动跑并对比基线
- **候选 B — Tracing 框架**：基于 LTTng / bpftrace 风格的 tracepoint 机制，记录 plugin/hal/sim 三层的执行路径
- **候选 C — 错误注入框架**：在 HAL 层引入 fault injection 钩子，验证 driver 层错误处理
- **候选 D — 自定义**：由 owner 决定

## 后续

详细设计待 Phase 3+ roadmap 启动后由对应 owner 填充。

## 相关文档

- `docs/02_architecture/post-refactor-architecture.md` §3.3
