# Design: sim_graph_launch 真实异步实现

> **状态**: ✅ Approved（ADR-040/041/043 全部 Accepted 作为架构基础）
> **关联 ADR**: ADR-040 (Puller Fence Completion), ADR-041 (Graph → GPFIFO), ADR-043 (CP Boundary) — 全部 Accepted
> **关联 Change**: `openspec/changes/2026-07-15-phase4-sim-graph-launch-real-impl/`

## 1. 问题

当前 `sim_graph_launch`（`sim/graph.cpp:153-170`）是 PoC 实现：分配 sim fence → 立即 `sim_fence_id_signal()` → 返回 fence_id。fence **从不真正异步**，与真实 CUDA Graph launch 行为不符。

目标：`sim_graph_launch` 通过 `GpuQueueEmu::submit()` → `HardwarePullerEmu` FSM → `handleComplete()` → `sim_fence_id_signal()` 路径实现真实异步执行，fence 仅在 Puller 完成 batch 后 signal。

## 2. 架构概览

```
User ioctl(GPU_IOCTL_GRAPH_LAUNCH, args)   [0x58]
  ↓
handleGraphLaunch(args)                      [drv: gpgpu_device.cpp]
  ├─ sim_graph_launch(exec, stream, &gpfifo, &count)  ← sim C-ABI 只读查表
  ├─ getQueue(stream_id)                              ← drv queue 查找
  ├─ q->submit(gpfifo_addr, count, fence_id)          ← GpuQueueEmu → Puller
  ├─ hal_doorbell_ring(stream_id)                     ← HAL
  └─ return fence_id                                  ← sim fence
  ↓
HardwarePullerEmu::runLoop() (独立线程)
  FETCH → DECODE → SCHEDULE → DISPATCH → COMPLETE
  ↓
handleComplete()
  if (current_index_ >= total_entries_ && pending_fence_id_ != 0)
    sim_fence_id_signal(pending_fence_id_)   ← fence 完成!
```

**关键边界（ADR-043 D4）**：
- `sim_graph_launch`：只做只读查表，不调 Puller、不 signal fence
- `handleGraphLaunch`（drv）：查 queue + 调 `q->submit()` + doorbell + return fence_id

## 3. 详细设计

### 3.1 Puller Fence Completion（ADR-040）

**`HardwarePullerEmu` 变更**：

```cpp
// hardware_puller_emu.h
class HardwarePullerEmu {
  uint64_t pending_fence_id_ = 0;  // NEW

  // 签名变更
  void submitBatch(uint64_t gpfifo_addr, uint32_t entry_count,
                   uint64_t fence_id = 0);  // NEW param
};

// hardware_puller_emu.cpp — handleComplete() 末尾:
if (current_index_ >= total_entries_ && pending_fence_id_ != 0) {
    sim_fence_id_signal(pending_fence_id_);
    pending_fence_id_ = 0;
}
```

**`GpuQueueEmu` 变更**（透传）：

```cpp
// gpu_queue_emu.h
int submit(uint64_t gpfifo_addr, uint32_t entry_count,
           uint64_t fence_id = 0);  // NEW param

// gpu_queue_emu.cpp
int GpuQueueEmu::submit(uint64_t gpfifo_addr, uint32_t entry_count, uint64_t fence_id) {
    if (!puller_) return -ENODEV;
    puller_->submitBatch(gpfifo_addr, entry_count, fence_id);
    return 0;
}
```

**线程安全**：`pending_fence_id_` 由 `submitBatch()` 写入（driver 线程），由 `handleComplete()` 读取/清零（Puller 线程）。由于 `handleComplete()` 仅在 `current_index_ >= total_entries_` 时读取（此时 batch 已全量完成，submitBatch 不会并发写入），无竞态条件。

### 3.2 Graph Node → GPFIFO 预编译（ADR-041）

**`ExecutableGraph` 变更**（`sim/graph.cpp`）：

```cpp
struct ExecEntry {
    uint64_t source_graph;
    std::vector<uint64_t> kernel_addrs;
    std::vector<gpu_gpfifo_entry> precompiled_entries;  // NEW
    uint64_t gpfifo_gpu_addr;                             // NEW
    uint32_t entry_count;                                 // NEW
};
```

**`sim_graph_instantiate` 新增**：

