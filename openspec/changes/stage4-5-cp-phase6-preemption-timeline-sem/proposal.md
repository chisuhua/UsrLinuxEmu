## Why

Stage 4.5 是 GPU 驱动抢占与跨引擎同步能力的收口阶段。当前 GlobalScheduler 仅支持 Round-Robin + 基础优先级（Stage 4.4），缺少：① 多级优先级调度 +  starvation 保护（ADR-045），② mid-batch 抢占与 context save/restore（ADR-046），③ 跨引擎同步原语 timeline semaphore（ADR-049），④ ADR-040 fence 路径迁移到 timeline sem 消除双实现。这 4 个能力是支撑后续多引擎调度（Stage 5+）的前置条件。

## What Changes

- **Priority Scheduling 升级**：ChannelManager Round-Robin → 多级优先级队列 + starvation 保护（`kStarvationThreshold=10`，复用现有常量）
- **Preemption Engine**：mid-batch context save/restore（基于 ADR-054 MQD/HQD state）+ quantum 管理 + puller FSM 抢占检查点（entry/batch 边界）
- **Timeline Semaphore**：`sem_create/signal/wait/query/destroy` 原语，waiter 回调（非阻塞）FIFO 唤醒，`gpfifo_entry.timeline` 字段消费
- **ADR-040 迁移**：`sim_fence_id_signal` 路径迁移到 timeline sem，消除双实现
- **HAL ops 扩展**：新增 preempt / timeline sem 相关 ~3 个函数指针
- **Sim C-ABI Backdoor**：测试用 backdoor 符号（ADR-057 D5），不经 `GPU_IOCTL_*` 暴露
- **测试**：`test_preemption_standalone` + `test_timeline_semaphore_standalone` + 并发压力测试

## Capabilities

### New Capabilities
- `priority-scheduling`: 多级优先级队列调度 + starvation 保护
- `preemption-engine`: mid-batch context save/restore + quantum 管理 + 抢占检查点
- `mqd-hqd-state-ops`: `save_context()` / `restore_context()` API，对接 `mqd_state.cpp`
- `timeline-semaphore`: `sem_create/signal/wait/query/destroy` 原语 + waiter 回调
- `adr-040-migration`: `sim_fence_id_signal` → timeline sem signal 触发源

### Modified Capabilities
- `hal-ops`: 新增 preempt / timeline sem fn-ptrs

## Impact

- `plugins/gpu_driver/sim/hardware/mqd_state.{h,cpp}` — context save/restore 扩展
- `plugins/gpu_driver/sim/scheduler/global_scheduler.cpp` — Round-Robin → 多级优先级 + preemption engine
- `plugins/gpu_driver/sim/scheduler/channel_state.{h,cpp}` — 通道运行态，抢占切换点
- `plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp` — FSM 中插入抢占检查点
- `plugins/gpu_driver/sim/hardware/channel_manager.cpp` — 通道状态管理扩展
- `plugins/gpu_driver/hal/gpu_hal.h` — HAL ops 新增 preemption/fence fn-ptrs
- `plugins/gpu_driver/drv/gpgpu_device.cpp` — 新 ioctl handler 入口（可选）
- `plugins/gpu_driver/shared/gpu_queue.h` — `gpfifo_entry.timeline` 字段
- `plugins/gpu_driver/shared/gpu_ioctl.h` — 内部保留区 0x68-0x6F
- `docs/00_adr/adr-049.md` — 修订 D1 wait 语义
- `docs/00_adr/adr-040.md` — 迁移注记
