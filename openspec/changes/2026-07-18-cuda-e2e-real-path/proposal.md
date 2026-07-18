# Change: cuda-e2e-real-path

> **状态**: 📋 PROPOSED
> **优先级**: 🔴 P1
> **创建**: 2026-07-18
> **来源**: Oracle E2E gap analysis (2026-07-18)
> **依赖**: C-12 KFD Multi-File Integration (✅ completed)
> **前置**: UsrLinuxEmu 104/104 ctest, TaskRunner 13/13 ctest
> **工作目录**: `openspec/changes/2026-07-18-cuda-e2e-real-path/`

## Why

经过 Oracle 分析，从 TaskRunner 启动 CUDA 程序端到端运行的**结构链路已完整**（GpuDriverClient → ioctl → GpgpuDevice → Puller FSM → fence signal），但**数据不实际流动**。

三个致命缺口：

1. **BO 内存不可读写**：`hal_user.cpp` 的 `gpu_buddy_alloc` 只返回 heap 内偏移量，`GpgpuDevice::mmap` 对 BO 返回 `MAP_FAILED`。无 OS 内存映射，`cuMemcpyHtoD` 数据无处可放
2. **MEMCPY 不拷贝数据**：Puller FSM 对所有 entry 类型走统一状态机路径，`GPU_OP_MEMCPY` 没有真实 `memcpy`
3. **fence 立即 signal**：`CudaScheduler::submit_launch` 在 ioctl 返回后**立即** `sync_mgr_.signal_fence()`，不等 sim 层完成

本 change 修复这三个缺口，并写 E2E 集成测试验证 `cuMemAlloc → cuMemcpyHtoD → cuLaunchKernel → cuStreamSynchronize → cuMemcpyDtoH` 全链路。

## What Changes

### 1. UsrLinuxEmu 端：真实内存 + 真实 memcpy

**1.1 `hal_user.cpp` — 真实 mmap 内存分配**
- `user_mem_alloc`: `gpu_buddy_alloc` 返回偏移量后，实际 `mmap` 分配真实页
- 或：不改 `hal_user`，在 `GpgpuDevice::mmap` 中为 BO offset 做 mmap
- `GpgpuDevice::handleMapBo`: 返回可 dereference 的指针（需在 `bo_map_` 中额外存储 mmap 地址）

**1.2 `hardware_puller_emu.cpp` — GPU_OP_MEMCPY 真实拷贝**
- DISPATCH 或 DECODE 状态中，检测 `entry.method == GPU_OP_MEMCPY`
- 从 entry payload 提取 dst/src/size，调用 `memcpy`
- 或走 HAL 的 `hal_mem_read`/`hal_mem_write`

### 2. TaskRunner 端：fence 异步语义修复

**2.1 `cuda_scheduler.cpp` — 删除立即 signal_fence**
- `submit_launch` / `submit_memcpy` / `submit_mem_alloc`: 删除 `sync_mgr_.signal_fence(fence)` 调用
- 改为由 UsrLinuxEmu sim 层的 `handleComplete` signal fence
- TaskRunner 侧 `wait_fence` 正常等待

### 3. Puller FSM 增强：kernel launch no-op

**3.1 `hardware_puller_emu.cpp` — GPU_OP_LAUNCH_KERNEL 处理**
- DISPATCH 状态中，`GlobalScheduler::enqueue` 调用 `GpfifoToLaunchParamsTranslator`
- 提取 kernel name + grid/block dims，打印日志后完成

### 4. E2E 集成测试

**4.1 TaskRunner 端 `tests/umd/test_cuda_e2e_real.cpp`**
- `cuMemAlloc(4096)` → 获得 device ptr
- `cuMemcpyHtoD("hello", 5)` → 数据写入 GPU
- `cuLaunchKernel("dummy", 1,1,1, 1,1,1)` → kernel 启动
- `cuStreamSynchronize` → 等待完成
- `cuMemcpyDtoH(buf, 5)` → 读回
- `REQUIRE(memcmp(buf, "hello", 5) == 0)`

## Acceptance

### UsrLinuxEmu 端
- [ ] `MAP_BO` 返回的 `gpu_va` 可读写（写 8 字节 → 读回 → 验证相等）
- [ ] `GPU_OP_MEMCPY` entry 走真实 `memcpy` 路径
- [ ] `GPU_OP_LAUNCH_KERNEL` entry 被正确解码（kernel name + grid/block dims 日志输出）
- [ ] 104/104 ctest PASS（无回归）
- [ ] docs-audit 43/43 PASS

### TaskRunner 端
- [ ] `CudaScheduler` fence 异步语义正确（不立即 signal）
- [ ] `test_cuda_e2e_real` 编译通过 + 3 TEST_CASEs PASS
- [ ] 现有 13/13 ctest PASS（无回归）

## 测试方法

```bash
# UsrLinuxEmu 端
cd build && ctest -R "test_kfd_l1_l2_bridge" --output-on-failure

# TaskRunner E2E test
cd external/TaskRunner/build_umd
cmake .. -DTASKRUNNER_BUILD_MODE=umd-evolution \
  -DUsrLinuxEmu_DIR=/workspace/project/UsrLinuxEmu/build/cmake
cmake --build . -j4 --target test_cuda_e2e_real
./test_cuda_e2e_real

# 双仓回归
ctest --test-dir /workspace/project/UsrLinuxEmu/build -j4      # 104/104
ctest --test-dir external/TaskRunner/build_umd -j4              # 13/13
```

## 关联 ADR

- ADR-036 (three-way separation)
- ADR-018 (driver/sim separation)
- ADR-023 (HAL interface)
- ADR-040 (Puller Fence Completion)
- ADR-035 §Rule 5.1 (cross-repo sync)

## 关联 SSOT

- `docs/05-advanced/kfd-multi-file.md`（C-12 完成状态）
- `docs/roadmap/stage-3-v1.0.md`（Stage 3 v1.0）
- `external/TaskRunner/AGENTS.md`（scope classification）
