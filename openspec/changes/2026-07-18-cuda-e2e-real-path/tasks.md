# Tasks: cuda-e2e-real-path

> **状态**: ✅ APPROVED（v1.2 — 2026-07-19 修订，经两轮双审查）
> **修订说明 v1.2**: 修复 device_ptr 语义（gpu_va 非 host_ptr）、wait_fence 超时返回值、cuLaunchKernel API 修正、HAL 封装妥协记录
> **目标**: CUDA 程序 E2E 运行（cuMemAlloc → memcpy → launch kernel → sync）全链路打通
> **工期**: ~15h（2.5d）
> **依赖**: C-12 KFD Multi-File Integration ✅
> **涉及仓**: UsrLinuxEmu + TaskRunner（跨仓，遵循 ADR-035 §Rule 5.1：TaskRunner 先 commit）

---

## 架构背景

当前 E2E 链路结构完整但数据不流动。三个致命缺口 + TaskRunner 侧 device_ptr 语义缺口：

```
cuMemAlloc → driver_->alloc_bo_vram()
    → ioctl(ALLOC_BO) → hal_mem_alloc → gpu_buddy_alloc (返回 dev_addr, HAL_HEAP_BASE 范围)
    → 需获取 gpu_va（=dev_addr）作为 device_ptr，host_ptr 另存
    → CudaScheduler 未调用 map_bo → device_ptr 是 MemoryManager token

cuMemcpyHtoD → driver_->submit_memcpy(src=input, dst=device_ptr, is_h2d=true)
    → GpuDriverClient 编码 entry { payload[0]=src, payload[1]=dst, payload[2]=size }
    → Puller 收到 dst=gpu_va（必须在 HAL_HEAP_BASE 范围，HAL bounds 校验才能通过）
    → Puller FSM → DISPATCH → enqueue() → COMPLETE (NO memcpy!)

cuLaunchKernel → driver_->submit_launch()
    → 立即 sync_mgr_.signal_fence() → fake completion
    （UsrLinuxEmu ADR-040 已落地，TaskRunner 未对接）

cuStreamSynchronize → fence 已立即 signal → 实际不等待 sim 操作
```

> ⚠️ **Payload 契约（交叉验证确认）**：
> - `gpu_driver_client.h:322-324`：`payload[0]=src_addr, payload[1]=dst_addr, payload[2]=size`
> - `gpu_types.h:42`：`u64 payload[7]`
> - `gpgpu_device.cpp:395-397`同步路径：`u64 src=e.payload[0]; u64 dst=e.payload[1]`
> - **结论**：`payload[0]=src, payload[1]=dst`

> 🔴 **device_ptr 语义（v1.2 关键修正）**：
> - 传给 Puller HAL 的 GPFIFO entry payload 地址必须是 **gpu_va**（HAL_HEAP_BASE 范围 0x100000000+）
> - `hal_user.cpp:39`：`heap_off = dev_addr - HAL_HEAP_BASE` — dev_addr 必须在 BASE 范围，host_ptr 传入会下溢
> - TaskRunner 用 gpu_va 作为 device_ptr，host_ptr 另存于 DeviceMemory.host_ptr

---

## Phase 0: Payload 契约交叉验证（不重复执行，已确认）

- [x] 0.1 `gpu_driver_client.h:322-324`：`payload[0]=src, payload[1]=dst` ✅
- [x] 0.2 `gpgpu_device.cpp:395-397` 同步路径一致 ✅
- [x] 0.3 `gpu_types.h:42`：`payload[7]` ✅
- [x] 0.4 H2D: `src=host_ptr, dst=device_ptr`（=gpu_va）; D2H: `src=device_ptr`, `dst=host_ptr` ✅
- [x] 0.5 `hal_user.cpp:39`：`heap_off = dev_addr - HAL_HEAP_BASE` — 要求 dev_addr ∈ [BASE, BASE+SIZE) ✅

---

## Phase A: UsrLinuxEmu 端内存真实化 + TaskRunner 内存映射（3.5h）

### A.1 分析当前内存模型

