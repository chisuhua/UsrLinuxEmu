## Why

Stage 4.5 已交付**优先级调度 + starvation 保护**（Stage 4.4）以及**MQD/HQD state 管理**（ADR-054），但**抢占引擎**（ADR-046）与**Timeline Semaphore**（ADR-049）均处于 PROPOSED 状态未实施。本 change 闭合 Stage 4.5 剩余的两条 ADR 链：

- **抢占链**：ADR-044 + ADR-054 → ADR-045 → **ADR-046**（mid-batch context save/restore + quantum 管理）
- **跨引擎同步链**：ADR-040 → ADR-047 → **ADR-049**（timeline sem create/signal/wait/query/destroy，修订 D1 为 waiter 回调）

两条链在本阶段汇合，支撑"**抢占 + 最小跨引擎 fence**"打包交付，是 Stage 4.5 的核心能力闭合。

## What Changes

### 优先级调度升级（ADR-045）

- `ChannelManager` Round-Robin → 多级优先级队列（复用 Stage 4.4 已有结构）
- 复用现有常量 `kStarvationThreshold = 10`（基线回归验证）

### 抢占引擎（ADR-046）

- `mqd_state_preempt()` / `mqd_state_resume()` 复用 ADR-054 已交付 API
- FSM 抢占检查点**仅在 entry/batch 边界**插入（FETCH 前或 DISPATCH 后），禁止 mid-entry 抢占
- IB 嵌套（`jump_stack_`）状态下禁止抢占
- **触发与生效分离**：HIGH 优先级 batch 提交时自动标记抢占，context save 仅在当前 batch 完成边界执行
- 抢占触发为**优先级自动触发**，无用户态抢占控制入口（明确排除 `GPU_IOCTL_PREEMPT_CONTROL`）

### MQD/HQD state 扩展（ADR-054 扩展）

- `mqd_state_preempt()` / `mqd_state_resume()` API 完整化
- 对接 `mqd_state.cpp` 已交付状态机
- `ChannelSemaphoreState` 随上下文保存/恢复

### Timeline Semaphore（ADR-049 修订 D1）

- **D1 显式修订**：wait 语义由**阻塞**改为 **waiter 回调注册**（Puller 线程禁止 blocking wait，防死锁绕开 starvation 保护）
- 词汇：`sem_create(initial)` / `sem_signal(value, monotonic strict)` / `sem_wait(handle, callback)` / `sem_query(handle)` / `sem_destroy(handle)`
- `gpfifo_entry.timeline` 字段消费：batch 完成时自动 `sem_signal`，wait_value 未达时挂起
- `sem_signal` **严格大于当前值**（等于也拒绝），drv 层校验
- FIFO waiter 队列，支持多 waiter
- `sem_destroy` 存在注册 waiter 时唤醒并报错

### ADR-040 迁移

- `sim_fence_id_signal` 路径迁移到 timeline sem（作为 `sem_signal` 触发源之一）
- **清除双实现**（`grep sim/fence_id.* sim_fence_id_signal` 验证无双实现）
- `fence_create/fence_read` 薄封装：`sem_create(0)` / `sem_query()>0`，signal 由 Puller 完成回调触发

### HAL ops 扩展（ADR-023）

- 新增 ~3 个 fn-ptrs：`sem_create` / `sem_signal` / `sem_destroy`（或合并 `sem_*` 单 ops）
- 保留驱动侧 per-channel pending fence 表（fence_id → sem handle 映射），不修改 `mqd.h` 共享 ABI（ADR-035 Rule 5.1）

### Sim C-ABI Backdoor（ADR-057 D5 专项）

- backdoor 符号存在于 plugin `.so`（`nm` 验证）
- `drv/` 层不调用 backdoor（`grep -rn backdoor plugins/gpu_driver/drv/` 输出为空）
- 测试入口采用 backdoor，不新增 `GPU_IOCTL_*`

### 关键约束

- ② 驱动代码仅通过 HAL fn-ptrs 访问 ③ sim（ADR-023 边界规则）
- sem value 为 atomic 或 mutex 保护；signal release / query+waiter acquire 语义
- 公共 ioctl 头不变：`shared/gpu_ioctl.h` 无新增 ioctl 号（`GPU_IOCTL_SEM_*` 预留 0x70-0x7F 号段，本次不占用）

## Capabilities

### New Capabilities

- `preemption-engine`: 抢占引擎 — mqd_state_preempt/resume + 边界检查点 + ChannelSemaphoreState 保存/恢复
- `timeline-semaphore`: Timeline Semaphore（ADR-049 D1 修订）— sem_create/signal/wait(query/callback)/destroy + gpfifo_entry.timeline 消费 + ADR-040 迁移

### Modified Capabilities

（无现有 spec-level 行为变更；`preemption-engine-finish` 是基线回归对象，本 change 在其上**叠加**新能力，不修改其 spec）

## Impact

### 代码

- `plugins/gpu_driver/sim/hardware/mqd_state.{h,cpp}` — 扩展 `mqd_state_preempt()` / `mqd_state_resume()` API
- `plugins/gpu_driver/sim/scheduler/global_scheduler.cpp` — 多级优先级队列回归验证 + 抢占调度集成
- `plugins/gpu_driver/sim/scheduler/channel_state.{h,cpp}` — `ChannelSemaphoreState` 保存/恢复
- `plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp` — FSM 边界检查点 + sem 完成回调 + gpfifo_entry.timeline 消费
- `plugins/gpu_driver/sim/hardware/channel_manager.cpp` — 通道状态管理扩展
- `plugins/gpu_driver/sim/semaphore/` (new) — Timeline Semaphore 实现（sem state + waiter queue）
- `plugins/gpu_driver/sim/fence_id_signal.cpp` — 删除或迁移（去双实现）
- `plugins/gpu_driver/hal/gpu_hal.h` — 新增 preempt / timeline sem fn-ptrs（~3 个）
- `plugins/gpu_driver/drv/gpgpu_device.cpp` — 新增 per-channel pending fence 表（fence_id → sem handle）

### 测试

- `tests/test_preemption_standalone.cpp` (new)
- `tests/test_timeline_semaphore_standalone.cpp` (new)
- `tests/test_priority_sched_standalone.cpp` — 回归基线
- `tests/test_concurrent_preempt.cpp` (new) — 并发压力（N 次 preempt/resume × 并发 submit）

### ADR 状态

- **ADR-045** PROPOSED → **ACCEPTED**（Stage 4.4 已部分实施，本 change 闭合）
- **ADR-046** PROPOSED → **ACCEPTED**
- **ADR-047** PROPOSED → **ACCEPTED**（acquire/release 原语由 timeline sem 实现）
- **ADR-049** PROPOSED → **ACCEPTED**（D1 修订落盘：阻塞 → waiter 回调）
- **ADR-040** 迁移注记落盘：`sim_fence_id_signal` → timeline sem signal 触发源