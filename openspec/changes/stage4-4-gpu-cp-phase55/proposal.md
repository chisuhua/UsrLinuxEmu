## Why

Stage 4 路线图定义 GPU 命令处理器从 Phase 4 到 Phase 7 的分阶段演进。当前 Phase 4（图启动真实化）和 Phase 4.1-4.3（BAR/ioremap/DMA 一致性）已完成。本 change 推进到 **Phase 5.5：优先级调度 + 硬件同步原语 + 间接缓冲区**。

当前 `GlobalScheduler` 仅支持 FIFO 调度，Puller FSM 缺少硬件 semaphore WAIT/RELEASE 和 Indirect Buffer 跳转能力。这三个能力是后续 Phase 6（抢占/跨引擎）和 Phase 7（Green Context）的前置依赖。

## What Changes

- **Priority Scheduling**: `ChannelState` 新增优先级字段（IDLE/LOW/NORMAL/HIGH/REALTIME 5 级），`GlobalScheduler` Runlist 按优先级重排 dispatch
- **Semaphore/Barrier**: Puller FSM FETCH 阶段实现 semaphore WAIT（阻塞直到条件满足），COMPLETE 阶段实现 semaphore RELEASE；支持 Barrier AND/OR 跨 stream 同步
- **Indirect Buffer**: GPFIFO entry 新增 JUMP 指令类型，Puller 支持跳转到另一个 pushbuffer 地址继续 FETCH；IB reference 数据结构
- 配套测试 `test_priority_sched_standalone`、`test_semaphore_barrier_standalone`、`test_indirect_buffer_standalone`

## Capabilities

### New Capabilities
- `priority-scheduling`: ChannelState priority 字段 + GlobalScheduler Runlist 重排
- `semaphore-barrier`: Puller FSM semaphore WAIT/RELEASE + Barrier AND/OR 同步
- `indirect-buffer`: GPFIFO JUMP 指令 + Puller 跳转 + IB reference 管理

### Modified Capabilities
- (无现有 spec-level 行为变更)

## Impact

- `plugins/gpu_driver/sim/scheduler/`: GlobalScheduler Runlist 重排逻辑
- `plugins/gpu_driver/sim/hardware/`: Puller FSM FETCH/DECODE/COMPLETE 阶段扩展
- `plugins/gpu_driver/shared/gpu_queue.h`: GPFIFO entry 格式扩展（priority + IB flag）
- `tests/`: 3 个新 standalone 测试