- [ ] A.1.1 阅读 `plugins/gpu_driver/hal/hal_user.cpp:57-72`（`user_mem_alloc`）
- [ ] A.1.2 阅读 `plugins/gpu_driver/drv/gpgpu_device.cpp:689-725`（`GpgpuDevice::mmap`）
- [ ] A.1.3 阅读 `plugins/gpu_driver/drv/gpgpu_device.cpp:261-283`（`handleMapBo`）
- [ ] A.1.4 阅读 `external/TaskRunner/src/test_fixture/cuda_scheduler.cpp:90-123`（`submit_mem_alloc`）
- [ ] A.1.5 阅读 `GpgpuDevice::handleAllocBo`，确认 `BoInfo` 结构体和 `bo_map_`

### A.2 实现：GpgpuDevice::mmap 支持 BO 映射

- [ ] A.2.1 在 `GpgpuDevice::mmap` 中新增 BO 映射分支
- [ ] A.2.2 扩展 `BoInfo`（增加 `host_ptr` 字段，注明 `/* user-space simulation only */`）
- [ ] A.2.3 在 `handleAllocBo` 中存储 `host_ptr = hc->heap + (dev_addr - HAL_HEAP_BASE)`（⚠️ 架构妥协：直接读 `hal_user_context::heap`）
- [ ] A.2.4 在 `handleMapBo` 中返回 host_ptr
- [ ] A.2.5 确认 FREE_BO 时无需 munmap

### A.3 TaskRunner 端：映射 BO 并正确设置 device_ptr

> 🔴 **v1.2 关键**：`device_ptr` 必须 = `gpu_va`（HAL_HEAP_BASE 范围），host_ptr 另存

- [ ] A.3.1 验证 `IGpuDriver::alloc_bo_vram` 返回值是否包含 gpu_va
- [ ] A.3.2 修改 `MemoryManager::allocate` 签名，增加 `host_ptr` 参数（默认 `nullptr`）
- [ ] A.3.3 修改 `CudaScheduler::submit_mem_alloc`：`device_ptr = gpu_va`，host_ptr 存入 `DeviceMemory`
- [ ] A.3.4 确认 `MemoryManager::find(gpu_va)` 可用
- [ ] A.3.5 确认 `MemoryManager::free()` 对 DEVICE_LOCAL + 外部 host_ptr 不调 `std::free`
- [ ] A.3.6 确认 `bo_handles_[device_ptr]` 的 key 一致（现在是 gpu_va）

### A.4 验证

- [ ] A.4.1 UsrLinuxEmu: `cmake --build build --target gpu_driver_plugin` 0 errors
- [ ] A.4.2 TaskRunner: `cmake -B build_umd && cmake --build build_umd -j4` 0 errors
- [ ] A.4.3 UsrLinuxEmu test: ALLOC_BO→MAP_BO→memcpy(gpu_va, "hello", 5)→memcmp pass
- [ ] A.4.4 TaskRunner test: `submit_mem_alloc` → `device_ptr` 在 HAL_HEAP_BASE 范围

---

## Phase B: Puller FSM MEMCPY 真实化（HAL 路径）（2.5h）

### B.1 分析

- [ ] B.1.1 阅读 `plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp:171-182`（DISPATCH）
- [ ] B.1.2 阅读 `hal_user.cpp:37-55`（`user_mem_read/write`）— 确认 `dev_addr` 必须在 HAL_HEAP_BASE 范围
- [ ] B.1.3 确认 `struct gpu_hal_ops` 暴露 `mem_read`/`mem_write` 函数指针

### B.2 实现

- [ ] B.2.1 DISPATCH 状态中增加 MEMCPY 分支：
  - `src >= HAL_HEAP_BASE && src < HAL_HEAP_BASE + HAL_HEAP_SIZE` → D2H（`hal_mem_read`）
  - 否则 H2D（`hal_mem_write`）
  - HAL 返回非 0 时记录错误（不静默 transition to COMPLETE）
- [ ] B.2.2 同样处理 `GPU_OP_MEMSET`（on-demand）

### B.3 验证

