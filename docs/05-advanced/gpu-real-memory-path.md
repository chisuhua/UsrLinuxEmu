# GPU 真实内存路径设计

> **状态**: ✅ IMPLEMENTED (2026-07-20, change `cuda-e2e-real-path`)
> **对应 ADR**: [ADR-036](../00_adr/adr-036-three-way-separation.md) (3 区分架构)
> **关联 change**: [openspec/changes/2026-07-18-cuda-e2e-real-path](../../openspec/changes/2026-07-18-cuda-e2e-real-path/proposal.md)
> **最后更新**: 2026-07-20

---

## 1. 概述

本文档描述 UsrLinuxEmu 中 **GPU 设备内存的真实化实现**，即从 `cuMemAlloc` → `cuMemcpyHtoD` → `cuMemcpyDtoH` 全链路中，数据如何在 TaskRunner（用户态驱动）和 UsrLinuxEmu（硬件模拟）之间**实际流动**。

### 1.1 背景（修复前）

经过 Oracle 分析，修复前的结构链路虽完整但**数据不实际流动**，存在三个致命缺口：

1. **BO 内存不可读写**：`MAP_BO` 返回的 `gpu_va` 是 buddy 偏移量（`HAL_HEAP_BASE` 范围内），不可解引用
2. **MEMCPY 不拷贝数据**：Puller FSM 对所有 entry 走统一 `scheduler_->enqueue()` 路径，`GPU_OP_MEMCPY` 没有真实拷贝
3. **Fence 立即 signal**：`CudaScheduler` 在 ioctl 返回后立即 signal fence，不等待 sim 层完成

### 1.2 核心设计原则

- **一块内存，两条路径**：整个 GPU 设备内存基于 HAL 的单一 `std::malloc(256MB)` heap。TaskRunner 通过 `mmap`/`map_bo` 直接访问，Puller 通过 `hal_mem_read`/`hal_mem_write` 间接访问，两者操作同一块物理内存
- **双地址语义**：`gpu_va`（`HAL_HEAP_BASE` 范围内 dev_addr）供 Puller HAL 寻址；`host_ptr`（真实堆指针）供 TaskRunner 直接读写
- **架构妥协**：`gpgpu_device.cpp` 直接读取 `hal_user_context::heap` 计算 `host_ptr`，绕过了 HAL 封装层（已记录为架构债务）

---

## 2. 内存架构

### 2.1 物理布局

```
hc->heap = std::malloc(HAL_HEAP_SIZE = 256 MB)
     │
     │  HAL_HEAP_BASE = 0x100000000 (4 GiB)
     │
     ├── offset 0x00000000  ← 对应 gpu_va = 0x100000000
     ├── offset 0x00001000  ← 对应 gpu_va = 0x100001000
     ├── ...
     └── offset 0x0FFFFFFF  ← 对应 gpu_va = 0x1FFFFFFFF
     
     buddy allocator 在 [HAL_HEAP_BASE, HAL_HEAP_BASE + HAL_HEAP_SIZE) 范围内
     管理所有 sub-allocation
```

### 2.2 两条访问路径

```
                    ┌──────────────────────┐
                    │    hc->heap (256MB)   │
                    │  唯一的物理 backing   │
                    └──────────┬───────────┘
                 ┌─────────────┼──────────────┐
                 │                             │
          路径 1: 直接指针                 路径 2: HAL 函数
          ──────────────                  ────────────
     TaskRunner 侧                     Puller FSM 侧
          │                               │
     mmap / map_bo                   hal_mem_read
     返回 host_ptr                   hal_mem_write
     (= hc->heap + offset)          (dev_addr → heap_off
          │                          → hc->heap + heap_off)
          │                               │
     TaskRunner 直接                Puller 通过 HAL
     memcpy 读写                     memcpy 读写
          │                               │
          └──────────┬────────────────────┘
                     │
              同一块内存，双向立即可见
```

### 2.3 关键数据结构

**hal_user_context**（`plugins/gpu_driver/hal/hal_user.h`）

```cpp
struct hal_user_context {
    uint8_t *heap;             // 设备内存堆 (std::malloc 分配)
    struct gpu_buddy buddy;    // buddy allocator 管理 sub-allocation
    std::mutex heap_lock;      // 堆访问互斥锁
    bool buddy_initialized;    // 懒初始化标志
    // ...
};
```

