# IOCTL 命令参考（System C）

本文档列出 UsrLinuxEmu 模拟的 `/dev/gpgpu0` 设备支持的 **System C**（`GPU_IOCTL_*`）ioctl 命令全集。旧版 ioctl 集已归档至 `archive/system_b_drivers/gpu/`，不再维护，新代码禁止使用。

**最后验证**: 2026-06-16 (commit `374d463`)
**对应 SSOT**: `docs/02_architecture/post-refactor-architecture.md` 附录 A
**头文件**: `plugins/gpu_driver/shared/gpu_ioctl.h`（Queue 结构在 `gpu_queue.h`）
**设备路径**: `/dev/gpgpu0`, `/dev/gpgpu1`
**魔术数**: `'G'`（0x47）

> 任务运行器（TaskRunner）通过 `external/TaskRunner/` 子模块下的 symlink 共享同一份头文件，确保从仿真切到真实内核驱动时**零修改迁移**。

---

## 0. 前置依赖链

Phase 2 强制要求按以下顺序建立 GPU 上下文。跳步将返回 `-EINVAL` 或 `-ENOENT`。

```
  open("/dev/gpgpu0", O_RDWR)
        │
        ▼
  GPU_IOCTL_GET_DEVICE_INFO (0x20)           ◀── 探测能力
        │
        ▼
  GPU_IOCTL_CREATE_VA_SPACE (0x30)           ◀── 必选：所有 GPU 操作都需在 VA Space 内
        │
        ▼
  GPU_IOCTL_CREATE_QUEUE (0x40)              ◀── 必选：queue_type ∈ {COMPUTE, COPY, GRAPHICS}
        │
        ▼
  GPU_IOCTL_MAP_QUEUE_RING (0x42)            ◀── 必选：绑定 TaskRunner 提供的共享内存作 Ring Buffer
        │
        ▼
  GPU_IOCTL_ALLOC_BO (0x10)                  ◀── 显存分配（按需）
        │
        ▼
  GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH (0x01)   ◀── 提交 GPFIFO 批次
        │
        ▼
  GPU_IOCTL_WAIT_FENCE (0x13)                ◀── 等待 fence_id 触发
        │
        ▼
  GPU_IOCTL_FREE_BO (0x11)                   ◀── 释放 BO
  GPU_IOCTL_DESTROY_QUEUE (0x41)             ◀── 销毁 Queue
  GPU_IOCTL_DESTROY_VA_SPACE (0x31)          ◀── 必须最后：无 attach queue 时才能销毁
```

> ⚠️ 任何在 `CREATE_VA_SPACE` 之前调用的 `CREATE_QUEUE` 都会得到 `-EINVAL`（参见 `gpgpu_device.cpp:407-411`）。Phase 1 的"隐式默认 queue"在 Phase 2 已移除。

---

## 1. 编号总览

