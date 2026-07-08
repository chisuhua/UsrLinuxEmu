# Change: phase4-sim-graph-launch-real-impl

> **状态**: 📋 PROPOSED
> **优先级**: 🔵 P3
> **创建**: 2026-07-15
> **来源**: F.6 follow-up（`3b2eeef docs(openspec): add F.6 follow-up — sim_graph_launch real async impl`） + cross-repo doc B-1 决策
> **依赖**: C-02 stage3-ioctl-dispatch-completeness
> **工作目录**: `openspec/changes/2026-07-15-phase4-sim-graph-launch-real-impl/`

## Why

当前 `sim_graph_launch` 是 PoC 实现：调用后 fence **立即 signal**，而非真正异步执行 task。这与真实 CUDA runtime 行为不符，阻碍 TaskRunner Phase 4 E2E 测试。

F.6 follow-up 已 doc（见 `3b2eeef` commit）。需要**真实异步实现**。

## What Changes

### 1. sim_graph_launch 重写

`plugins/gpu_driver/sim/graph.cpp`：
- **当前**：fence 立即 signal
- **改为**：通过 `GpuQueueEmu::submit(uint64_t gpfifo_addr, uint32_t entry_count)` 调度执行
- fence 返回后，等 GlobalScheduler tick 后才 signal

### 2. 跨仓决策对齐

cross-repo doc §3.1.1 B-1 决策：
> TaskRunner spec 必须避免引用 `GpuQueueEmu::submit_batch` 或 `GpuQueueEmu::enqueue`（**不存在**）。
> 实际只有 `GpuQueueEmu::submit(uint64_t gpfifo_addr, uint32_t entry_count)` (`gpu_queue_emu.h:113`)。
>
> 集成路径：`gpu_graph_launch_args` 携带 `exec_handle` + `stream_id` → driver-side handler 转 `gpfifo_addr + entry_count` → 调 `GpuQueueEmu::submit`。

## Acceptance

- [ ] sim_graph_launch fence 不立即 signal
- [ ] 调用 `wait_fence` 等待真实 task 调度完成
- [ ] 加 test case 验证 async 行为
- [ ] 在 `gpfifo_addr` mmap 后从 userspace 可读
- [ ] Cross-repo: TaskRunner Phase 4 E2E 测试能跑通完整 graph launch

## 测试方法

```bash
# UsrLinuxEmu 侧
cd build && ctest -R "test_sim_graph" -V

# 跨仓联调
cd external/TaskRunner/build && ctest -R "test_cu_graph"
```

## Cross-Repo 影响

需要 TaskRunner 侧相应更新测试用例（验证 graph launch async 行为）。

## Dependencies

- **C-02** stage3-ioctl-dispatch-completeness（必须先合入，runtime path 可达）
- Phase 4 GPU IOCTL 0x57 (GRAPH_LAUNCH) 必须可访问
