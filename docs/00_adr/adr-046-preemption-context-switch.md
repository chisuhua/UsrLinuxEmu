# ADR-046: Preemption & Context Switch

**状态**: 📋 PROPOSED（Phase 6，ADR-054 之后）
**日期**: 2026-07-09
**提案人**: Sisyphus（GPU CP 蓝图完整性填充）
**关联 ADR**: ADR-021 (Puller FSM), ADR-044 (HyperQueue), ADR-045 (Priority Scheduling), ADR-054 (MQD/HQD)
**关联 Change**: 无（Phase 6 规划）

---

## Context

ADR-044 的 ChannelManager Round-Robin 和 ADR-045 的优先级调度都是 **batch 级**调度——一个 batch 必须执行完才能切换到另一通道。

真实 GPU 支持多种级别的抢占：

- **Batch-level (channel switch)**：当前 batch 完成后切换。ADR-044 已实现。
- **Dispatch-level (mid-batch)**：正在执行的 batch 中途暂停，切换到高优先级通道。NVIDIA PBDMA 通过 RAMFC save/restore 实现。
- **Wavefront-level**：kernel 执行到某个 wavefront 时暂停（CUDA cooperative groups 需求）。

本 ADR 定义 Dispatch-level 和 Wavefront-level 抢占的模拟策略。

---

## Decision

### D1: 抢占级别

| 级别 | 触发条件 | Save/Restore 内容 | 用途 |
|------|---------|-------------------|------|
| **Batch-level** | 当前 batch 完成 | 无需 | ADR-044 Round-Robin |
| **Dispatch-level** | 高优先级通道有新 batch 提交 | `ChannelState`：(gpfifo_addr, current_index, total_entries, pending_fence_id) + Puller 内部状态 | Green Context (ADR-056)、优先级抢占 |
| **Wavefront-level** | 暂不实现 | Grid/block 进度计数器 | Cooperative groups |

**Phase 6 只实现 Dispatch-level**。

### D2: Preempt 协议

```
高优先级通道提交新 batch
  → ChannelManager::submitBatch(high_prio_ch, ...)
  → 检测到当前 active 通道 priority < high_prio_ch.priority
  → 触发 preempt：
      1. 保存当前通道状态到对应 ChannelState（saveContext）
      2. 标记当前通道为 PREEMPTED（而非 ACTIVE）
      3. 激活高优先级通道
      4. Puller 从高优先级通道继续执行
  → 高优先级 batch 完成后：
      5. 恢复被抢占通道的状态（restoreContext）
      6. 标记为 ACTIVE
      7. 从保存的 current_index 继续执行
```

### D3: Save/Restore 格式 — 复用 ADR-054 MQD

`ChannelState::saveContext()` 将 Puller 状态序列化到 `ChannelState`：

```cpp
struct PreemptContext {
    uint64_t gpfifo_addr;
    uint32_t current_index;
    uint32_t total_entries;
    uint64_t pending_fence_id;
    // Puller 内部状态（FETCH/DECODE 中间产物）
};
```

在 ADR-054 的 MQD 结构中预留 `PreemptContext` 字段。

### D4: 不实现 Wavefront-level 抢占

Wavefront-level 抢占需要 CU simulator（ADR-022）的 grid 进度跟踪 + SIMD stack save/restore，Phase 6 scope 外。

---

## Consequences

- ✅ Green Context (ADR-056) 的抢占基础就绪
- ✅ 优先级调度（ADR-045）从 batch 级升级为 dispatch 级公平性
- ⚠️ 显式依赖 ADR-054：ADR-054 必须先 ✅ Accepted 才能实施 046
- ⚠️ Puller FSM 复杂度增加：需新增 PREEMPT_CHECK 状态
- ⚠️ 保存/恢复 Puller 内部状态可能引入竞态（Puller 运行在独立线程）

### Phase 6 触发条件

- ADR-054 (MQD/HQD) ✅ Accepted 且已实现
- ADR-044 (ChannelManager) + ADR-045 (Priority Scheduling) 已实现
- TaskRunner 提出 Green Context 或 dispatch-level 抢占测试需求