| 编号 | 宏 | 方向 | 参数结构 | 处理器 |
|------|----|------|----------|--------|
| 0x01 | `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH` | `_IOW` | `gpu_pushbuffer_args` | `handlePushbufferSubmitBatch` |
| 0x02 | `GPU_IOCTL_REGISTER_MMU_EVENT_CB` | `_IOW` | `gpu_mmu_event_cb_args` | ⚠️ 未接线（Phase 2.5） |
| 0x03 | `GPU_IOCTL_REGISTER_FIRMWARE_CB` | `_IOW` | `gpu_firmware_cb_args` | ⚠️ 未接线（Phase 2.5） |
| 0x10 | `GPU_IOCTL_ALLOC_BO` | `_IOWR` | `gpu_alloc_bo_args` | `handleAllocBo` |
| 0x11 | `GPU_IOCTL_FREE_BO` | `_IOW` | `u32`（handle） | `handleFreeBo` |
| 0x12 | `GPU_IOCTL_MAP_BO` | `_IOWR` | `gpu_map_bo_args` | `handleMapBo` |
| 0x13 | `GPU_IOCTL_WAIT_FENCE` | `_IOW` | `gpu_wait_fence_args` | `handleWaitFence` |
| 0x20 | `GPU_IOCTL_GET_DEVICE_INFO` | `_IOR` | `gpu_device_info` | `handleGetDeviceInfo` |
| 0x30 | `GPU_IOCTL_CREATE_VA_SPACE` | `_IOWR` | `gpu_va_space_args` | `handleCreateVASpace` |
| 0x31 | `GPU_IOCTL_DESTROY_VA_SPACE` | `_IOW` | `gpu_va_space_handle_t` | `handleDestroyVASpace` |
| 0x32 | `GPU_IOCTL_REGISTER_GPU` | `_IOW` | `gpu_register_gpu_args` | `handleRegisterGPU` |
| 0x40 | `GPU_IOCTL_CREATE_QUEUE` | `_IOWR` | `gpu_queue_args` | `handleCreateQueue` |
| 0x41 | `GPU_IOCTL_DESTROY_QUEUE` | `_IOW` | `gpu_queue_handle_t` | `handleDestroyQueue` |
| 0x42 | `GPU_IOCTL_MAP_QUEUE_RING` | `_IOWR` | `gpu_queue_map_ring_args` | `handleMapQueueRing` |
| 0x43 | `GPU_IOCTL_QUERY_QUEUE` | `_IOWR` | `gpu_queue_info_args` | `handleQueryQueue` |

> 0x02/0x03 在头文件中已定义但尚未挂入 `GpgpuDevice::getIoctlTablePtr`（Phase 2.5 任务）。`ioctl()` 收到未识别请求会返回 `-EINVAL`。

---

## 2. 公共类型

```c
#include "shared/gpu_types.h"
#include "shared/gpu_queue.h"

typedef u64 gpu_va_space_handle_t;   // VA Space 句柄
typedef u64 gpu_queue_handle_t;      // Queue 句柄
typedef u32 gpu_stream_id_t;         // CUDA stream / Vulkan queue ID
typedef u32 gpu_channel_id_t;        // GPU 通道标识

/* 内存域 */
#define GPU_MEM_DOMAIN_VRAM 0x1     // 设备本地显存
#define GPU_MEM_DOMAIN_GTT  0x2     // GART（GPU 可访问系统内存）
#define GPU_MEM_DOMAIN_CPU  0x4     // 系统内存

/* BO 分配标志 */
#define GPU_BO_DEVICE_LOCAL 0x1     // 设备本地
#define GPU_BO_HOST_VISIBLE 0x2     // CPU 可访问
#define GPU_BO_CXL_SHARED  0x4      // CXL.cache 一致性（融合设备）

/* 提交标志 */
#define GPU_SUBMIT_FENCE        0x1
#define GPU_SUBMIT_INTERRUPT    0x2
#define GPU_SUBMIT_PRIORITY_HIGH 0x4

/* Queue 类型 */
#define GPU_QUEUE_COMPUTE  0x0
#define GPU_QUEUE_COPY     0x1
#define GPU_QUEUE_GRAPHICS 0x2

/* GPFIFO 方法 */
#define GPU_OP_LAUNCH_KERNEL   0x100
#define GPU_OP_LAUNCH_CPU_TASK 0x101
#define GPU_OP_MEMCPY          0x102
#define GPU_OP_MEMSET          0x103
#define GPU_OP_FENCE           0x104
```

---

## 3. 命令提交类

### 3.1 `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH` (0x01)

提交一批 GPFIFO entry。模拟器把 `entries_addr` 处的 `gpu_gpfifo_entry[]` 写入设备内存，并触发 Hardware Puller 状态机。

```c
struct gpu_pushbuffer_args {
  u32 stream_id;     // 目标 stream / queue
  u64 entries_addr;  // 用户态 GPFIFO entry 数组地址
  u32 count;         // entry 数量（1..16）
  u32 flags;         // GPU_SUBMIT_*
  u64 fence_id;      // OUT：返回的 fence 标识
  u64 va_space_handle;  // IN：VA Space 句柄（0=向后兼容，跳过校验）
};
```

