# Change: cuda-e2e-real-path

> **状态**: ✅ COMPLETED（2026-07-20，所有 6 个 Phase 已实施）
> **优先级**: 🔴 P1
> **创建**: 2026-07-18
> **修订历史**:
>   - v1.0 (2026-07-18): 初版
>   - v1.1 (2026-07-19): payload 契约对齐、HAL 路径、fence 超时、TaskRunner memory map
>   - **v1.2 (2026-07-19): device_ptr 语义修复(gpu_va 非 host_ptr)、wait_fence 返回值修正、cuLaunchKernel API 修正、HAL 封装妥协记录**
> **来源**: Oracle E2E gap analysis (2026-07-18)
> **依赖**: C-12 KFD Multi-File Integration (✅ completed)
> **前置**: UsrLinuxEmu 104/104 ctest, TaskRunner 13/13 ctest
> **工作目录**: `openspec/changes/2026-07-18-cuda-e2e-real-path/`

## Why

经过 Oracle 分析，从 TaskRunner 启动 CUDA 程序端到端运行的**结构链路已完整**（GpuDriverClient → ioctl → GpgpuDevice → Puller FSM → fence signal），但**数据不实际流动**。

三个致命缺口：

1. **BO 内存不可读写**：`hal_user.cpp` 的 `gpu_buddy_alloc` 只返回 heap 内偏移量（dev_addr，在 `HAL_HEAP_BASE=0x100000000` 范围内），`GpgpuDevice::mmap` 对 BO 返回 `MAP_FAILED`。`handleMapBo` 返回的 `gpu_va` 是 buddy 偏移量，**不可 dereference**。同时 TaskRunner 的 `CudaScheduler::submit_mem_alloc` 未调用 `map_bo`，`device_ptr` 是 `MemoryManager` 内部 token（非真实 GPU VA），导致 GPFIFO entry 的 dst_addr 为无效 token。
2. **MEMCPY 不拷贝数据**：Puller FSM 对所有 entry 类型走统一 `scheduler_->enqueue()` 路径，`GPU_OP_MEMCPY` 没有真实拷贝。
3. **fence 立即 signal**：`CudaScheduler::submit_launch` / `submit_memcpy_*` 在 ioctl 返回后**立即** `sync_mgr_.signal_fence()`，不等 sim 层完成（UsrLinuxEmu 侧 ADR-040 已落地，`handlePushbufferSubmitBatch` 通过 `sim_fence_id_alloc` + Puller `handleComplete` 正确 signal，但 TaskRunner 未对接）。

本 change 修复这三个缺口，并写 E2E 集成测试验证 `cuMemAlloc → cuMemcpyHtoD → cuLaunchKernel → cuStreamSynchronize → cuMemcpyDtoH` 全链路。

## What Changes

### 1. UsrLinuxEmu 端：真实内存

**1.1 `gpgpu_device.cpp` — `GpgpuDevice::mmap` 支持 BO 映射**
- 在 `mmap()` 中新增 BO 分支：当 offset 匹配 `bo_map_` 中的 handle 时，返回 BO 的 host_ptr 指针
- `hal_user.cpp` 已经 `std::malloc(HAL_HEAP_SIZE)` 分配了真实 heap — host_ptr = `hc->heap + (dev_addr - HAL_HEAP_BASE)`
- 增加 `length <= bo_size` 校验，`prot` 暂不强制执行（用户态模拟简化）
- **不新增 HAL API**（避免 ADR 流程）
- ⚠️ **HAL 封装妥协**：`gpgpu_device.cpp` 需直接读取 `hal_user_context::heap` 字段（绕过 HAL 抽象），记录为架构债务

**1.2 `gpgpu_device.cpp` — `handleMapBo` 返回 host_ptr 和 gpu_va**
- 扩展 `BoInfo` 增加 `void* host_ptr` 字段（注明 `/* user-space simulation only */`）
- `handleMapBo` 返回 `reinterpret_cast<u64>(host_ptr)`
- `handleAllocBo` 存储 `host_ptr = hc->heap + (dev_addr - HAL_HEAP_BASE)` 和 `dev_addr`（两者都存）

**1.3 BO 生命周期**：方案使用 `hc->heap + offset`，不分配独立页。FREE_BO 时 `hal_mem_free` 回收 buddy 区间，无需 munmap。

