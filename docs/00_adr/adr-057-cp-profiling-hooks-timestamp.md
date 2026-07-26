# ADR-057: CP Profiling Hooks & Timestamp Query

**状态**: ✅ 已采纳 (Accepted)（Phase 5，可与 ADR-048 并行）
**日期**: 2026-07-27
**提案人**: Sisyphus（Oracle CP 蓝图审查建议新增）
**关联 ADR**: ADR-021 (Puller FSM), ADR-022 (CU Emulation), ADR-048 (Interrupt)
**关联 Change**: 无（Phase 5 规划）
**修订**: 2026-07-27 - Oracle 评审后修订 (D5 暴露路径明确、ABI 影响分析、resolve 语义约束、D4 条件标注)

---

## Context

TaskRunner 作为驱动验证框架，需要测量 kernel 执行时间才能验证驱动优化效果。真实 GPU 提供 profiling 能力：

- **Vulkan**: `VK_QUERY_TYPE_TIMESTAMP`、`VK_QUERY_TYPE_PIPELINE_STATISTICS`
- **D3D12**: `ID3D12QueryHeap` with `D3D12_QUERY_TYPE_TIMESTAMP`
- **CUDA**: `cuEventRecord` + `cuEventElapsedTime`
- **AMD**: AQL `completion_signal`（fence value 携带 timestamp）

当前 sim 层的 operator-level emulation（ADR-022）是同步的，没有时间维度。但用户态模拟可以加入 **logical timestamp**（entry dispatch 顺序号）作为轻量 profiling，驱动测试不需要真实时钟。

### 范围

- 不建模真实 GPU 时钟--logical tick（单调递增计数器）足够驱动验证
- 性能计数器（L2 cache hit rate、SM occupancy）对驱动开发无价值，不补
- Profiling hooks 是 sim 层责任，不与 HAL 耦合

---

## Decision

### D1: logical tick 计数器

```cpp
// sim/ 层全局
static std::atomic<uint64_t> g_sim_tick{0};

// Puller DISPATCH 阶段：
g_sim_tick.fetch_add(1);
```

每个 entry dispatch 递增一次 logical tick。

### D2: sim_timestamp_query C-ABI

```cpp
// sim/fence_id.h 新增

typedef uint64_t timestamp_query_handle_t;

// 创建 timestamp query（返回 handle）
int sim_timestamp_query_create(timestamp_query_handle_t *handle_out);

// 记录当前 tick（在 Puller DISPATCH 时写入）
int sim_timestamp_query_record(timestamp_query_handle_t handle);

// 查询记录的 tick 值（阻塞直到已记录）
int sim_timestamp_query_resolve(timestamp_query_handle_t handle, uint64_t *tick_out, uint32_t timeout_ms);

// 销毁
int sim_timestamp_query_destroy(timestamp_query_handle_t handle);
```

实现：`std::atomic<uint64_t>` 初始化为 `UINT64_MAX`（未记录），record 时写入当前 `g_sim_tick`，resolve 时阻塞 poll 直到值被写入。

**resolve 语义约束**：当前 sim 层是单线程同步模型（ADR-022 operator-level emulation）。`sim_timestamp_query_resolve()` 必须在 `submitBatch()` 返回之后调用--在 submit 返回前调用 resolve 会导致死锁（record 尚未执行）。`timeout_ms` 参数提供死锁逃逸：超时后返回 `-ETIMEDOUT`，调用方应将其视为 profiling 失败而非致命错误。未来引入异步调度（Phase 6 抢占，ADR-046）后此约束需重新评估。

### D3: gpu_gpfifo_entry 新增 timestamp slot

```cpp
typedef struct {
    // ... 现有字段 ...
    timestamp_query_handle_t ts_query;  // 此 entry dispatch 时记录 tick（0 = 不记录）
} gpu_gpfifo_entry;
```

Puller DISPATCH 阶段：

```cpp
if (entry.ts_query != 0) {
    sim_timestamp_query_record(entry.ts_query);
}
```

**ABI 影响分析**：`gpu_gpfifo_entry` 是 TaskRunner 共享的 `__attribute__((packed))` 结构体（当前 76 字节），通过符号链接在 UsrLinuxEmu 与 TaskRunner 之间共享（`plugins/gpu_driver/shared/gpu_types.h`）。新增 `u64 ts_query` 字段会将结构体扩展到 84 字节，**改变 ABI**。

**备选方案**：

