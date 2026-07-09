# Change: phase4-sim-graph-launch-real-impl

> **状态**: 🚀 ACTIVE（ADR-040/041/043 已 Accepted，所有 artifacts 完备）
> **优先级**: 🔵 P3
> **创建**: 2026-07-15
> **最后更新**: 2026-07-09（Metis 二次审查后与 ADR-040/041/043 对齐）
> **来源**: F.6 follow-up（`3b2eeef docs(openspec): add F.6 follow-up — sim_graph_launch real async impl`） + cross-repo doc B-1 决策
> **依赖**: C-02 stage3-ioctl-dispatch-completeness（✅ 已合并）
> **架构基础**: ADR-040 (Puller Fence Completion), ADR-041 (Graph → GPFIFO Serialization), ADR-043 (CP Portability Boundary)
> **工作目录**: `openspec/changes/2026-07-15-phase4-sim-graph-launch-real-impl/`

## Why

当前 `sim_graph_launch` 是 PoC 实现：调用后 fence **立即 signal**，而非真正异步执行 task。这与真实 CUDA runtime 行为不符，阻碍 TaskRunner Phase 4 E2E 测试。

F.6 follow-up 已 doc（见 `3b2eeef` commit）。需要**真实异步实现**。

## What Changes

### 1. Puller Fence Completion 机制（ADR-040）

`HardwarePullerEmu` 新增 fence completion 回调：

- `submitBatch(uint64_t gpfifo_addr, uint32_t entry_count, uint64_t fence_id = 0)` — 签名变更
- `handleComplete()` 中，当 batch 全量完成（`current_index_ >= total_entries_`）时调用 `sim_fence_id_signal(pending_fence_id_)`
- `GpuQueueEmu::submit()` 签名同步变更以透传 `fence_id`

**范围扩大**：同步修复 pushbuffer 路径的 fence 永不 signal bug——`handlePushbufferSubmitBatch` 改用 `sim_fence_id_alloc()` 替代 `hal_fence_create()`，传入 Puller 路径。

### 2. Graph Node → GPFIFO 预编译（ADR-041）

`sim_graph_instantiate` 新增 graph node → GPFIFO entry 序列化：

- `ExecutableGraph` 新增 `precompiled_entries`（`std::vector<gpu_gpfifo_entry>`）、`gpfifo_gpu_addr`、`entry_count`
- 序列化遵循现有 `GpfifoToLaunchParamsTranslator` 的 pack 约定（`gpfifo_translator.h:18-19`）
- Phase 4 使用 sim-internal heap 存储 GPFIFO buffer；Phase 5 迁移到 HAL `gpfifo_alloc`/`gpfifo_free`

**KERNEL node** → `GPU_OP_LAUNCH_KERNEL` entry:
```
payload[0] = kernel GPU VA
payload[1] = packed grid dim: x | (y<<16) | (z<<24)
payload[2] = packed block dim: x | (y<<8)  | (z<<16)
payload[3] = kernargs GPU VA (instantiate 时解析)
```

**MEMCPY node** → `GPU_OP_MEMCPY` entry:
```
payload[0] = src_va, payload[1] = dst_va, payload[2] = size
```

### 3. sim_graph_launch 重构（ADR-043 CP 边界）

`sim_graph_launch` 改为查表输出模式，不调 Puller、不 signal fence：

```cpp
// 新签名（sim/graph.h）
int sim_graph_launch(uint64_t exec_handle, uint32_t stream_id,
                     uint64_t *gpfifo_addr_out, uint32_t *entry_count_out);
```

drv 层 handler（`handleGraphLaunch`）负责：
1. 调 `sim_graph_launch` 获取 `gpfifo_addr + entry_count`
2. `getQueue(stream_id)` 查找 queue
3. `q->submit(gpfifo_addr, entry_count, fence_id)` 提交
4. `hal_doorbell_ring(stream_id)` 触发 Puller
5. `return fence_id` 返回给用户

### 4. 跨仓决策对齐

cross-repo doc §3.1.1 B-1 决策：
> 实际只有 `GpuQueueEmu::submit(uint64_t gpfifo_addr, uint32_t entry_count)` (`gpu_queue_emu.h:113`)。
> `submit_batch` / `enqueue` **不存在**。