**BoInfo**（`plugins/gpu_driver/drv/gpgpu_device.h`）

```cpp
struct BoInfo {
    u64 gpu_va;        // 设备虚拟地址 (HAL_HEAP_BASE 范围)
    u64 size;
    u32 domain;
    u32 flags;
    void* host_ptr;    // 用户态仿真专用: hc->heap + (gpu_va - HAL_HEAP_BASE)
};
```

**DeviceMemory**（`external/TaskRunner/include/shared/memory_manager.hpp`）

```cpp
struct DeviceMemory {
    uint64_t device_ptr;   // gpu_va (HAL_HEAP_BASE 范围)
    uint64_t size;
    void* host_ptr;        // 真实堆指针 (TaskRunner 直接读写的地址)
    bool externally_managed;  // true = host_ptr 归 HAL heap 管，不 std::free
};
```

---

## 3. 完整调用过程

### 3.1 分配：cuMemAlloc

```
cuMemAlloc(&dptr, 4096)
    │
    └── CudaRuntimeApi::malloc(&dptr, 4096)             [include/umd/cuda_runtime_api.hpp]
         │
         └── CudaScheduler::submit_mem_alloc(4096)      [src/test_fixture/cuda_scheduler.cpp]
              │
              ├── Step 1: driver_->alloc_bo_vram(4096)
              │       │                                  ────── ioctl(GPU_IOCTL_ALLOC_BO) ───→
              │       │
              │       │   UsrLinuxEmu 侧:
              │       │   ┌─────────────────────────────────────────────────────┐
              │       │   │ GpgpuDevice::handleAllocBo                          │
              │       │   │   ├─ hal_mem_alloc(4096, &gpu_va)                  │
              │       │   │   │     └─ gpu_buddy_alloc → gpu_va = 0x100000000 │
              │       │   │   │                                                │
              │       │   │   ├─ host_ptr = hc->heap + (gpu_va - HAL_HEAP_BASE)│
              │       │   │   │     = hc->heap + 0                             │
              │       │   │   │     ⚠️ 架构妥协: 直接读 hal_user_context::heap │
              │       │   │   │                                                │
              │       │   │   ├─ BoInfo{gpu_va, size, ..., host_ptr}           │
              │       │   │   │     → bo_map_[handle]                          │
              │       │   │   │                                                │
              │       │   │   └─ args->gpu_va = gpu_va                        │
              │       │   └─────────────────────────────────────────────────────┘
              │       │
              │       │   ← return bo_handle
              │       │   GpuDriverClient::alloc_bo:
              │       │     bo_gpu_va_cache_[handle] = args.gpu_va  ← 缓存 gpu_va
              │       │
              │  bo_handle = handle (e.g. 1)
              │
              ├── Step 2: driver_->get_bo_gpu_va(bo_handle)
              │       │   → return bo_gpu_va_cache_[1] = 0x100000000
              │  gpu_va = 0x100000000
              │
              ├── Step 3: driver_->map_bo(bo_handle, 4096)
              │       │                                  ────── ioctl(GPU_IOCTL_MAP_BO) ───→
              │       │
              │       │   UsrLinuxEmu 侧:
              │       │   ┌─────────────────────────────────────────────────────┐
              │       │   │ GpgpuDevice::handleMapBo                            │
              │       │   │   args->gpu_va = bo_map_[handle].host_ptr          │
              │       │   │     = hc->heap + 0                                 │
              │       │   │     ⚠️ 字段名 gpu_va 不再准确，实际存 host_ptr     │
              │       │   └─────────────────────────────────────────────────────┘
              │       │
              │       │   ← return reinterpret_cast<void*>(args.gpu_va)
              │  host_ptr = hc->heap + 0 (真实可解引用指针)
              │
              ├── Step 4: memory_mgr_.allocate(4096, DEVICE_LOCAL, host_ptr)
              │       │   → DeviceMemory{
              │       │       device_ptr = token (MemoryManager 内部)
              │       │       host_ptr   = external (不归 MemoryManager 管)
              │       │       externally_managed = true
              │       │     }
              │       │
              │       │   bo_handles_[gpu_va] = bo_handle
              │       │   gpu_va_to_token_[gpu_va] = mem.device_ptr
              │
              └── Step 5: signal fence (alloc 同步操作，立即完成)
                    result.device_ptr = gpu_va (= 0x100000000)
                    result.fence_id   = fence->id
                    *dptr = gpu_va
```

