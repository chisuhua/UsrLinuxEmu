# ADR-044: 多通道调度与 HyperQueue 语义

**状态**: ✅ 已采纳 (Accepted)（Phase 5+，不阻塞 Phase 4）
**日期**: 2026-07-27
**提案人**: Sisyphus（GPU 命令处理器架构完整性审查）
**关联 ADR**: ADR-021 (Hardware Puller), ADR-024 (User Mode Queue), ADR-036 (3-way separation)
**关联 Change**: 无（Phase 5 规划）
**修订**: 2026-07-27 - Oracle 评审后修订 (MAX_QUEUES 修正、scanQueues 关系澄清、线程安全声明、FSM 图补 SEMAPHORE)

---

## Context

当前 `HardwarePullerEmu` 是单通道顺序消费模型：

```
submitBatch(gpfifo_addr, count) → 设置单一批次
runLoop() → 逐条 fetch/decode/dispatch → handleComplete()
→ 下一 batch
```

当前 Puller 的 `doorbell_emu.h` 定义 `MAX_QUEUES = 1024`（doorbell 通知槽数），但 `ChannelManager` 的 `MAX_CHANNELS = 32` 是**有意为之的 HyperQueue 对齐**：对应 NVIDIA Kepler+ 的 32 路硬件工作队列复用，而非 doorbell 槽位的当前限制。Phase 5 实现时 ring-buffer 队列本身成为 channel，`MAX_CHANNELS` 约束的是 Puller 可同时调度的活跃通道数，与 doorbell 的 1024 槽不冲突。

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
    static constexpr uint32_t MAX_CHANNELS = 32;  // HyperQueue 对齐（非 MAX_QUEUES 限制，doorbell MAX_QUEUES=1024）
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

### D2.1: 与 scanQueues 的关系

`HardwarePullerEmu` 当前（Phase 4 前已存在）有 `scanQueues()` 方法，用于在 ring-buffer 队列路径上轮询已注册 `GpuQueueEmu` 队列，找到有 pending entry 的队列。

Phase 5 引入 `ChannelManager` 后，**`CHANNEL_SWITCH` 状态替代/吸收 scanQueues 用于 ioctl submitBatch 路径**：

- **ring-buffer 队列路径**：现有 `scanQueues()` 继续用于 `GpuQueueEmu` 的 ring buffer 消费。Phase 5 中这些 ring-buffer 队列本身成为 channel（`ChannelState.queue` 指向 `GpuQueueEmu*`），`scanQueues` 的轮询逻辑被 `ChannelManager::nextReadyChannel()` 的 Round-Robin 取代。
- **ioctl submitBatch 路径**：`submitBatch()` 直接注册 channel，不再走 `scanQueues`。`CHANNEL_SWITCH` 负责通道选择，替代 scanQueues 的"扫描所有队列找 pending"线性查找。
- **迁移策略**：Phase 5 实现时 `scanQueues()` 可保留为 `CHANNEL_SWITCH` 内部实现细节（scanQueues 找到 queue_id -> 映射到 channel_id），或重构为 `ChannelManager::nextReadyChannel()` 的直接调用。前者向后兼容，后者更清晰。

### D2.2: ChannelManager 线程安全

`ChannelManager` 跨两个线程上下文：

| 线程 | 操作 | ChannelManager 方法 |
|------|------|---------------------|
| **ioctl 线程** | 写入：注册通道、提交 batch | `registerChannel()`, `submitBatch()` |
| **Puller 线程** | 读取：选择就绪通道、yield 通道 | `nextReadyChannel()`, `yieldChannel()` |

**约束**：ioctl 线程的写操作（register/submit）必须与 Puller 线程的读操作（nextReadyChannel/yieldChannel）互斥保护。