1. **复用 `_reserved:15` 位作为 flag + 写 tick 到现有 `semaphore_va`**（零 ABI 变更）：
   - 取 `_reserved` 中 1 位作为 `ts_query_enable` 标志
   - record 时将 tick 写入 `semaphore_va`（语义从"信号量地址"复用为"时间戳值"）
   - 优点：零 ABI 变更，更接近真实 GPU 的 timestamp-to-memory 语义
   - 缺点：`semaphore_va` 语义重载，与 `release` 标志交互复杂；profiling 与 semaphore 不能同时使用

2. **新增独立字段 `ts_query`**（当前选择，显式跨仓同步）：
   - 新增 `u64 ts_query` 字段（0 = 不记录）
   - 优点：语义清晰，profiling 与 semaphore 完全解耦
   - 缺点：ABI 变更（76 -> 84 字节），需要 UsrLinuxEmu 与 TaskRunner 同步更新

**选择理由**：采用方案 2。语义清晰性优先于 ABI 兼容性--`gpu_gpfifo_entry` 尚在早期阶段（Phase 5），不存在已发布的稳定 ABI。跨仓同步通过符号链接共享头文件保证（TaskRunner 侧 `TaskRunner/UsrLinuxEmu -> ../../UsrLinuxEmu/`），仅需同 commit 更新两侧代码。方案 1 的 `semaphore_va` 重载在真实 GPU 中不常见（timestamp 通常有独立寄存器），会降低模拟保真度。

### D4: 与 ADR-048 Interrupt 联动（条件性）

> **条件标注**：本联动设计依赖 ADR-048（中断与事件模型）。ADR-048 已于 2026-07-27 同批 Accepted，本联动设计可生效。若 ADR-048 状态回退，本节应视为 Deferred。

Timestamp query resolve 支持中断模式：record 后可选触发 `InterruptVector::TIMESTAMP_READY` 中断（通过 ADR-048 的 interrupt 机制）。默认使用 poll 模式。

### D5: 暴露路径决策

**Phase 5 暴露路径：测试后门（test backdoor）**

TaskRunner 在 Phase 5 通过**直接链接 sim C-ABI** 访问 timestamp query 功能，与现有 sim 原语（`sim_graph_*`、`sim_mem_pool_*`）的访问模式完全一致：

```
TaskRunner  --direct link-->  sim_timestamp_query_create()
                               sim_timestamp_query_record()
                               sim_timestamp_query_resolve()
                               sim_timestamp_query_destroy()
```

这意味着：
- TaskRunner 直接调用 `sim/fence_id.h` 中声明的 C 函数
- **不经过 ioctl 层**（不新增 `GPU_IOCTL_TIMESTAMP_QUERY_*` 编号）
- 适用于 Phase 5 测试场景：驱动测试需要绕过标准 ioctl 路径直接控制 sim 层

**Phase 5.5 延后：ioctl 暴露**

`GPU_IOCTL_TIMESTAMP_QUERY_*` 系列 ioctl（`CREATE`/`RECORD`/`RESOLVE`/`DESTROY`）延后至 Phase 5.5。届时需：
1. 在 `plugins/gpu_driver/shared/gpu_ioctl.h` 新增 4 个 ioctl 编号
2. 在 `GpgpuDevice::ioctl` 派发表新增 4 个 handler
3. 通过 HAL 层路由到 sim 实现

Phase 5.5 触发条件：真实驱动需要通过标准 ioctl 路径访问 profiling 数据（而非测试后门）。

---

## Consequences

- ✅ TaskRunner 可测量 kernel 执行时间（logical tick 差值）
- ✅ 不与真实时钟耦合--logical tick 在模拟器中可复现
- ✅ 通过 `gpu_gpfifo_entry.ts_query` 字段，每条 entry 可独立记录
- ✅ Phase 5 通过 test backdoor 暴露，零 ioctl 编号消耗；Phase 5.5 再补 ioctl 路径
- ⚠️ logical tick 不是真实时间--驱动性能优化需要真实硬件验证，模拟器仅做正确性验证
- ⚠️ 不实现 pipeline statistics query（占用率、cache 统计等--这些是应用层需求）
- ⚠️ ABI 变更：`gpu_gpfifo_entry` 从 76 字节扩展到 84 字节，需 UsrLinuxEmu + TaskRunner 同步更新
- ⚠️ resolve 语义约束：单线程同步 sim 模型下，resolve() 必须在 submit 返回后调用

### Phase 5 触发条件

- ADR-040 (fence completion) ✅ 已实现
- TaskRunner 需要 profiling 测试（cuEventElapsedTime 等）
