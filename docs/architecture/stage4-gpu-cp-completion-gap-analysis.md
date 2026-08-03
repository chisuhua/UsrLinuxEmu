# Stage 4 GPU CP 完整化 — 架构差距分析

> **创建时间**: 2026-07-28
> **主体事实修订**: 2026-08-03
> **来源**: guide-arch Phase 3 — 架构差距分析
> **关联路线图**: [stage-4-bar-ioremap.md](../roadmap/stage-4-bar-ioremap.md)
> **关联 ADR**: ADR-040~057（GPU CP Blueprint）、ADR-064（内存模型分阶段）、ADR-069（BAR/ioremap）、ADR-073（DMA coherent）

---

## 1. 当前状态 vs 目标状态

### 已达成（当前状态 — Stage 4 全阶段已归档）

| 子阶段 | 交付物 | 归档 Changes |
|--------|--------|-------------|
| **4.1** BAR + ioremap | VRAM backing store、ioremap/readl/writel compat、dma_alloc_coherent、BAR 映射 | `stage4-1-bar-ioremap` |
| **4.2** CP Phase 4 — 图启动 | Puller fence callback、Graph→GPFIFO、CP 可移植性边界、sim_mem_pool Real VA | 3 changes（33+15+17 tasks） |
| **4.3** CP Phase 5 — 方法编解码 + HyperQueue | PM4 codec、ChannelManager、Interrupt/Event、MQD/HQD、Timestamp/Profiling、HQD register | `stage4-3-cp-phase5-method-hyperqueue`（51 tasks）+ `stage4-3-integration-wiring`（7 tasks） |
| **4.4** CP Phase 5.5 — 优先级 + 信号量 | Priority Scheduling、Semaphore/Barrier、Indirect Buffer | ✅ 已归档 — `openspec/changes/archive/2026-07-28-stage4-4-gpu-cp-phase55/`（28/28 tasks，commit `452e298`） |
| **4.5** CP Phase 6 — 抢占 + 跨引擎 | Preemption、Cross-engine sync、Predication、AQL/PM4 | ✅ 已归档 — 4 changes：`2026-07-29-stage4-5-cp-phase6-preemption-timeline-sem/`（28/28）、`2026-07-30-stage4-5-cp-phase6-preemption-engine-finish/`（53/53）、`2026-07-31-stage4-5-cp-phase6-predication-aql/`（40/40）、`2026-07-31-stage4-5-cp-phase6-preemption-timeline-sem-gaps/`（81/81） |
| **4.6** CP Phase 7 — Green Context | Green Context、PDL | ✅ 已归档 (2026-08-01) — `openspec/changes/archive/2026-08-01-stage4-6-cp-phase7-green-context-pdl/`（merge commit `c6f6ed3`，71/85 tasks；14 项未勾选 = verify 测试 + inline HAL helpers + standalone 测试） |

**全部 6 个子阶段均已归档**（2026-07-28 ~ 2026-08-01）。Stage 4 目标状态已达成，不再有待达成子阶段。

---

## 2. 差距分析

### 2.1 基础设施差距（剩余项）

Stage 4 各阶段的核心基础设施（优先级调度、Semaphore/Barrier、Indirect Buffer、抢占、跨引擎 fence、Predication、Green Context/PDL）均已随 4.4-4.6 交付。剩余差距仅两类：

| 差距项 | 当前状态 | 目标状态 | 归属 |
|--------|---------|---------|------|
| 4.6 closeout 残留 | Green Context/PDL 功能已交付（71/85），待补齐验证 | 85/85（tasks 1.6/1.7/2.4/3.5 verify 测试 + 4.6/7.6 inline HAL helpers + 8.x/9.1 standalone 测试） | 4.6 follow-up |
| PM4 microcode | AQL 解析已交付，PM4 为 stub（`format=2` 返回 -ENOSYS） | PM4 完整解析 | ADR-052 Phase 6.5（显式延后） |
| 多引擎 Puller | 仅 COMPUTE 引擎；timeline semaphore 作为最小跨引擎 fence 已交付 | COPY/GRAPHICS 引擎 Puller 实例 + engine fence registry + `test_cross_engine_sync_standalone` | ADR-049（显式延后，需单独立项） |
| 多进程支持 | 单进程 | 多进程 | ADR-011（deferred） |

### 2.2 ADR 状态差距

| 子阶段 | 关联 ADR | ADR 状态 | 备注 |
|--------|---------|----------|------|
| 4.4 | ADR-045（Priority）、ADR-047（Semaphore）、ADR-050（IB） | ✅ **Accepted**（2026-07-28，4.4 gpu-cp-phase55 backfill 登记） | 随 `stage4-4-gpu-cp-phase55` 实施完成 |
| 4.5 | ADR-046（Preemption） | ✅ **Accepted**（2026-07-30，preemption-engine-finish 实施完成） | 随 `stage4-5-cp-phase6-preemption-engine-finish` 实施完成 |
| 4.5 | ADR-049（Cross-engine） | ✅ **Accepted**（2026-07-29，D1 修订为 waiter 回调模式） | timeline semaphore 为最小跨引擎 fence；多引擎延后 |
| 4.5 | ADR-051（Predication）、ADR-052（AQL/PM4） | ✅ **Accepted**（2026-07-31，predication-aql；ADR-052 PM4 deferred 至 Phase 6.5 per D3） | 随 `stage4-5-cp-phase6-predication-aql` 实施完成 |
| 4.6 | ADR-056（Green Context/PDL） | ✅ **Accepted**（2026-08-01） | 随 `stage4-6-cp-phase7-green-context-pdl` 实施完成 |