- [ ] B.3.1 `cmake --build build --target gpu_driver_plugin` 0 errors
- [ ] B.3.2 test: H2D→D2H roundtrip
- [ ] B.3.3 test: 越界 MEMCPY → HAL bounds 返回 -EINVAL → Puller 不 crash
- [ ] B.3.4 ctest 104/104 PASS

---

## Phase C: Fence 异步语义修复（3h）

> 🔴 **v1.2 关键**：wait_fence 必须检查 `status_out`，超时 `status=0` 不能返回成功
>
> 🔴 **Oracle 审查 (2026-07-19)**: C.2.4 存在数据一致性风险，**必须推迟**——需先验证 HAL heap 与 GEM BO mmap 是否共享 backing store。若 Puller 写 HAL heap 但 TaskRunner 读 map_bo 指针（不同内存），D2H 会读到脏数据。
>
> ADR-040 已落地：Puller `handleComplete` → `sim_fence_id_signal(pending_fence_id_)` → `handleWaitFence`。链路通畅。

### C.0 前置：backing store 验证（阻塞 C.2.4）

- [ ] C.0.1 验证 `hal_user.cpp` 的 HAL heap（`hc->heap = std::malloc(HAL_HEAP_SIZE)`）与 GEM BO mmap（`GpgpuDevice::mmap` 返回的 `host_ptr`）是否是同一块物理内存
  - 若是同一 backing → C.2.4 可安全推进
  - 若不是 → 需架构调整（让 Puller 通过 VA→BO 反向映射直接访问 map_bo 指针）
- [ ] C.0.2 若 backing 共享，确认 `hal_mem_write` 写入 `hc->heap + offset` 后，`map_bo` 返回的 `host_ptr` 能读回一致数据

### C.1 分析

- [ ] C.1.1 阅读 `gpgpu_device.cpp:438-479`（`handleWaitFence`）— 超时 `args.status=0`
- [ ] C.1.2 阅读 `gpu_driver_client.h:369-384`（`GpuDriverClient::wait_fence`）— 超时被 swallow
- [ ] C.1.3 阅读 `cuda_scheduler.cpp:229-265`（`submit_launch`）— 立即 signal_fence + task.state 提前设 COMPLETED

### C.2 实现

- [ ] C.2.1 修正 `GpuDriverClient::wait_fence`：`return (args.status == 1) ? 0 : -ETIMEDOUT`
- [ ] C.2.2 修正 `submit_launch`：`wait_fence(driver_fence, 5000, &status)` + 检查 `status==1` 才 signal
- [ ] C.2.3 同样修正 `submit_memcpy_h2d` 和 `submit_memcpy_d2h`
- [ ] C.2.4 **推迟**：移除 `memory_mgr_.memcpy_h2d/d2h` 调用（等待 C.0 backing store 验证通过）
- [ ] C.2.5 `submit_mem_alloc` 保留立即 signal
- [ ] C.2.6 检查 `cuStreamSynchronize` 中 `wait_fence(fence_id, 0, &status)` — timeout=0 无限等待
- [ ] C.2.7 **新增**：修正 `submit_launch` 的 `task.state` 时机——从 ioctl 返回后立即设 `COMPLETED` 改为 `wait_fence` 成功后才设

### C.3 验证

- [ ] C.3.1 TaskRunner: `cmake -B build_umd && cmake --build build_umd -j4` 0 errors
- [ ] C.3.2 TaskRunner: `ctest --test-dir build_umd` 13/13 PASS
- [ ] C.3.3 test: `wait_fence(fence, 5000, &status)` status=0 → -ETIMEDOUT（不挂死）
- [ ] C.3.4 test: `wait_fence(fence, 5000, &status)` status=1 → 0（正常完成）
- [ ] C.3.5 test: Puller MEMCPY HAL 失败时 fence 不 signal（验证 Phase B 错误修复）

---

## Phase D: Kernel Launch No-op（1.5h）

> ⚠️ **v1.2**：需确认 translator 被显式调用

### D.1 分析

- [ ] D.1.1 阅读 `gpfifo_translator.cpp`（translator::translate）
- [ ] D.1.2 阅读 `hardware_puller_emu.cpp:177-183`（DISPATCH）— 当前 `scheduler_->enqueue()` 只入队不调 translator
- [ ] D.1.3 确认 `plugin.cpp:58-66` callback 绑定

