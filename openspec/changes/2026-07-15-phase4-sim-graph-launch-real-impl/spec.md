# Spec: sim_graph_launch 真实异步实现

> **状态**: ✅ Approved（ADR-040/041/043 全部 Accepted）
> **关联**: ADR-040 (Puller Fence Completion), ADR-041 (Graph → GPFIFO), ADR-043 (CP Boundary) — 全部 Accepted

## ADDED

### Fence Completion in HardwarePullerEmu

`HardwarePullerEmu::handleComplete()` 在 batch 全量完成后调用 `sim_fence_id_signal(pending_fence_id_)`，当 `pending_fence_id_ != 0` 且 `current_index_ >= total_entries_`。

**Given** Puller 已通过 `submitBatch(gpfifo_addr, entry_count, fence_id)` 提交 batch
**When** `runLoop()` 处理完所有 entries（每条 entry 经过 FETCH→DECODE→DISPATCH→COMPLETE）
**Then** `handleComplete()` 检测 `current_index_ >= total_entries_` 且 `pending_fence_id_ != 0`
**And** 调用 `sim_fence_id_signal(pending_fence_id_)`
**And** 将 `pending_fence_id_` 重置为 0

### Graph Instantiate Precompilation

`sim_graph_instantiate` 将 graph node metadata 预编译为 `gpu_gpfifo_entry[]`，存储于 `ExecutableGraph.precompiled_entries`，并分配 HAL-addressable buffer 存储。

**Given** 一个包含 KERNEL 和 MEMCPY node 的 graph
**When** 调用 `sim_graph_instantiate(graph_handle, &exec_handle)`
**Then** `ExecutableGraph.precompiled_entries` 包含翻译后的 GPFIFO entries
**And** KERNEL node payload 格式为：`[0]`=kernel_va, `[1]`=packed_grid, `[2]`=packed_block, `[3]`=kernargs_va
**And** MEMCPY node payload 格式为：`[0]`=src_va, `[1]`=dst_va, `[2]`=size
**And** `gpfifo_gpu_addr` 指向 HAL-addressable buffer（Phase 4 sim heap, Phase 5 HAL gpfifo_alloc）
**And** `entry_count` = `precompiled_entries.size()`

### sim_graph_launch Output Mode

`sim_graph_launch` 改为查表输出模式，不调 Puller、不 signal fence。

**Given** 一个已 instantiated 的 executable（`exec_handle`）
**When** 调用 `sim_graph_launch(exec_handle, stream_id, &gpfifo_addr, &entry_count)`
**Then** 返回 0（成功），`gpfifo_addr` = executable 的 `gpfifo_gpu_addr`，`entry_count` = executable 的 `entry_count`
**And** 不调用 `sim_fence_id_alloc()` 或 `sim_fence_id_signal()`

**Given** 一个无效的 `exec_handle`
**When** 调用 `sim_graph_launch`
**Then** 返回 `-EINVAL`

### Graph Launch Async Fence

`handleGraphLaunch` 通过 `sim_graph_launch` → `q->submit()` → Puller → `handleComplete()` 路径实现 async fence。

**Given** 有效的 `exec_handle` 和 `stream_id`
**When** 用户调用 `ioctl(dev, GPU_IOCTL_GRAPH_LAUNCH, &args)`
**Then** `handleGraphLaunch` 返回 sim fence_id（`>= SIM_FENCE_ID_BASE`）
**And** 调用 `sim_fence_id_check(fence_id, &signaled)` 时，在 Puller 完成前 `signaled=false`
**And** Puller `handleComplete()` 完成后，`signaled=true`

### Pushbuffer Fence Fix

Pushbuffer 路径同步修复：`handlePushbufferSubmitBatch` 通过 Puller 路径时使用 sim fence，不再使用 HAL fence（永不 signal）。

**Given** 用户调用 `ioctl(dev, GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, &args)`
**When** handler 走 Puller 路径（`q->submit()` + `hal_doorbell_ring()`）
**Then** 返回的 fence_id 在 sim fence 范围（`>= SIM_FENCE_ID_BASE`）
**And** Puller 完成后 `sim_fence_id_check(fence_id, &signaled)` 返回 `signaled=true`

## MODIFIED

### GpuQueueEmu::submit() Signature

**Before**: `int submit(uint64_t gpfifo_addr, uint32_t entry_count)`
**After**: `int submit(uint64_t gpfifo_addr, uint32_t entry_count, uint64_t fence_id = 0)`

`fence_id=0` 表示"不触发 fence 完成回调"，保持向后兼容。

### HardwarePullerEmu::submitBatch() Signature

**Before**: `void submitBatch(uint64_t gpfifo_addr, uint32_t entry_count)`
**After**: `void submitBatch(uint64_t gpfifo_addr, uint32_t entry_count, uint64_t fence_id = 0)`

### sim_graph_launch() Signature

**Before**: `int64_t sim_graph_launch(uint64_t exec_handle, uint32_t stream_id)`
**After**: `int sim_graph_launch(uint64_t exec_handle, uint32_t stream_id, uint64_t *gpfifo_addr_out, uint32_t *entry_count_out)`

返回值从 fence_id 改为状态码（0=成功，-EINVAL=无效 exec_handle）。

### ExecutableGraph (ExecEntry) Structure

**Before**:
```cpp
struct ExecEntry { uint64_t source_graph; };
```

**After**:
```cpp
struct ExecEntry {
    uint64_t source_graph;
    std::vector<uint64_t> kernel_addrs;
    std::vector<gpu_gpfifo_entry> precompiled_entries;
    uint64_t gpfifo_gpu_addr;
    uint32_t entry_count;
};
```

## REMOVED

### sim_graph_launch 中的立即 fence signal

移除 `sim_graph_launch` 中的 `sim_fence_id_alloc()` + `sim_fence_id_signal()` 调用。fence 现在由 Puller 在 `handleComplete()` 中 signal。