**结果**：
- `*dptr` = `0x100000000`（gpu_va，Puller 可用）
- `host_ptr` = `hc->heap + 0`（TaskRunner 可直接 memcpy 读写）
- `bo_gpu_va_cache_[1]` = `0x100000000`
- `gpu_va_to_token_[0x100000000]` = token

---

### 3.2 H2D 拷贝：cuMemcpyHtoD

```
cuMemcpyHtoD(dptr=0x100000000, src="hello", 5)
    │
    └── CudaRuntimeApi::memcpy(dptr, "hello", 5, HostToDevice)
         │
         └── CudaScheduler::submit_memcpy_h2d(device_ptr=0x100000000, 
                                              offset=0, "hello", 5)
              │
              ├── Step 1: 编码 GPFIFO entry
              │       driver_->submit_memcpy(stream=0,
              │           src = reinterpret_cast<uint64_t>("hello"),  ──→ payload[0]
              │           dst = device_ptr,                           ──→ payload[1]
              │           size = 5,                                   ──→ payload[2]
              │           is_h2d = true)
              │       │
              │       │           ────── ioctl(GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH) ──→
              │       │
              │       │   UsrLinuxEmu 侧: HardwarePullerEmu FSM
              │       │   ┌──────────────────────────────────────────────────────┐
              │       │   │ State::DISPATCH                                      │
              │       │   │                                                       │
              │       │   │   method == GPU_OP_MEMCPY                            │
              │       │   │   src  = payload[0] = host_ptr ("hello" 的地址)      │
              │       │   │   dst  = payload[1] = gpu_va (0x100000000)           │
              │       │   │   size = payload[2] = 5                              │
              │       │   │                                                       │
              │       │   │   size > 0 && size <= HAL_HEAP_SIZE → ✓             │
              │       │   │                                                       │
              │       │   │   src_is_device =                                    │
              │       │   │     src >= HAL_HEAP_BASE                             │
              │       │   │     && src < HAL_HEAP_BASE + HAL_HEAP_SIZE           │
              │       │   │     = host_ptr >= 0x100000000 → ✗ (host_ptr 很小)   │
              │       │   │                                                       │
              │       │   │   → hal_mem_write(hal_, dst, src, 5)                 │
              │       │   │       heap_off = dst - HAL_HEAP_BASE                 │
              │       │   │                = 0x100000000 - 0x100000000 = 0       │
              │       │   │       heap_off + size = 5 ≤ HAL_HEAP_SIZE → ✓        │
              │       │   │       memcpy(hc->heap + 0, "hello", 5)              │
              │       │   │                                                       │
              │       │   │   ret == 0 → 成功                                    │
              │       │   │   transitionTo(State::COMPLETE)                      │
              │       │   └──────────────────────────────────────────────────────┘
              │       │
              │       │   State::COMPLETE → handleComplete()
              │       │       sim_fence_id_signal(pending_fence_id_)
              │       │
              │  ← driver_fence = fence_id (正数)
              │
              ├── Step 2: 等待 Puller 完成
              │       driver_->wait_fence(driver_fence, 5000, &status)
              │       status == 1 → Puller 已 signal
              │
              └── Step 3: sync_mgr_.signal_fence(fence)  ← 通知 TaskRunner 调用者
```

**结果**：`hc->heap[0..4] = "hello"`，Puller 通过 HAL 写入

---

### 3.3 D2H 拷贝：cuMemcpyDtoH

