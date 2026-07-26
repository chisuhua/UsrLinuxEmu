# ADR-048: Interrupt & Event Model

**状态**: ✅ 已采纳 (Accepted)（Phase 5，可与 ADR-044 并行）
**日期**: 2026-07-27
**提案人**: Sisyphus（GPU CP 蓝图完整性填充）
**关联 ADR**:
- [ADR-021](adr-021-hardware-puller.md) ✅ Puller FSM - interrupt_raise
- [ADR-023](adr-023-hal-interface.md) ✅ HAL 接口契约（"追加不改"扩展原则）
- [ADR-040](adr-040-puller-fence-completion.md) ✅ Fence Completion
- [ADR-042](adr-042-pushbuffer-method-encoding.md) ✅ Pushbuffer Method 编码（NOTIFY_INTR entry 定义）
- [ADR-047](adr-047-hardware-semaphore-barrier.md) 📋 Hardware Semaphore
- [ADR-055](adr-055-cp-error-handling-engine-recovery.md) ⏸️ CP 错误处理（ENGINE_HANG 依赖，Deferred-Never）
- [ADR-060](adr-060-message-notification-threading.md) ✅ kernel_workqueue（中断异步分发基础设施）
- [ADR-062](adr-062-hal-event-signal-extension.md) ✅ hal_event_signal（FENCE_SIGNALED/NOTIFY_INTR 共享通道）
**关联 Change**: 无（Phase 5 规划）

**修订**: 2026-07-27 - Oracle 评审后修订 (ADR-060/062 集成、唤醒路径补全、HAL 签名按 ADR-023 追加原则、交叉引用、ENGINE_HANG 标记 reserved、out-of-scope 明确)

---

## Context

当前 `HardwarePullerEmu::handleComplete()` 仅在 `current_entry_.release` 时调用 `hal_->interrupt_raise()`，发送一个无参数中断。

真实 GPU 的中断模型远比这丰富：

- **NVIDIA**：`NOTIFY_INTR` - 在 pushbuffer 指定位置触发中断；`REF_CNT` - 引用计数递减到 0 时触发
- **AMD**：AQL `completion_signal` - 每个 kernel dispatch packet 可指定完成信号 handle
- **Intel**：`MI_SEMAPHORE_SIGNAL` -> 写 fence value -> `MI_USER_INTERRUPT` 生成中断

### 与 ADR-040/047 的关系

- ADR-040（completion token）：Puller batch 完成时 signal fence -> **可选**触发 interrupt（poll 模式不需要）
- ADR-047（hardware semaphore）：GPU 内同步，不触发 CPU 中断
- 本 ADR（interrupt）：CPU 通知机制，是 fence completion 的 **通知通道之一**（另一个是 poll）

### 与 ADR-060/062 的关系（Oracle 评审后新增）

- [ADR-060](adr-060-message-notification-threading.md) 提供了 `kernel_workqueue` 异步分发基础设施。中断 handler **必须**通过 workqueue 异步调度，**不能**在 Puller 线程上同步执行。
- [ADR-062](adr-062-hal-event-signal-extension.md) 已定义 `hal_event_signal()` op，将事件投递到 `kfd_events_thread_` 的 `kernel_workqueue`。FENCE_SIGNALED 和 NOTIFY_INTR 中断**共享** `hal_event_signal` + workqueue 通道，**不存在**独立的中断分发管线。

---

## Decision

### D1: 中断向量

```cpp
enum class InterruptVector : uint8_t {
    FENCE_SIGNALED  = 0,  // fence 完成后触发（来自 ADR-040）
    NOTIFY_INTR     = 1,  // pushbuffer 中显式 NOTIFY_INTR entry（方法编码见 ADR-042）
    GPU_FAULT       = 2,  // GPU 页故障（Phase 6+ MMU 集成）
    ENGINE_HANG     = 3,  // 引擎挂起 -- reserved，Phase 5 不实现（依赖 ADR-055 Deferred-Never）
};
```

