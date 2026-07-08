# ADR-053: Doorbell Aggregation & Over-subscription

**状态**: ⏸️ 显式 Deferred（触发条件：不满足。用户态模拟无硬件队列数限制）
**日期**: 2026-07-09
**提案人**: Sisyphus（GPU CP 蓝图完整性填充）
**关联 ADR**: ADR-024 (User Mode Queue), ADR-044 (HyperQueue)
**关联 Change**: 无

---

## Context

真实 AMD GPU 的 MES（Micro Engine Scheduler）支持 over-subscription——用户态 queue 数量可以超过硬件物理 queue 数量。当队列映射到硬件时，未映射的队列使用 "Aggregated Doorbell" 机制：用户写 doorbell 时不是直接唤醒硬件队列，而是通知 MES 调度器，由 MES 决定将哪个用户队列映射到哪个硬件队列。

在 UsrLinuxEmu 中，`MAX_CHANNELS=32` 是一个**软上限**（不是物理限制）。用户态模拟器可以任意增加通道数，不存在"物理硬件队列数不足"的限制。因此 over-subscription 和 Aggregated Doorbell 在模拟环境中**无实际需求**。

## Decision

**不实现 over-subscription 和 Aggregated Doorbell。**

- `MAX_CHANNELS` 保持为可配置编译常量（默认 32）
- 所有队列视为"始终映射"状态
- Doorbell 直接唤醒对应通道的 Puller

## Phase 触发条件（仅在以下情况重新打开）

- TaskRunner owner 明确要求："需要模拟 64 个 CUDA stream 在 32 通道硬件上 over-subscribe 的行为"
- 或 ROADMAP 增加 "real-hw queue limit simulation" 阶段
- 当前（2026-07-09）这两个条件均不满足

## Consequences

- ✅ 简化调度模型，无需引入 map/unmap 状态机
- ⚠️ 与真实 AMD GPU 行为有偏差（但 CUDA 路径无此语义，不影响 TaskRunner 测试）

---

## 讨论历史

- **2026-07-09**: Oracle CP 蓝图审查建议标 Never。理由：用户态模拟无真实硬件队列限制，over-subscription 无意义。