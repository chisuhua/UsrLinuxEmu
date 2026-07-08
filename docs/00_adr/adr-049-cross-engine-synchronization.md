# ADR-049: Cross-Engine Synchronization

**状态**: 📋 PROPOSED（Phase 6，ADR-047 之后）
**日期**: 2026-07-09
**提案人**: Sisyphus（GPU CP 蓝图完整性填充）
**关联 ADR**: ADR-021 (Puller FSM), ADR-044 (HyperQueue), ADR-047 (Hardware Semaphore)
**关联 Change**: 无（Phase 6 规划）

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

### D1: 引入 sim_timeline_semaphore 新原语

```cpp
// sim/fence_id.h 新增
// 与 ADR-040 的 sim_fence_id（completion token）和 ADR-047 的 hardware semaphore 不同：
// timeline semaphore 有 monotonically increasing value + history + cross-engine 适用

typedef uint64_t semaphore_handle_t;

int sim_timeline_semaphore_create(uint64_t initial_value, semaphore_handle_t *handle_out);
int sim_timeline_semaphore_signal(semaphore_handle_t handle, uint64_t value);  // 写入 value
int sim_timeline_semaphore_wait(semaphore_handle_t handle, uint64_t value, uint32_t timeout_ms);  // 阻塞直到 >= value
int sim_timeline_semaphore_query(semaphore_handle_t handle, uint64_t *current_value_out);
int sim_timeline_semaphore_destroy(semaphore_handle_t handle);
```

实现：线程安全的 `std::atomic<uint64_t>` + `std::condition_variable` 等待。

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

### Phase 6 触发条件

- ADR-047 (hardware semaphore) ✅ Accepted
- 多引擎 Puller（ADR-044 ChannelManager with multi-engine）已实现
- TaskRunner 需要跨引擎 fence 测试（compute → copy pipeline）