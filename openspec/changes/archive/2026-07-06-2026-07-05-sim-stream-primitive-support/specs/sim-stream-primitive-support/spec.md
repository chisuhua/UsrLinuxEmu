# Spec: sim-stream-primitive-support

> **Capability**: `sim-stream-primitive-support`
> **Type**: New capability
> **Status**: PROPOSED
> **Date**: 2026-07-05

## Purpose

为 TaskRunner Phase 3.1（Stream Capture + CUDA Graph）和 Phase 3.2（Memory Pool）提供后端 sim 原语支撑，使 TaskRunner 的 cuStreamCapture/CUgraph/cuMemPool* API shim 可通过 GpuDriverClient 真实路径调用，而非仅 CudaStub 假后端。

## Requirements

### REQ-1: Stream Capture 原语

**SHALL**: UsrLinuxEmu sim 层 SHALL 提供 3 个 stream capture 接口，允许外部代码（TaskRunner CudaScheduler / GpuDriverClient）开始 / 结束 / 查询 stream 上的 capture 状态。

#### Scenario 1.1: Begin capture on idle stream

**WHEN** 外部代码调用 `sim_stream_capture_begin(stream_id, mode)` 且 stream 当前不在 capture 中

**THEN** sim SHALL 创建 `CaptureState`，标记 stream 为 `SIM_STREAM_CAPTURE_ACTIVE`，后续 `GpuQueueEmu::enqueue` 调用 SHALL 记录到 `CaptureState.nodes` 而非直接提交

**AND** 返回值 SHALL 为 0

#### Scenario 1.2: End capture

**WHEN** 外部代码调用 `sim_stream_capture_end(stream_id, &graph_handle_out)` 且 stream 当前为 `SIM_STREAM_CAPTURE_ACTIVE`

**THEN** sim SHALL 创建新 graph handle，将 `CaptureState.nodes` 转为 graph metadata，标记 stream 为 `SIM_STREAM_CAPTURE_NONE`，释放 `CaptureState`

**AND** `*graph_handle_out` SHALL 为有效 graph handle（≥1）

#### Scenario 1.3: Query capture status

**WHEN** 外部代码调用 `sim_stream_capture_status(stream_id, &status_out)`

**THEN** sim SHALL 写入当前 capture 状态（`NONE` / `ACTIVE` / `INVALID`）

**AND** 返回值 SHALL 为 0

#### Scenario 1.4: Error — Begin on already-capturing stream

**WHEN** 外部代码调用 `sim_stream_capture_begin(stream_id, mode)` 且 stream 当前为 `SIM_STREAM_CAPTURE_ACTIVE`

**THEN** sim SHALL 标记 stream 为 `SIM_STREAM_CAPTURE_INVALID`

**AND** 返回值 SHALL 为 -1

#### Scenario 1.5: Error — End on non-capturing stream

**WHEN** 外部代码调用 `sim_stream_capture_end(stream_id, &graph_handle_out)` 且 stream 当前为 `SIM_STREAM_CAPTURE_NONE`

**THEN** sim SHALL 不修改 stream 状态

**AND** 返回值 SHALL 为 -1

#### Scenario 1.6: Error — Unsupported capture mode (Fix-10)

**WHEN** 外部代码调用 `sim_stream_capture_begin(stream_id, mode)` 且 `mode` 不是 `SIM_CAPTURE_MODE_GLOBAL (0)`

**THEN** sim SHALL:
- 不修改 stream 状态
- 返回 -1（EINVAL）

**Phase 3.1 范围**：仅 `SIM_CAPTURE_MODE_GLOBAL` 受支持。`SIM_CAPTURE_MODE_THREAD_LOCAL` 和 `SIM_CAPTURE_MODE_RELAXED` 留待 Phase 3.x。

### REQ-2: Graph 原语

**SHALL**: UsrLinuxEmu sim 层 SHALL 提供 7 个 graph 接口，允许创建 / 销毁 / 节点添加 / 实例化 / 启动 / 销毁 executable。

#### Scenario 2.1: Create graph

**WHEN** 外部代码调用 `sim_graph_create(&graph_handle_out)`

**THEN** sim SHALL 创建新 graph，分配 `graph_handle`

**AND** `*graph_handle_out` SHALL 为有效 graph handle（≥1）

#### Scenario 2.2: Add kernel node

**WHEN** 外部代码调用 `sim_graph_add_kernel_node(graph_handle, kernel_index, grid, block, kernargs_bo_handle)`

