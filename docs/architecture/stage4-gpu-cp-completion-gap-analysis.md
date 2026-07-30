# Stage 4 GPU CP 完整化 — 架构差距分析

> **创建时间**: 2026-07-28
> **来源**: guide-arch Phase 3 — 架构差距分析
> **关联路线图**: [stage-4-bar-ioremap.md](../roadmap/stage-4-bar-ioremap.md)
> **关联 ADR**: ADR-040~057（GPU CP Blueprint）、ADR-064（内存模型分阶段）、ADR-069（BAR/ioremap）、ADR-073（DMA coherent）

---

## 1. 当前状态 vs 目标状态

### 已达成（当前状态 — 已完成）

| 子阶段 | 交付物 | 归档 Changes |
|--------|--------|-------------|
| **4.1** BAR + ioremap | VRAM backing store、ioremap/readl/writel compat、dma_alloc_coherent、BAR 映射 | `stage4-1-bar-ioremap` |
| **4.2** CP Phase 4 — 图启动 | Puller fence callback、Graph→GPFIFO、CP 可移植性边界、sim_mem_pool Real VA | 3 changes（33+15+17 tasks） |
| **4.3** CP Phase 5 — 方法编解码 + HyperQueue | PM4 codec、ChannelManager、Interrupt/Event、MQD/HQD、Timestamp/Profiling、HQD register | `stage4-3-cp-phase5-method-hyperqueue`（51 tasks）+ `stage4-3-integration-wiring`（7 tasks） |

### 目标状态（待达成）

| 子阶段 | ADR | 关键交付 | 状态 |
|--------|-----|---------|------|
| **4.4** CP Phase 5.5 — 优先级 + 信号量 | ADR-045/047/050 | Priority scheduling、Semaphore/Barrier、Indirect Buffer | ❌ 未开始 |
| **4.5** CP Phase 6 — 抢占 + 跨引擎 | ADR-046/049/051/052 | Preemption、Cross-engine sync、Predication、AQL/PM4 | ❌ 未开始 |
| **4.6** CP Phase 7 — Green Context | ADR-056 | Green Context、PDL | ❌ 未开始 |

---

## 2. 差距分析

### 2.1 基础设施差距

| 差距项 | 当前状态 | 目标状态 | 影响 | 工作量估计 |
|--------|---------|---------|------|-----------|
| 优先级调度层 | ChannelManager Round-Robin（无优先级） | 多级优先级队列 + starvation 保护 | 4.4 阻塞项 | ~3 天 |
| 同步原语 | 单 stream fence 跟踪 | 跨 stream semaphore acquire/release + barrier | 4.4 核心项 | ~4 天 |
| Indirect Buffer | 无 IB 支持 | IB chain walking + 链表管理 | 4.4 交付项 | ~2 天 |
| 抢占机制 | 无 mid-batch preemption | Context save/restore + quantum 管理 | 4.5 核心项 | ~5 天 |
| 跨引擎同步 | 单引擎调度 | COMPUTE↔COPY↔GRAPHICS 间 fence + 依赖追踪 | 4.5 核心项 | ~4 天 |
| Predication | 无条件执行 | Conditional draw/dispatch + predicate register | 4.5 交付项 | ~2 天 |
| AQL/PM4 | 仅 UsrNative PM4 格式 | HSA AQL packet + PM4 microcode 双格式 | 4.5 扩展项 | ~3 天 |
| Green Context | 全量 context switch | Lightweight GC + PDL | 4.6 核心项 | ~5 天 |

### 2.2 ADR 状态差距

| 子阶段 | 关联 ADR | ADR 状态 | 风险 |
|--------|---------|----------|------|
| 4.4 | ADR-045（Priority）、ADR-047（Semaphore）、ADR-050（IB） | 📋 **PROPOSED**（未经 Accepted） | **高** — 未 Accepted 的 ADR 不能作为 implement 输入 |
| 4.5 | ADR-046（Preemption）、ADR-049（Cross-engine）、ADR-051（Predication）、ADR-052（AQL/PM4） | 📋 **PROPOSED** | **高** — 同上 |
| 4.6 | ADR-056（Green Context/PDL） | 📋 **PROPOSED** | **中** — Green Context 设计复杂度高 |

### 2.3 集成测试差距

| 测试类型 | 当前覆盖 | 目标 | 差距 |
|---------|---------|------|------|
| 优先级调度 | 无 | `test_priority_sched_standalone` | ❌ |
| Semaphore 跨 stream | 无 | `test_semaphore_barrier_standalone` | ❌ |
| Indirect Buffer | 无 | `test_indirect_buffer_standalone` | ❌ |
| 抢占 context 恢复 | 无 | `test_preemption_standalone` | ❌ |
| 跨引擎 fence | 无 | `test_cross_engine_sync_standalone` | ❌ |
| Predication | 无 | `test_predication_standalone` | ❌ |
| Green Context | 无 | `test_green_context_standalone`、`test_pdl_latency_standalone` | ❌ |
| Stage 4 集成 | 4.1-4.3 各自 standalone | 统一 Stage 4 集成测试 | ❌ |

### 2.4 代码层差距

| 层 | 当前代码 | 4.4-4.6 需要扩展 | 工作量预估 |
|---|---------|-----------------|-----------|
| ① kernel 层 | io.h、dma-mapping.h 就绪 | 无额外工作（4.1 已交付） | 0 |
| ② drv/ 层 | GpgpuDevice ioctl 派发表、HAL ops (~18) | 新增 HAL ops（priority、semaphore、preemption） | ~15% of total |
| ③ sim/ 层 | ChannelManager、PM4 codec、Interrupt/Event | PriorityScheduler、SemaphoreEngine、PreemptionEngine、CrossEngineSync、PredicationUnit、GreenContext | ~60% of total |
| 测试 | 4.2/4.3 standalone 测试 | 6 个新 standalone 测试 + 集成测试 | ~25% of total |