**约束**:

| 项 | 值 | 来源 |
|----|----|------|
| `count` 范围 | `1..16` | `gpgpu_device.cpp:269` |
| `entries_addr` | 用户态虚拟地址，含 `count` 个 `gpu_gpfifo_entry` | 同上 |
| `count == 0` | 立即返回 `-EINVAL` | 同上 |
| `va_space_handle != 0` | 强制校验 VA Space 存在 + `stream_id` 已 attach | `gpgpu_device.cpp` `handlePushbufferSubmitBatch`（Phase 2 v0.1.3 起）|
| `va_space_handle == 0` | 跳过校验（向后兼容 sentinel）| 设计 D1 |

**返回值**:

| 返回 | 含义 |
|------|------|
| `0` | 提交成功；`fence_id` 已填 |
| `-EINVAL` | `count` 越界 / entry 含未知 method / VA Space 不存在 / Queue 未 attach |
| `-EFAULT` | `argp` 为空 |
| `-ENOMEM` | `hal_fence_create` 失败 |

**示例**:

```c
#include "shared/gpu_ioctl.h"

struct gpu_gpfifo_entry entries[2] = {
  { .valid=1, .method=GPU_OP_MEMCPY, .payload={0x1000, 0x2000, 4096} },
  { .valid=1, .method=GPU_OP_FENCE }
};
struct gpu_pushbuffer_args args = {
  .stream_id = 0,
  .entries_addr = (u64)entries,
  .count = 2,
  .flags = GPU_SUBMIT_FENCE
};
ioctl(fd, GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, &args);
// args.fence_id 现在可用 GPU_IOCTL_WAIT_FENCE 等待
```

---

### 3.2 `GPU_IOCTL_REGISTER_MMU_EVENT_CB` (0x02)

注册 MMU 事件回调（页面迁移、TLB 刷新等）。UsrLinuxEmu 的 MMU Event Dispatcher 在事件发生时调用。

```c
struct gpu_mmu_event_cb_args {
  u64 callback_fn;  // void (*)(const struct gpu_mmu_event_context *)
  u64 user_data;    // 透传给回调
};
```

**当前状态**: 定义于头文件但未挂入 ioctl 分发表（Phase 2.5 待实现）。调用会返回 `-EINVAL`。

**示例**:

```c
void on_mmu_event(const struct gpu_mmu_event_context* ctx) { /* ... */ }
struct gpu_mmu_event_cb_args args = {
  .callback_fn = (u64)on_mmu_event,
  .user_data = (u64)my_state_ptr
};
int ret = ioctl(fd, GPU_IOCTL_REGISTER_MMU_EVENT_CB, &args);
```

---

### 3.3 `GPU_IOCTL_REGISTER_FIRMWARE_CB` (0x03)

注册固件回调。当 Hardware Puller 解码到 `OP_LAUNCH_CPU_TASK` entry 时，在固件线程上下文调用。

```c
struct gpu_firmware_cb_args {
  u64 callback_fn;  // void (*)(const struct gpu_cpu_task_desc *)
  u64 user_data;
};
```

**当前状态**: 同 0x02，未接线，调用返回 `-EINVAL`。`GPU_OP_LAUNCH_CPU_TASK` entry 当前在 `handlePushbufferSubmitBatch` 中仅打印日志（`gpgpu_device.cpp:353-357`）。

**示例**:

```c
void on_firmware_task(const struct gpu_cpu_task_desc* d) { /* ... */ }
struct gpu_firmware_cb_args args = {
  .callback_fn = (u64)on_firmware_task,
  .user_data = 0
};
ioctl(fd, GPU_IOCTL_REGISTER_FIRMWARE_CB, &args);
```

---

## 4. 内存管理类

### 4.1 `GPU_IOCTL_ALLOC_BO` (0x10)