```cpp
int sim_graph_instantiate(uint64_t graph_handle, uint64_t *exec_handle_out) {
    // ... existing validation ...

    ExecEntry exec;
    exec.source_graph = graph_handle;

    // 1. 解析 kernel addresses
    for (auto &node : graph.nodes) {
        if (node.type == KERNEL)
            exec.kernel_addrs.push_back(resolve_kernel_addr(node.kernel.kernel_index));
    }

    // 2. 预编译 GPFIFO entries
    graph_to_gpfifo(graph, exec.precompiled_entries);

    // 3. 分配 HAL-addressable buffer，写入 entries
    uint64_t gpu_addr;
    void *sim_buf = sim_gpfifo_alloc(exec.precompiled_entries.size() * sizeof(gpu_gpfifo_entry), &gpu_addr);
    memcpy(sim_buf, exec.precompiled_entries.data(), ...);
    // hal_->mem_write(gpu_addr, sim_buf, size);  // 写入 HAL 地址空间
    exec.gpfifo_gpu_addr = gpu_addr;
    exec.entry_count = exec.precompiled_entries.size();

    exec_table_[exec_handle] = std::move(exec);
    return 0;
}
```

**`graph_to_gpfifo()` 翻译规则**（ADR-041 D4）：

| Graph Node | GPFIFO Entry |
|-----------|-------------|
| KERNEL | `method=GPU_OP_LAUNCH_KERNEL`, `payload[0]`=kernel_va, `payload[1]`=packed_grid, `payload[2]`=packed_block, `payload[3]`=kernargs_va |
| MEMCPY | `method=GPU_OP_MEMCPY`, `payload[0]`=src_va, `payload[1]`=dst_va, `payload[2]`=size |

**pack 辅助函数**（复用 `gpfifo_translator.h` 现有实现）：

```cpp
static uint64_t pack_grid_dim(uint32_t x, uint32_t y, uint32_t z) {
    return static_cast<uint64_t>(x) | (static_cast<uint64_t>(y) << 16) | (static_cast<uint64_t>(z) << 24);
}
static uint64_t pack_block_dim(uint32_t x, uint32_t y, uint32_t z) {
    return static_cast<uint64_t>(x) | (static_cast<uint64_t>(y) << 8) | (static_cast<uint64_t>(z) << 16);
}
```

**Phase 4 sim heap 分配**：

```cpp
// sim/graph.cpp — 内部实现
void* sim_gpfifo_alloc(size_t size, uint64_t *gpu_addr_out) {
    static std::vector<uint8_t> sim_heap;
    static constexpr uint64_t SIM_HEAP_BASE = 0x20000000;  // sim-only VA range
    size_t offset = sim_heap.size();
    sim_heap.resize(offset + size);
    *gpu_addr_out = SIM_HEAP_BASE + offset;
    return sim_heap.data() + offset;
}
```

Phase 5 迁移到 HAL `gpfifo_alloc`/`gpfifo_free`（ADR-023 扩展）。

### 3.3 sim_graph_launch 重构（ADR-043 D4）

```cpp
// sim/graph.cpp — 新实现
int sim_graph_launch(uint64_t exec_handle, uint32_t stream_id,
                     uint64_t *gpfifo_addr_out, uint32_t *entry_count_out) {
    (void)stream_id;  // 保留参数，drv handler 使用

    auto it = exec_table_.find(exec_handle);
    if (it == exec_table_.end())
        return -EINVAL;

    *gpfifo_addr_out = it->second.gpfifo_gpu_addr;
    *entry_count_out = it->second.entry_count;
    return 0;
}
```

### 3.4 drv handler 重写

```cpp
// gpgpu_device.cpp — handleGraphLaunch
int GpgpuDevice::handleGraphLaunch(const struct gpu_graph_launch_args *args) {
    uint64_t gpfifo_addr;
    uint32_t entry_count;
    int ret = sim_graph_launch(args->exec_handle, args->stream_id,
                               &gpfifo_addr, &entry_count);
    if (ret < 0) return ret;

    auto q = getQueue(args->stream_id);
    if (!q) return -EINVAL;

    uint64_t fence_id = sim_fence_id_alloc();
    q->submit(gpfifo_addr, entry_count, fence_id);
    hal_doorbell_ring(hal_, args->stream_id);

    return static_cast<int>(fence_id);
}
```

### 3.5 Pushbuffer 路径同步修复（ADR-040 D3）