**THEN** sim SHALL 验证 `graph_handle` 有效，将 kernel node metadata 追加到 graph

**AND** 返回值 SHALL 为 0

#### Scenario 2.2.1: kernargs_bo_handle 语义 (Fix-8)

- `kernargs_bo_handle == 0`：表示该 kernel 无参数（无 kernargs BO），driver SHALL NOT 验证 BO 存在性
- `kernargs_bo_handle != 0`：表示该 kernel 的参数 BO handle，driver SHALL 验证 BO 存在性，否则返回 -EINVAL

#### Scenario 2.3: Add memcpy node

**WHEN** 外部代码调用 `sim_graph_add_memcpy_node(graph_handle, src_va, dst_va, size, is_h2d)`

**THEN** sim SHALL 验证 `graph_handle` 有效，将 memcpy node metadata 追加到 graph

**AND** 返回值 SHALL 为 0

#### Scenario 2.4: Instantiate graph

**WHEN** 外部代码调用 `sim_graph_instantiate(graph_handle, &exec_handle_out)`

**THEN** sim SHALL 验证 graph 中所有 node 的 BO handle 有效

**AND** 创建 executable handle

**AND** `*exec_handle_out` SHALL 为有效 exec handle（≥1）

#### Scenario 2.5: Launch instantiated graph (Fix-3 修订)

**WHEN** 外部代码调用 `sim_graph_launch(exec_handle, stream_id)`

**THEN** sim SHALL:
- 验证 exec_handle 有效
- 将 graph node metadata 转换为 gpfifo entries（kernel node → GPFIFO entry，memcpy node → no-op since 内联）
- driver handler 把 `exec_handle + stream_id` 转为 `gpfifo_addr + entry_count`
- 调用现有 `GpuQueueEmu::submit(uint64_t, uint32_t)` 路径（**不是** `submit_batch`，**不是** `enqueue`）
- 通过 `sim_fence_id_alloc()` 获取 fence_id（范围 `[1<<32, INT64_MAX]`，见 REQ-9）
- 返回 fence_id（≥ 1<<32）

#### Scenario 2.6: Destroy graph / executable

**WHEN** 外部代码调用 `sim_graph_destroy(graph_handle)` 或 `sim_graph_destroy_exec(exec_handle)`

**THEN** sim SHALL 释放对应资源

**AND** 返回值 SHALL 为 0

#### Scenario 2.7: Error — Invalid handle

**WHEN** 任何 graph 接口使用无效的 `graph_handle` / `exec_handle`

**THEN** sim SHALL 返回 -1

**AND** 不修改任何状态

#### Scenario 2.8: Error — Launch uninstantiated graph

**WHEN** 外部代码调用 `sim_graph_launch(exec_handle, stream_id)` 但 exec_handle 不存在

**THEN** sim SHALL 返回 -1

### REQ-3: Memory Pool 原语

**SHALL**: UsrLinuxEmu sim 层 SHALL 提供 8 个 memory pool 接口，允许创建 / 销毁 / 分配 / 属性管理 / trim。

#### Scenario 3.1: Create pool (Fix-2 修订)

**WHEN** 外部代码调用 `sim_mem_pool_create(&props, &pool_handle_out)`

**THEN** sim SHALL:
- 验证 `va_space_handle` 有效
- 向 VA Space 申请 `[props.va_base, props.va_base + props.size)` 子范围
- 分配 pool handle，记录 pool VA 区间 + 初始化 PoolInternalEntry map
- 返回 `va_base` / `va_limit` 字段

**AND** `*pool_handle_out` SHALL 为有效 pool handle（≥1）

#### Scenario 3.2: Sync alloc

**WHEN** 外部代码调用 `sim_mem_pool_alloc(pool_handle, size, &va_out)`

**THEN** sim SHALL 调用现有 `alloc_bo`（libgpu_core/gpu_buddy）分配 BO

**AND** `*va_out` SHALL 为有效 VA 地址

#### Scenario 3.3: Async alloc / free

**WHEN** 外部代码调用 `sim_mem_pool_alloc_async(pool_handle, size, stream_id, &va_out)` 或 `sim_mem_pool_free_async(va, stream_id)`

**THEN** sim SHALL 通过现有 `submit_memcpy` 路径提交（实际为 no-op memcpy）

**AND** 返回 fence_id（≥1）

#### Scenario 3.4: Set / Get attribute