### 2. UsrLinuxEmu 端：真实 memcpy（HAL 路径）

**2.1 `hardware_puller_emu.cpp` — DISPATCH 状态增加 MEMCPY 分支**

> ⚠️ **Payload 契约**（交叉验证确认：`gpu_driver_client.h:322-324`）：
> ```cpp
> entry.payload[0] = src_addr;  // 源地址
> entry.payload[1] = dst_addr;  // 目标地址
> entry.payload[2] = size;      // 拷贝大小
> ```
> `gpu_types.h:42` 确认 `u64 payload[7]`。H2D: `src=host_ptr, dst=device_ptr`；D2H: `src=device_ptr, dst=host_ptr`。

- DISPATCH 状态中，检测 `entry.method == GPU_OP_MEMCPY`
- 走 HAL `hal_mem_read` / `hal_mem_write`（含 bounds 校验）：
  ```cpp
  u64 src = entry.payload[0];
  u64 dst = entry.payload[1];
  u64 size = entry.payload[2];

  if (size == 0 || size > MAX_MEMCPY_SIZE) {
    transitionTo(State::COMPLETE);
    break;
  }

  // 方向判定：HAL_HEAP_BASE ≤ src < HAL_HEAP_BASE+HAL_HEAP_SIZE → D2H
  // hal_mem_read/write 内部: heap_off = dev_addr - HAL_HEAP_BASE → bounds check
  bool src_is_device = (src >= HAL_HEAP_BASE &&
                        src < HAL_HEAP_BASE + HAL_HEAP_SIZE);
  if (src_is_device) {
    hal_mem_read(hal_, src, reinterpret_cast<void*>(dst), size);
  } else {
    hal_mem_write(hal_, dst, reinterpret_cast<const void*>(src), size);
  }
  transitionTo(State::COMPLETE);
  ```
- `hal_mem_read/write` 已做 bounds 校验（`heap_off + size > HAL_HEAP_SIZE` 返回 `-EINVAL`）
- 注意：`device_ptr` 在 Puller 侧必须是 **gpu_va**（HAL_HEAP_BASE 范围内），不是 host_ptr

### 3. TaskRunner 端：fence 异步语义修复 + memory map

> 🔴 **v1.2 关键修正**：TaskRunner 的 `device_ptr` 必须使用 **gpu_va**（HAL_HEAP_BASE 范围内的 dev_addr），**不是** host_ptr。host_ptr 另存于 `DeviceMemory.host_ptr` 供 TaskRunner 本地直接访问 BO 内容。

**3.1 `cuda_scheduler.cpp` — 删除立即 signal + 带超时等待**

> ⚠️ UsrLinuxEmu 侧 ADR-040 已落地：`sim_fence_id_alloc` → Puller `handleComplete` signal。

- `submit_launch`:
  - 删除 `sync_mgr_.signal_fence(fence)` 
  - `driver_fence > 0` 时调用 `driver_->wait_fence(driver_fence, 5000, &status)`，**必须检查 status=1 才 signal**
  - `driver_fence <= 0` 时返回 `-EIO`（不 fallback 立即 signal）
- `submit_memcpy_h2d` / `submit_memcpy_d2h`:
  - 同上 pattern
  - 移除 `memory_mgr_.memcpy_h2d/d2h` 调用（避免 Puller 时序冲突）
- `submit_mem_alloc`:
  - 保留立即 signal（alloc 同步操作）
  - **修正**：调用 `driver_->map_bo(bo_handle)` 获取 host_ptr 存入 `DeviceMemory.host_ptr`
  - `result.device_ptr = gpu_va`（从 `alloc_bo_vram` 获取，不是 map_bo 返回的 host_ptr！）
  - 这样 Puller 收到 GPFIFO entry 的 dst=gpu_va 在 HAL_HEAP_BASE 范围内，HAL bounds 校验通过

**3.2 `memory_manager.hpp` — 增加 host_ptr 参数**
- `MemoryManager::allocate(size, type, host_ptr)` 增加 `host_ptr` 参数（默认 `nullptr`）
- `free()` 中 DEVICE_LOCAL + 外部 host_ptr **不** `std::free`（归 HAL heap 管）
- `find()` 按 `device_ptr`（=gpu_va）查找，与 token 地址范围不冲突

