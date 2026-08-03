# ADR-049: Cross-Engine Synchronization

**状态**: ✅ Accepted（Stage 4.5 实施修订 D1）
**日期**: 2026-07-09（首次提案），2026-07-29（D1 修订）
**提案人**: Sisyphus（GPU CP 蓝图完整性填充）
**关联 ADR**: ADR-021 (Puller FSM), ADR-044 (HyperQueue), ADR-047 (Hardware Semaphore)
**关联 Change**: `openspec/changes/stage4-5-cp-phase6-preemption-timeline-sem/`

---

## Context

当前模拟器只有 COMPUTE 引擎（GRAPHICS/COPY 引擎预留但未实现多引擎并行）。当 Phase 5+ 支持多引擎时（Compute + Copy + Graphics 独立 Puller），需要跨引擎同步：

- Compute engine 执行 kernel → 完成后 signal → Copy engine 等待 signal → 执行 memcpy
- D3D12 `D3D12_COMMAND_LIST_TYPE_COPY` → `D3D12_COMMAND_LIST_TYPE_COMPUTE` 的 fence 依赖

真实 GPU 通过 **timeline semaphore**（Vulkan `VK_SEMAPHORE_TYPE_TIMELINE` / D3D12 `ID3D12Fence`）实现跨引擎同步：
- 每个 semaphore 有 monotonically increasing `value`
- `signal(value)` 写入 value；`wait(value)` 阻塞直到 `semaphore >= value`
- 跨 engine 适用：Compute signal(v=5)，Copy wait(v=5)

当前 sim 层无此原语：ADR-040 的 sim fence 是 boolean（signaled/not signaled），ADR-047 的 hardware semaphore 是 single value（无 history）。

### 依赖声明

本 ADR 依赖 ADR-047（hardware semaphore 是 timeline semaphore 的单 slot 退化），必须在 047 ✅ 之后实施。

---

## Decision

### D1: 引入 SemaphoreManager class（waiter 回调模式）

根据 Stage 4.5 实施经验，**修订 D1 wait 语义**：由阻塞 wait 改为 waiter 回调注册（non-blocking），避免 Puller 线程阻塞导致 starvation 保护失效。

```cpp
// 实际实现: plugins/gpu_driver/sim/semaphore_manager.h (Stage 4.5)
class SemaphoreManager {
  uint64_t create(uint64_t initial);                                           // 创建
  int signal(uint64_t handle, uint64_t value);                                 // 单调递增 signal
  int wait(uint64_t handle, uint64_t expected,
           std::function<void(uint64_t)> callback, uint64_t user_data);        // 注册 waiter 回调
  uint64_t query(uint64_t handle);                                             // 查询当前值
  int destroy(uint64_t handle);                                                // 销毁
};
```

**关键差异**（修订后）：
- `wait` 改为注册 `std::function<void(uint64_t)>` 回调（非阻塞），waiter 存储为 FIFO 队列
- `signal` 递增时按 FIFO 顺序唤醒已就绪的 waiter
- 跨线程安全：`value` 为 `std::atomic<uint64_t>`（release/acquire），waiter 列表由 `std::mutex` 保护，回调在 unlock 后执行（防死锁）
- 不使用 `std::condition_variable`（原方案避免 Puller 线程阻塞）

**HAL 接口**（C 兼容）：
```c
int (*hal_sem_create)(void *ctx, uint64_t initial, uint64_t *out_handle);
int (*hal_sem_signal)(void *ctx, uint64_t handle, uint64_t value);
int (*hal_sem_wait)(void *ctx, uint64_t handle, uint64_t expected,
                    void (*callback)(uint64_t user_data), uint64_t user_data);
int (*hal_sem_query)(void *ctx, uint64_t handle, uint64_t *out_val);
int (*hal_sem_destroy)(void *ctx, uint64_t handle);
```

**fence 迁移**：
- `fence_create` → `sem_create(0)`；`fence_read` → `sem_query() > 0`
- 现有 `sim_fence_id_signal` 路径通过 `g_fence_sem_mgr` 全局指针桥接到 `sem_signal`
- ADR-049 词汇（create/signal/wait/query/destroy）作为 timeline semaphore 的标准原语

### D2: Engine 间信号传递

```
Compute Engine (Puller 1):
  kernel_dispatch(entry)
    → COMPLETE: sim_timeline_semaphore_signal(my_sem, 5)

Copy Engine (Puller 2):
  next_entry 包含 semaphore_wait：
    → FETCH: sim_timeline_semaphore_wait(my_sem, 5, timeout)
    → 阻塞直到 Compute signal
    → 继续 DECODE → memcpy dispatch
```

`gpu_gpfifo_entry` 新增 `timeline_semaphore` 字段：

```cpp
struct {
    semaphore_handle_t handle;
    uint64_t wait_value;   // 等待达到此值（0 = 不等待）
    uint64_t signal_value; // 完成后写入此值（0 = 不 signal）
} timeline;
```

### D3: 跨引擎 fence（Phase 6+）

每个引擎独立拥有 `sim_timeline_semaphore`，drv handler 负责创建和关联 semaphore handle。

```cpp
// drv: 创建 shared semaphore
semaphore_handle_t sem;
sim_timeline_semaphore_create(0, &sem);

// 提交到 Compute engine：
entry.timeline.signal_value = 5;
// 提交到 Copy engine：
entry.timeline.wait_value = 5;
```

---

## Consequences

- ✅ 跨 Compute/Copy/Graphics 引擎同步
- ✅ 与 Vulkan timeline semaphore / D3D12 fence 语义对齐
- ⚠️ 显式依赖 ADR-047（hardware semaphore 是单 slot 退化）
- ⚠️ `sim_timeline_semaphore` 新增 5 个 C-ABI 函数，增加 sim 层 API 表面积
- ⚠️ `gpu_gpfifo_entry` 新增 timeline 字段（~16 bytes）
- ⚠️ Phase 6+ multi-engine Puller 延后（见下文触发条件）— 当前 sim 仅 COMPUTE 引擎 Puller，COPY/GRAPHICS Puller 实例 + engine fence registry 待真机 driver 验证期触发

### Phase 6 触发条件

- ADR-047 (hardware semaphore) ✅ Accepted
- 多引擎 Puller（ADR-044 ChannelManager with multi-engine）已实现
- TaskRunner 需要跨引擎 fence 测试（compute → copy pipeline）

### Phase 6+ 触发条件（deferred follow-up — multi-engine Puller）

Phase 6（timeline semaphore 基础实现）已交付。但跨引擎 fence 完整 D3（per-engine sim_timeline_semaphore + drv handler 创建 shared semaphore + multi-engine Puller 真实并行执行）需以下条件触发：

1. TaskRunner 真实机驱动验证：CUDA/HIP 多引擎 pipeline（compute → copy → graphics 真实驱动并行执行）
2. sim 层注册 COPY/GRAPHICS 引擎 Puller 实例（当前仅 COMPUTE）
3. TaskRunner 端提交 Compute+Cpy+GFX 混合 batch，验证 engine fence registry 生效

**未触发**前的当前状态：`test_cross_engine_sync_standalone` 显式延后，需单独立项；主线保留 timeline semaphore 基础（最小跨引擎 fence）已交付（per ADR-049 D2 + stage4-5-cp-phase6-preemption-timeline-sem archive）。

**关联 ADR**：ADR-044（multi-channel HyperQueue scheduling — multi-engine 调度底座）+ ADR-049 本 ADR。