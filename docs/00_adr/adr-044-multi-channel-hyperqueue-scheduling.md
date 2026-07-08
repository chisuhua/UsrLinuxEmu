# ADR-044: 多通道调度与 HyperQueue 语义

**状态**: 📋 PROPOSED（Phase 5+，不阻塞 Phase 4）
**日期**: 2026-07-09
**提案人**: Sisyphus（GPU 命令处理器架构完整性审查）
**关联 ADR**: ADR-021 (Hardware Puller), ADR-024 (User Mode Queue), ADR-036 (3-way separation)
**关联 Change**: 无（Phase 5 规划）

---

## Context

当前 `HardwarePullerEmu` 是单通道顺序消费模型：

```
submitBatch(gpfifo_addr, count) → 设置单一批次
runLoop() → 逐条 fetch/decode/dispatch → handleComplete()
→ 下一 batch
```

所有 32 个队列（`MAX_QUEUES=32`）共享同一个 Puller 实例，但 Puller 一次只处理一个 batch。没有通道切换、时间片、或调度公平性机制。

真实 GPU 的多队列调度远比这复杂：

- **NVIDIA Kepler+ (HyperQueue)**：32 路硬件工作队列复用。PBDMA Puller 通过 Runlist（`RAMRL` 寄存器 + `RAMFC` context save area）在多个通道间时间片切换。`GP_PUT`/`GP_GET` 指针协调生产/消费。
- **AMD MES (Micro Engine Scheduler)**：两级调度——固件（MES）决策用户队列 → 硬件队列映射 + 硬件 Queue Manager 选择就绪队列。支持 over-subscription（map/unmap）+ Aggregated Doorbell。
- **Intel Execlist**：每引擎独立的 Submission Queue（最多 8 context），Load 命令触发上下文切换。

当 TaskRunner 需要测试多 Stream 并行提交（多个 CUDA stream 同时 submit）时，单通道模型无法模拟多流间的调度竞争（如 stream 优先级、不同 queue 的交替消费）。

### 约束

- 不在 Phase 4 实现（不阻塞 `sim-graph-launch-real-impl`）
- 不实现完整的 MES/MQD/HQD 硬件状态——仅模拟"多个通道公平调度"的核心语义
- 当前 Puller FSM（ADR-021）已支持 7 个状态，需新增 `CHANNEL_SWITCH` 状态

---

## Decision

### D1: 引入 ChannelManager

```cpp
// sim/hardware/channel_manager.h — Phase 5 新增
struct ChannelState {
    uint32_t channel_id;
    GpuQueueEmu* queue;              // 绑定的队列实例
    enum { IDLE, ACTIVE, PREEMPTED } state;
    uint64_t gpfifo_addr;            // 当前 batch 的 GPFIFO 地址
    uint32_t current_index;          // 当前消费到的 entry 索引
    uint32_t total_entries;          // 当前 batch 的 entry 总数
    uint64_t pending_fence_id;       // 当前 batch 的 fence（继承 ADR-040）
    uint64_t time_slice_start;       // 时间片起始 tick
    uint32_t entries_consumed;       // 本时间片已消费 entry 数
};

class ChannelManager {
public:
    static constexpr uint32_t MAX_CHANNELS = 32;  // 对应 MAX_QUEUES
    static constexpr uint32_t TIME_SLICE_ENTRIES = 1024;  // 每时间片最多消费 1024 条 entry

    int registerChannel(uint32_t channel_id, GpuQueueEmu* queue);
    void submitBatch(uint32_t channel_id, uint64_t gpfifo_addr, uint32_t entry_count, uint64_t fence_id);
    std::optional<ChannelState*> nextReadyChannel();  // 轮转选择下一个就绪通道
    void yieldChannel(uint32_t channel_id);            // 当前通道时间片耗尽
    bool hasWork();                                     // 任何通道有待处理 batch
};
```

### D2: Runlist 调度 — 简单轮转

调度策略：Round-Robin，每通道每次获得 `TIME_SLICE_ENTRIES=1024` 条 entry 的时间片。

