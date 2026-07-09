# ADR-041: Graph Node → GPFIFO Entry 序列化

**状态**: ✅ Accepted
**日期**: 2026-07-09
**提案人**: Sisyphus（Phase 4 sim-graph-launch-real-impl 架构审查）
**关联 ADR**: ADR-036 (3-way separation), ADR-018 (driver-sim separation), ADR-021 (Hardware Puller), ADR-024 (User Mode Queue), ADR-040 (Puller Fence Completion)
**关联 Change**: `openspec/changes/2026-07-15-phase4-sim-graph-launch-real-impl/`

---

## Context

`sim_graph_instantiate` 将 graph node metadata（`NodeMetadata`: kernel_index, grid/block dims, kernargs_bo_handle）打包为 `ExecutableGraph`。但 `ExecutableGraph` 当前仅存逻辑描述，不包含 Puller 可消费的 `gpu_gpfifo_entry[]` 数组。

`sim_graph_launch` 需要将这些 metadata 转为硬件可消费的 GPFIFO entries，才能走 `GpuQueueEmu::submit()` → `HardwarePullerEmu::submitBatch()` 路径。

当前 PoC（`sim/graph.cpp:153-170`）绕过此问题——直接在 `sim_graph_launch` 中立即 signal fence，不走 Puller。

### 约束

- 遵循 ADR-036 3 区分：序列化逻辑应放在 sim 层（③），但最终 `GpuQueueEmu::submit()` 调用应由 drv 层 handler 发起（ADR-043 CP 可移植性边界）
- GPFIFO buffer 需要在 HAL-addressable 的 GPU 地址空间分配
- 真实 CUDA Graph（arXiv 2604.26889, CUDA 13.0）：instantiate 时预编译 pushbuffer，launch 时单 doorbell 提交——序列化应是"重操作"（instantiate），不应在"热路径"（launch）上
- `gpu_gpfifo_entry` 当前格式（`gpu_queue.h`）支持 `GPU_OP_LAUNCH_KERNEL` 和 `GPU_OP_MEMCPY` 两种 entry 类型

---

## Decision

### D1: 序列化时机 — instantiate 阶段预编译

序列化在 `sim_graph_instantiate` 中完成，不在 `sim_graph_launch` 热路径上实时翻译。

理由：匹配真实 CUDA Graph 语义——instantiate 是重操作（验证 + 编译），launch 是轻操作（提交预编译 buffer）。这与 arXiv 2604.26889 描述的 CUDA 13.0 "合并 pushbuffer + 单 doorbell" 模式一致。

### D2: 序列化位置 — sim 层内部

`ExecutableGraph` 新增字段：

```cpp
struct ExecutableGraph {
    uint64_t graph_handle;                          // 来源 graph
    std::vector<uint64_t> kernel_addrs;             // instantiate 时解析的 kernel 地址
    std::vector<gpu_gpfifo_entry> precompiled_entries;  // 预编译的 GPFIFO entries
    uint64_t gpfifo_gpu_addr;                       // HAL 地址空间中的 buffer 地址
    uint32_t entry_count;                            // precompiled_entries.size()
};
```

`sim_graph_instantiate` 中调用内部函数 `graph_to_gpfifo(graph, exec.precompiled_entries)` 完成翻译。

### D3: GPFIFO buffer 分配 — 两阶段策略

**Phase 4（短期）**：使用 sim 层内部 heap。

```cpp
// sim/ 内部：直接用 std::vector，写入时通过 hal_->mem_write 到 HAL 地址空间
void* sim_gpfifo_alloc(size_t size, uint64_t *gpu_addr_out);
```

**Phase 5（长期）**：新增 HAL 函数指针 `gpfifo_alloc` / `gpfifo_free`，纳入 ADR-023 HAL 接口契约。

```c
// gpu_hal_ops 新增：
int (*gpfifo_alloc)(void *hal_ctx, uint64_t size, uint64_t *gpu_addr_out);
int (*gpfifo_free)(void *hal_ctx, uint64_t gpu_addr, uint64_t size);
```

Phase 4 期间 HAL 扩展走 ADR 流程周期太长，先以 sim heap 方案实现功能，Phase 5 迁移到 HAL。

### D4: 翻译规则

`gpu_gpfifo_entry.payload` 为 `u64 payload[7]`。编码遵循现有 `GpfifoToLaunchParamsTranslator` 的 pack 约定（`gpfifo_translator.h:18-19`）。

| Graph Node 类型 | GPFIFO Entry payload 布局 |
|----------------|--------------------------|
| `GPU_GRAPH_NODE_KERNEL`（kernel_index=N） | `[0]`=kernel GPU VA（`kernel_addrs[N]`, u64），`[1]`=packed grid dim：`grid_x \| (grid_y << 16) \| (grid_z << 24)`，`[2]`=packed block dim：`block_x \| (block_y << 8) \| (block_z << 16)`，`[3]`=kernargs GPU VA（u64，instantiate 时从 `kernargs_bo_handle` 解析），`[4-6]`=reserved (0) |
| `GPU_GRAPH_NODE_MEMCPY`（src_va, dst_va, size） | `[0]`=src_va (u64)，`[1]`=dst_va (u64)，`[2]`=size (u64)，`[3-6]`=reserved (0) |

**pack 辅助函数**（复用 `gpfifo_translator.h` 现有实现）：

```cpp
static uint64_t pack_grid_dim(uint32_t x, uint32_t y, uint32_t z) {
    return static_cast<uint64_t>(x) | (static_cast<uint64_t>(y) << 16) | (static_cast<uint64_t>(z) << 24);
}
static uint64_t pack_block_dim(uint32_t x, uint32_t y, uint32_t z) {
    return static_cast<uint64_t>(x) | (static_cast<uint64_t>(y) << 8) | (static_cast<uint64_t>(z) << 16);
}
```

