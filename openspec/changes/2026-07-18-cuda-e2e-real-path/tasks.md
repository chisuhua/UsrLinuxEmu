# Tasks: cuda-e2e-real-path

> **状态**: 📋 PROPOSED
> **目标**: CUDA 程序 E2E 运行（cuMemAlloc → memcpy → launch kernel → sync）全链路打通
> **工期**: 1.5 天（~9h）
> **依赖**: C-12 KFD Multi-File Integration ✅
> **涉及仓**: UsrLinuxEmu + TaskRunner（跨仓，但本次可单仓实施 UsrLinuxEmu 核心改动，TaskRunner 后补）

---

## 架构背景

当前 E2E 链路结构完整但数据不流动。三个致命缺口：

```
cuMemAlloc → driver_->alloc_bo_vram()
    → ioctl(ALLOC_BO) → hal_mem_alloc → gpu_buddy_alloc (heap offset, NOT mmap)
    → MAP_BO → gpu_va = offset (CANNOT dereference)

cuMemcpyHtoD → driver_->submit_memcpy()
    → ioctl(PUSHBUFFER_SUBMIT_BATCH, MEMCPY entry)
    → Puller FSM → DISPATCH → enqueue() → COMPLETE (NO memcpy!)

cuLaunchKernel → driver_->submit_launch()
    → ioctl(PUSHBUFFER_SUBMIT_BATCH, LAUNCH_KERNEL entry)
    → Immediate sync_mgr_.signal_fence() (NOT waiting for sim!)
```

---

## Phase A: UsrLinuxEmu 端内存真实化（2h）

### A.1 分析 hal_user 内存模型

- [ ] A.1.1 阅读 `plugins/gpu_driver/hal/hal_user.cpp:57-71`（`user_mem_alloc`）
  - 当前使用 `gpu_buddy_alloc` 分配 heap 内偏移量
  - `HAL_HEAP_BASE` 和 `HAL_HEAP_SIZE` 定义在 `hal_user.h`
  - heap buffer（`hc->heap`）在 `hal_user_init` 中初始化
- [ ] A.1.2 阅读 `plugins/gpu_driver/drv/gpgpu_device.cpp:689-725`（`GpgpuDevice::mmap`）
  - 当前对非 DOORBELL/非 QUEUE_RING 的 offset 返回 `MAP_FAILED`
- [ ] A.1.3 阅读 `plugins/gpu_driver/drv/gpgpu_device.cpp:261-283`（`handleMapBo`）
  - 当前 `args->gpu_va = it->second.gpu_va` — 返回 buddy offset，非真指针

### A.2 实现：GpgpuDevice::mmap 支持 BO 映射

- [ ] A.2.1 在 `GpgpuDevice::mmap` 中新增 BO 映射分支：
  ```cpp
  // 在现有 switch-case 之前，先检查是否有 BO 匹配此 offset
  auto bo_it = bo_map_.find(static_cast<u32>(offset)); // offset 即 handle
  if (bo_it != bo_map_.end()) {
    // 通过 hal_user 的真实 heap 基址返回可读写指针
    void* base = hal_get_heap_base(hal_);  // 新增 HAL API
    return static_cast<char*>(base) + bo_it->second.gpu_va - HAL_HEAP_BASE;
  }
  ```
  **OR 更简方案**：直接 `mmap(MAP_ANONYMOUS)` 新页 + 返回指针 + 在 `bo_map_` 中存储 `void*`

- [ ] A.2.2 在 `handleAllocBo` 中存储 `hal_heap_ptr` 到 `bo_map_`：
  - 扩展 `BoInfo { u64 gpu_va; u64 size; u32 domain; u32 flags; void* host_ptr; }`
  - `host_ptr = hal_get_heap_base(hal_) + (gpu_va - HAL_HEAP_BASE)`

- [ ] A.2.3 在 `handleMapBo` 中返回 host_ptr（而非 buddy offset）：
  ```cpp
  args->gpu_va = reinterpret_cast<u64>(it->second.host_ptr);
  ```

### A.3 验证

- [ ] A.3.1 写 test：`GPU_IOCTL_ALLOC_BO(4096)` → `GPU_IOCTL_MAP_BO` → `memcpy(gpu_va, "hello", 5)` → `memcmp(gpu_va, "hello", 5)` == 0
- [ ] A.3.2 `cmake --build build --target test_kfd_l1_l2_bridge_standalone`
- [ ] A.3.3 `./build/bin/test_kfd_l1_l2_bridge_standalone` — 新增 TEST_CASE PASS

---

## Phase B: Puller FSM MEMCPY 真实化（2h）

### B.1 分析当前 MEMCPY 路径

