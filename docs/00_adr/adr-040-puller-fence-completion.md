# ADR-040: HardwarePullerEmu Fence Completion 回调机制

**状态**: ✅ Accepted
**日期**: 2026-07-09
**提案人**: Sisyphus（Phase 4 sim-graph-launch-real-impl 架构审查）
**关联 ADR**: ADR-021 (Hardware Puller FSM), ADR-023 (HAL Interface), ADR-024 (User Mode Queue), ADR-036 (3-way separation)
**关联 Change**: `openspec/changes/2026-07-15-phase4-sim-graph-launch-real-impl/`

---

## Context

当前 `HardwarePullerEmu::handleComplete()`（`sim/hardware/hardware_puller_emu.cpp:241-246`）仅当 `current_entry_.release` 置位时调用 `hal_->interrupt_raise()`，**不 signal 任何 fence**。

这导致两条提交路径的 fence 完成机制都存在缺陷：

1. **Pushbuffer 路径**：`handlePushbufferSubmitBatch` 通过 `hal_fence_create` 创建 HAL fence 并返回给用户，但 Puller 从不 signal 该 fence。`handleWaitFence` 将永远超时。

2. **Graph launch 路径**（PoC）：`sim_graph_launch`（`sim/graph.cpp:153-170`）绕过此问题——分配 sim fence 后**立即 signal**，不走 Puller 路径。但这不是真正的异步执行。

Phase 4 需要将 `sim_graph_launch` 改为通过 `GpuQueueEmu::submit()` → Puller 路径异步执行，同时修复 pushbuffer 路径的 fence 信号缺失。

### 约束

- Puller 运行在独立后台线程（`runLoop()`），fence signal 必须在 Puller 线程内完成
- HAL fence（范围 `[1, 2³²-1]`）和 sim fence（范围 `[2³², INT64_MAX]`）是两个独立命名空间。Puller 作为 sim 层组件，应只管理 sim fence
- `GpuQueueEmu::submit()` 当前签名不包含 fence_id 参数
- `HardwarePullerEmu::submitBatch()` 当前签名不包含 fence_id 参数

---

## Decision

### D1: fence_id 通过 submitBatch 传入 Puller

`HardwarePullerEmu::submitBatch()` 签名变更：

```cpp
// 旧签名
void submitBatch(uint64_t gpfifo_addr, uint32_t entry_count);

// 新签名
void submitBatch(uint64_t gpfifo_addr, uint32_t entry_count, uint64_t fence_id = 0);
```

`fence_id=0` 表示"不触发 fence 完成回调"，保持向后兼容。非零值时存入成员 `pending_fence_id_`。

新增成员：

```cpp
uint64_t pending_fence_id_ = 0;  // sim fence to signal on batch completion
```

### D2: fence signal 在 handleComplete 中触发

`handleComplete()` 中，当前 entry dispatch 完成后，检测 batch 是否全量完成：

```cpp
// 在 handleComplete() 末尾，current_index_ 递增之后：
if (current_index_ >= total_entries_ && pending_fence_id_ != 0) {
    sim_fence_id_signal(pending_fence_id_);
    pending_fence_id_ = 0;  // 单次触发，防止重复 signal
}
```

**时机语义**：最后一条 entry 进入 COMPLETE 状态后 signal。此时 entry 已被 dispatcher 确认，对应 kernel 已被 scheduler 调度。

> **概念澄清**：`sim_fence_id` 是 **completion token**（batch 完成事件），不是 **hardware semaphore**（GPU 内部同步原语）。Hardware semaphore 由 ADR-021 §决策 3（简单模型）和 ADR-047（扩展模型）定义。两者关系：
> - Completion token（本 ADR）：drv 层创建，Puller 在 batch 完成时 signal，drv 通过 WAIT_FENCE 轮询。用于 CPU 侧同步。
> - Hardware semaphore（ADR-021/047）：Puller FSM 内消费，用于 GPU 内 entry 间同步（WAIT/RELEASE）。
> - Timeline semaphore（ADR-049）：跨引擎同步，是 hardware semaphore 的跨引擎扩展。

### D3: Puller 只管理 sim fence

Puller 是 sim 层组件（③），不管理 HAL fence（②↔③ 桥接层）。