### D.2 实现

- [ ] D.2.1 在 DISPATCH 中 LAUNCH_KERNEL 分支显式调用 `translator_->translate(current_entry_)`
- [ ] D.2.2 验证 callback 输出 `[GpuPlugin] LaunchCallback: kernel=dummy grid=(1,1,1) block=(1,1,1)`

### D.3 验证

- [ ] D.3.1 `cmake --build build --target gpu_driver_plugin` 0 errors
- [ ] D.3.2 `./build/bin/test_gpu_plugin` — 验证日志输出

---

## Phase E: E2E 集成测试（3h）

> 🔴 **v1.2**：E2E 测试使用 CUDA Driver API（cuModuleLoad + cuModuleGetFunction + cuLaunchKernel(CUfunction)）

### E.1 Happy Path 测试

- [ ] E.1.1 创建 `external/TaskRunner/tests/umd/test_cuda_e2e_real.cpp`
  - 使用 CUDA Driver API（`#include <cuda.h>`）
  - `cuModuleLoad("dummy.cubin")` + `cuModuleGetFunction(&kernel, mod, "dummy")`
  - `cuLaunchKernel(kernel, 1,1,1, 1,1,1, 0, 0, kernelParams, nullptr)`
  - 若 Driver API shim 不支持 module load → 改用 `CudaRuntimeApi::launch_kernel("dummy", ...)`
- [ ] E.1.2 注册到 CMake

### E.2 失败路径测试

- [ ] E.2.1 `TEST_CASE("MEMCPY oversize returns error")`
- [ ] E.2.2 `TEST_CASE("free then map should fail")`
- [ ] E.2.3 `TEST_CASE("fence wait timeout")`
- [ ] E.2.4 `TEST_CASE("memcpy to unallocated GPU VA")`

### E.3 构建 + 运行

- [ ] E.3.1 `cmake -B build_umd && cmake --build build_umd -j4 --target test_cuda_e2e_real`
- [ ] E.3.2 `./test_cuda_e2e_real` — 所有 TEST_CASE PASS

---

## Phase F: 验收（0.5h）

- [ ] F.1 UsrLinuxEmu 104/104 ctest PASS（无回归）
- [ ] F.2 TaskRunner 13/13 → 14/14 ctest PASS
- [ ] F.3 docs-audit 43/43 PASS
- [ ] F.4 `test_cuda_e2e_real` 全 PASS（4 happy + 3 fail）
- [ ] F.5 跨仓同步 ADR-035 §Rule 5.1 验证
- [ ] F.6 `MAP_BO` 返回 gpu_va 可读写验证
- [ ] F.7 `GPU_OP_MEMCPY` HAL 路径验证（H2D 写→D2H 读→数据一致）
- [ ] F.8 `GpuDriverClient::wait_fence` 超时不返回成功（status=0→-ETIMEDOUT）
- [ ] F.9 Puller 收到 device_ptr 在 HAL_HEAP_BASE 范围，bounds 校验通过

---

## 任务统计

| Phase | 任务数 | 预估 |
|-------|--------|------|
| 0 (交叉验证) | 5 ✅ | 已完成 |
| A (内存真实化 + TaskRunner map) | 14 ✅ | 已完成 |
| B (MEMCPY HAL 路径) | 9 ✅ | 已完成 |
| C (Fence 异步 + 超时) | 12 | 3h |
| D (Kernel No-op) | 6 | 1.5h |
| E (E2E 测试 + 失败路径) | 9 | 3h |
| F (验收) | 9 | 0.5h |
| **总计** | **64** | **~15h (2.5d)** |
| B (MEMCPY HAL 路径) | 7 | 2.5h |
| C (Fence 异步 + 超时) | 9 | 3h |
| D (Kernel No-op) | 6 | 1.5h |
| E (E2E 测试 + 失败路径) | 9 | 3h |
| F (验收) | 9 | 0.5h |
| **总计** | **59** | **~15h (2.5d)** |