- [ ] B.1.1 阅读 `plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp:171-182`（DISPATCH 状态）
  - 当前对**所有 entry 类型**走统一 `scheduler_->enqueue()` 路径
- [ ] B.1.2 阅读 `plugins/gpu_driver/sim/scheduler/global_scheduler.cpp:43-54`（`selectEngine`）
  - 已正确分类 MEMCPY→COPY、LAUNCH_KERNEL→COMPUTE
  - 但 `enqueue()` 只是入队，不做实际工作
- [ ] B.1.3 确认 GPFIFO entry 的 payload 格式：
  ```c
  struct gpu_gpfifo_entry {
    u32 valid, release, method, pad;
    u64 payload[4];
  };
  // GPU_OP_MEMCPY: payload[0]=dst, payload[1]=src, payload[2]=size
  ```

### B.2 实现：DISPATCH 状态中增加 MEMCPY 分支

- [ ] B.2.1 在 `hardware_puller_emu.cpp` 的 `case State::DISPATCH` 中，先检查 entry 类型：
  ```cpp
  case State::DISPATCH: {
    if (current_entry_.method == GPU_OP_MEMCPY) {
      void* dst = reinterpret_cast<void*>(current_entry_.payload[0]);
      const void* src = reinterpret_cast<void*>(current_entry_.payload[1]);
      size_t size = static_cast<size_t>(current_entry_.payload[2]);
      if (dst && src && size > 0) {
        std::memcpy(dst, src, size);
      }
      transitionTo(State::COMPLETE);
      break;
    }
    // 原来逻辑：enqueue to scheduler
    if (scheduler_) {
      EngineType engine = scheduler_->selectEngine(current_entry_);
      scheduler_->enqueue(current_entry_, engine);
    }
    transitionTo(State::COMPLETE);
    break;
  }
  ```

- [ ] B.2.2 同样处理 `GPU_OP_MEMSET` 和 `GPU_OP_FENCE`（on-demand）

### B.3 验证

- [ ] B.3.1 `cmake --build build --target gpu_driver_plugin` 0 errors
- [ ] B.3.2 写 test：ALLOC_BO → MAP_BO → memcpy to gpu_va → PUSHBUFFER_SUBMIT_BATCH(MEMCPY) → verify data at dst
- [ ] B.3.3 ctest 104/104 PASS（无回归）

---

## Phase C: Fence 异步语义修复（2h）

### C.1 分析当前 fence 路径

- [ ] C.1.1 阅读 `external/TaskRunner/src/test_fixture/cuda_scheduler.cpp:116,153,189,222,265`
  - `sync_mgr_.signal_fence(fence)` 在 ioctl 返回后立即调用 —— **这是 bug**
- [ ] C.1.2 阅读 `external/TaskRunner/src/test_fixture/cuda_scheduler.cpp:229-265`（`submit_launch`）
  - `driver_->submit_launch()` 调用 `driver_->submit_pushbuffer()` → ioctl `PUSHBUFFER_SUBMIT_BATCH`
  - `driver_->submit_pushbuffer` 返回 `fence_id`（来自 sim 层）
  - `CudaScheduler` 应该用这个 `driver_fence` 等待，而非立即 signal

### C.2 实现：删除立即 signal，用 wait 替代

- [ ] C.2.1 在 `submit_launch` 中删除 `sync_mgr_.signal_fence(fence)`：
  ```cpp
  // OLD (line 263-265):
  // 创建 fence
  auto fence = sync_mgr_.create_fence();
  sync_mgr_.signal_fence(fence);  // ← 删除这行

  // NEW:
  auto fence = sync_mgr_.create_fence();
  // fence 由 driver_->wait_fence(driver_fence) 或 sim 层回调 signal
  // 当前阶段：driver_fence 不为 0 时，等待 sim 层完成
  if (driver_fence > 0) {
    driver_->wait_fence(driver_fence, 0);  // 同步等待
    sync_mgr_.signal_fence(fence);
  } else {
    sync_mgr_.signal_fence(fence);  // fallback: 无 fence 时保持旧行为
  }
  ```

- [ ] C.2.2 同样修复 `submit_memcpy_h2d` / `submit_memcpy_d2h`（line 189, 222）

- [ ] C.2.3 `submit_mem_alloc` 保留立即 signal（alloc 确实是同步操作，不需等待）

### C.3 验证

- [ ] C.3.1 TaskRunner: `cmake -B build -DTASKRUNNER_BUILD_MODE=test-fixture && cmake --build build -j4` 0 errors
- [ ] C.3.2 TaskRunner: `ctest --test-dir build` 4/4 PASS（test-fixture mode）
- [ ] C.3.3 `driver_->wait_fence` 验证：确认 UsrLinuxEmu sim 层的 `handleComplete` → `sim_fence_id_signal` 是正确的 fence signal 源

