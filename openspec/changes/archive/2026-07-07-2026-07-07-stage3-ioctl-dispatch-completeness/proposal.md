# Change: stage3-ioctl-dispatch-completeness

> **状态**: 📋 PROPOSED
> **优先级**: 🔴 P0（architectural bug）
> **创建**: 2026-07-07
> **来源**: PR #26（已存在，未 merge）
> **依赖**: C-01 fix-docs-audit-runstage2-and-baseline
> **工作目录**: `openspec/changes/2026-07-07-stage3-ioctl-dispatch-completeness/`

## Why

PR #20 添加了 18 个新 IOCTL（0x50-0x67），但**只更新了 `gpu_ioctls[]` DRM 表**，没有更新 runtime 派发路径 `GpgpuDevice::ioctl::kTable`。结果：

- `ioctl(fd, GPU_IOCTL_STREAM_CAPTURE_BEGIN, &args)` 在 runtime 返回 `-EINVAL`（找不到匹配 handler）
- 18 个新 IOCTL 中 **只有 3 个（mempool）**真正可达
- Stream Capture + Graph 10 个 IOCTL 完全 unreachable

`test_kfd_portability_phase31_standalone` 只验证 IOCTL `#define` 数字（编译期），不实际调用，所以未发现。

## What Changes

应用 PR #26 的内容（已存在，5 files, +758/-2）：

### 1. `plugins/gpu_driver/drv/gpgpu_device.h`
- `kNumIoctls`: 16 → 31
- 18 个新 member function handler 声明

### 2. `plugins/gpu_driver/drv/gpgpu_device.cpp`
- 18 个新 `kTable` entries
- 18 个新 member function impl（delegate 到 sim_*）

### 3. `tests/test_gpu_plugin.cpp` (+282 行)
- 18 个新 `TEST_CASE_METHOD`（每个新 IOCTL 一个）
- 每个测试：open `/dev/gpgpu0` → `ioctl(GPU_IOCTL_*, &args)` → verify return + OUT fields

### 4. `tests/test_gpu_driver_client_phase31_standalone.cpp` (NEW, 9 cases)
跨仓集成测试，模拟 GpuDriverClient 调用链：
- STREAM_CAPTURE_BEGIN/END/STATUS 链
- GRAPH_CREATE→ADD_NODE→INSTANTIATE→LAUNCH→DESTROY_EXEC 链
- MEM_POOL_CREATE/ALLOC/ALLOC_ASYNC 链

### 5. `tests/CMakeLists.txt`
- 把 `test_gpu_plugin` 移到 `CATCH2_SIM_TESTS`
- 加新 binary

## Acceptance

- [ ] PR #26 merged
- [ ] `kNumIoctls = 31`
- [ ] 27/27 新测试 PASS
- [ ] 84/84 → 111/111 ctest 全绿
- [ ] docs-audit §2.6 更新 expected 到 31（合并到 PR #26 之后的 C-01 修复中）

## 测试方法

```bash
cd build
ctest -R test_gpu_plugin                       # +18 cases
ctest -R test_gpu_driver_client_phase31        # NEW 9 cases
ctest                                          # 111/111
```

## Cross-Repo 影响

是 TaskRunner Phase 4 真实化（`cu*` real bridge via `GpuDriverClient`）的前置。修复后，TaskRunner `GpuDriverClient::submit_graph` 等 15 个 override 真正可达 UsrLinuxEmu sim 层。

## Dependencies

- **C-01** fix-docs-audit（必须先合入，否则 docs-audit 阻塞 PR #26）
- Stage 1.4 Tier-1/Tier-2 边界契约保持（G1-G4 invariant）

## Unblocks

- C-05 (errno audit)
- C-08 (phase4-sim-graph-launch)
- C-09 (phase4-cu-mempool-real-va)