**WHEN** 外部代码调用 `sim_mem_pool_set_attr(pool_handle, attr, value, size)` 或 `sim_mem_pool_get_attr(pool_handle, attr, &value, size)`

**THEN** sim SHALL 验证 `attr` 在白名单内（`RELEASE_THRESHOLD` / `REUSE_FOLLOW_EVENT_DEPENDENCIES`）

**AND** 存储 / 读取属性值

#### Scenario 3.5: Trim pool

**WHEN** 外部代码调用 `sim_mem_pool_trim(pool_handle, min_bytes)`

**THEN** sim SHALL 接受调用，记录 trim 请求（实际 trim 留待后续实现）

**AND** 返回 0

#### Scenario 3.6: Error — Alloc exceeding pool size (Fix-2 修订)

**WHEN** 外部代码调用 `sim_mem_pool_alloc(pool_handle, size, &va_out)` 且请求大小超过 pool 剩余容量

**THEN** sim SHALL:
- 在 pool VA 区间 `[va_base, va_limit)` 内查找失败
- **不**触发 `alloc_bo`（避免 buddy 全局 heap 污染）
- 返回 `-2`（即 `SIM_POOL_ERR_NOSPC`）

#### Scenario 3.7: Error — Unknown attribute

**WHEN** 外部代码调用 `sim_mem_pool_set_attr` / `get_attr` 且 `attr` 不在白名单

**THEN** sim SHALL 返回 -1

### REQ-4: IOCTL 编号预留

**SHALL**: `plugins/gpu_driver/shared/gpu_ioctl.h` SHALL 在 0x50-0x67 范围内定义 18 个新 IOCTL 编号，并附完整 struct 定义。

#### Scenario 4.1: IOCTL 编号定义

**WHEN** 编译 `gpu_ioctl.h`

**THEN** 18 个新 IOCTL 编号 SHALL 可用：`GPU_IOCTL_STREAM_CAPTURE_BEGIN` (0x50) ... `GPU_IOCTL_MEM_POOL_TRIM` (0x67)

**AND** **17 个 struct SHALL 完整定义**（详见 [`design.md §IOCTL 结构体`](../../design.md)）：7 个原有 (`gpu_stream_capture_args` / `gpu_stream_capture_status_args` / `gpu_graph_create_args` / `gpu_graph_add_kernel_node_args` / `gpu_graph_add_memcpy_node_args` / `gpu_graph_instantiate_args` / `gpu_graph_launch_args`) + 5 个 Oracle C3 补全 (`gpu_graph_destroy_args` / `gpu_graph_destroy_exec_args` / `gpu_mem_pool_destroy_args` / `gpu_mem_pool_free_async_args` / `gpu_mem_pool_trim_args`) + 5 个 mempool (`gpu_mem_pool_props` / `gpu_mem_pool_create_args` / `gpu_mem_pool_alloc_args` / `gpu_mem_pool_alloc_async_args` / `gpu_mem_pool_attr_args`)

### REQ-5: GpuDriverClient 转发（澄清 — 不在本 change scope）

**SHALL**: TaskRunner 侧的 `external/TaskRunner/src/test_fixture/gpu_driver_client.cpp` SHALL 实现 15 个 forwarding 方法（10 graph/capture + 5 mempool），每个方法对应一个或多个 IOCTL。

**本 change 不包含 GpuDriverClient forwarding 实现**（Oracle C2 修正）。forwarding 在 TaskRunner 跨仓 PR Step 3 中实现，依赖本 change 的 IOCTL #define（通过 symlink 访问）。

#### Scenario 5.1: stream_capture_begin 转发

**WHEN** GpuDriverClient::stream_capture_begin(stream_id, mode) 被调用

**THEN** GpuDriverClient SHALL 调用 `GPU_IOCTL_STREAM_CAPTURE_BEGIN` IOCTL

**AND** 返回 0 / -1

#### Scenario 5.2: stream_capture_end 转发

**WHEN** GpuDriverClient::stream_capture_end(stream_id, &graph_handle_out) 被调用

**THEN** GpuDriverClient SHALL 调用 `GPU_IOCTL_STREAM_CAPTURE_END` IOCTL

**AND** 写入 `*graph_handle_out`

#### Scenario 5.3: submit_graph 转发

**WHEN** GpuDriverClient::submit_graph(exec_handle, stream_id) 被调用

**THEN** GpuDriverClient SHALL 调用 `GPU_IOCTL_GRAPH_LAUNCH` IOCTL

**AND** 返回 fence_id（≥1）

