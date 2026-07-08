# ADR-045: Priority Scheduling in GlobalScheduler

**状态**: 📋 PROPOSED（Phase 5.5，ADR-054 + ADR-044 之后）
**日期**: 2026-07-09
**提案人**: Sisyphus（GPU CP 蓝图完整性填充）
**关联 ADR**: ADR-021 (Puller FSM), ADR-044 (HyperQueue), ADR-054 (MQD/HQD)
**关联 Change**: 无（Phase 5.5 规划）

---

## Context

当前 `GlobalScheduler` 仅支持 FIFO 调度——所有 entry 按到达顺序 dispatch，无优先级区分。

ADR-021 §决策 5 预留了 "Phase 2 实现优先级队列" 的接口，但从未落地。真实 GPU（CUDA stream priority、AMD MES priority levels）支持多级优先级调度：

- **CUDA**：`cudaStreamCreateWithPriority` 允许用户指定 stream 优先级（high/normal/low），高优先级 stream 的 kernel 应被优先调度
- **AMD MES**：支持 4 级优先级（IDLE=0, NORMAL=1, FOCUS=2, REALTIME=3），硬件队列按优先级抢占
- **NVIDIA HyperQueue**：TSG 可绑定优先级 class，PBDMA Puller 按优先级选择下一通道

### Scope 限制

本 ADR 仅处理 **Runlist 重排 + 通道优先级字段**。**不实现 mid-batch 抢占**——mid-batch 抢占统一归 ADR-046。

---

## Decision

### D1: Priority 字段定义

`ChannelState`（ADR-044）新增 `priority` 字段：

```cpp
enum class ChannelPriority : uint8_t {
    IDLE     = 0,  // 空闲（无待处理 work）
    LOW      = 1,
    NORMAL   = 2,
    HIGH     = 3,
    REALTIME = 4,
};

struct ChannelState {
    // ... 现有字段 ...
    ChannelPriority priority = ChannelPriority::NORMAL;
};
```

CUDA stream priority 映射：`cudaStreamPriorityDefault=0` → `NORMAL`；负数→`HIGH/REALTIME`；正数→`LOW`。

### D2: Runlist 重排

`ChannelManager::nextReadyChannel()` 从 Round-Robin 改为优先级优先：

```
ChannelManager::nextReadyChannel():
  1. 扫描所有 ACTIVE 通道
  2. 选择 priority 最高的通道
  3. 同 priority 内 Round-Robin（从 last_channel 位置继续扫描）
  4. 返回选中通道
```

**不实现**：低优先级通道的饥饿保护（先保持简单，观察 TaskRunner 测试是否需要）。

### D3: 不实现 mid-batch 抢占

高优先级通道的新 batch 提交**不会**抢占正在执行的低优先级 batch。当前 batch 执行完后再重排 Runlist。

Mid-batch 抢占由 ADR-046 负责。

---

## Consequences

- ✅ 多 CUDA stream 优先级测试可用
- ✅ 不改 Puller FSM，仅改 ChannelManager 选择逻辑
- ⚠️ 无饥饿保护：低优先级通道在高负载下可能永远得不到调度
- ⚠️ 依赖 ADR-054 的 ChannelState 结构（MQD 格式），但 054 不含 priority 字段——需在 054 中预留

### Phase 5 触发条件

- ADR-054 (MQD/HQD) ✅ Accepted
- ADR-044 (ChannelManager) 已实现
- TaskRunner 提出多 stream 优先级测试需求