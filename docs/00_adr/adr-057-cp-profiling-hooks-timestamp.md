# ADR-057: CP Profiling Hooks & Timestamp Query

**状态**: 📋 PROPOSED（Phase 5，可与 ADR-048 并行）
**日期**: 2026-07-09
**提案人**: Sisyphus（Oracle CP 蓝图审查建议新增）
**关联 ADR**: ADR-021 (Puller FSM), ADR-022 (CU Emulation), ADR-048 (Interrupt)
**关联 Change**: 无（Phase 5 规划）

---

## Context

TaskRunner 作为驱动验证框架，需要测量 kernel 执行时间才能验证驱动优化效果。真实 GPU 提供 profiling 能力：

- **Vulkan**: `VK_QUERY_TYPE_TIMESTAMP`、`VK_QUERY_TYPE_PIPELINE_STATISTICS`
- **D3D12**: `ID3D12QueryHeap` with `D3D12_QUERY_TYPE_TIMESTAMP`
- **CUDA**: `cuEventRecord` + `cuEventElapsedTime`
- **AMD**: AQL `completion_signal`（fence value 携带 timestamp）

当前 sim 层的 operator-level emulation（ADR-022）是同步的，没有时间维度。但用户态模拟可以加入 **logical timestamp**（entry dispatch 顺序号）作为轻量 profiling，驱动测试不需要真实时钟。

### 范围

- 不建模真实 GPU 时钟——logical tick（单调递增计数器）足够驱动验证
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

### D4: 与 ADR-048 Interrupt 联动

Timestamp query resolve 支持中断模式：record 后可选触发 `InterruptVector::TIMESTAMP_READY` 中断（通过 ADR-048 的 interrupt 机制）。默认使用 poll 模式。

---

## Consequences

- ✅ TaskRunner 可测量 kernel 执行时间（logical tick 差值）
- ✅ 不与真实时钟耦合——logical tick 在模拟器中可复现
- ✅ 通过 `gpu_gpfifo_entry.ts_query` 字段，每条 entry 可独立记录
- ⚠️ logical tick 不是真实时间——驱动性能优化需要真实硬件验证，模拟器仅做正确性验证
- ⚠️ 不实现 pipeline statistics query（占用率、cache 统计等——这些是应用层需求）

### Phase 5 触发条件

- ADR-040 (fence completion) ✅ 已实现
- TaskRunner 需要 profiling 测试（cuEventElapsedTime 等）