> **ENGINE_HANG 说明**：此向量 **reserved，不在 Phase 5 实现**。依赖 [ADR-055](adr-055-cp-error-handling-engine-recovery.md)（状态 ⏸️ Deferred-Never）。用户态 operator-level emulation 是同步执行，不存在 "hang" 场景。若未来 ROADMAP 增加 real-hw passthrough mode，ADR-055 重新打开后此向量才有意义。

### D2: FENCE_SIGNALED 中断

ADR-040 的 `sim_fence_id_signal()` 调用后，**可选择**触发中断：

```cpp
// handleComplete() 中：
if (current_index_ >= total_entries_ && pending_fence_id_ != 0) {
    sim_fence_id_signal(pending_fence_id_);
    if (interrupt_enabled_) {
        // 通过 hal_event_signal 投递到 workqueue（per ADR-062 D4）
        // 旧接口 interrupt_raise 仅传 vector，新路径走 hal_event_signal
        hal_event_signal(hal_, pasid, event_id, FENCE_SIGNALED_MASK);
    }
    pending_fence_id_ = 0;
}
```

`interrupt_enabled_` 默认 `false`（poll 模式），TaskRunner 可通过 ioctl 开启。中断与 poll 模式共存：`interrupt_enabled_ = false` 时走 busy-poll WAIT_FENCE；`interrupt_enabled_ = true` 时走中断唤醒 WAIT_FENCE（见 D6 唤醒路径）。

### D3: NOTIFY_INTR entry

`gpu_gpfifo_entry` 新增 `GPU_OP_NOTIFY_INTR` 类型（方法编码定义见 [ADR-042](adr-042-pushbuffer-method-encoding.md) §D2 Layer 2）：

```cpp
// payload: { interrupt_vector, user_data }
entry.method = GPU_OP_NOTIFY_INTR;
entry.payload[0] = InterruptVector::NOTIFY_INTR;
entry.payload[1] = user_data;  // 传给中断处理器的 cookie
```

Puller DECODE 阶段识别此 entry -> DISPATCH 直接跳到 COMPLETE -> 触发 `hal_event_signal()`（共享 ADR-062 workqueue 通道，与 FENCE_SIGNALED 走同一异步路径）。

### D4: 中断 handler 注册（vector -> handler 路由表）

drv 层注册中断 handler，`interrupt_register` 作为 **vector -> handler 路由表**：

```cpp
// hal_gpu_hal_ops 追加（Phase 5 ADR-023 扩展，遵循"追加不改"原则）
int (*interrupt_register)(void *hal_ctx, InterruptVector vec,
                           void (*handler)(InterruptVector, uint64_t user_data));
```

**路由表语义**：`interrupt_register` 将 `(vector -> handler)` 映射存入路由表。当 workqueue 取出事件后，按 vector 查表调用对应 handler。**不存在独立的中断分发管线** -- 分发复用 [ADR-062](adr-062-hal-event-signal-extension.md) 已定义的 `hal_event_signal` + `kernel_workqueue` 通道。

### D5: 异步分发与死锁防护（ADR-060 集成）

**关键设计约束**：中断 handler dispatch **必须**通过 [ADR-060](adr-060-message-notification-threading.md) 的 `kernel_workqueue` 异步执行，**禁止**在 Puller 线程上同步调用 handler。

**死锁场景分析**：

```
如果同步在 Puller 线程上调用 handler:

  Puller thread                      drv ioctl thread
  ─────────────                      ─────────────────
  handleComplete()
    -> sim_fence_id_signal()
    -> interrupt_raise()             ioctl(WAIT_FENCE, ...)
       -> handler() {                    -> wait on WaitQueue
            wake WaitQueue              }
                                         ^^^^ 永远等不到!
       }
       ^^^^ handler 内部可能需要
       获取 drv 层锁，但 drv 层
       此时正持有 ioctl 路径的锁
       -> 死锁!
```

