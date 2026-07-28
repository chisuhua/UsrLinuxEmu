# Stage 4 GPU CP 完整化 — 架构差距分析

> **生成**: 2026-07-28 | **更新**: 2026-07-28 (4.3 ✅ 追加)
> **分析范围**: Stage 4.4–4.6 (GPU CP Phase 5.5–7)
> **对比基线**: 当前实现 (4.1 ✅ 4.2 ✅ 4.3 ✅) → Stage 4 终态
> **关联 ADR**: ADR-045~056 (GPU CP Blueprint Phase 5.5–7)
> **维护者**: UsrLinuxEmu Architecture Team

---

## 1. 分析摘要

| 维度 | 状态 |
|------|------|
| **当前实现** | 4.1 ✅ (BAR/ioremap), 4.2 ✅ (CP Phase 4), 4.3 ✅ (CP Phase 5) |
| **目标** | 4.4–4.6 完整交付 → 蓝图 §③ 硬件模拟成熟态 |
| **差距等级** | **中** — 3 个子阶段未启动, 8 个 ADR 仍 PROPOSED (注: 4.3 的 5 个 ADR 已在之前交付中升 Accepted) |
| **前置依赖** | 4.3 ✅ 已完成, 4.4 可直接启动 |
| **估算工作量** | 4.4: ~1w / 4.5: ~2w / 4.6: ~1w |

---

## 2. 当前状态 vs 目标状态

### 2.1 当前实现 (Delivered)

| 子阶段 | 交付物 | 状态 |
|--------|--------|------|
| **4.1 BAR/ioremap** | `ioremap`/`readl`/`writel` compat, `dma_alloc_coherent`, PCIe BAR 映射, 独立 VRAM backing store | ✅ 已归档 |
| **4.2 CP Phase 4** | Puller fence 回调, Graph→GPFIFO, CP 可移植性边界, `sim_mem_pool` Real VA | ✅ 已归档 (3 changes) |
| **4.3 CP Phase 5** | Method 编解码, ChannelManager RR 调度, 中断/事件, MQD/HQD 状态机, Profiling | ✅ 已归档 (2 changes) |
| **Stage 3** | 性能/errno/文档/CI/v1.0 Release | ✅ 已完成 |

### 2.2 目标状态 (Remaining for Stage 4 Complete)

| 子阶段 | 目标 | 当前差距 |
|--------|------|----------|
| **4.4 CP Phase 5.5** | 优先级调度 + Semaphore/Barrier + Indirect Buffer | ❌ 未开始, ADR-045/047/050 仍 PROPOSED |
| **4.5 CP Phase 6** | 抢占/上下文切换 + 跨引擎同步 + Predication + AQL/PM4 | ❌ 未开始, ADR-046/049/051/052 仍 PROPOSED |
| **4.6 CP Phase 7** | Green Context + PDL | ❌ 未开始, ADR-056 仍 PROPOSED |

---

## 3. 详细差距分析

### 3.1 ADR 差距: 需升 Accepted

| ADR | 子阶段 | 当前状态 | 差距 | 阻塞项 |
|-----|--------|----------|------|--------|
| ADR-045 Priority Scheduling | 4.4 | 📋 PROPOSED | 未评审, 缺优先级 level 定义 | 可独立起 (4.3 ChannelManager 底座已就绪) |
| ADR-047 Semaphore/Barrier | 4.4 | 📋 PROPOSED | 未评审, 缺跨 stream 同步语义 | 依赖 ADR-045 (优先级→同步) |
| ADR-050 Indirect Buffer | 4.4 | 📋 PROPOSED | 未评审, 缺 chain walking 设计 | 可独立 |
| ADR-046 Preemption/Context Switch | 4.5 | 📋 PROPOSED | 未评审, 缺 save/restore 机制 | 依赖 4.4 (调度→抢占) |
| ADR-049 Cross-engine Sync | 4.5 | 📋 PROPOSED | 未评审, 缺 engine fence 设计 | 依赖 ADR-047 (sem→跨引擎) |
| ADR-051 Predication | 4.5 | 📋 PROPOSED | 未评审, 缺条件执行粒度 | 可独立 |
| ADR-052 AQL/PM4 Native | 4.5 | 📋 PROPOSED | 未评审, 缺 packet 格式对齐 | 依赖 ADR-042 (method→AQL, 4.3 已交付) |
| ADR-056 Green Context/PDL | 4.6 | 📋 PROPOSED | 未评审, 缺 context 切换优化 | 依赖 4.5 (跨引擎→Green Ctx) |

### 3.2 架构差距: 三层覆盖面