**实现模式**：采用 Issue #21 的 snapshot 模式。`HardwarePullerEmu::scanQueues()` 在 Issue #21 修复中已使用 `mutex_` 下的 snapshot 模式（拷贝 `(qid, queue*)` 对），避免在遍历过程中被并发 register/unregister 修改。`ChannelManager` 应遵循同一模式：

```cpp
// ChannelManager 内部
std::mutex channel_mutex_;

std::optional<ChannelState*> nextReadyChannel() {
    std::lock_guard<std::mutex> lock(channel_mutex_);
    // snapshot：拷贝 channel 列表副本后再遍历选择
    // 避免遍历期间 registerChannel/submitBatch 修改 channel_states_
}
```

参考 Issue #21 regression test（`test_hardware_puller_emu_concurrent_regression_standalone.cpp`）验证 snapshot 模式在并发 register/unregister 下的安全性。

### D2.3: 与 GlobalScheduler 的分层关系

`ChannelManager`（本 ADR 引入）与 `GlobalScheduler`（已存在）是两个不同层次的调度抽象，无背压交互：

| 层次 | 组件 | 职责 | 时机 |
|------|------|------|------|
| **Fetch-pre 仲裁** | `ChannelManager` | 多通道 Round-Robin 选择 + 时间片切换 | `CHANNEL_SWITCH` 状态：决定**从哪个通道 fetch 下一条 entry** |
| **Post-decode 引擎派发** | `GlobalScheduler` | 引擎类型选择（COMPUTE/COPY/GRAPHICS）+ 入队 | `SCHEDULE` 状态：决定 decode 后的 entry **派发到哪个引擎** |

`CHANNEL_SWITCH` 是 **fetch 前的通道仲裁**，`GlobalScheduler` 是 **decode 后的引擎派发**。两者正交：

```
CHANNEL_SWITCH (选通道) -> FETCH -> DECODE -> SCHEDULE -> GlobalScheduler.enqueue(entry, selectEngine(entry))
```

`ChannelManager` 不产生 backpressure 给 `GlobalScheduler`，反之亦然。`GlobalScheduler` 的队列溢出不影响 `ChannelManager` 的通道切换决策。

### D3: 不实现（Phase 5 scope 外）

| 概念 | 理由 |
|------|------|
| **优先级抢占**（mid-batch preemption） | 需要 MQD/HQD 硬件状态 save/restore，复杂度高 |
| **MQD/HQD 状态管理** | 真硬件对应 VRAM object，模拟成本高 |
| **Aggregated Doorbell** | 需要 over-subscription 模型，Phase 3 无此需求 |
| **Over-subscription**（用户队列数 > 硬件队列数） | 同上 |
| **时间片配置**（每通道不同时间片） | 先统一 1024 entries，后续按优先级扩展 |

### D4: Puller FSM 变更

在现有 7 状态基础上新增 `CHANNEL_SWITCH` 状态。当前 Puller FSM（per `hardware_puller_emu.h`）包含 7 状态：`IDLE -> FETCH -> DECODE -> SCHEDULE -> DISPATCH -> SEMAPHORE -> COMPLETE`。`SEMAPHORE` 状态在 `DECODE` 之后、`SCHEDULE` 之前触发（当 entry 的 `release` 标志为 true 时），用于等待信号量。Phase 5 新增 `CHANNEL_SWITCH` 作为通道调度入口：

```
IDLE -> CHANNEL_SWITCH（新）-> FETCH -> DECODE -> SEMAPHORE（条件）-> SCHEDULE -> DISPATCH -> COMPLETE
                                        │                     ↑                                     │
                                        └── 无 release ────────┘                                     │
                                                               └──── 时间片耗尽则回 CHANNEL_SWITCH ──┘
```

`SEMAPHORE` 状态流程：`DECODE` 后检查 `entry.release` 标志 -> 若 true 则进入 `SEMAPHORE` 等待信号量 -> 信号量满足后进入 `SCHEDULE` -> `DISPATCH`。若 `release` 为 false，直接 `DECODE -> SCHEDULE`。

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