---

## 3. 依赖关系

```
4.4 (Priority/Sem/IB) ← 依赖 ← 4.3 调度底座 (ChannelManager + PM4 codec)
        ↓
4.5 (Preemption/Cross-engine/Pred/AQL) ← 依赖 ← 4.4 同步原语 (Semaphore)
        ↓
4.6 (Green Context/PDL) ← 软依赖 ← 4.5 跨引擎 sync
```

**关键路径**：4.4 完成后才能启动 4.5；4.5 完成后才能推进 4.6。

**4.1-4.3 对 4.4-4.6 无硬阻断**（4.1 BAR 映射已交付、4.2/4.3 调度底座已就绪）。

---

## 4. 风险与缓解

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|---------|
| ADR-045/047/050/046/049/051/052/056 均未 Accepted | **高** | **阻断** | 启动 guide-arch Phase 2 审查这些 PROPOSED ADR，至少先 Accepted 4.4 的三份 |
| Priority Scheduling 与现有 Round-Robin 架构冲突 | 中 | 中 | 设计评审：扩展而非替换 ChannelManager |
| Green Context 设计复杂度高 | 中 | 高 | 提前 review ADR-056，可能需要独立 PoC |
| 测试覆盖不足导致回归 | 中 | 中 | TDD：每个新功能先写 standalone 测试 |
| 4.4-4.6 工作量超预估 | 中 | 中 | 每个 Phase 独立交付（延续 4.2/4.3 模式），不捆绑 |

---

## 5. 建议推进路径

### Phase A（短期 — ADR 审查 + Accepted）
- review 并 Accepted ADR-045(Priority)、ADR-047(Semaphore)、ADR-050(IB)
- 完成 arch-done → 进入 guide-plan → propose 4.4 changes

### Phase B（中期 — 实施 4.4）
- Implement Priority Scheduling（扩展 ChannelManager）
- Implement Semaphore/Barrier（新建 SemaphoreEngine）
- Implement Indirect Buffer（chain walking）
- Standalone tests：3 个新 test binary
- 依赖分析 + plan-done → guide-ship execute

### Phase C（中期 — 实施 4.5）
- Preemption（context save/restore + quantum timer）
- Cross-engine sync（engine fence registry）
- Predication（predicate register + conditional skip）
- AQL/PM4 双格式支持
- Standalone tests：4 个新 test binary

### Phase D（长期 — 实施 4.6）
- Green Context 轻量级上下文
- PDL（Push Doorbell List）
- GC↔正常 context 共存调度

---

## 6. 总结

| 指标 | 数值 |
|------|------|
| 已完成子阶段 | 3 (4.1, 4.2, 4.3) |
| 待完成子阶段 | 3 (4.4, 4.5, 4.6) |
| PROPOSED ADR 待审查 | 8 份 |
| 缺失的 standalone 测试 | 7 个 |
| 估算总工作量 | ~28-30 人天 |
| **首要阻塞** | **8 份 PROPOSED ADR 需 Accepted** |

---

## 附录：2026-07-30 修订（4.5 change 审查同步）

本文成文于 2026-07-28，此后部分状态已推进。以下修订以事实为准，原文表格保留作历史记录：

1. **4.4 已交付**：`2026-07-28-stage4-4-gpu-cp-phase55` 已归档（28/28 tasks），含 Priority Scheduling、Semaphore/Barrier、Indirect Buffer 及 3 个 standalone 测试。§1/§2 中 "4.4 ❌ 未开始" 已过时。
2. **ADR-049 已 Accepted**（2026-07-29 D1 修订为 waiter 回调模式），timeline semaphore 由 `2026-07-29-stage4-5-cp-phase6-preemption-timeline-sem` 交付。
3. **4.5 拆分为两个 change**：`stage4-5-cp-phase6-preemption-engine-finish`（ADR-046 收尾：MQD save/restore + pending fence 表 + 边界处理）与 `stage4-5-cp-phase6-predication-aql`（ADR-051 + ADR-052），后者依赖前者先落地（共用 `channel_state.{h,cpp}`，predicate 状态保存依赖 preempt/resume 接线）。
4. **§2.1 "quantum 管理" 措辞修正**：ADR-046 D2 的抢占触发模型为事件驱动（高优先级 batch 到达），无时间片概念。preemption-engine-finish 不实现 quantum timer，已在该 change design.md Non-Goals 显式声明。
5. **§2.1 跨引擎同步范围修正**：ADR-049 timeline semaphore 作为**最小跨引擎 fence** 已交付；多引擎（COPY/GRAPHICS，当前 sim 仅 COMPUTE 引擎）Puller 实例、engine fence registry、`test_cross_engine_sync_standalone` 显式延后，需单独立项，不在 4.5 两个 change 范围。
6. **§2.1 AQL/PM4 "双格式" 修正**：per ADR-052 D3，PM4 解析延后至 Phase 6.5；4.5 仅交付 AQL 解析 + PM4 stub（`format=2` 返回 -ENOSYS）。
7. **§6 "首要阻塞" 进展**：ADR-049 ✅；ADR-046/051/052 随上述两个 change 的实施 flip 为 Accepted；ADR-045/047/050 已由 4.4 实施、由 preemption-engine-finish task 6.3 补登记 Accepted；ADR-056 待 Phase 7。