**正确路径（异步 workqueue dispatch）**：

```cpp
// hal_mock.cpp 中 interrupt_raise_ex / hal_event_signal 实现:
int hal_event_signal(void *ctx, u32 pasid, u32 event_id, u64 events) {
    auto* wq = kfd_events_get_workqueue();  // ADR-060 kernel_workqueue
    if (!wq) return -EAGAIN;
    wq->enqueue([ctx, event_id, events, pasid]() {
        // 在 workqueue worker 线程上执行（非 Puller 线程）
        sim_signal_event(pasid, event_id, events);
        // -> 查 interrupt_register 路由表 -> 调 handler
        auto* handler = lookup_handler(vector);
        if (handler) handler(vector, user_data);
    });
    return 0;  // 立即返回，不阻塞 Puller
}
```

**为什么必须异步**：
1. drv 层在 ioctl 路径持锁（如 VFS lock、device mutex），Puller 线程同步回调会 interlock
2. Puller 线程不能阻塞 -- 它需要继续处理后续 GPFIFO entries
3. handler 可能执行耗时操作（唤醒等待队列、写 event page），不应阻塞 sim 线程

### D6: 唤醒路径（Event Model 的 "通知" 半边）

端到端中断唤醒路径：

```
① Puller 线程: handleComplete()
    -> sim_fence_id_signal(fence_id)
    -> hal_event_signal(pasid, event_id, FENCE_SIGNALED)
       │  (投递到 kernel_workqueue，立即返回)
       ▼
② workqueue worker 线程: 取出事件
    -> sim_signal_event(pasid, event_id, events)
    -> 查 interrupt_register 路由表 -> 调 handler(FENCE_SIGNALED, fence_id)
       │
       ▼
③ handler: 唤醒 WaitQueue
    -> WaitQueue::wake(fence_id)
       │  (唤醒所有等待此 fence_id 的线程)
       ▼
④ ioctl 线程: WAIT_FENCE (interrupt mode)
    -> WaitQueue::wait(fence_id) 返回
    -> ioctl(GPU_IOCTL_WAIT_FENCE) 返回 0
    -> TaskRunner 继续执行
```

**与 poll 模式对比**：

| 模式 | WAIT_FENCE 行为 | CPU 开销 | 延迟 |
|------|----------------|----------|------|
| poll (`interrupt_enabled_=false`) | busy-poll 循环读 fence_read | 高（100% CPU spin） | 低（~1us） |
| interrupt (`interrupt_enabled_=true`) | WaitQueue 阻塞睡眠，被 handler 唤醒 | 低（睡眠期间 0%） | 中（workqueue 调度 ~10us） |

poll 模式适合低延迟单元测试；interrupt 模式适合真实驱动行为模拟（TaskRunner 验证等待语义正确性）。

### D7: interrupt_raise 签名修正（ADR-023 追加不改原则）

[ADR-023](adr-023-hal-interface.md) Decision 4 明确："预留扩展空间，Phase 2 可新增但**不修改现有函数签名**"。

**当前签名**（`gpu_hal.h:48`）：
```c
void (*interrupt_raise)(void *ctx, uint32_t vector);
```

该签名**缺少 `user_data` 参数** -- 无法将 NOTIFY_INTR 的 `user_data` cookie 传递给 handler。

**修正方案**：遵循 ADR-023 "追加不改" 原则，**追加新 op** 而非修改现有签名：

```c
// gpu_hal_ops 追加（不修改 interrupt_raise）
/* ADR-048 扩展: 带上下文的中断触发 */
void (*interrupt_raise_ex)(void *ctx, uint32_t vector, uint64_t user_data);
```

**旧 op 处理**：
- `interrupt_raise(ctx, vector)` **保留但标记 deprecated** -- 内部等价于 `interrupt_raise_ex(ctx, vector, 0)`（user_data=0）
- 新代码**必须**使用 `interrupt_raise_ex`
- 现有调用方（`HardwarePullerEmu::handleComplete()`）逐步迁移到 `interrupt_raise_ex`