**3.3 `gpu_driver_client.h` — 修正 wait_fence 超时语义**

> 🔴 **v1.2 P0 修正**：`GpuDriverClient::wait_fence`（line 369-384）当前 `ioctl >= 0` 即返回成功，**超时被 swallow**。

```cpp
// 修正后（约 line 378-384）：
uint32_t status = 0;
int ret = ioctl(fd_, GPU_IOCTL_WAIT_FENCE, &args);
if (ret < 0) return -errno;
if (status_out) *status_out = args.status;
return (args.status == 1) ? 0 : -ETIMEDOUT;  // ← 关键修改：status≠1 返回超时
```

### 4. Puller FSM 增强：kernel launch no-op

**4.1 `hardware_puller_emu.cpp` — GPU_OP_LAUNCH_KERNEL 处理**
- DISPATCH 状态中，显式调用 `translator_->translate(current_entry_)` 触发 callback
- ⚠️ **验证**：当前 `scheduler_->enqueue()` 只是入队不调 translator，需要在 DISPATCH 中显式调用
- 提取 kernel name + grid/block dims，打印日志后 transition to COMPLETE

### 5. E2E 集成测试

> 🔴 **v1.2 修正**：`cuLaunchKernel` 第一个参数必须是 `CUfunction`，需先 `cuModuleLoad` + `cuModuleGetFunction`。

**5.1 TaskRunner 端 `tests/umd/test_cuda_e2e_real.cpp`**（使用 CUDA Driver API shim）：

```cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include <cuda.h>  // CUDA Driver API
#include <cstring>

TEST_CASE("CUDA E2E: alloc → memcpy H2D → launch kernel → sync → memcpy D2H") {
  // 1. Alloc 4KB
  CUdeviceptr dptr = 0;
  CHECK(cuMemAlloc(&dptr, 4096) == CUDA_SUCCESS);
  REQUIRE(dptr != 0);

  // 2. Load dummy module + function
  CUmodule mod = 0;
  CUfunction kernel = 0;
  // 使用 fake.cubin 或空 payload（shim 层 no-op）
  CHECK(cuModuleLoad(&mod, "dummy.cubin") == CUDA_SUCCESS);
  REQUIRE(mod != 0);
  CHECK(cuModuleGetFunction(&kernel, mod, "dummy") == CUDA_SUCCESS);
  REQUIRE(kernel != 0);

  // 3. H2D memcpy
  const char input[] = "hello";
  REQUIRE(cuMemcpyHtoD(dptr, input, 5) == CUDA_SUCCESS);

  // 4. Launch kernel
  void* kernelParams[] = { &dptr };
  REQUIRE(cuLaunchKernel(kernel, 1,1,1, 1,1,1, 0, 0, kernelParams, nullptr) == CUDA_SUCCESS);

  // 5. Sync
  REQUIRE(cuCtxSynchronize() == CUDA_SUCCESS);

  // 6. D2H + verify
  char output[5] = {};
  REQUIRE(cuMemcpyDtoH(output, dptr, 5) == CUDA_SUCCESS);
  CHECK(std::memcmp(output, input, 5) == 0);

  cuMemFree(dptr);
  cuModuleUnload(mod);
}
```

**路径 A**：如果 CUDA Driver API shim 不支持 `cuModuleLoad` → 改用 `CudaRuntimeApi::launch_kernel("dummy", ...)`。

**5.2 失败路径测试**
```cpp
TEST_CASE("MEMCPY oversize returns error") { /* size=0 或 >MAX → -EINVAL */ }
TEST_CASE("free then map should fail") { /* FREE_BO 后 MAP_BO → -EINVAL */ }
TEST_CASE("fence wait timeout") { /* wait_fence(5000) status=0 → -ETIMEDOUT */ }
```

## Acceptance