**无 PROPOSED 条目**：8 份关联 ADR（ADR-045/047/050/046/049/051/052/056）全部 Accepted，均可作为实施输入。

### 2.3 集成测试差距

| 测试类型 | 状态 | 说明 |
|---------|------|------|
| 优先级调度 | ✅ `test_priority_sched_standalone` | 4.4 交付 |
| Semaphore 跨 stream | ✅ `test_semaphore_barrier_standalone` | 4.4 交付 |
| Indirect Buffer | ✅ `test_indirect_buffer_standalone` | 4.4 交付 |
| 抢占 context 恢复 | ✅ `test_preemption_standalone` | 4.5 交付 |
| Timeline Semaphore | ✅ `test_timeline_semaphore_standalone` | 4.5 交付 |
| Predication | ✅ `test_predication_standalone` | 4.5 交付 |
| PDL | ✅ `test_pdl_standalone`（7 scenarios，commit `0c55bde`）+ `test_context_type_standalone` | 4.6 交付（merge `c6f6ed3` 新增 2 个测试） |
| Green Context | ❌ `test_green_context_standalone` 未创建 | 4.6 closeout follow-up（tasks 8.1-8.7） |
| 跨引擎 fence | ❌ `test_cross_engine_sync_standalone` 未创建 | ADR-049 多引擎 Puller follow-up |
| Stage 4 集成 | ✅ 各子阶段 standalone + `test_concurrent_preempt` | 4.4-4.6 交付 |

已交付测试全部 ✅；仅剩 4.6 closeout 的 `test_green_context_standalone` 与 ADR-049 多引擎的 `test_cross_engine_sync_standalone` 仍在 follow-up。

### 2.4 代码层差距（现状）

| 层 | 当前代码 | 状态 |
|---|---------|------|
| ① kernel 层 | io.h、dma-mapping.h 就绪 | ✅ 无需扩展（4.1 已交付） |
| ② drv/ 层 | GpgpuDevice ioctl 派发表、HAL ops（4.6 后 29→33 fn-ptr） | ✅ 4.4-4.6 所需 HAL ops（priority、semaphore、preemption、green context、PDL）已交付；MQD 128B ABI 保留 |
| ③ sim/ 层 | ChannelManager、PM4 codec、Interrupt/Event + PriorityScheduler、SemaphoreEngine、PreemptionEngine、PredicationUnit、GreenContext、PDL | ✅ 4.4-4.6 全部交付 |
| 测试 | 各子阶段 standalone 测试 | ⚠️ 仅 `test_green_context_standalone`（+ inline HAL helpers）待 4.6 closeout |

---

## 3. 依赖关系

```
4.4 (Priority/Sem/IB) ← 依赖 ← 4.3 调度底座 (ChannelManager + PM4 codec)
        ↓
4.5 (Preemption/Cross-engine/Pred/AQL) ← 依赖 ← 4.4 同步原语 (Semaphore)
        ↓
4.6 (Green Context/PDL) ← 软依赖 ← 4.5 跨引擎 sync
```

**关键路径已执行完毕**：4.4（2026-07-28）→ 4.5（2026-07-29 ~ 07-31）→ 4.6（2026-08-01）按序交付并归档。

**4.1-4.3 对 4.4-4.6 无硬阻断**（4.1 BAR 映射已交付、4.2/4.3 调度底座已就绪），与规划一致。

---

## 4. 风险与缓解

| 风险 | 概率 | 影响 | 缓解措施 | 现状 |
|------|------|------|---------|------|
| ADR-045/047/050/046/049/051/052/056 均未 Accepted | 高 | 阻断 | 启动 guide-arch Phase 2 审查这些 PROPOSED ADR，至少先 Accepted 4.4 的三份 | ✅ 已消除：8 份全部 Accepted（2026-07-28 ~ 08-01） |
| Priority Scheduling 与现有 Round-Robin 架构冲突 | 中 | 中 | 设计评审：扩展而非替换 ChannelManager | ✅ 已按扩展路径交付（4.4） |
| Green Context 设计复杂度高 | 中 | 高 | 提前 review ADR-056，可能需要独立 PoC | ✅ 已交付（4.6，merge `c6f6ed3`） |
| 测试覆盖不足导致回归 | 中 | 中 | TDD：每个新功能先写 standalone 测试 | ✅ 各子阶段测试已交付；4.6 closeout 补 `test_green_context_standalone` |
| 4.4-4.6 工作量超预估 | 中 | 中 | 每个 Phase 独立交付（延续 4.2/4.3 模式），不捆绑 | ✅ 各 Phase 独立归档 |