**inline wrapper**：
```c
static inline void hal_interrupt_raise_ex(struct gpu_hal_ops *hal,
                                           uint32_t vec, uint64_t user_data) {
    hal->interrupt_raise_ex(hal->ctx, vec, user_data);
}
```

---

## Consequences

- ✅ CPU 侧事件通知：fence completion + pushbuffer 中断
- ✅ 与 ADR-040 poll 模式共存（`interrupt_enabled_` 开关）
- ✅ 异步 workqueue dispatch 防止 Puller/drv 死锁（ADR-060 集成）
- ✅ FENCE_SIGNALED/NOTIFY_INTR 共享 ADR-062 `hal_event_signal` + workqueue 通道，无独立分发管线
- ✅ 唤醒路径完整：interrupt_raise -> workqueue -> handler -> WaitQueue -> WAIT_FENCE ioctl 返回
- ✅ HAL 签名按 ADR-023 "追加不改" 原则扩展（`interrupt_raise_ex` 追加，旧 op 保留 deprecated）
- ⚠️ HAL 新增 `interrupt_register` + `interrupt_raise_ex`，需走 ADR 流程（本 ADR 即此流程）
- ⚠️ 中断在用户态模拟器中是 workqueue 函数调用而非真实 MSI-X，语义简化
- ⚠️ ENGINE_HANG 向量 reserved，Phase 5 不实现（依赖 ADR-055 Deferred-Never）

### Phase 5 触发条件

- ADR-040 (fence completion) ✅ 已实现
- ADR-060 (kernel_workqueue) ✅ 已 Accepted
- ADR-062 (hal_event_signal) ✅ 已 Accepted
- TaskRunner 需要中断模式测试（替代 busy-poll WAIT_FENCE）

---

## Out of Scope（Phase 5 明确不做）

以下功能 **不在 Phase 5 范围**，留待后续阶段或永不实现：

| 功能 | 原因 |
|------|------|
| **Per-vector masking**（中断屏蔽寄存器） | 用户态模拟无真实中断控制器，mask 语义在 workqueue 层模拟成本高且无实际收益 |
| **MSI-X multi-vector routing** | 用户态模拟无真实 PCIe MSI-X 硬件，多向量路由在 workqueue 单 worker 下无意义 |
| **Interrupt coalescing**（中断合并） | sim 层事件频率低（每 batch 完成最多 1 次中断），合并无性能收益 |
| **ENGINE_HANG 中断处理** | 依赖 ADR-055（⏸️ Deferred-Never），operator-level emulation 不会 hang |
| **GPU_FAULT 中断** | Phase 6+ MMU 集成时实现，Phase 5 仅有枚举占位 |

---

## 讨论历史

- **2026-07-09**: 初始提案。来自 GPU CP 蓝图完整性填充。
- **2026-07-27**: Oracle 评审修订。7 项修复应用：①ADR-060 workqueue 集成（异步 dispatch 防死锁）；②ADR-062 协调（FENCE_SIGNALED/NOTIFY_INTR 共享 `hal_event_signal` + workqueue 通道，无独立管线）；③唤醒路径补全（interrupt_raise -> workqueue -> handler -> WaitQueue -> WAIT_FENCE ioctl 返回）；④`interrupt_raise` 签名修正（按 ADR-023 "追加不改" 追加 `interrupt_raise_ex`，旧 op deprecated）；⑤交叉引用补全（NOTIFY_INTR->ADR-042, dispatch->ADR-060, events->ADR-062）；⑥ENGINE_HANG 标记 reserved（依赖 ADR-055 Deferred-Never）；⑦Out-of-scope 明确（per-vector masking / MSI-X / coalescing 不在 Phase 5）。

---

**维护者**: UsrLinuxEmu Architecture Team
**最后更新**: 2026-07-27（Oracle 评审后修订，状态升 ✅ 已采纳）