### UsrLinuxEmu 端
- [ ] `MAP_BO` 返回的 `gpu_va` 可读写（写 8 字节 → 读回 → 验证相等）
- [ ] `GPU_OP_MEMCPY` entry 走 HAL `hal_mem_read`/`hal_mem_write` 路径（H2D + D2H 双向）
- [ ] Puller 收到的 `device_ptr`（payload[0]/[1]）是 gpu_va（HAL_HEAP_BASE 范围内），HAL bounds 校验通过
- [ ] `GPU_OP_LAUNCH_KERNEL` 被正确解码（显式调用 translator）
- [ ] `GpuDriverClient::wait_fence` **超时不返回成功**（status=0 → -ETIMEDOUT）
- [ ] 104/104 ctest PASS（无回归）
- [ ] docs-audit 43/43 PASS

### TaskRunner 端
- [ ] `submit_mem_alloc` 返回 `device_ptr = gpu_va`（不是 host_ptr），host_ptr 存入 `DeviceMemory.host_ptr`
- [ ] `CudaScheduler` fence 异步：等 driver_fence 完成（status=1）后才 signal
- [ ] `wait_fence(driver_fence, 5000, &status)` 正确检查 status，超时不 signal
- [ ] `submit_memcpy_*` 移除 `memory_mgr_.memcpy_*` 调用
- [ ] `test_cuda_e2e_real` 编译通过 + 4 happy + 3 fail TEST_CASEs PASS
- [ ] 13/13 ctest PASS（无回归）→ 14/14

### 跨仓同步（ADR-035 §Rule 5.1）
- [ ] TaskRunner 先 commit（含 E2E 测试 + fence 修复 + memory map）
- [ ] UsrLinuxEmu end commit + bump submodule pointer
- [ ] 双仓 `ctest` 全部 PASS

## 测试方法

```bash
# UsrLinuxEmu 端
cd build && ctest -R "test_kfd_l1_l2_bridge" --output-on-failure

# TaskRunner E2E test
cd external/TaskRunner
cmake -B build_umd -DTASKRUNNER_BUILD_MODE=umd-evolution
cmake --build build_umd -j4 --target test_cuda_e2e_real
./test_cuda_e2e_real

# 双仓回归
ctest --test-dir /workspace/project/UsrLinuxEmu/build -j4      # 104/104
ctest --test-dir external/TaskRunner/build_umd -j4              # 13/13 → 14/14
```

## 关联 ADR

- ADR-036 (three-way separation)
- ADR-018 (driver/sim separation)
- ADR-023 (HAL interface)
- ADR-040 (Puller Fence Completion) — **UsrLinuxEmu 侧已落地**
- ADR-035 §Rule 5.1 (cross-repo sync) — **严格遵循 TaskRunner 先 commit**

## 关键设计决策

| 决策点 | 选择 | 理由 |
|--------|------|------|
| BO 映射方案 | `GpgpuDevice::mmap` 返回 heap 基址 | 不改 HAL 接口（免 ADR），不分配新页 |
| MEMCPY 实现 | 走 HAL `hal_mem_read/write` | 有 bounds 校验，安全；H2D/D2H 方向自动判定 |
| Payload 契约 | `payload[0]=src, payload[1]=dst` | 与 `gpu_driver_client.h:322-324` 一致 |
| Fence 超时 | 5000ms + 必须检查 status=1 | 避免 sim crash 永久挂死 + 超时不被 swallow |
| **TaskRunner device_ptr** | **gpu_va**（HAL_HEAP_BASE 范围内） | Puller HAL 要求 dev_addr ∈ [BASE, BASE+SIZE)，host_ptr 传入会下溢 |
| host_ptr | 另存于 `DeviceMemory.host_ptr` | TaskRunner 本地直接读写 BO 内容时用 |
| E2E API | CUDA Driver API（cuModuleLoad + cuLaunchKernel(CUfunction)） | 符合 CUDA 标准 API 语义 |
| 跨仓顺序 | TaskRunner 先 commit | ADR-035 §Rule 5.1 |
| HAL 封装 | `gpgpu_device.cpp` 直接读 `hal_user_context::heap` | 架构妥协，记录为债务（避免改 HAL API 免 ADR） |
| `wait_fence` 超时语义 | `status_out=0` → `return -ETIMEDOUT` | 当前 `status=0` 被 swallow 为成功（P0 修复） |

## 关联 SSOT

- `docs/05-advanced/kfd-multi-file.md`（C-12 完成状态）
- `docs/roadmap/stage-3-v1.0.md`（Stage 3 v1.0）
- `external/TaskRunner/AGENTS.md`（scope classification）