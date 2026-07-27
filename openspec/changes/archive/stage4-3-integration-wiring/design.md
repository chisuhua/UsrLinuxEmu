# Design: Stage 4.3 Integration Wiring

## Goal

将 5 个已实现的 sim 模块接入 Puller FSM 和生产路径，替换简易桩代码。

## Integration Points

### 1. CHANNEL_SWITCH FSM → Puller runLoop()

**文件**: `hardware_puller_emu.h/.cpp`

在 FSM 枚举中新增 `CHANNEL_SWITCH` 状态，插入 IDLE 和 FETCH 之间：

```
IDLE → CHANNEL_SWITCH → FETCH → DECODE → SEMAPHORE → SCHEDULE → DISPATCH → COMPLETE → CHANNEL_SWITCH
```

ChannelManager 的 `nextReadyChannel()` 确定下一个有 pending work 的通道。无 ready channel 时回退到 IDLE。

### 2. ChannelManager → runLoop()

将 Puller 的 `submitBatch()` 改为通过 ChannelManager 路由，每个通道独立跟踪 gpfifo_addr/current_index/pending_fence_id。

### 3. BAR0 HQD → bar_sim.cpp

在 BAR0 MMIO 空间（offset 0x4000+channel*64）注册 HQD 控制位：
- `HQD_CTL (0x00)`: writel(HQD_CTL_ACTIVE) → mqd_state_activate
- `HQD_CTL (0x00)`: writel(HQD_CTL_PREEMPT) → mqd_state_preempt
- `HQD_STATUS (0x04)`: readl → MQD.state

### 4. HAL Mock → interrupt ops

在 `hal_mock.cpp` 注册 `interrupt_register` + `interrupt_raise_ex`，连接到 sim/hardware/interrupt.cpp 的实现。

### 5. kernel_workqueue → interrupt.cpp

将 `std::thread(...).detach()` 替换为 `kernel_workqueue::enqueue()`（ADR-060），确保 handler 在独立工作线程中异步执行。

### 6. Puller DISPATCH → tick + timestamp

在 DISPATCH 阶段递增全局 `g_sim_tick`，对每个已调度的 entry 检查 `ts_query` handle，非零时调用 `timestamp_query_record`。

## Verification

- 9 个现有测试保持 PASS
- ctest 全量回归无新增失败
- docs-audit 无新增警告