`handlePushbufferSubmitBatch` 同步适配：

```cpp
// 旧: hal_fence_create(hal_, &fence_id);  // HAL fence，永远不 signal
// 新:
uint64_t fence_id = sim_fence_id_alloc();  // sim fence，Puller 完成后 signal
q->submit(gpfifo_addr, args->count, fence_id);
```

`handleWaitFence` 同步适配（`gpgpu_device.cpp`），匹配 `gpu_drm_driver.cpp:282-288` 已有的 sim fence 查询逻辑：

```cpp
// 如果 fence_id >= SIM_FENCE_ID_BASE，使用 sim_fence_id_check
// 否则使用 hal_fence_read（向后兼容）
```

## 4. 文件变更清单

| 文件 | 变更类型 | 说明 |
|------|---------|------|
| `sim/hardware/hardware_puller_emu.h` | 修改 | 新增 `pending_fence_id_`；`submitBatch` 签名变更 |
| `sim/hardware/hardware_puller_emu.cpp` | 修改 | `handleComplete()` 中 signal fence；`submitBatch` 实现更新 |
| `sim/gpu_queue_emu.h` | 修改 | `submit()` 签名变更 |
| `sim/gpu_queue_emu.cpp` | 修改 | `submit()` 透传 fence_id |
| `sim/graph.h` | 修改 | `sim_graph_launch` 签名变更 |
| `sim/graph.cpp` | 修改 | `sim_graph_launch` 改为查表输出；`sim_graph_instantiate` 预编译 entries；`graph_to_gpfifo()` 新增 |
| `drv/gpgpu_device.cpp` | 修改 | `handleGraphLaunch` 重写；`handlePushbufferSubmitBatch` 适配 sim fence；`handleWaitFence` 适配双命名空间 |
| `drv/gpu_drm_driver.cpp` | 修改 | `gpu_ioctl_graph_launch` 重写 |
| `tests/test_sim_graph_standalone.cpp` | 修改 | 重写测试：验证 gpfifo_addr/entry_count 输出 + async fence |
| `tests/test_hardware_puller_emu_standalone.cpp` | 修改 | 新增 fence signal 验证 |
| `tests/test_gpu_ioctl_standalone.cpp` | 修改 | 验证 pushbuffer fence 通过 Puller 完成 |

## 5. 线程安全

- `pending_fence_id_`: 由 `submitBatch()` 写入（driver 线程），由 `handleComplete()` 读取/清零（Puller 线程）。语义保证：`handleComplete()` 仅在 `current_index_ >= total_entries_` 时读取（batch 已全量完成），此时 `submitBatch()` 不可能并发写入。安全。
- `sim_fence_table_`: 已有 `std::mutex` 保护（`fence_id.cpp`），`sim_fence_id_signal()` 内部加锁。
- `exec_table_`: 单线程操作（driver dispatch path），无需锁。

## 6. 兼容性

- `fence_id=0` 哨兵：`0` 不在 sim fence 范围 `[2³², INT64_MAX]` 内，安全。旧调用方不传 fence_id 时默认 `0`，Puller 不 signal（向后兼容）。
- `sim_graph_launch` 签名变更：破坏 ABI，影响调用方（`gpgpu_device.cpp`、`gpu_drm_driver.cpp`、TaskRunner shim）。此变更在 Phase 4 scope 内。
- `gpu_gpfifo_entry.payload` 布局未变（仍然 `u64[7]`），无跨仓 ABI 破坏。

## 架构参考

- [ADR-040 Puller Fence Completion](../../docs/00_adr/adr-040-puller-fence-completion.md) — fence completion 机制
- [ADR-041 Graph Node → GPFIFO Serialization](../../docs/00_adr/adr-041-graph-node-to-gpfifo-serialization.md) — 预编译策略与 payload 格式
- [ADR-043 CP Portability Boundary](../../docs/00_adr/adr-043-cp-portability-boundary.md) — drv/sim 边界
- [ADR-021 Hardware Puller FSM](../../docs/00_adr/adr-021-hardware-puller.md) — Puller 状态机
- [ADR-024 User Mode Queue](../../docs/00_adr/adr-024-user-mode-queue-submission.md) — 双路径架构
- [ADR-036 3-Way Separation](../../docs/00_adr/adr-036-three-way-separation.md) — 架构总原则