**剩余风险**均为已显式延后的 follow-up 项（ADR-052 Phase 6.5、ADR-049 多引擎、ADR-011 multiprocess），非未决阻塞。

---

## 5. 建议推进路径

### Phase A（短期 — ADR 审查 + Accepted） ✅ 已完成
- ADR-045(Priority)、ADR-047(Semaphore)、ADR-050(IB) 已 Accepted（2026-07-28）
- 4.4 change 已 propose、实施并归档

### Phase B（中期 — 实施 4.4） ✅ 已完成
- Priority Scheduling（扩展 ChannelManager）、Semaphore/Barrier（SemaphoreEngine）、Indirect Buffer（chain walking）
- 3 个 standalone test binary
- `stage4-4-gpu-cp-phase55` 归档（28/28，commit `452e298`）

### Phase C（中期 — 实施 4.5） ✅ 已完成
- Preemption（context save/restore）、Cross-engine sync（timeline semaphore）、Predication（predicate register）、AQL（+PM4 stub）
- 4 个 change 归档（2026-07-29 ~ 07-31）

### Phase D（长期 — 实施 4.6） ✅ 已完成（2026-08-01）
- Green Context 轻量级上下文、PDL（Push Doorbell List）、GC↔正常 context 共存调度
- `stage4-6-cp-phase7-green-context-pdl` 归档（merge `c6f6ed3`，71/85）

### Phase E（4.6 closeout — follow-up）
- **A1**: 补齐 verify 测试（tasks 1.6/1.7/2.4/3.5）
- **A2**: 补齐 inline HAL helpers（tasks 4.6/7.6）
- **A3**: 补齐 standalone 测试（tasks 8.1-8.7 创建 `test_green_context_standalone`；9.1）
- 可选延后项（需单独立项）：ADR-052 PM4 microcode（Phase 6.5）、ADR-049 多引擎 Puller、ADR-011 multiprocess

---

## 6. 总结

| 指标 | 数值 |
|------|------|
| 已完成子阶段 | 6 (4.1, 4.2, 4.3, 4.4, 4.5, 4.6) |
| 待完成子阶段 | 0 |
| PROPOSED ADR 待审查 | 0 |
| 已 Accepted 关联 ADR | 8 份（ADR-045/047/050/046/049/051/052/056） |
| 缺失的 standalone 测试 | 1 个（`test_green_context_standalone`，4.6 closeout） |
| 估算总工作量 | ~28-30 人天（4.1-4.3 规划值） |
| **首要阻塞** | **已无首要阻塞**；follow-up：4.6 closeout（A1/A2/A3）+ ADR-052 Phase 6.5 + ADR-049 multi-engine + ADR-011 multiprocess |

---

## 附录：2026-07-30 修订（4.5 change 审查同步）

> 本附录为历史修正记录。2026-08-03 主体事实修订后，正文已直接反映下列事实，本附录保留作历史参考，不再与正文并行更新。

本文成文于 2026-07-28，此后部分状态已推进。以下修订以事实为准，原文表格保留作历史记录：

1. **4.4 已交付**：`2026-07-28-stage4-4-gpu-cp-phase55` 已归档（28/28 tasks），含 Priority Scheduling、Semaphore/Barrier、Indirect Buffer 及 3 个 standalone 测试。§1/§2 中 "4.4 ❌ 未开始" 已过时。
2. **ADR-049 已 Accepted**（2026-07-29 D1 修订为 waiter 回调模式），timeline semaphore 由 `2026-07-29-stage4-5-cp-phase6-preemption-timeline-sem` 交付。
3. **4.5 拆分为两个 change**：`stage4-5-cp-phase6-preemption-engine-finish`（ADR-046 收尾：MQD save/restore + pending fence 表 + 边界处理）与 `stage4-5-cp-phase6-predication-aql`（ADR-051 + ADR-052），后者依赖前者先落地（共用 `channel_state.{h,cpp}`，predicate 状态保存依赖 preempt/resume 接线）。
4. **§2.1 "quantum 管理" 措辞修正**：ADR-046 D2 的抢占触发模型为事件驱动（高优先级 batch 到达），无时间片概念。preemption-engine-finish 不实现 quantum timer，已在该 change design.md Non-Goals 显式声明。
5. **§2.1 跨引擎同步范围修正**：ADR-049 timeline semaphore 作为**最小跨引擎 fence** 已交付；多引擎（COPY/GRAPHICS，当前 sim 仅 COMPUTE 引擎）Puller 实例、engine fence registry、`test_cross_engine_sync_standalone` 显式延后，需单独立项，不在 4.5 两个 change 范围。
6. **§2.1 AQL/PM4 "双格式" 修正**：per ADR-052 D3，PM4 解析延后至 Phase 6.5；4.5 仅交付 AQL 解析 + PM4 stub（`format=2` 返回 -ENOSYS）。
7. **§6 "首要阻塞" 进展**：ADR-049 ✅；ADR-046/051/052 随上述两个 change 的实施 flip 为 Accepted；ADR-045/047/050 已由 4.4 实施、由 preemption-engine-finish task 6.3 补登记 Accepted；ADR-056 待 Phase 7。