- **kernargs 解析**：`kernargs_bo_handle` → GPU VA 在 `sim_graph_instantiate` 中通过 HAL `hal_->mem_lookup(handle)` 完成，结果存入 `kernargs_gpu_va`。Graph node 的 `kernargs_bo_handle` 存储的是 BO handle（用户态分配的 buffer），instantiate 时解析为 GPU VA
- 不处理 barrier node（Phase 3.1 scope 外，延后至 Phase 5+）
- 不处理条件节点：CUDA Graph IF/WHILE/SWITCH 条件节点是**图级**操作，由 graph executor 负责（在 sim/graph.cpp 或 TaskRunner 层），不属于 CP 的 GPFIFO 序列化范畴。硬件级 predication 由 ADR-051 §A 定义

### D5: sim_graph_launch 重构

`sim_graph_launch` 不再立即 signal fence。改为 output 模式——返回 `gpfifo_addr` 和 `entry_count`，由 drv 层 handler 调 `GpuQueueEmu::submit()`：

```cpp
// sim/graph.h — 新签名
int sim_graph_launch(uint64_t exec_handle, uint32_t stream_id,
                     uint64_t *gpfifo_addr_out, uint32_t *entry_count_out);
// 返回: 0=成功, -EINVAL=无效 exec_handle, -ENOENT=exec_handle 未 instantiate
```

drv 层 handler（`handleGraphLaunch`）负责：

```cpp
uint64_t gpfifo_addr;
uint32_t entry_count;
int ret = sim_graph_launch(args->exec_handle, args->stream_id,
                           &gpfifo_addr, &entry_count);
if (ret < 0) return ret;

auto q = getQueue(args->stream_id);
if (!q) return -EINVAL;

uint64_t fence_id = sim_fence_id_alloc();  // 分配 sim fence（ADR-040）
q->submit(gpfifo_addr, entry_count, fence_id);  // 通过 GpuQueueEmu → Puller
hal_doorbell_ring(hal_, args->stream_id);

return fence_id;  // 返回给用户，用户通过 WAIT_FENCE 等待
```

---

## Consequences

### 正面

- ✅ `sim_graph_launch` 路径复用现有 Puller + Scheduler + fence completion（ADR-040）基础设施
- ✅ instantiate 预编译 → launch 轻量，匹配真实 CUDA Graph 语义
- ✅ 序列化在 sim 层，不污染 drv 层可移植代码

### 负面

- ⚠️ `ExecutableGraph` 结构体新增 2 个字段（`precompiled_entries` + `gpfifo_gpu_addr`），影响 `graph.h` 的 C ABI 兼容性
- ⚠️ Phase 4 sim heap 方案是临时方案，Phase 5 需迁移到 HAL `gpfifo_alloc`
- ⚠️ 不处理 barrier / 条件节点，graph 支持的节点类型受限
- ⚠️ `sim_graph_launch` 签名从 "返回 fence_id" 变为 "output gpfifo_addr + entry_count"，破坏现有调用方兼容性（但现有调用方只有 PoC，影响面可控）
- ⚠️ `stream_id` 到 `GpuQueueEmu` 的查找由 drv 层负责（符合 ADR-043 CP 可移植性边界），需确保 `stream_id` 映射与 ADR-033 R2 mapping 契约一致（`stream_id = LOW32(queue_handle)`）

### 迁移

1. `ExecutableGraph` 新增字段（`graph.h`）
2. `sim_graph_instantiate` 中增加 `graph_to_gpfifo()` 调用
3. `sim_graph_launch` 签名变更为 output 模式，移除 `sim_fence_id_signal()`
4. `handleGraphLaunch`（`gpgpu_device.cpp`）重写为：调 `sim_graph_launch` → `getQueue()` → `q->submit()` → `return fence_id`
5. 同步更新 `gpu_drm_driver.cpp` 的 `gpu_ioctl_graph_launch`
6. `test_sim_graph_standalone` 需重写：移除"fence 立即 signal"断言，改为验证 `gpfifo_addr` / `entry_count` 正确返回

---

## 讨论历史

- **2026-07-09**: 初始提案。来自 Phase 4 `sim-graph-launch-real-impl` 审查：`sim_graph_launch` 的 exec_handle → gpfifo 转换路径未设计。
- cross-repo doc B-1（`external/TaskRunner/docs/superpowers/cross-repo-prs/2026-07-05-phase3-1-stream-mempool-coordination.md` §3.1.1）：确认 `GpuQueueEmu::submit(uint64_t, uint32_t)` 是唯一合法接口，`submit_batch` / `enqueue` 不存在。
- **2026-07-09 (修正)**：D4 payload 格式修正。原版错误地将 8 个字段展开写入 `payload[7]`，修正为复用现有 `GpfifoToLaunchParamsTranslator` 的 pack 约定（grid 压缩为 1 个 u64，block 压缩为 1 个 u64，kernargs_gpu_va 使用 payload[3]）。共使用 4/7 slot，留 3 slot 为未来扩展。
- **2026-07-09 (升级为 Accepted)**：基于代码追踪验证（`gpu_types.h:36` `gpu_gpfifo_entry.payload[7]`；`gpfifo_translator.h:18-19` `packGrid: x|(y<<16)|(z<<24)`；`gpgpu_device.cpp:357-365` pushbuffer 当前使用同一 pack 约定；`graph.cpp:20-24` `KernelNodeMetadata` 结构匹配 D4 序列化源）。D1 instantiate=重操作/launch=轻操作设计匹配 CUDA Graph 真实语义。D5 签名变更边界划分与 ADR-043 D4 一致。Oracle 审查因任务超时未返回，采用 Metis 审查 + 代码直接验证作为升级依据。