```
cuMemcpyDtoH(host_buf, dptr=0x100000000, 5)
    │
    └── CudaRuntimeApi::memcpy(host_buf, dptr, 5, DeviceToHost)
         │
         └── CudaScheduler::submit_memcpy_d2h(host_buf, 0x100000000, 0, 5)
              │
              ├── Step 1: 编码 GPFIFO entry
              │       driver_->submit_memcpy(
              │           src = device_ptr (0x100000000),  ──→ payload[0]
              │           dst = host_buf,                   ──→ payload[1]
              │           size = 5,                         ──→ payload[2]
              │           is_h2d = false)
              │       │
              │       │   UsrLinuxEmu 侧: Puller DISPATCH
              │       │   ┌──────────────────────────────────────────────────────┐
              │       │   │   src  = payload[0] = 0x100000000 (gpu_va)           │
              │       │   │   dst  = payload[1] = host_buf (用户态地址)          │
              │       │   │   size = payload[2] = 5                              │
              │       │   │                                                       │
              │       │   │   src_is_device =                                    │
              │       │   │     0x100000000 >= HAL_HEAP_BASE                     │
              │       │   │     && 0x100000000 < BASE + SIZE → ✓                 │
              │       │   │                                                       │
              │       │   │   → hal_mem_read(hal_, src, dst, 5)                  │
              │       │   │       heap_off = 0x100000000 - HAL_HEAP_BASE = 0     │
              │       │   │       heap_off + 5 ≤ HAL_HEAP_SIZE → ✓              │
              │       │   │       memcpy(host_buf, hc->heap + 0, 5)             │
              │       │   │                                                       │
              │       │   │   ret == 0 → 成功                                    │
              │       │   │   transitionTo(State::COMPLETE)                      │
              │       │   └──────────────────────────────────────────────────────┘
              │       │
              │  ← driver_fence
              │
              ├── Step 2: wait_fence(driver_fence, 5000, &status)
              │
              └── host_buf 已有 "hello"
```

**结果**：`host_buf = "hello"`，Puller 从 `hc->heap[0]` 读出

---

### 3.4 释放：cuMemFree

```
cuMemFree(dptr=0x100000000)
    │
    └── CudaScheduler::submit_mem_free(0x100000000)
         │
         ├── gpu_va_to_token_[0x100000000] → token
         │       → memory_mgr_.find(token) → DeviceMemory
         │
         ├── driver_->free_bo(bo_handle)       ──→ ioctl(GPU_IOCTL_FREE_BO)
         │       │                               → GpgpuDevice::handleFreeBo
         │       │                               → gpu_buddy_free(0x100000000)
         │       │                               → bo_map_.erase(handle)
         │
         ├── memory_mgr_.free(mem)
         │       externally_managed = true → 不调 std::free
         │
         └── gpu_va_to_token_.erase(0x100000000)
```

---

## 4. Fence 异步语义

### 4.1 修复前 vs 修复后

| 阶段 | 修复前 | 修复后 |
|------|--------|--------|
| `submit_launch` ioctl 返回后 | 立即 `signal_fence` | `wait_fence(driver_fence, 5000, &status)` 等待 Puller 完成 |
| `task.state` | 立即设 `COMPLETED` | `wait_fence` 成功后设 `COMPLETED`，失败设 `FAILED` |
| `wait_fence` 超时 | `args.status=0` 被 swallow 为成功 | `return (status==1) ? 0 : -ETIMEDOUT` |

### 4.2 Fence 生命周期

```
TaskRunner                        UsrLinuxEmu
─────────                         ────────────
submit_launch()
  │
  ├─ ioctl(SUBMIT_BATCH)
  │       │
  │       │     handlePushbufferSubmitBatch()
  │       │       sim_fence_id_alloc() → fence_id
  │       │       submitBatch(gpfifo, count, fence_id)
  │       │       Puller FSM 开始处理...
  │       │
  │  ← driver_fence
  │
  ├─ wait_fence(driver_fence, 5000, &status)
  │       │                          Puller: COMPLETE
  │       │                            → handleComplete()
  │       │                            → sim_fence_id_signal(fence_id)
  │       │
  │       │     handleWaitFence(fence_id)
  │       │       → args.status = 1
  │       │       → wake up
  │  ← status = 1, ret = 0
  │
  └─ task.state = COMPLETED
     sync_mgr_.signal_fence()
```

---

## 5. 架构决策记录

### 5.1 关键决策

| 决策点 | 选择 | 理由 |
|--------|------|------|
| BO 映射方案 | `GpgpuDevice::mmap` 返回 `hc->heap + offset` | 不改 HAL 接口（免 ADR 流程），不分配独立页 |
| MEMCPY 实现 | Puller FSM HAL 路径 (`hal_mem_read/write`) | 有 bounds 校验，安全；方向自动判定 |
| Payload 契约 | `payload[0]=src, payload[1]=dst, payload[2]=size` | 与 `gpu_driver_client.h:322-324` 交叉验证 |
| TaskRunner device_ptr | gpu_va（`HAL_HEAP_BASE` 范围） | Puller HAL 要求 `dev_addr ∈ [BASE, BASE+SIZE)`，host_ptr 传入会下溢 |
| host_ptr 管理 | `externally_managed=true` | 不 std::free，归 HAL heap 的 buddy allocator 管理 |
| Fence 超时 | 5000ms + 必须 `status==1` | 避免 sim crash 永久挂死 + 超时不被 swallow |