---

## Phase D: Kernel Launch No-op（1h）

### D.1 分析当前 LAUNCH_KERNEL 路径

- [ ] D.1.1 阅读 `plugins/gpu_driver/sim/scheduler/translator/gpfifo_translator.cpp`
  - `GpfifoToLaunchParamsTranslator::translate()` 已解析 kernel name + grid/block dims
  - 通过 `launch_cb_` 回调通知

### D.2 实现：确保 launch callback 被正确触发

- [ ] D.2.1 验证 `GlobalScheduler::enqueue()` 对 LAUNCH_KERNEL entry 调用了 translator：
  - 当前 `enqueue()` 至少会将 entry 入队。如果 translator 已绑定，则回调会被触发。
  - 确认回调链：`plugin.cpp:58-66` 中 `scheduler.setLaunchCallback(...)` 已设置
- [ ] D.2.2 若 translator 未被调用，在 DISPATCH 状态的特殊处理中增加：
  ```cpp
  case GPU_OP_LAUNCH_KERNEL:
    // Call translator to extract params + trigger callback
    // GpfifoToLaunchParamsTranslator::translate(entry);
    break;
  ```

### D.3 验证

- [ ] D.3.1 `cmake --build build --target gpu_driver_plugin` 0 errors
- [ ] D.3.2 运行 `./build/bin/test_gpu_plugin` — 验证 kernel launch 日志输出：
  ```text
  [GpuPlugin] LaunchCallback: kernel=dummy grid=(1,1,1) block=(1,1,1)
  ```

---

## Phase E: E2E 集成测试（2h）

### E.1 TaskRunner 端：创建 test_cuda_e2e_real.cpp

- [ ] E.1.1 创建 `external/TaskRunner/tests/umd/test_cuda_e2e_real.cpp`:
  ```cpp
  // SCOPE: UMD-EVOLUTION
  #define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
  #include "doctest.h"
  #include "cuda_runtime_api.hpp"
  #include <cstring>

  TEST_CASE("CUDA E2E: alloc → memcpy H2D → launch kernel → sync → memcpy D2H") {
    // 1. Alloc 4KB
    CUdeviceptr dptr = 0;
    REQUIRE(cuMemAlloc(&dptr, 4096) == CUDA_SUCCESS);

    // 2. H2D memcpy
    const char* input = "hello";
    REQUIRE(cuMemcpyHtoD(dptr, input, 5) == CUDA_SUCCESS);

    // 3. Launch no-op kernel
    CUstream stream = 0;
    REQUIRE(cuLaunchKernel(dptr, 0, 1, 1, 1, 1, 1, 1, 0, stream, nullptr, nullptr) == CUDA_SUCCESS);

    // 4. Sync
    REQUIRE(cuStreamSynchronize(stream) == CUDA_SUCCESS);

    // 5. D2H memcpy + verify
    char output[5] = {};
    REQUIRE(cuMemcpyDtoH(output, dptr, 5) == CUDA_SUCCESS);
    REQUIRE(std::memcmp(output, input, 5) == 0);

    cuMemFree(dptr);
  }
  ```

- [ ] E.1.2 注册到 `cmake/UMDEvolution.cmake`:
  ```cmake
  add_executable(test_cuda_e2e_real tests/umd/test_cuda_e2e_real.cpp)
  target_link_libraries(test_cuda_e2e_real PRIVATE cuda_taskrunner)
  add_test(NAME test_cuda_e2e_real COMMAND test_cuda_e2e_real)
  ```

- [ ] E.1.3 构建 + 运行：
  ```bash
  cmake -B build_umd -DTASKRUNNER_BUILD_MODE=umd-evolution
  cmake --build build_umd -j4 --target test_cuda_e2e_real
  ./test_cuda_e2e_real
  ```

---

## Phase F: 验收（0.5h）

- [ ] F.1 UsrLinuxEmu 104/104 ctest PASS（无回归）
- [ ] F.2 TaskRunner 13/13 → 14/14 ctest PASS（新增 test_cuda_e2e_real）
- [ ] F.3 docs-audit 43/43 PASS
- [ ] F.4 `test_cuda_e2e_real` 全 PASS（不 SKIP）

---

## 任务统计

| Phase | 任务数 | 预估 |
|-------|-------:|------|
| A (内存真实化) | 9 | 2h |
| B (MEMCPY) | 7 | 2h |
| C (Fence) | 8 | 2h |
| D (Kernel No-op) | 5 | 1h |
| E (E2E 测试) | 3 | 2h |
| F (验收) | 4 | 0.5h |
| **总计** | **36** | **~9.5h (1.5d)** |