新增 `fence_id` 参数后签名变为 `submit(uint64_t, uint32_t, uint64_t = 0)`，保持向后兼容。

## 破坏性 API 变更

| API | 旧签名 | 新签名 |
|-----|--------|--------|
| `HardwarePullerEmu::submitBatch()` | `(uint64_t, uint32_t)` | `(uint64_t, uint32_t, uint64_t = 0)` |
| `GpuQueueEmu::submit()` | `(uint64_t, uint32_t)` | `(uint64_t, uint32_t, uint64_t = 0)` |
| `sim_graph_launch()` | `(uint64_t, uint32_t)` → `int64_t` | `(uint64_t, uint32_t, uint64_t*, uint32_t*)` → `int` |

`fence_id=0` 哨兵值表示"不触发 fence 完成回调"。`0` 不在 sim fence 范围 `[2³², INT64_MAX]` 内，安全。

## Acceptance

- [ ] `sim_graph_launch` 输出 `gpfifo_addr` + `entry_count`，不 signal fence
- [ ] `sim_graph_instantiate` 预编译 GPFIFO entries，存入 `ExecutableGraph`
- [ ] `handleGraphLaunch` 调 `q->submit()` → doorbell → return fence_id，fence 不立即 signal
- [ ] Puller `handleComplete()` 在 batch 全量完成后 signal `sim_fence_id`
- [ ] `WAIT_FENCE` 轮询到 fence signaled 后返回成功
- [ ] Pushbuffer 路径同步修复：fence 通过 Puller 完成 signal
- [ ] `test_sim_graph_standalone` 更新：验证 gpfifo_addr/entry_count 输出 + async fence 行为
- [ ] `test_hardware_puller_emu_standalone` 更新：验证 fence signal 路径
- [ ] `test_gpu_ioctl_standalone` 更新：验证 pushbuffer fence 不再永不 signal
- [ ] Cross-repo: TaskRunner `test_cu_graph` E2E 测试 PASS

## 测试方法

```bash
# UsrLinuxEmu 侧单元测试
cd build && ctest -R "test_sim_graph" -V
cd build && ctest -R "test_hardware_puller" -V
cd build && ctest -R "test_gpu_ioctl" -V

# 全量回归
cd build && ctest --output-on-failure

# 跨仓联调（UMD-EVOLUTION 模式）
cd external/TaskRunner && mkdir -p build && cd build
cmake -DTASKRUNNER_BUILD_MODE=umd-evolution ..
make -j4
ctest -R test_cu_graph -V
```

## Cross-Repo 影响

- **TaskRunner 侧**：`sim_graph_launch` 签名变更需同步更新 shim 层
- **ABI 共享**：`gpu_gpfifo_entry` payload 布局未变（仍然 `u64[7]`）
- **E2E 验证**：TaskRunner `test_cu_graph` 从 UMD-EVOLUTION 模式构建

## Dependencies

- **C-02** stage3-ioctl-dispatch-completeness（✅ 已合并，PR #26）
- **GPU IOCTL 0x58** (GRAPH_LAUNCH) — handler 已存在，需重写实现
- **ADR-040** (Puller Fence Completion) — 架构决策基础
- **ADR-041** (Graph → GPFIFO Serialization) — 架构决策基础
- **ADR-043** (CP Portability Boundary) — 架构决策基础

## 架构参考

- [ADR-040 Puller Fence Completion](../../docs/00_adr/adr-040-puller-fence-completion.md)
- [ADR-041 Graph Node → GPFIFO Serialization](../../docs/00_adr/adr-041-graph-node-to-gpfifo-serialization.md)
- [ADR-043 CP Portability Boundary](../../docs/00_adr/adr-043-cp-portability-boundary.md)
- [ADR-021 Hardware Puller FSM](../../docs/00_adr/adr-021-hardware-puller.md)
- [ADR-024 User Mode Queue](../../docs/00_adr/adr-024-user-mode-queue-submission.md)
- [ADR-036 3-Way Separation](../../docs/00_adr/adr-036-three-way-separation.md)