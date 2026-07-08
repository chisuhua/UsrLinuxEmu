# ADR-048: Interrupt & Event Model

**状态**: 📋 PROPOSED（Phase 5，可与 ADR-044 并行）
**日期**: 2026-07-09
**提案人**: Sisyphus（GPU CP 蓝图完整性填充）
**关联 ADR**: ADR-021 (Puller FSM — interrupt_raise), ADR-040 (Fence Completion), ADR-047 (Hardware Semaphore)
**关联 Change**: 无（Phase 5 规划）

---

## Context

当前 `HardwarePullerEmu::handleComplete()` 仅在 `current_entry_.release` 时调用 `hal_->interrupt_raise()`，发送一个无参数中断。

真实 GPU 的中断模型远比这丰富：

- **NVIDIA**：`NOTIFY_INTR` — 在 pushbuffer 指定位置触发中断；`REF_CNT` — 引用计数递减到 0 时触发
- **AMD**：AQL `completion_signal` — 每个 kernel dispatch packet 可指定完成信号 handle
- **Intel**：`MI_SEMAPHORE_SIGNAL` → 写 fence value → `MI_USER_INTERRUPT` 生成中断

### 与 ADR-040/047 的关系

- ADR-040（completion token）：Puller batch 完成时 signal fence → **可选**触发 interrupt（poll 模式不需要）
- ADR-047（hardware semaphore）：GPU 内同步，不触发 CPU 中断
- 本 ADR（interrupt）：CPU 通知机制，是 fence completion 的 **通知通道之一**（另一个是 poll）

---

## Decision

### D1: 中断向量

```cpp
enum class InterruptVector : uint8_t {
    FENCE_SIGNALED  = 0,  // fence 完成后触发（来自 ADR-040）
    NOTIFY_INTR     = 1,  // pushbuffer 中显式 NOTIFY_INTR entry
    GPU_FAULT       = 2,  // GPU 页故障（Phase 6+ MMU 集成）
    ENGINE_HANG     = 3,  // 引擎挂起（ADR-055，标 Never）
};
```

### D2: FENCE_SIGNALED 中断

ADR-040 的 `sim_fence_id_signal()` 调用后，**可选择**触发中断：

```cpp
// handleComplete() 中：
if (current_index_ >= total_entries_ && pending_fence_id_ != 0) {
    sim_fence_id_signal(pending_fence_id_);
    if (interrupt_enabled_) {
        hal_->interrupt_raise(InterruptVector::FENCE_SIGNALED, pending_fence_id_);
    }
    pending_fence_id_ = 0;
}
```

`interrupt_enabled_` 默认 `false`（poll 模式），TaskRunner 可通过 ioctl 开启。

### D3: NOTIFY_INTR entry

`gpu_gpfifo_entry` 新增 `GPU_OP_NOTIFY_INTR` 类型：

```cpp
// payload: { interrupt_vector, user_data }
entry.method = GPU_OP_NOTIFY_INTR;
entry.payload[0] = InterruptVector::NOTIFY_INTR;
entry.payload[1] = user_data;  // 传给中断处理器的 cookie
```

Puller DECODE 阶段识别此 entry → DISPATCH 直接跳到 COMPLETE → 触发 `hal_->interrupt_raise()`。

### D4: 中断 handler 注册

drv 层注册中断 handler：

```cpp
// hal_gpu_hal_ops 新增（Phase 5 ADR-023 扩展）
int (*interrupt_register)(void *hal_ctx, InterruptVector vec,
                           void (*handler)(InterruptVector, uint64_t user_data));
```

---

## Consequences

- ✅ CPU 侧事件通知：fence completion + pushbuffer 中断
- ✅ 与 ADR-040 poll 模式共存（中断可选）
- ⚠️ HAL 新增 `interrupt_register` 函数，需走 ADR 流程
- ⚠️ 中断在用户态模拟器中是函数调用而非真实 MSI-X，语义简化

### Phase 5 触发条件

- ADR-040 (fence completion) ✅ 已实现
- TaskRunner 需要中断模式测试（替代 busy-poll WAIT_FENCE）