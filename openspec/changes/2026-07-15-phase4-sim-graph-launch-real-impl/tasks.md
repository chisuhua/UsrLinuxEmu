# Tasks: phase4-sim-graph-launch-real-impl

> **状态**: 📋 PROPOSED
> **目标**: sim_graph_launch 真实异步执行（替换 PoC）

## 1. 设计（半天）

- [ ] 1.1 读 `external/TaskRunner/docs/superpowers/cross-repo-prs/2026-07-05-phase3-1-stream-mempool-coordination.md` §3.1.1 B-1 决策
- [ ] 1.2 设计 sim_graph_launch → GpuQueueEmu::submit 桥接
- [ ] 1.3 设计 gpfifo_addr 内存布局（用户态 allocated）
- [ ] 1.4 设计 fence signal 路径（通过 GlobalScheduler 回调）
- [ ] 1.5 写 design.md 详细文档

## 2. 实现（3-4 天）

- [ ] 2.1 `plugins/gpu_driver/sim/graph.cpp`：sim_graph_launch 重写
- [ ] 2.2 加 fence scheduler 集成
- [ ] 2.3 driver-side handler (`gpgpu_device.cpp` 或 `gpu_drm_driver.cpp`) 改用 GpuQueueEmu::submit
- [ ] 2.4 加新 unit test（async fence 不是 immediate signal）

## 3. 测试（半天）

- [ ] 3.1 单测：sim_graph_launch 返回后 fence NOT signalled
- [ ] 3.2 单测：scheduler tick 后 fence signalled
- [ ] 3.3 集成：test_gpu_driver_client_phase31 验证 launch 完整链
- [ ] 3.4 跨仓：TaskRunner test_cu_graph 端到端

## 4. 验证 / commit（半天）

- [ ] 4.1 ctest 111/111 PASS（含新测试）
- [ ] 4.2 docs-audit 无新 warning
- [ ] 4.3 cross-repo TaskRunner E2E 测试 PASS
- [ ] 4.4 commit：`feat(sim): real async graph_launch via GpuQueueEmu::submit`
- [ ] 4.5 PR + merge，通知 TaskRunner 侧