#### Scenario 5.4: mem_pool_create 转发

**WHEN** GpuDriverClient::mem_pool_create(va_space_handle, size, flags, &pool_handle_out) 被调用

**THEN** GpuDriverClient SHALL 调用 `GPU_IOCTL_MEM_POOL_CREATE` IOCTL

**AND** 写入 `*pool_handle_out`

#### Scenario 5.5-5.9: 其他 5 个 mempool 方法

**SHALL** 遵循相同转发模式

### REQ-6: KFD ioctl handler dispatch

**SHALL**: `plugins/gpu_driver/drv/gpu_drm_driver.cpp` SHALL 新增 18 个 ioctl handler（与 0x50-0x67 IOCTL 编号一一对应），每个 handler 实现参数校验 + 调 sim 原语。

#### Scenario 6.1: 所有新 IOCTL 派发正确

**WHEN** 任何 0x50-0x67 IOCTL 被调用

**THEN** 对应 handler SHALL 被触发

**AND** 参数校验失败 SHALL 返回 `-EINVAL`

**AND** sim 调用失败 SHALL 返回 `-ENOSYS`

### REQ-7: 测试覆盖

**SHALL**: 5 个新 standalone test binary SHALL 覆盖新功能：

1. `test_sim_stream_capture_standalone`（≥6 cases）
2. `test_sim_graph_standalone`（≥12 cases）
3. `test_sim_mem_pool_standalone`（≥11 cases）
4. `test_gpu_driver_client_phase31_standalone`（9 cases）
5. `test_kfd_portability_phase31_standalone`（18 cases）

#### Scenario 7.1: 测试通过

**WHEN** 跑全部 5 个新 test binary

**THEN** 全部 SHALL PASS

**AND** 行覆盖率 SHALL ≥80%

### REQ-8: 回归零容忍

**SHALL**: 现有 Stage 1.4 Tier-1/Tier-2 70+/70+ 全部 standalone test SHALL 继续 PASS。

#### Scenario 8.1: Tier-1/Tier-2 测试无 regression

**WHEN** 跑 `build/bin/test_*_standalone` + `test_gpu_plugin`

**THEN** 全部 SHALL PASS（与 Stage 1.4 完成后基线一致）

### REQ-9: fence_id 命名空间隔离（Fix-1 新增）

**SHALL**: sim 层 fence_id 与 driver 层 fence_id SHALL 分配在不相交的整数范围内，且 `GPU_IOCTL_WAIT_FENCE` handler SHALL 根据 fence_id 范围分发到对应处理路径。

#### Scenario 9.1: fence_id 范围划分

- sim 层 fence_id SHALL `>= (1 << 32)`（通过 `sim_fence_id_alloc()` 分配）
- driver 层 fence_id SHALL `< (1 << 32)`（通过现有 `hal_fence_create()` 分配，与 Stage 1.4 一致）

#### Scenario 9.2: wait_fence handler 分发

- 当 fence_id `< (1 << 32)` → handler SHALL 调 `hal_fence_read()`（driver 层路径）
- 当 fence_id `>= (1 << 32)` → handler SHALL 调 `sim_fence_id_check()`（sim 层新路径）
- 两路径任一返回 signaled=true 即返回 0

#### Scenario 9.3: 范围不冲突

**WHEN** 同时分配 sim 层 fence_id 和 driver 层 fence_id

**THEN** 任意两个 fence_id 不相等（unique 范围保证）

## Cross-References

- 关联 ADR:
  - [ADR-015](../00_adr/adr-015-gpu-ioctl-unification.md)
  - [ADR-032](../00_adr/adr-032-h2-5-igpu-driver-abstraction.md)
  - [ADR-033](../00_adr/adr-033-h3-phase2-lifecycle.md)
  - [ADR-035](../00_adr/adr-035-governance-policy.md)
- 关联 changes:
  - TaskRunner cross-repo PR: `external/TaskRunner/docs/superpowers/cross-repo-prs/2026-07-05-phase3-1-stream-mempool-coordination.md`
  - 前置: Stage 1.4 Tier-1 (`80f6a44`) + Tier-2 (`9378153`)
- 关联 TaskRunner 文档:
  - `external/TaskRunner/docs/superpowers/specs/2026-07-02-phase3-stream-capture-design.md`
  - `external/TaskRunner/docs/superpowers/specs/2026-07-02-phase3-mempool-design.md`
  - `external/TaskRunner/docs/umd-evolution/roadmap/phase-3-deferred.md`