### 5.2 架构妥协（债务记录）

| 妥协项 | 位置 | 影响 | 后续处理 |
|--------|------|------|----------|
| 直接读 `hal_user_context::heap` | `gpgpu_device.cpp:224-228` | 驱动层绕过 HAL 封装，3 区分边界模糊 | 引入 `hal_get_host_ptr()` API 并走 ADR 流程 |
| `args->gpu_va` 存 host_ptr | `handleMapBo:284` | 字段语义不准确，误导性 | 重命名为 `user_ptr` 或新增专用字段 |

### 5.3 相关 ADR

- [ADR-036](../00_adr/adr-036-three-way-separation.md) — 3 区分架构原则
- [ADR-040](../00_adr/adr-040-puller-fence-completion.md) — Puller Fence 完成机制
- [ADR-035](../00_adr/adr-035-governance-policy.md) §Rule 5.1 — 跨仓同步协议
- [ADR-020](../00_adr/adr-020-libgpu-core-buddy-extraction.md) — buddy allocator 独立库

---

## 6. 测试覆盖

### 6.1 UsrLinuxEmu 侧

| 测试 | 覆盖内容 |
|------|---------|
| `test_gpu_plugin.cpp` | MAP_BO 返回 host_ptr 可读可写验证 |
| `test_hardware_puller_emu_standalone` | Puller MEMCPY HAL 路径 |
| `test_gpu_mmap_and_submit_standalone` | mmap + submit 端到端 |

### 6.2 TaskRunner 侧

| 测试 | 覆盖内容 |
|------|---------|
| `test_cuda_e2e_real.cpp` | 完整 E2E: alloc → H2D → launch → D2H 全链路（需 `/dev/gpgpu0` 环境） |
| `test_cuda_scheduler.cpp` | submit_mem_alloc / submit_memcpy_* fence 异步语义 |

### 6.3 回归门禁

```bash
# UsrLinuxEmu
ctest --test-dir build -j4     # 104/104 PASS

# TaskRunner
ctest --test-dir build_umd -j4 # 14/14 PASS

# 文档审计
tools/docs-audit.sh --strict   # 43/43 PASS
```

---

## 7. 代码路径索引

| 组件 | 路径 | 关键函数 |
|------|------|----------|
| HAL 上下文 | `plugins/gpu_driver/hal/hal_user.h` | `struct hal_user_context` |
| HAL 内存分配 | `plugins/gpu_driver/hal/hal_user.cpp:55-70` | `user_mem_alloc` |
| HAL 内存读写 | `plugins/gpu_driver/hal/hal_user.cpp:35-53` | `user_mem_read`, `user_mem_write` |
| Buddy 分配器 | `libgpu_core/gpu_buddy.c` | `gpu_buddy_alloc`, `gpu_buddy_free` |
| BO 管理 | `plugins/gpu_driver/drv/gpgpu_device.cpp:214-290` | `handleAllocBo`, `handleMapBo` |
| mmap 实现 | `plugins/gpu_driver/drv/gpgpu_device.cpp:695-713` | `GpgpuDevice::mmap` (BO 分支) |
| Puller FSM | `plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp:178-213` | `State::DISPATCH` |
| 内存管理 | `external/TaskRunner/include/shared/memory_manager.hpp` | `MemoryManager::allocate`, `DeviceMemory` |
| 调度器 | `external/TaskRunner/src/test_fixture/cuda_scheduler.cpp` | `submit_mem_alloc`, `submit_memcpy_*` |
| 驱动客户端 | `external/TaskRunner/include/test_fixture/gpu_driver_client.h` | `alloc_bo`, `map_bo`, `get_bo_gpu_va`, `wait_fence` |
| E2E 测试 | `external/TaskRunner/tests/umd/test_cuda_e2e_real.cpp` | 5 个 TEST_CASE |