分配 GPU buffer object（GEM/TTM 后端）。返回 GEM handle 与 GPU 虚拟地址。

```c
struct gpu_alloc_bo_args {
  u64 size;    // IN：字节数
  u32 domain;  // IN：GPU_MEM_DOMAIN_*
  u32 flags;   // IN：GPU_BO_*
  u32 handle;  // OUT：GEM 句柄
  u64 gpu_va;  // OUT：GPU 虚拟地址
};
```

**约束**:

| 项 | 值 | 来源 |
|----|----|------|
| `domain == 0` | 拒绝，返回 `-EINVAL` | `gpgpu_device.cpp:192` |
| `size == 0` | 由 `hal_mem_alloc` 决定行为 | HAL 层 |
| handle 上限 | 65535（`HandleManager::max_handles_` 默认） | `gpgpu_device.cpp:59-65` |

**返回值**:

| 返回 | 含义 |
|------|------|
| `0` | 成功；`handle` 和 `gpu_va` 已填 |
| `-EINVAL` | `domain == 0` |
| `-ENOMEM` | HAL 分配失败或 handle 池耗尽 |
| `-EFAULT` | `argp` 为空 |

**示例**:

```c
struct gpu_alloc_bo_args args = {
  .size = 4 * 1024 * 1024,
  .domain = GPU_MEM_DOMAIN_VRAM,
  .flags  = GPU_BO_DEVICE_LOCAL
};
if (ioctl(fd, GPU_IOCTL_ALLOC_BO, &args) == 0) {
  printf("handle=%u va=0x%llx\n", args.handle, args.gpu_va);
}
```

---

### 4.2 `GPU_IOCTL_FREE_BO` (0x11)

释放 BO。参数是 GEM handle（`u32`）。

**约束**:

| 项 | 值 |
|----|-----|
| `handle == 0` | 返回 `-EINVAL`（保留 handle） |
| handle 不存在 | 返回 `-EINVAL`（不打印 ENOENT） |

**示例**:

```c
u32 handle = /* 从 ALLOC_BO 取得 */;
if (ioctl(fd, GPU_IOCTL_FREE_BO, &handle) != 0) {
  fprintf(stderr, "FREE_BO failed\n");
}
```

**返回值**:

| 返回 | 含义 |
|------|------|
| `0` | 释放成功 |
| `-EINVAL` | `handle` 无效或未分配 |
| `-EFAULT` | `argp` 为空 |

---

### 4.3 `GPU_IOCTL_MAP_BO` (0x12)

把 BO 映射进 GPU 虚拟地址空间，返回当前 `gpu_va`。

```c
struct gpu_map_bo_args {
  u32 handle;  // IN：GEM 句柄
  u32 flags;   // IN：映射标志（暂未使用，预留）
  u64 gpu_va;  // OUT：GPU 虚拟地址
};
```

**示例**:

```c
struct gpu_map_bo_args args = { .handle = h, .flags = 0 };
if (ioctl(fd, GPU_IOCTL_MAP_BO, &args) == 0) {
  // args.gpu_va 现可在 GPFIFO entry.payload 中使用
}
```

**返回值**:

| 返回 | 含义 |
|------|------|
| `0` | 成功；`gpu_va` 已填 |
| `-EINVAL` | `handle` 不存在或 BO 已被释放 |
| `-EFAULT` | `argp` 为空 |

---

### 4.4 `GPU_IOCTL_WAIT_FENCE` (0x13)

阻塞等待 fence 触发。Phase 1 是 poll 实现，Phase 2 改为 event-driven（MSI-X 回调）。

```c
struct gpu_wait_fence_args {
  u64 fence_id;    // IN：来自 PUSHBUFFER_SUBMIT_BATCH 的返回值
  u32 timeout_ms;  // IN：超时毫秒数，0 = 永久
  u32 status;      // OUT：1=signaled, 0=timeout, -1=error
};
```