- Pushbuffer 路径：handler（`handlePushbufferSubmitBatch`）在走 Puller 路径时改用 `sim_fence_id_alloc()` 替代 `hal_fence_create()`，将 sim fence_id 传入 `submitBatch()`
- Graph launch 路径：handler（`handleGraphLaunch`）调用 `sim_fence_id_alloc()`，将 sim fence_id 传入
- `WAIT_FENCE` 逻辑需同步适配：sim fence 通过 `sim_fence_id_check()` 查询，HAL fence 通过 `hal_fence_read()` 查询

### D4: GpuQueueEmu 适配

`GpuQueueEmu::submit()` 签名变更以透传 fence_id：

```cpp
// 旧签名（gpu_queue_emu.h:113）
int submit(uint64_t gpfifo_addr, uint32_t entry_count);

// 新签名
int submit(uint64_t gpfifo_addr, uint32_t entry_count, uint64_t fence_id = 0);
```

实现透传：

```cpp
int GpuQueueEmu::submit(uint64_t gpfifo_addr, uint32_t entry_count, uint64_t fence_id) {
    if (!puller_) return -ENODEV;
    puller_->submitBatch(gpfifo_addr, entry_count, fence_id);
    return 0;
}
```

---

## Consequences

### 正面

- ✅ Pushbuffer 和 graph launch 两条路径的 fence 完成机制统一通过 Puller
- ✅ 修复现有 pushbuffer 路径 fence 永不 signal 的 bug
- ✅ `sim_graph_launch` 可实现真正的异步执行（fence 不在 launch 时立即 signal）
- ✅ 架构简洁：Puller 仅管理 sim fence，职责清晰

### 负面

- ⚠️ **破坏性 API 变更**：`HardwarePullerEmu::submitBatch()` 和 `GpuQueueEmu::submit()` 签名变化，影响所有调用方：
  - `GpuQueueEmu::submit()` 调用方（`gpgpu_device.cpp:334`）
  - `HardwarePullerEmu::submitBatch()` 测试代码
- ⚠️ Pushbuffer 路径 handler 需从 HAL fence 切换到 sim fence，`WAIT_FENCE` 逻辑需同步调整
- ⚠️ `test_hardware_puller_emu_standalone` 和 `test_gpu_ioctl_standalone` 需增加 fence signal 验证用例
- ⚠️ `fence_id=0` 哨兵值约定需文档化（`0` 是合法 sim fence 值吗？sim fence 范围 `[2³², INT64_MAX]`，`0` 不在范围内，安全）

### 迁移

1. `HardwarePullerEmu` 增加 `pending_fence_id_` 成员 + `handleComplete()` 中 signal 逻辑
2. `GpuQueueEmu::submit()` 增加 `fence_id` 参数并透传
3. `handlePushbufferSubmitBatch` 改为 `sim_fence_id_alloc()` + 传入 `submit()`
4. `handleGraphLaunch`（Phase 4 新实现）使用 `sim_fence_id_alloc()` + 传入 `submit()`
5. 更新 `handleWaitFence` 以支持 sim fence 查询
6. 更新所有受影响的测试

---

## 讨论历史

- **2026-07-09**: 初始提案。来自 Phase 4 `sim-graph-launch-real-impl` Metis 审查（session `ses_0be73d6d1ffeD9w0kS5ukUX6ND`）：识别出 Puller 无 fence completion 机制为阻塞问题。
- **2026-07-09 (升级为 Accepted)**：基于 Metis 二次审查 + 代码追踪验证（`hardware_puller_emu.cpp:241-246` `handleComplete()` 仅发中断不 signal fence；`HardwarePullerEmu` 独立 runLoop 线程模型；sim/HAL fence 双命名空间 `[1,2³²-1]` vs `[2³²,INT64_MAX]`）。线程安全：`pending_fence_id_` 在 batch 完成后由 `handleComplete()` 读取，与 `submitBatch()` 写入时序无重叠（D2 的 `current_index_ >= total_entries_` 检测保证）。`fence_id=0` 哨兵安全（不在 sim fence 范围内）。Oracle 审查因任务超时未返回，采用 Metis 审查 + 代码直接验证作为升级依据。