| 层 | 4.4 影响 | 4.5 影响 | 4.6 影响 |
|----|----------|----------|----------|
| **① 内核环境模拟** | — | 上下文保存/恢复 | — |
| **② 可移植驱动代码** | HAL ops (priority, semaphore) | HAL ops (preempt, cross-engine) | HAL ops (green context) |
| **③ 硬件模拟** | 优先级队列、Semaphore 控制器、IB walker | Preemption 引擎、Cross-engine fence 路由、Predication 门控 | Green Context 切换器、PDL doorbell |

**关键架构关注点**:
- HAL ops 数量: 当前 ~18 ops (含 4.3 新增), 目标 ≤ 25 — 4.4~4.6 预计新增 4~7 个 ops, 仍在预算内
- ③ sim 调度层次已定型: GlobalScheduler (引擎级) → ChannelManager (stream 级, 4.3 ✅) → 待追加 Priority/Semaphore 层

### 3.3 测试差距

| 子阶段 | 计划测试 | 当前测试覆盖 | 差距 |
|--------|----------|-------------|------|
| 4.4 | 3 个 standalone 测试 | 0 | ❌ 无 |
| 4.5 | 3 个 standalone 测试 | 0 | ❌ 无 |
| 4.6 | 2 个 standalone 测试 | 0 | ❌ 无 |

### 3.4 集成差距

当前集成链路 (已交付):
```
ioctl → GpgpuDevice dispatch → HAL → Puller FSM → Method Decode (PM4)
→ ChannelManager RR Scheduler → GlobalScheduler → (已实现 Phase 4+5)
```

目标集成链路 (4.4~4.6 后):
```
... → Method Decode → ChannelManager → Priority Queue → Semaphore Gate
→ Preemption Engine → Cross-engine Fence → Green Context Switch → PDL Doorbell → GlobalScheduler
```

---

## 4. 依赖链与执行顺序

```
4.1 BAR/ioremap ──────────────────────────────────────────────────────┐
   (✅)                                                                  │
4.2 CP Phase 4 ──> 4.3 CP Phase 5 ──> 4.4 CP Phase 5.5 ──> 4.5 CP ──>┤
   (✅)              (✅)               (Priority/Sem)     Phase 6      │
                                                           (Preempt/    │
                                                            Cross)      │
                                                                  4.6   │
                                                              Phase 7   │
                                                            (Green Ctx) │
                                                                         │
                          (全部 ✅ 4.1~4.3)  4.4~4.6 待交付 <───────────┘
```

**执行建议**:
1. **Wave 1**: 4.4 启动 — 4.3 底座已就绪, ADR-045/050 可并行 review
2. **Wave 2**: 4.5 在 4.4 Semaphore 就绪后启动 (跨引擎 fence 依赖 sem)
3. **Wave 3**: 4.6 最后 (依赖 4.5 跨引擎同步)

---

## 5. 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| 8 个 ADR 从 PROPOSED → Accepted 耗时 | 中 | 中 | 并行评审: ADR-045/050/051 无依赖, 可先批 |
| 抢占 save/restore 上下文数据量大 | 中 | 高 | 4.5 先做轻量级抢占 (仅保存关键寄存器), 再做完整 |
| 跨引擎 fence 调试复杂度高 | 高 | 高 | 4.5 先做单引擎内 fence 验证, 再扩展到跨引擎 |
| Green Context PDL 延迟目标 <100ns 难达成 | 中 | 中 | 先做功能性实现, 延迟优化作为 P2 deferred |

---

## 6. 差距总结

| 差距类别 | 数量 | 影响 | 优先级 |
|----------|------|------|--------|
| **ADR 待升 Accepted** | 8 | 🔴 阻塞实现启动 | **P1** |
| **实现未开始 (4.4~4.6)** | 3 个子阶段 | 🔴 核心差距 | **P1** |
| **测试为零** | 8 个 standalone 测试 | 🟡 事后可补 | **P2** |
| **HAL ops 膨胀风险** | ~18 → 22~25 | 🟢 预算内 | **P3** |

---

## 7. 推荐行动

1. **立即**: 并行审批评审 ADR-045/050/051 (无依赖, 可先批) + ADR-047/046/049/052/056
2. **Wave 1**: 创建 4.4 change (Priority + Semaphore + Indirect Buffer)
3. **Wave 2**: 创建 4.5 change (Preemption + Cross-engine)
4. **Wave 3**: 创建 4.6 change (Green Context)

---

*本差距分析基于 roadmap.md + stage-4-bar-ioremap.md + ADR-040~057 + post-refactor-architecture.md 生成。初版 (2026-07-28) 假设 4.3 未完成; 更新版 (2026-07-28) 修正为 4.3 ✅, 范围缩至 4.4–4.6。*