**注意**: 当前 poll 间隔 1ms（`gpgpu_device.cpp:377`）。仿真下 fence 实际上在 `PUSHBUFFER_SUBMIT_BATCH` 提交时即被 `hal_fence_create` 注册，所以通常立即返回。

**示例**:

```c
struct gpu_wait_fence_args w = { .fence_id = args.fence_id, .timeout_ms = 5000 };
ioctl(fd, GPU_IOCTL_WAIT_FENCE, &w);
if (w.status == 1) puts("fence signaled");
```

**返回值**:

| 返回 | 含义 |
|------|------|
| `0` | 调用完成；看 `status` 区分 signaled/timeout |
| `-EFAULT` | `argp` 为空 |

---

## 5. 设备信息

### 5.1 `GPU_IOCTL_GET_DEVICE_INFO` (0x20)

查询设备能力。Phase 1.5 在基础字段上扩展了 11 个性能字段（warp size、SIMD 数、peak FP32 等）。

```c
struct gpu_device_info {
  u32 vendor_id, device_id;
  u64 vram_size, bar0_size;
  u32 max_channels, compute_units, gpfifo_capacity, cache_line_size;

  /* Phase 1.5 扩展 */
  u32 warp_size;
  u32 max_clock_frequency;           // MHz
  u32 driver_version;                // 0xMMmmRR
  u32 firmware_version;              // 0xMMmm
  u32 simd_count;
  u32 max_memory_clock_frequency;    // MHz
  u32 memory_bus_width;              // bits
  u32 peak_fp32_gflops;              // 理论峰值
  u32 pcie_bandwidth;                // Mbps
  u32 architecture_id;
  char marketing_name[64];
};
```

**模拟器默认值**（`gpgpu_device.cpp:20-40`）:

| 字段 | 值 |
|------|----|
| `vendor_id` | 0x1000 |
| `device_id` | 0x1001 |
| `vram_size` | 8 GiB |
| `bar0_size` | 16 MiB |
| `compute_units` | 64 |
| `gpfifo_capacity` | 1024 |
| `warp_size` | 32（NVIDIA 风格） |
| `peak_fp32_gflops` | 17000（17 TFLOPS） |
| `marketing_name` | `"UsrLinuxEmu Simulator v1"` |

**示例**:

```c
struct gpu_device_info info = {0};
ioctl(fd, GPU_IOCTL_GET_DEVICE_INFO, &info);
printf("%s: %llu MB VRAM, %u CUs, %u TFLOPS\n",
       info.marketing_name, info.vram_size >> 20,
       info.compute_units, info.peak_fp32_gflops / 1000);
```

**返回值**:

| 返回 | 含义 |
|------|------|
| `0` | 成功；`info` 已填充 |
| `-EFAULT` | `argp` 为空 |

---

## 6. VA Space 管理（Phase 2）

### 6.1 `GPU_IOCTL_CREATE_VA_SPACE` (0x30)

创建一个 GPU 虚拟地址空间。所有后续 GPU 操作必须携带有效的 `va_space_handle`。

```c
struct gpu_va_space_args {
  u32 page_size;                       // IN：0=4KB, 1=64KB
  u32 flags;                           // IN：保留，置 0
  gpu_va_space_handle_t va_space_handle; // OUT：VA Space 句柄
};
```

**约束**:

| 项 | 值 | 来源 |
|----|----|------|
| `page_size` | 仅接受 0 或 1 | `gpgpu_device.cpp:642` |
| handle 0 | 视为溢出，返回 `-ENOMEM` | `gpgpu_device.cpp:650-654` |

**示例**:

```c
struct gpu_va_space_args vs = { .page_size = 0, .flags = 0 };
ioctl(fd, GPU_IOCTL_CREATE_VA_SPACE, &vs);
u64 va_space = vs.va_space_handle;  // 后续所有 Queue/BO 都需要
```

**返回值**:

| 返回 | 含义 |
|------|------|
| `0` | 成功；`va_space_handle` 已填 |
| `-EINVAL` | `page_size > 1` |
| `-ENOMEM` | handle 溢出 |
| `-EFAULT` | `argp` 为空 |

---

### 6.2 `GPU_IOCTL_DESTROY_VA_SPACE` (0x31)

销毁 VA Space。**前提**: 该 VA Space 下没有 attach 的 queue（否则返回 `-EBUSY`）。

```c
// 参数类型：gpu_va_space_handle_t (u64)，按指针传入
u64 handle = vs_handle;
ioctl(fd, GPU_IOCTL_DESTROY_VA_SPACE, &handle);
```

**返回值**:

| 返回 | 含义 | 来源 |
|------|------|------|
| `0` | 销毁成功 | `gpgpu_device.cpp:690` |
| `-ENOENT` | handle 不存在 | `gpgpu_device.cpp:678` |
| `-EBUSY` | 还有 queue 未销毁 | `gpgpu_device.cpp:684-688` |
| `-EFAULT` | `argp` 为空 | 同上 |

---

### 6.3 `GPU_IOCTL_REGISTER_GPU` (0x32)

把一个 GPU 注册到 VA Space，为多 GPU / P2P 预留。

```c
struct gpu_register_gpu_args {
  gpu_va_space_handle_t va_space_handle;  // IN
  u32 gpu_id;                             // IN
  u32 flags;                              // IN：保留
};
```

**当前实现**: 仅打印日志并返回 0（多 GPU 真正实现在 Phase 3，`gpgpu_device.cpp:709-712`）。

**示例**:

```c
struct gpu_register_gpu_args r = { .va_space_handle = vs, .gpu_id = 0, .flags = 0 };
ioctl(fd, GPU_IOCTL_REGISTER_GPU, &r);
```

**返回值**:

| 返回 | 含义 |
|------|------|
| `0` | 注册成功（当前是 no-op 确认） |
| `-ENOENT` | `va_space_handle` 不存在 |
| `-EFAULT` | `argp` 为空 |

---

## 7. Queue 管理（Phase 2，ADR-024）

### 7.1 `GPU_IOCTL_CREATE_QUEUE` (0x40)

创建 GPU 命令队列。**强制要求** `va_space_handle` 已存在（前置依赖链第 2 步）。

```c
struct gpu_queue_args {
  gpu_va_space_handle_t va_space_handle;  // IN
  u32 queue_type;                         // IN：GPU_QUEUE_*
  u32 priority;                           // IN：0-100
  u64 ring_buffer_size;                   // IN：entry 数，0=默认 1024
  gpu_queue_handle_t queue_handle;        // OUT
  u64 doorbell_pgoff;                     // OUT：mmap page offset
};
```

**doorbell offset 计算**（`gpgpu_device.cpp:427`）:
```
doorbell_pgoff = DOORBELL_ALLOC_BASE + handle * DOORBELL_ALLOC_STRIDE
```
当前 `DOORBELL_ALLOC_BASE = 0x10000`，`DOORBELL_ALLOC_STRIDE = 0x1000`。后续 `mmap(fd, 4096, ..., doorbell_pgoff)` 获得 doorbell 页。

**约束**:

| 项 | 值 |
|----|-----|
| `queue_type > GPU_QUEUE_GRAPHICS` | 返回 `-EINVAL` |
| VA Space 不存在 | 返回 `-EINVAL` |
| `ring_buffer_size == 0` | 默认 `GPU_MAX_RING_ENTRIES` (1024) |

**示例**:

```c
struct gpu_queue_args q = {
  .va_space_handle = va_space,
  .queue_type = GPU_QUEUE_COMPUTE,
  .priority = 50,
  .ring_buffer_size = 256
};
ioctl(fd, GPU_IOCTL_CREATE_QUEUE, &q);
// mmap doorbell
void* db = mmap(NULL, 4096, PROT_WRITE, MAP_SHARED, fd, q.doorbell_pgoff);
```

**返回值**:

| 返回 | 含义 |
|------|------|
| `0` | 成功；`queue_handle` 和 `doorbell_pgoff` 已填 |
| `-EINVAL` | `queue_type` 越界或 VA Space 不存在 |
| `-ENOMEM` | handle 溢出 |
| `-EFAULT` | `argp` 为空 |

---

### 7.2 `GPU_IOCTL_DESTROY_QUEUE` (0x41)

销毁 queue，自动从所属 VA Space 解除 attach（`gpgpu_device.cpp:454-465`）。

```c
u64 qh = q.queue_handle;
ioctl(fd, GPU_IOCTL_DESTROY_QUEUE, &qh);
```

**返回值**:

| 返回 | 含义 |
|------|------|
| `0` | 销毁成功 |
| `-ENOENT` | handle 不存在 |
| `-EFAULT` | `argp` 为空 |

---

### 7.3 `GPU_IOCTL_MAP_QUEUE_RING` (0x42)

把 TaskRunner 提供的共享内存绑定为 Ring Buffer。**前置**: `queue_handle` 必须已 CREATE。

```c
// 定义在 shared/gpu_queue.h
struct gpu_queue_map_ring_args {
  u64 queue_handle;  // IN：来自 CREATE_QUEUE
  u64 ring_addr;     // IN：共享内存起始地址
};
```

**绑定大小**（`gpgpu_device.cpp:487-488`）:
```
size = sizeof(gpu_ring_header) + ringSize() * sizeof(gpu_gpfifo_entry)
```

**示例**:

```c
// 1. TaskRunner 端创建 shm
int shm_fd = shm_open("/gpu_ring0", O_CREAT|O_RDWR, 0666);
ftruncate(shm_fd, 4096 * 16);
void* ring = mmap(NULL, 4096*16, PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd, 0);

// 2. 绑定到 queue
struct gpu_queue_map_ring_args m = {
  .queue_handle = q.queue_handle,
  .ring_addr = (u64)ring
};
ioctl(fd, GPU_IOCTL_MAP_QUEUE_RING, &m);
```

**返回值**:

| 返回 | 含义 |
|------|------|
| `0` | 绑定成功；Puller 可消费该 Ring |
| `-ENOENT` | `queue_handle` 不存在 |
| `-ENOMEM` | `attachSharedMemory` 失败 |
| `-EFAULT` | `argp` 为空 |

---

### 7.4 `GPU_IOCTL_QUERY_QUEUE` (0x43)

查询 queue 状态，包括类型、doorbell offset、ring 地址、待处理 entry 数。

```c
// 定义在 shared/gpu_queue.h
struct gpu_queue_info_args {
  u64 queue_handle;       // IN
  u32 queue_type;         // OUT
  u32 queue_id;           // OUT：内部 ID
  u64 doorbell_offset;    // OUT
  u64 ring_addr;          // OUT：未映射时为 0
  u32 ring_size;          // OUT
  u32 pending_count;      // OUT：未消费 entry 数
};
```

**示例**:

```c
struct gpu_queue_info_args i = { .queue_handle = q.queue_handle };
ioctl(fd, GPU_IOCTL_QUERY_QUEUE, &i);
printf("type=%u ring=%u pending=%u\n",
       i.queue_type, i.ring_size, i.pending_count);
```

**返回值**:

| 返回 | 含义 |
|------|------|
| `0` | 成功；所有 OUT 字段已填 |
| `-ENOENT` | `queue_handle` 不存在 |
| `-EFAULT` | `argp` 为空 |

---

## 8. 错误码速查

| 错误码 | 数值 | 常见触发 |
|--------|------|----------|
| `0` | 0 | 成功 |
| `-EINVAL` | -22 | 参数越界、handle 失效、未知 method |
| `-EFAULT` | -14 | `argp` 为空指针 |
| `-ENOMEM` | -12 | HAL 分配失败、handle 池耗尽 |
| `-ENOENT` | -2 | VA Space / Queue handle 不存在 |
| `-EBUSY` | -16 | 销毁 VA Space 时仍有 attached queue |
| `-ENOTTY` | -25 | ioctl 命令不识别（仅当 `ioctl()` 落到非表驱动路径时） |