```
ChannelManager::nextReadyChannel():
  for i in 0..(MAX_CHANNELS-1):
    ch = &channels[(last_channel + i) % MAX_CHANNELS]
    if ch.state == ACTIVE && ch.current_index < ch.total_entries:
      last_channel = ch.id
      return ch
  return nullopt

HardwarePullerEmu::runLoop():
  while(true):
    ch = channel_manager.nextReadyChannel()
    if !ch: wait on CV; continue

    // 切换通道：保存当前 Puller 状态到 ch，从 ch 恢复 Puller 状态
    restoreChannel(ch)
    处理 ch 的 entries（最多 TIME_SLICE_ENTRIES 条）
    if entries_consumed >= TIME_SLICE_ENTRIES:
      saveChannel(ch)  // 保存进度
      channel_manager.yieldChannel(ch.id)  // 放回队列尾部
    elif ch.current_index >= ch.total_entries:
      // batch 完成：signal fence
      sim_fence_id_signal(ch.pending_fence_id)
      ch.state = IDLE
```

### D3: 不实现（Phase 5 scope 外）

| 概念 | 理由 |
|------|------|
| **优先级抢占**（mid-batch preemption） | 需要 MQD/HQD 硬件状态 save/restore，复杂度高 |
| **MQD/HQD 状态管理** | 真硬件对应 VRAM object，模拟成本高 |
| **Aggregated Doorbell** | 需要 over-subscription 模型，Phase 3 无此需求 |
| **Over-subscription**（用户队列数 > 硬件队列数） | 同上 |
| **时间片配置**（每通道不同时间片） | 先统一 1024 entries，后续按优先级扩展 |

### D4: Puller FSM 变更

在现有 7 状态基础上新增 `CHANNEL_SWITCH` 状态：

```
IDLE → CHANNEL_SWITCH（新）→ FETCH → DECODE → SCHEDULE → DISPATCH → COMPLETE
                                       ↑                                    │
                                       └──── 时间片耗尽则回 CHANNEL_SWITCH ──┘
```

`CHANNEL_SWITCH` 状态职责：
1. 从 `ChannelManager::nextReadyChannel()` 获取下一就绪通道
2. 从 `ChannelState` 恢复 `current_gpfifo_addr_`、`current_index_`、`total_entries_`、`pending_fence_id_`
3. 若当前通道的 batch 已完成：signal fence → 标记 IDLE → 重新选择通道
4. 若无就绪通道：wait on CV

---

## Consequences

### 正面

- ✅ 为多 Stream 并行测试提供硬件调度语义
- ✅ 与 HyperQueue 概念对齐：32 通道、Round-Robin 调度、时间片
- ✅ 不增加 Puller 单 batch 处理的核心复杂度（entry 处理逻辑不变）

### 负面

- ⚠️ Puller FSM 新增 `CHANNEL_SWITCH` 状态，增加复杂度
- ⚠️ 需要性能分析确认时间片粒度（1024 entries）是否合理：
  - 太粗：调度不公平，一个 batch 占满 Puller
  - 太细：通道切换开销大（保存/恢复状态）
- ⚠️ 不实现 MES 语义 → 与真实 AMD GPU 行为有偏差，但不影响 CUDA 路径的 TaskRunner 测试
- ⚠️ `ChannelManager` 引入新文件，需更新 CMakeLists.txt
- ⚠️ 现有测试假设 Puller 是单通道模型，需更新

### 迁移

1. Phase 5：新增 `sim/hardware/channel_manager.h/.cpp`
2. `HardwarePullerEmu` 增加 `CHANNEL_SWITCH` 状态
3. `GpuQueueEmu::submit()` 改为调用 `channel_manager.submitBatch()`
4. 更新 `test_hardware_puller_emu_standalone`：增加多通道交替调度测试
5. 新增 `test_channel_manager_standalone`：Round-Robin 公平性测试

---

## 讨论历史

- **2026-07-09**: 初始提案。来自 GPU 命令处理器架构完整性审查：识别出单通道模型无法模拟 HyperQueue 多流并行调度。
- 真实硬件参考：
  - NVIDIA [envytools FIFO/Puller](https://envytools.readthedocs.io/en/latest/hw/fifo/)：RAMRL runlist、RAMFC context save、TSG timeslice
  - AMD [MES spec](https://gpuopen.com/download/micro_engine_scheduler.pdf)：两级调度、over-subscription
  - NVIDIA [Open GPU Doc `dev_ram.ref.txt`](https://github.com/nvidia/open-gpu-doc)：`NV_PFIFO_RUNLIST_BASE`、`RUNQUEUE_SELECTOR`
  - Intel PRM Vol 8：Execlist Submission Queue、Load 命令上下文切换
  - [external/TaskRunner/plans/archive/gpu_queue_architecture_research.md](../external/TaskRunner/plans/archive/gpu_queue_architecture_research.md)：AMD CDNA2 ACE vs NVIDIA HyperQ 对比研究