> ⚠️ 模拟器 `GpgpuDevice::ioctl()` 对未识别请求返回 `-EINVAL` 而非 `-ENOTTY`（`gpgpu_device.cpp:135-137`），与 Linux 惯例略有差异。这是已知小问题，跟踪在 issue 中。

---

## 9. 完整使用样例

下面把前置依赖链串成一个最小可编译示例（依赖 `<sys/ioctl.h>` 与 `shared/gpu_ioctl.h`）：

```c
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "shared/gpu_ioctl.h"

int main(void) {
  int fd = open("/dev/gpgpu0", O_RDWR);
  if (fd < 0) return 1;

  // 1. 探测
  struct gpu_device_info info = {0};
  ioctl(fd, GPU_IOCTL_GET_DEVICE_INFO, &info);

  // 2. 建 VA Space
  struct gpu_va_space_args vs = { .page_size = 0, .flags = 0 };
  ioctl(fd, GPU_IOCTL_CREATE_VA_SPACE, &vs);

  // 3. 建 queue
  struct gpu_queue_args q = {
    .va_space_handle = vs.va_space_handle,
    .queue_type = GPU_QUEUE_COMPUTE, .priority = 50, .ring_buffer_size = 256
  };
  ioctl(fd, GPU_IOCTL_CREATE_QUEUE, &q);

  // 4. mmap doorbell
  volatile u32* doorbell = mmap(NULL, 4096, PROT_WRITE, MAP_SHARED,
                                fd, q.doorbell_pgoff);
  *doorbell = 1;  // 触发 doorbell ring

  // 5. 分配 BO
  struct gpu_alloc_bo_args bo = { .size = 4096, .domain = GPU_MEM_DOMAIN_VRAM,
                                  .flags = GPU_BO_DEVICE_LOCAL };
  ioctl(fd, GPU_IOCTL_ALLOC_BO, &bo);

  // 6. 提交 pushbuffer
  struct gpu_gpfifo_entry e = { .valid=1, .method=GPU_OP_FENCE };
  struct gpu_pushbuffer_args pb = {
    .stream_id = 0, .entries_addr = (u64)&e, .count = 1, .flags = 0
  };
  ioctl(fd, GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, &pb);

  // 7. 等 fence
  struct gpu_wait_fence_args w = { .fence_id = pb.fence_id, .timeout_ms = 1000 };
  ioctl(fd, GPU_IOCTL_WAIT_FENCE, &w);

  // 8. 清理
  ioctl(fd, GPU_IOCTL_FREE_BO, &bo.handle);
  ioctl(fd, GPU_IOCTL_DESTROY_QUEUE, &q.queue_handle);
  ioctl(fd, GPU_IOCTL_DESTROY_VA_SPACE, &vs.va_space_handle);
  close(fd);
  return 0;
}
```

---

## 10. 相关文档

- SSOT 架构: [`docs/02_architecture/post-refactor-architecture.md`](../02_architecture/post-refactor-architecture.md) 附录 A
- API 参考: [`docs/06-reference/api-reference.md`](api-reference.md)
- 队列设计 ADR: `docs/00_adr/adr-024-user-mode-queue-submission.md`
- IOCTL 统一 ADR: `docs/00_adr/adr-015-gpu-ioctl-unification.md`
- 驱动分层 ADR: `docs/00_adr/adr-018-driver-sim-separation.md`
- TaskRunner 集成: [`docs/07-integration/taskrunner-index.md`](../07-integration/taskrunner-index.md)

---

**最后验证**: 2026-06-16 (commit `374d463`)
**对应头文件**: `plugins/gpu_driver/shared/gpu_ioctl.h`、`gpu_queue.h`、`gpu_types.h`
