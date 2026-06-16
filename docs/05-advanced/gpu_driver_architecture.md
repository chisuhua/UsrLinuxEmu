# UsrLinuxEmu GPU 驱动仿真架构

> **SSOT 引用**: 本文档基于 [docs/02_architecture/post-refactor-architecture.md](../02_architecture/post-refactor-architecture.md) (SSOT) 第 §1.2、§1.3、§1.4 节，与代码 commit `374d463` 同步。
>
> **最后验证**: 2026-06-16 (commit `374d463`)
>
> **维护者**: UsrLinuxEmu Architecture Team
>
> **适用对象**: GPU 驱动开发工程师、硬件仿真工程师、TaskRunner 调度器开发团队

---

## 目录

- [§1 概述](#1-概述)
- [§2 四层架构总览](#2-四层架构总览)
- [§3 Layer 1 — 驱动层 (`drv/`)](#3-layer-1--驱动层-drv)
- [§4 Layer 2 — 硬件抽象层 (`hal/`)](#4-layer-2--硬件抽象层-hal)
- [§5 Layer 3 — 公共契约层 (`shared/`)](#5-layer-3--公共契约层-shared)
- [§6 Layer 4 — 仿真层 (`sim/`)](#6-layer-4--仿真层-sim)
- [§7 算法核心 (`libgpu_core/`)](#7-算法核心-libgpu_core)
- [§8 IOCTL 体系 (System C)](#8-ioctl-体系-system-c)
- [§9 关键数据流](#9-关键数据流)
- [§10 数据模型](#10-数据模型)
- [§11 插件加载与生命周期](#11-插件加载与生命周期)
- [§12 ADR 索引](#12-adr-索引)
- [§13 验证与测试](#13-验证与测试)
- [§14 与 TaskRunner 的边界](#14-与-taskrunner-的边界)
- [§15 总结](#15-总结)

---

## §1 概述

### 1.1 设计目标

`plugins/gpu_driver/` 是 UsrLinuxEmu 的核心交付物。它在用户态仿真一个 GPGPU 设备，对外暴露 `/dev/gpgpu0` 设备节点，对 TaskRunner 提供与真实内核驱动完全相同的 IOCTL 表面。

三个核心承诺:

1. **零改动迁移**: TaskRunner 通过同一份 `shared/` 头文件访问仿真器与未来真实 `.ko`，运行时切换不需要改一行代码。
2. **驱动可移植**: 驱动代码 (`drv/`) 只依赖 `hal/` 接口，不直接调用仿真层。移植到内核时，替换 `hal/` 实现为寄存器读写即可。
3. **算法可复用**: 纯算法部分 (`libgpu_core/`) 提取为 C99 库，无外部依赖，可直接复制到任何 C 编译环境。

### 1.2 物理位置

```
plugins/gpu_driver/
├── CMakeLists.txt          # 插件构建入口
├── plugin.cpp              # 插件入口，导出 `module mod` 符号
├── drv/                    # Layer 1: 驱动代码 (移植到真实内核的目标)
├── hal/                    # Layer 2: 硬件抽象层 (drv ↔ sim 的契约)
├── sim/                    # Layer 4: 仿真实现 (仅用户态)
└── shared/                 # Layer 3: 公共头文件 (ABI 契约)

libgpu_core/                # 算法核心: 纯 C buddy allocator
├── CMakeLists.txt
├── include/gpu_buddy.h
├── src/buddy.c
└── test/                    # 独立 C 单元测试
```

`plugins/gpu_driver/` 是动态加载的 `.so` (MODULE 库)，`libgpu_core/` 是静态库 (`.a`)。两者通过 `target_link_libraries` 在 `plugins/gpu_driver/CMakeLists.txt` 集成。

### 1.3 与 TaskRunner 的边界

| 组件 | 职责 | 技术边界 | 交付物 |
|------|------|----------|--------|
| **UsrLinuxEmu** | 驱动/硬件仿真 | 实现 `/dev/gpgpu0` 设备节点，仿真 doorbell、puller、scheduler、VRAM 分配 | GPU 设备插件 + IOCTL 头文件 |
| **TaskRunner** | 调度/固件 | 构建 GPFIFO 命令流，管理 CPU/GPU 任务依赖 | 通过 IOCTL 提交命令、mmap Ring Buffer/Doorbell |
| **Shared Interface** | 接口契约 | `shared/gpu_*.h` 头文件定义标准 IOCTL 与数据模型 | 两项目符号链接共享的头文件目录 |

零耦合原则: UsrLinuxEmu **绝不包含** TaskRunner 的调度逻辑或固件二进制。TaskRunner **绝不直接调用** UsrLinuxEmu 内部函数。所有交互通过 `/dev/gpgpu0` 的标准 IOCTL + mmap。

---

## §2 四层架构总览

```
┌─────────────────────────────────────────────────────────────────┐
│                       用户应用层 (User Apps)                      │
│   tests/  •  external/TaskRunner (submodule)  •  User drivers  │
└─────────────────────────────────────────────────────────────────┘
                                ↓ ioctl(fd, GPU_IOCTL_*, ...)
┌─────────────────────────────────────────────────────────────────┐
│                  内核模拟框架层 (Kernel Framework)               │
│   src/kernel/*.cpp  +  include/kernel/*.h  (SHARED lib)        │
│   • VFS (Meyers singleton)  •  ModuleLoader  •  WaitQueue      │
│   • ServiceRegistry  •  Logger  •  PollWatcher                 │
│   include/linux_compat/: u8/u32, ERR_PTR, _IOR, GFP_*          │
└─────────────────────────────────────────────────────────────────┘
                                ↓ dlopen("plugins/plugin_gpu_driver.so")
┌─────────────────────────────────────────────────────────────────┐
│              设备驱动层 — plugins/gpu_driver/                   │
│                                                                  │
│  ┌─ Layer 1: drv/ ──────────────────────────────────────────┐  │
│  │  GpgpuDevice (ioctl dispatch via kTable[kNumIoctls=13])  │  │
│  │  gpu_drm_driver.cpp (DRM-style ioctl 描述符表)            │  │
│  └──────────────────────────────────────────────────────────┘  │
│                              ↓ HAL ops 调用                      │
│  ┌─ Layer 2: hal/ ──────────────────────────────────────────┐  │
│  │  gpu_hal.h: struct gpu_hal_ops (11 个函数指针)            │  │
│  │  hal_user.{h,cpp}  : 真实用户态实现                          │  │
│  │  hal_mock.{h,cpp}  : 单元测试 mock                          │  │
│  └──────────────────────────────────────────────────────────┘  │
│                              ↓                                     │
│  ┌─ Layer 4: sim/ ──────────────────────────────────────────┐  │
│  │  sim/scheduler/   : GlobalScheduler + GpfifoTranslator   │  │
│  │  sim/hardware/    : HardwarePullerEmu (FSM), DoorbellEmu │  │
│  │  gpu_queue_emu.*  : Ring Buffer 消费者                     │  │
│  │  sim/buddy_allocator.cpp, sim/fence_sim.cpp               │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                  │
│  ┌─ Layer 3: shared/ ───────────────────────────────────────┐  │
│  │  gpu_ioctl.h  gpu_types.h  gpu_queue.h  gpu_events.h     │  │
│  │  gpu_regs.h    ← canonical 头，TaskRunner 符号链接共享       │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                  │
│  ┌─ libgpu_core/ (顶层) ─────────────────────────────────────┐  │
│  │  纯 C buddy allocator (gpu_buddy.h + buddy.c)             │  │
│  │  无外部依赖，可直接复制到内核驱动                            │  │
│  └──────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

依赖方向 (ADR-018 强制):

```
drv/  ──►  hal/  ──►  sim/  ──►  libgpu_core/
   │           │           │
   │           │           └── 纯 C, 零外部依赖
   │           └── 11 fn-ptr, 构造注入
   └── 通过 HAL 间接访问硬件，不直接调 sim/
```

`shared/` 不属于任何一层，是 ABI 契约。`sim/` 移植到内核时被替换为真实硬件寄存器操作；`libgpu_core/` 直接复制到内核模块即可。

---

## §3 Layer 1 — 驱动层 (`drv/`)

### 3.1 职责

`drv/` 包含 GpgpuDevice 类与 DRM driver 桩。代码风格遵循 ADR-018 的"可移植 C++ 子集":

| 允许 | 禁止 |
|------|------|
| 基础 C++ (类、继承、虚函数) | RTTI (`typeid`, `dynamic_cast`) |
| `linux_compat` 容器 | `std::vector`, `std::map` (内核无 STL) |
| Linux 错误码 (`-EINVAL`, `-ENOMEM`) | C++ 异常 |
| 简单 RAII (`lock_guard` 等价) | `std::shared_ptr` (插件上下文可用) |
| `int`, `u32`, `u64` (linux_compat 类型) | `std::cout` (用 Logger) |

### 3.2 文件清单

```
plugins/gpu_driver/drv/
├── CMakeLists.txt
├── gpgpu_device.h          (138 行) — GpgpuDevice 类定义
├── gpgpu_device.cpp        (714 行) — ioctl handler 实现
└── gpu_drm_driver.cpp      (288 行) — DRM-style ioctl 描述符表
```

### 3.3 `GpgpuDevice` 类骨架

`GpgpuDevice` 继承自 `usr_linux_emu::FileOperations`，重写 `ioctl()` / `open()` / `close()` / `mmap()`。

```cpp
class GpgpuDevice : public usr_linux_emu::FileOperations {
 public:
  static constexpr size_t kNumIoctls = 13;

  explicit GpgpuDevice(struct gpu_hal_ops* hal);
  ~GpgpuDevice();

  void setPuller(std::shared_ptr<HardwarePullerEmu> puller);

  long ioctl(int fd, unsigned long request, void* argp) override;
  int open(const char* path, int flags) override;
  int close(int fd) override;
  void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset) override;

  // ========== VA Space 管理 (Phase 2) ==========
  struct VASpace {
    uint64_t handle;
    uint32_t page_size;   // 0=4KB, 1=64KB
    uint32_t flags;
    std::vector<uint64_t> attached_queues;
  };
  long createVASpace(uint32_t page_size, uint32_t flags, gpu_va_space_handle_t* out);
  long destroyVASpace(gpu_va_space_handle_t handle);
  bool vaSpaceExists(gpu_va_space_handle_t handle) const;
  long attachQueueToVASpace(gpu_va_space_handle_t va, uint64_t queue);

  // ========== Doorbell mmap 偏移 ==========
  static constexpr off_t QUEUE_RING_MMAP_BASE = 0x10000;
  static constexpr off_t DOORBELL_MMAP_OFFSET  = 0x20000;
  static constexpr uint64_t DOORBELL_ALLOC_BASE   = 0x10000;
  static constexpr uint64_t DOORBELL_ALLOC_STRIDE = 0x1000;  // 4KB per queue

 private:
  long handleGetDeviceInfo(void* argp);
  long handleAllocBo(void* argp);
  long handleFreeBo(void* argp);
  long handleMapBo(void* argp);
  long handlePushbufferSubmitBatch(void* argp);
  long handleWaitFence(void* argp);
  long handleCreateQueue(void* argp);
  long handleDestroyQueue(void* argp);
  long handleMapQueueRing(void* argp);
  long handleQueryQueue(void* argp);
  long handleCreateVASpace(void* argp);
  long handleDestroyVASpace(void* argp);
  long handleRegisterGPU(void* argp);

  struct gpu_hal_ops* hal_;
  std::map<u32, BoInfo> bo_map_;
  std::map<std::string, u32> registered_kernels_;
  std::shared_ptr<HardwarePullerEmu> puller_;
  std::unordered_map<uint64_t, std::shared_ptr<GpuQueueEmu>> queues_;
  std::unordered_map<gpu_va_space_handle_t, VASpace> va_spaces_;
};
```

### 3.4 IOCTL 表派发

`ioctl()` 通过 `getIoctlTablePtr()` 返回的静态表分派。表的每个条目是 `(ioctl 编号, 名称, handler 指针)` 三元组。

```cpp
// gpgpu_device.cpp:99
const IoctlEntry* GpgpuDevice::getIoctlTablePtr() {
  static const IoctlEntry kTable[kNumIoctls] = {
    {GPU_IOCTL_GET_DEVICE_INFO,            "GET_DEVICE_INFO",
        &GpgpuDevice::handleGetDeviceInfo},
    {GPU_IOCTL_ALLOC_BO,                   "ALLOC_BO",
        &GpgpuDevice::handleAllocBo},
    {GPU_IOCTL_FREE_BO,                    "FREE_BO",
        &GpgpuDevice::handleFreeBo},
    {GPU_IOCTL_MAP_BO,                     "MAP_BO",
        &GpgpuDevice::handleMapBo},
    {GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH,    "PUSHBUFFER_SUBMIT_BATCH",
        &GpgpuDevice::handlePushbufferSubmitBatch},
    {GPU_IOCTL_WAIT_FENCE,                 "WAIT_FENCE",
        &GpgpuDevice::handleWaitFence},
    {GPU_IOCTL_CREATE_QUEUE,               "CREATE_QUEUE",
        &GpgpuDevice::handleCreateQueue},
    {GPU_IOCTL_DESTROY_QUEUE,              "DESTROY_QUEUE",
        &GpgpuDevice::handleDestroyQueue},
    {GPU_IOCTL_MAP_QUEUE_RING,             "MAP_QUEUE_RING",
        &GpgpuDevice::handleMapQueueRing},
    {GPU_IOCTL_QUERY_QUEUE,                "QUERY_QUEUE",
        &GpgpuDevice::handleQueryQueue},
    {GPU_IOCTL_CREATE_VA_SPACE,            "CREATE_VA_SPACE",
        &GpgpuDevice::handleCreateVASpace},
    {GPU_IOCTL_DESTROY_VA_SPACE,           "DESTROY_VA_SPACE",
        &GpgpuDevice::handleDestroyVASpace},
    {GPU_IOCTL_REGISTER_GPU,               "REGISTER_GPU",
        &GpgpuDevice::handleRegisterGPU},
  };
  return kTable;
}
```

`ioctl()` 主循环遍历表，匹配 `request` 字段后调用 `handler`。`kNumIoctls = 13` 是编译期常量，方便 CI 检测条目缺失。

### 3.5 DRM Driver 桩 (`gpu_drm_driver.cpp`)

`gpu_drm_driver.cpp` 是一个**独立的 DRM 风格 ioctl 描述符**，证明同一组 `GPU_IOCTL_*` 编号可以同时出现在自定义 IOCTL 表与 `drm_ioctl_desc[]` 表中。

```cpp
// gpu_drm_driver.cpp:25-30 — DRM 编号别名
#define DRM_IOCTL_GET_DEVICE_INFO         GPU_IOCTL_GET_DEVICE_INFO
#define DRM_IOCTL_ALLOC_BO                GPU_IOCTL_ALLOC_BO
#define DRM_IOCTL_FREE_BO                 GPU_IOCTL_FREE_BO
#define DRM_IOCTL_MAP_BO                  GPU_IOCTL_MAP_BO
#define DRM_IOCTL_PUSHBUFFER_SUBMIT_BATCH GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH
#define DRM_IOCTL_WAIT_FENCE              GPU_IOCTL_WAIT_FENCE
```

该文件目前为 Phase 1.4 的桥接桩，验证 IOCTL 编号在 DRM 框架下同样可用。后续阶段会用真正的 `drm_ioctl_desc[]` 表替换 `gpu_drm_driver.cpp` 的主体。

### 3.6 mmio 设备标识

```cpp
constexpr u32 VENDOR_SIMULATED      = 0x1000;
constexpr u32 DEVICE_SIMULATED_V1   = 0x1001;
```

仿真设备的 vendor/device ID 在 `handleGetDeviceInfo()` 中返回。TaskRunner 通过 `GPU_IOCTL_GET_DEVICE_INFO` (0x20) 查询时拿到这些值。

### 3.7 ADR 引用

| ADR | 关联内容 |
|-----|---------|
| ADR-015 | `GPU_IOCTL_*` 编号统一为 System C |
| ADR-018 | 驱动/仿真代码分离 (drv 不直接调 sim) |
| ADR-021 | Puller 接口，`setPuller()` 注入 |
| ADR-023 | 所有硬件访问走 `hal_` 指针 |
| ADR-024 | Queue / Ring Buffer / Doorbell mmap 模型 |

---

## §4 Layer 2 — 硬件抽象层 (`hal/`)

### 4.1 职责

`hal/` 是 `drv/` 与 `sim/` 之间的契约层。`drv/` 通过 11 个函数指针间接访问"硬件"。在用户态，这些指针指向 `hal_user.cpp` 的实现 (内部调用 sim/)；在内核态，将指向真实的 MMIO 读写 (`writel`, `readl`, `memcpy_fromio`)。

### 4.2 文件清单

```
plugins/gpu_driver/hal/
├── CMakeLists.txt
├── gpu_hal.h            (96 行)  — struct gpu_hal_ops 接口
├── hal_user.h           (47 行)  — hal_user_init / destroy 签名
├── hal_user.cpp         (159 行) — 真实用户态实现
├── hal_mock.h           (49 行)  — mock 状态结构
├── hal_mock.cpp         (117 行) — 单元测试 mock
└── test_hal.cpp         — HAL 独立测试
```

### 4.3 `struct gpu_hal_ops` 接口表 (11 个函数指针)

| # | 函数指针 | 签名 | 返回 | 用途 |
|---|---------|------|------|------|
| 1 | `register_read` | `int (*)(void*, u64 off, u64 *out)` | `int` (0/-E) | 读硬件寄存器 |
| 2 | `register_write` | `int (*)(void*, u64 off, u64 val)` | `int` (0/-E) | 写硬件寄存器 |
| 3 | `mem_read` | `int (*)(void*, u64 dev, void *hst, u64 sz)` | `int` (0/-E) | 设备内存 DMA 读 |
| 4 | `mem_write` | `int (*)(void*, u64 dev, const void *hst, u64 sz)` | `int` (0/-E) | 设备内存 DMA 写 |
| 5 | `mem_alloc` | `int (*)(void*, u64 sz, u64 *out_addr)` | `int` (0/-E) | 分配 VRAM |
| 6 | `mem_free` | `int (*)(void*, u64 dev_addr)` | `int` (0/-E) | 释放 VRAM |
| 7 | `fence_create` | `int (*)(void*, u64 *out_id)` | `int` (0/-E) | 创建 fence 句柄 |
| 8 | `fence_read` | `int (*)(void*, u64 id, u64 *out_val)` | `int` (0/-E) | 读 fence 状态 |
| 9 | `doorbell_ring` | `void (*)(void*, u32 queue_id)` | `void` | 触发 doorbell (弹射式) |
| 10 | `interrupt_raise` | `void (*)(void*, u32 vector)` | `void` | 触发 MSI-X 中断 (弹射式) |
| 11 | `time_wait` | `void (*)(void*, u64 us)` | `void` | 等待微秒 (弹射式) |

返回类型分层:

- `int` 接口: 可能失败 (越界、地址非法、内存不足)，调用方必须检查。
- `void` 接口: 弹射式操作，不可能失败，调用方无需检查。

头文件提供对应 `static inline` 包装函数，零开销简化调用:

```c
// gpu_hal.h:74-92
static inline int hal_fence_create(struct gpu_hal_ops *hal, u64 *out_id) {
  return hal->fence_create(hal->ctx, out_id);
}
static inline void hal_doorbell_ring(struct gpu_hal_ops *hal, u32 qid) {
  hal->doorbell_ring(hal->ctx, qid);
}
```

### 4.4 完整接口定义

```c
// hal/gpu_hal.h — 简化版
#pragma once
#include <stdint.h>

struct gpu_hal_ops {
  void *ctx;   /* HAL 实现上下文 (用户态 → sim state, 内核态 → hw regs base) */

  /* 可能失败 → 返回 Linux 错误码 (0=成功, 负值=错误) */
  int  (*register_read)(void *ctx, uint64_t offset, uint64_t *out_val);
  int  (*register_write)(void *ctx, uint64_t offset, uint64_t val);
  int  (*mem_read)(void *ctx, uint64_t dev_addr, void *host_buf, uint64_t size);
  int  (*mem_write)(void *ctx, uint64_t dev_addr, const void *host_buf, uint64_t size);
  int  (*mem_alloc)(void *ctx, uint64_t size, uint64_t *out_dev_addr);
  int  (*mem_free)(void *ctx, uint64_t dev_addr);
  int  (*fence_create)(void *ctx, uint64_t *out_fence_id);
  int  (*fence_read)(void *ctx, uint64_t fence_id, uint64_t *out_val);

  /* 弹射式操作 → void */
  void (*doorbell_ring)(void *ctx, uint32_t queue_id);
  void (*interrupt_raise)(void *ctx, uint32_t vector);
  void (*time_wait)(void *ctx, uint64_t us);
};
```

### 4.5 `hal_user` — 真实用户态实现

`hal_user_init()` 把 `gpu_hal_ops` 11 个指针挂到 `hal_user.cpp` 的实现。`hal_user_context` 是私有状态:

```cpp
// hal/hal_user.h:21-41
struct hal_user_context {
  uint64_t regs[HAL_REGS_COUNT];          // 256 个模拟寄存器
  std::mutex regs_lock;

  uint8_t *heap;                          // 设备内存堆 (256 MB)
  struct gpu_buddy buddy;                 // VRAM 分配器 (libgpu_core)
  std::mutex heap_lock;
  bool buddy_initialized;

  bool fence_signaled[HAL_MAX_FENCES];    // 128 个 fence 槽位
  uint64_t fence_counter;
  std::mutex fence_lock;

  uint64_t doorbell_count;
  uint64_t interrupt_count;

  /* Doorbell 回调 (由 sim/ 设置, HAL 在 doorbell_ring 时调用) */
  void (*doorbell_ring_cb)(void *cb_ctx, uint32_t queue_id);
  void *doorbell_ring_cb_ctx;
};

void hal_user_init(struct gpu_hal_ops *hal, struct hal_user_context *ctx);
void hal_user_destroy(struct hal_user_context *ctx);
int  hal_user_set_doorbell_cb(struct hal_user_context *ctx,
                              void (*cb)(void *, uint32_t), void *cb_ctx);
```

`HAL_HEAP_SIZE = 256 MB`，通过 `libgpu_core` 的 `gpu_buddy_init()` 初始化堆。`mem_alloc` / `mem_free` 走 `gpu_buddy_alloc` / `gpu_buddy_free`。

### 4.6 `hal_mock` — 单元测试 mock

`hal_mock_init()` 把 11 个指针指向纯 C 的 mock 函数。`hal_mock_state` 记录每次调用的计数、最近的入参/出参，方便测试断言:

```c
// hal/hal_mock.h:11-47 (节选)
struct hal_mock_state {
  int register_read_count;
  int register_write_count;
  /* ... 11 个 count 字段 ... */
  int register_read_result;       /* 注入返回值 */
  uint64_t register_read_out;      /* 注入 out 参数 */
  uint64_t last_reg_write_val;
  uint32_t last_doorbell_queue;
  /* ... */
};

void hal_mock_init(struct gpu_hal_ops *hal, struct hal_mock_state *state);
```

mock 模式让 `drv/` 单元测试可以完全脱离 `sim/` 隔离运行。

### 4.7 构造注入模式

`GpgpuDevice` 通过构造函数接收 `struct gpu_hal_ops *`:

```cpp
class GpgpuDevice {
  explicit GpgpuDevice(struct gpu_hal_ops *hal) : hal_(hal) {}
  /* 所有硬件访问通过 hal_ 指针 */
private:
  struct gpu_hal_ops *hal_;
};
```

初始化链 (`plugin.cpp`):

```cpp
// plugin.cpp:30-78
static int plugin_init_internal() {
  static HalHolder hal_holder;
  hal_user_init(&hal_holder.hal, &hal_holder.ctx);
  g_hal = &hal_holder;
  hal_holder.puller = std::make_shared<HardwarePullerEmu>(&hal_holder.hal,
                                                          &hal_holder.doorbell,
                                                          &hal_holder.scheduler);
  /* ... */
  auto device = std::make_shared<GpgpuDevice>(&hal_holder.hal);
  device->setPuller(hal_holder.puller);
  VFS::instance().register_device(dev);
  return 0;
}
```

测试用法:

```cpp
struct gpu_hal_ops g_mock_hal;
struct hal_mock_state g_mock_state;
hal_mock_init(&g_mock_hal, &g_mock_state);
GpgpuDevice test_device(&g_mock_hal);
```

### 4.8 ADR 引用

| ADR | 关联内容 |
|-----|---------|
| ADR-018 | 驱动/仿真分离要求 `drv/ → hal/ → sim/` 方向 |
| ADR-023 | HAL 11 接口正式定义 (含 8 个 `int` + 3 个 `void` 分层) |

---

## §5 Layer 3 — 公共契约层 (`shared/`)

### 5.1 职责

`shared/` 头文件是 **ABI 契约**。UsrLinuxEmu 定义它们，TaskRunner 通过符号链接 (`TaskRunner/UsrLinuxEmu/plugins/gpu_driver/shared → ../../UsrLinuxEmu/plugins/gpu_driver/shared`) 访问同一份头文件。

任何 IOCTL 编号或结构体字段的修改必须同时更新两项目，并标记为 breaking change。

### 5.2 文件清单

```
plugins/gpu_driver/shared/
├── gpu_ioctl.h    (274 行)  — IOCTL 编号 + 参数结构体
├── gpu_types.h    (68 行)   — 跨平台类型 + GPFIFO entry 格式
├── gpu_queue.h    (83 行)   — Ring Buffer 头部 + Queue 描述符
├── gpu_events.h   (150 行)  — MMU 事件类型 + 回调签名
└── gpu_regs.h     (109 行)  — 硬件寄存器偏移定义
```

### 5.3 `gpu_types.h` — 基础类型与 GPFIFO Entry

```c
// 跨平台类型
typedef uint8_t  u8;   typedef int8_t  s8;
typedef uint16_t u16;  typedef int16_t s16;
typedef uint32_t u32;  typedef int32_t s32;
typedef uint64_t u64;  typedef int64_t s64;
typedef u64 gpu_va_t;
typedef u64 gpu_pa_t;
typedef u32 gpu_stream_id_t;
typedef u32 gpu_channel_id_t;
typedef u64 gpu_va_space_handle_t;
typedef u64 gpu_queue_handle_t;

/* GPFIFO entry (NVIDIA 兼容, 支持 CPU/GPU task fork) */
struct gpu_gpfifo_entry {
  u32 valid      : 1;
  u32 priv       : 1;
  u32 method     : 12;   /* OP_LAUNCH_KERNEL=0x100, OP_LAUNCH_CPU_TASK=0x101 */
  u32 subchannel : 3;
  u32 _reserved  : 15;
  u64 payload[7];         /* 方法参数 (kernel args / CPU task descriptor) */
  u64 semaphore_va;
  u32 semaphore_value;
  u32 release    : 1;     /* 0=WAIT, 1=RELEASE */
  u32 _pad       : 31;
} __attribute__((packed));

/* Memory domain (AMD ROCm 兼容) */
#define GPU_MEM_DOMAIN_VRAM 0x1
#define GPU_MEM_DOMAIN_GTT  0x2
#define GPU_MEM_DOMAIN_CPU  0x4
```

### 5.4 `gpu_ioctl.h` — 完整 IOCTL 编号

ioctl 编码使用 Linux 标准宏:

```c
#define GPU_IOCTL_BASE 'G'

#define GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH _IOW (GPU_IOCTL_BASE, 0x01, struct gpu_pushbuffer_args)
#define GPU_IOCTL_REGISTER_MMU_EVENT_CB   _IOW (GPU_IOCTL_BASE, 0x02, struct gpu_mmu_event_cb_args)
#define GPU_IOCTL_REGISTER_FIRMWARE_CB    _IOW (GPU_IOCTL_BASE, 0x03, struct gpu_firmware_cb_args)
#define GPU_IOCTL_ALLOC_BO                _IOWR(GPU_IOCTL_BASE, 0x10, struct gpu_alloc_bo_args)
#define GPU_IOCTL_FREE_BO                 _IOW (GPU_IOCTL_BASE, 0x11, u32)
#define GPU_IOCTL_MAP_BO                  _IOWR(GPU_IOCTL_BASE, 0x12, struct gpu_map_bo_args)
#define GPU_IOCTL_WAIT_FENCE              _IOW (GPU_IOCTL_BASE, 0x13, struct gpu_wait_fence_args)
#define GPU_IOCTL_GET_DEVICE_INFO         _IOR (GPU_IOCTL_BASE, 0x20, struct gpu_device_info)
#define GPU_IOCTL_CREATE_VA_SPACE         _IOWR(GPU_IOCTL_BASE, 0x30, struct gpu_va_space_args)
#define GPU_IOCTL_DESTROY_VA_SPACE        _IOW (GPU_IOCTL_BASE, 0x31, gpu_va_space_handle_t)
#define GPU_IOCTL_REGISTER_GPU            _IOW (GPU_IOCTL_BASE, 0x32, struct gpu_register_gpu_args)
#define GPU_IOCTL_CREATE_QUEUE            _IOWR(GPU_IOCTL_BASE, 0x40, struct gpu_queue_args)
#define GPU_IOCTL_DESTROY_QUEUE           _IOW (GPU_IOCTL_BASE, 0x41, gpu_queue_handle_t)
#define GPU_IOCTL_MAP_QUEUE_RING          _IOWR(GPU_IOCTL_BASE, 0x42, struct gpu_queue_map_ring_args)
#define GPU_IOCTL_QUERY_QUEUE             _IOWR(GPU_IOCTL_BASE, 0x43, struct gpu_queue_info_args)

/* Queue 类型 */
#define GPU_QUEUE_COMPUTE  0x0
#define GPU_QUEUE_COPY     0x1
#define GPU_QUEUE_GRAPHICS 0x2

/* BO 标志 */
#define GPU_BO_DEVICE_LOCAL 0x1
#define GPU_BO_HOST_VISIBLE 0x2
#define GPU_BO_CXL_SHARED   0x4
```

### 5.5 `gpu_queue.h` — Ring Buffer 头部

```c
struct gpu_ring_header {
  volatile uint32_t write_idx;    /* Producer index (用户态写入) */
  volatile uint32_t read_idx;     /* Consumer index (Puller 读取) */
  uint32_t capacity;              /* 容量 (entry 数, max 1024) */
  uint32_t flags;
  uint64_t fence_value;           /* 完成 fence (Puller 写入) */
  uint8_t  reserved[32];          /* 缓存行对齐 */
};

#define GPU_MAX_RING_ENTRIES 1024

/* Queue 描述符参数 */
struct gpu_create_queue_args {
  uint32_t queue_type;         /* GPU_QUEUE_COMPUTE / COPY */
  uint32_t priority;           /* 0-100 */
  uint32_t ring_size;          /* Ring Buffer 容量 (entry 数) */
  uint32_t reserved;
  uint64_t queue_handle;       /* OUT: 队列句柄 */
  uint64_t doorbell_pgoff;     /* OUT: Doorbell mmap page offset */
};
```

### 5.6 `gpu_events.h` — MMU 事件模型

事件语义与 Linux 内核 `mmu_interval_notifier` 保持一致，零改动迁移时事件流完全相同:

```c
enum gpu_mmu_event_type {
  GPU_MMU_EVENT_PAGE_INVALIDATE  = 1,  /* mmu_notifier_invalidate_range_start */
  GPU_MMU_EVENT_PAGE_REMAP       = 2,  /* mmu_notifier_invalidate_range_end + PTE 更新 */
  GPU_MMU_EVENT_TLB_FLUSH_RANGE  = 3,  /* 权限变更或解映射 */
  GPU_MMU_EVENT_CACHE_FLUSH      = 4,  /* CXL.cache 缓存行刷新 */
};

struct gpu_mmu_event_context {
  u64 va_start;
  u64 va_end;
  u64 old_pa;          /* 仅 PAGE_REMAP 有效 */
  u64 new_pa;          /* 仅 PAGE_REMAP 有效 */
  u64 cache_line_mask; /* 仅 CACHE_FLUSH 有效, 64 位位图 */
};

typedef void (*gpu_mmu_event_cb_fn)(enum gpu_mmu_event_type type,
                                    const struct gpu_mmu_event_context *ctx,
                                    void *user_data);
```

### 5.7 `gpu_regs.h` — 寄存器偏移

寄存器布局按子系统分组:

| 子系统 | 偏移范围 | 关键寄存器 |
|--------|---------|-----------|
| GPFIFO / Channel | 0x0000-0x0014 | `GPU_REG_GPFIFO_PUT`, `GPU_REG_GPFIFO_GET`, `GPU_REG_DOORBELL` |
| MMU | 0x1000-0x1018 | `GPU_REG_MMU_PT_BASE_*`, `GPU_REG_TLB_INVAL_*` |
| PCIe / DMA | 0x2000-0x2018 | `GPU_REG_DMA_*`, `GPU_DMA_CTRL_START` |
| Interrupt / MSI-X | 0x3000-0x3008 | `GPU_REG_IRQ_STATUS`, `GPU_IRQ_GPFIFO_COMPLETE` |
| CXL.cache | 0x4000-0x4004 | `GPU_REG_CXL_CTRL`, `GPU_REG_CXL_SF_STATUS` |

Doorbell 写操作映射到 `GPU_REG_DOORBELL` (0x0014)。

### 5.8 ADR 引用

| ADR | 关联内容 |
|-----|---------|
| ADR-015 | IOCTL 编号统一 (System B → System C) |
| ADR-017 | GPFIFO / Queue 抽象层 |
| ADR-024 | Ring Buffer 共享内存 + Doorbell mmap 路径 |

---

## §6 Layer 4 — 仿真层 (`sim/`)

### 6.1 职责

`sim/` 提供用户态硬件行为仿真。包括 Puller 状态机、Doorbell 寄存器、Ring Buffer 消费者、Scheduler、Translator。它**不**移植到内核，移植时由真实硬件替换。

### 6.2 文件清单

```
plugins/gpu_driver/sim/
├── CMakeLists.txt
├── buddy_allocator.cpp    (44 行)   — C++ 包装 libgpu_core
├── fence_sim.cpp          (33 行)   — Fence 仿真
├── doorbell_emu.cpp                  — Doorbell 寄存器实现
├── hardware_puller_emu.cpp (265 行) — Puller 状态机
├── gpu_queue_emu.h        (113 行)
├── gpu_queue_emu.cpp      (79 行)   — Ring Buffer 消费者
├── hardware/
│   ├── doorbell_emu.h      (52 行)
│   └── hardware_puller_emu.h (119 行)
└── scheduler/
    ├── global_scheduler.h  (56 行)
    ├── global_scheduler.cpp (54 行)
    └── translator/
        ├── gpfifo_translator.h
        └── gpfifo_translator.cpp
```

### 6.3 `HardwarePullerEmu` — Puller 状态机 (ADR-021)

```cpp
class HardwarePullerEmu {
 public:
  enum class State {
    IDLE, FETCH, DECODE, SCHEDULE, DISPATCH, SEMAPHORE, COMPLETE
  };

  HardwarePullerEmu(struct gpu_hal_ops* hal,
                    DoorbellEmu* doorbell,
                    GlobalScheduler* scheduler);
  ~HardwarePullerEmu();

  void start();   /* 启动后台线程 runLoop() */
  void stop();

  /* ioctl 路径: 提交 GPFIFO 批处理 */
  void submitBatch(u64 gpfifo_gpu_addr, u32 entry_count);

  /* Doorbell 回调 */
  void onDoorbell(u32 queue_id);

  /* Phase 2.5: 用户态队列 (mmap 路径) */
  void registerQueue(GpuQueueEmu* queue);
  void unregisterQueue(uint32_t queue_id);

 private:
  bool fetchEntry(gpu_gpfifo_entry* out);          /* ioctl 路径 */
  bool fetchFromQueue(uint32_t qid, gpu_gpfifo_entry* out); /* Ring Buffer 路径 */
  bool scanQueues(uint32_t* out_qid, gpu_gpfifo_entry* out);
  bool waitSemaphore();
  void releaseSemaphore();
  void handleComplete();
  void runLoop();    /* 状态机主循环 (后台线程) */
  void transitionTo(State next);

  struct gpu_hal_ops* hal_;
  DoorbellEmu* doorbell_;
  GlobalScheduler* scheduler_;
  State state_;
  std::thread thread_;
  std::mutex mutex_;
  std::condition_variable cv_;
  std::atomic<bool> running_{false};
  std::map<uint32_t, GpuQueueEmu*> active_queues_;
  gpu_gpfifo_entry current_entry_;
  std::atomic<int> interrupt_count_{0};
  std::atomic<bool> doorbell_pending_{false};
};
```

状态转换:

```
         ┌───────────┐
         │  IDLE     │ ← 等待 doorbell write
         └─────┬─────┘
               │ (hal_doorbell_read 检测到写入)
               ▼
         ┌───────────┐
         │ FETCH     │ ← HAL.mem_read 或从 Ring Buffer 取
         └─────┬─────┘
               │ (entry 取出)
               ▼
         ┌───────────┐
         │ DECODE    │ ← 解析 method + payload + semaphore
         └─────┬─────┘
               │
          ┌────┴────┐
          ▼         ▼
    ┌─────────┐ ┌──────────┐
    │ SCHEDULE │ │ SEMAPHORE│ ← GPU_OP_FENCE: WAIT/RELEASE
    └────┬────┘ └────┬─────┘
         │           │ (sem wait 完成)
         ▼           │
    ┌──────────┐     │
    │ DISPATCH │     │ ← 路由到正确的引擎
    └────┬─────┘     │
         │           │
         ▼           ▼
    ┌───────────┐
    │ COMPLETE  │ ← release==1 → hal_interrupt_raise
    └─────┬─────┘
          │
          ▼
    ┌───────────┐
    │  NEXT     │ ← 继续下一个 entry
    └─────┬─────┘
          │ (所有 entry 处理完)
          ▼
    ┌───────────┐
    │  IDLE     │ ← 等待下一个 doorbell
    └───────────┘
```

ADR-021 v3 修订 (2026-05-12): FETCH 阶段增加 `FETCH_SOURCE_SELECT`，根据是否绑定 `GpuQueueEmu` 选择 IOCTL_DMA 或 SHARED_RING 路径。

### 6.4 `DoorbellEmu` — Doorbell 寄存器仿真

```cpp
class DoorbellEmu {
 public:
  static constexpr u32 MAX_QUEUES = 1024;

  using DoorbellCallback = std::function<void(u32 queue_id)>;

  DoorbellEmu() { counts_.fill(0); pending_.fill(false); }

  void write(u32 queue_id);        /* 写 doorbell 寄存器 */
  bool poll(u32 queue_id) const;   /* Puller 轮询检测 */
  void acknowledge(u32 queue_id);  /* 清除 pending */
  void setCallback(DoorbellCallback cb);
  u64 getRingCount(u32 queue_id) const;

 private:
  std::array<u64, MAX_QUEUES> counts_{};
  std::array<bool, MAX_QUEUES> pending_{};
  DoorbellCallback callback_;
};
```

Doorbell 是 1024 个槽位的简单数组，每个槽位有 `pending` 标志与 `counts_` 计数器。`write()` 调用 `callback_` 通知 Puller (`HardwarePullerEmu::onDoorbell`)。

### 6.5 `GlobalScheduler` — 全局调度器

```cpp
enum class EngineType { COMPUTE, COPY, FIRMWARE };

struct WorkItem {
  gpu_gpfifo_entry entry;
  EngineType engine;
  void* user_data;
};

class GlobalScheduler {
 public:
  using EngineDispatchFn = std::function<void(const gpu_gpfifo_entry&, EngineType)>;

  void setDispatchCallback(EngineDispatchFn fn);
  void setLaunchCallback(GpfifoToLaunchParamsTranslator::LaunchParamsCallback cb);
  void registerKernel(uint32_t kernel_idx, const char* kernel_name);

  void enqueue(const gpu_gpfifo_entry& entry, EngineType engine);
  bool dequeue(WorkItem* out_item);
  size_t queueSize() const;
  void flush();

  EngineType selectEngine(const gpu_gpfifo_entry& entry);

 private:
  GpfifoToLaunchParamsTranslator translator_;
  EngineDispatchFn dispatch_fn_;
  std::queue<WorkItem> queue_;
  std::mutex mutex_;
  std::atomic<uint64_t> submission_id_{0};
};
```

调度流程:

```
Puller (DISPATCH 状态)
  ↓ enqueue(entry, selectEngine(entry))
GlobalScheduler::enqueue
  ↓
  ├─→ GpfifoToLaunchParamsTranslator::translate()
  │     └─→ LaunchParamsCallback (kernel_name, grid, block)
  └─→ EngineDispatchFn (按 EngineType 路由)
```

### 6.6 `GpuQueueEmu` — Ring Buffer 消费者 (ADR-024)

```cpp
class GpuQueueEmu {
 public:
  using DoorbellCallback = std::function<void(uint32_t queue_id)>;

  GpuQueueEmu(uint32_t queue_id, uint32_t queue_type,
              uint32_t priority, uint32_t ring_size);

  uint32_t queueId() const;
  uint32_t doorbellId() const { return queue_id_; }
  uint32_t priority() const;
  uint32_t ringSize() const;

  void setDoorbellCallback(DoorbellCallback cb);

  /* 消费者端: Puller 调用 */
  bool dequeue(gpu_gpfifo_entry* out_entry);
  bool hasPending() const;
  uint32_t pendingCount() const;

  /* 内存管理 */
  int attachSharedMemory(void* shm_addr, size_t size);
  struct gpu_ring_header* ringHeader() const { return ring_header_; }

  /* Doorbell 触发 (由 mmap 写操作调用) */
  void ringDoorbell() { if (doorbell_cb_) doorbell_cb_(queue_id_); }

 private:
  uint32_t queue_id_, queue_type_, priority_, ring_size_;
  struct gpu_ring_header* ring_header_ = nullptr;
  DoorbellCallback doorbell_cb_;
  std::mutex mutex_;
};
```

`GpuQueueEmu` 是 Ring Buffer 的消费者端。用户态通过 mmap 把 `gpu_ring_header + gpu_gpfifo_entry[]` 暴露给 TaskRunner，TaskRunner 写 `write_idx` + 写 `*doorbell_ptr` 即可触发 Puller 消费。

### 6.7 `buddy_allocator.cpp` / `fence_sim.cpp` — 辅助仿真

- `buddy_allocator.cpp` (44 行): C++ 包装 `libgpu_core` 的 `gpu_buddy_init` / `gpu_buddy_alloc` / `gpu_buddy_free`，供 sim 内部使用。
- `fence_sim.cpp` (33 行): 维护 fence ID → 状态映射，提供 `fence_signal` / `fence_wait` 接口。
- `gpfifo_translator.{h,cpp}`: `GpfifoToLaunchParamsTranslator` 把 GPFIFO entry 翻译成 kernel launch 参数，触发 `LaunchParamsCallback`。

### 6.8 ADR 引用

| ADR | 关联内容 |
|-----|---------|
| ADR-018 | 驱动/仿真分离 (`sim/` 不依赖 `drv/`) |
| ADR-021 | HardwarePullerEmu 状态机详细定义 |
| ADR-024 | GpuQueueEmu + Doorbell mmap 路径 |

---

## §7 算法核心 (`libgpu_core/`)

### 7.1 职责

`libgpu_core/` 是**纯 C 算法库**，零外部依赖。提取它的目的是让 buddy allocator 这种纯地址运算算法可以:

1. 直接复制到任何 C 编译环境 (用户态测试、内核模块、固件)。
2. 独立单元测试，不依赖驱动框架。
3. 内核开发者可以独立审查和修改算法代码。

### 7.2 文件清单

```
libgpu_core/
├── CMakeLists.txt
├── include/gpu_buddy.h   (113 行) — C 接口
├── src/buddy.c           (371 行) — 实现
└── test/                  — 独立 C 单元测试
```

### 7.3 零依赖约束

| 允许 | 禁止 |
|------|------|
| 纯 C (C99/C11) | `malloc` / `free` |
| 传入的缓冲区操作 | 系统调用 |
| 位运算 | STL / 容器 |
| 指针运算 | 锁 / 原子操作 |
| `assert()` | 日志输出 |
| `memcpy` / `memset` | `errno` |
| `bool` / `uint32_t` 等基础类型 | RTTI / 异常 |

无锁设计: 调用者负责外部同步。`buddy.c` 只操作自身数据结构。

### 7.4 `gpu_buddy` API

```c
/* 初始化: 传入调用者分配的内存区域, 库内不调用 malloc */
void gpu_buddy_init(struct gpu_buddy *buddy, uint64_t base, uint64_t size);

/* 分配/释放 */
int  gpu_buddy_alloc(struct gpu_buddy *buddy, uint64_t size, uint64_t *out_addr);
int  gpu_buddy_free(struct gpu_buddy *buddy, uint64_t addr);

/* 查询 */
void gpu_buddy_reset(struct gpu_buddy *buddy);
uint64_t gpu_buddy_free_size(const struct gpu_buddy *buddy);
int gpu_buddy_allocated_count(const struct gpu_buddy *buddy);
```

### 7.5 `gpu_buddy` 数据结构

```c
#define GPU_BUDDY_MIN_BLOCK_SHIFT 12  /* 4KB */
#define GPU_BUDDY_MIN_BLOCK_SIZE  (1ULL << GPU_BUDDY_MIN_BLOCK_SHIFT)
#define GPU_BUDDY_MAX_ORDER       21  /* 2^21 * 4KB = 8GB */
#define GPU_BUDDY_MAX_RECORDS     4096 /* 最大并发分配数 */

struct gpu_buddy {
  uint64_t base_addr;
  uint64_t pool_size;
  int max_order;

  /* 空闲链表 (按 order 索引) */
  struct gpu_buddy_block *free_lists[GPU_BUDDY_MAX_ORDER + 1];

  /* 空闲链表节点池 (固定大小, 无动态分配) */
  struct gpu_buddy_block block_pool[GPU_BUDDY_MAX_RECORDS + GPU_BUDDY_MAX_ORDER + 1];
  int block_pool_used;

  /* 已分配块追踪 */
  struct gpu_buddy_record records[GPU_BUDDY_MAX_RECORDS];
  int record_count;
};
```

`struct gpu_buddy` 由调用者分配并初始化。库内部**完全不自加锁**，调用者负责外部同步 (例如 `hal_user_context::heap_lock`)。

### 7.6 移植说明

把 `libgpu_core/` 复制到 `drivers/gpu/your_gpu/libgpu_core/`，加入该驱动的 `Makefile`:

```makefile
obj-y += libgpu_core/src/buddy.o
ccflags-y += -I$(src)/drivers/gpu/your_gpu/libgpu_core/include
```

不需要修改任何代码。`gpu_buddy.h` 中的 `u8`/`u32`/`u64` 类型与 `linux/types.h` 完全兼容。

### 7.7 ADR 引用

| ADR | 关联内容 |
|-----|---------|
| ADR-004 | Buddy allocator 作为 VRAM 分配算法 |
| ADR-015 | libgpu_core 可作为"未来真实驱动的可复用组件" |
| ADR-020 | libgpu_core 提取的纯度约束 (零依赖, 纯 C) |

---

## §8 IOCTL 体系 (System C)

### 8.1 历史

| 系统 | 状态 | 编号 | 头文件 |
|------|------|------|--------|
| System A (旧 CUDA 前缀) | 已删除 | — | — |
| System B (旧 GPGPU 前缀, 见 `archive/system_b_drivers/gpu/`) | 已归档 | 0x01-0x06 | `ioctl_gpgpu.h` (历史) |
| **System C (`GPU_IOCTL_*`)** | **当前使用** | **0x01-0x43** | `plugins/gpu_driver/shared/gpu_ioctl.h` |

System C 在 ADR-015 中确立，2026-04 引入。所有新代码必须使用 System C 编号。

### 8.2 完整编号表

`GpgpuDevice` 实际分派的 13 个 IOCTL (`kTable[kNumIoctls=13]`):

| # | 编号 | 宏 | 方向 | 参数结构 | 层级 | 阶段 |
|---|------|----|------|---------|------|------|
| 1 | 0x20 | `GPU_IOCTL_GET_DEVICE_INFO` | `_IOR` | `struct gpu_device_info` | 设备信息 | Phase 1 |
| 2 | 0x10 | `GPU_IOCTL_ALLOC_BO` | `_IOWR` | `struct gpu_alloc_bo_args` | 内存 | Phase 1 |
| 3 | 0x11 | `GPU_IOCTL_FREE_BO` | `_IOW` | `u32` (handle) | 内存 | Phase 1 |
| 4 | 0x12 | `GPU_IOCTL_MAP_BO` | `_IOWR` | `struct gpu_map_bo_args` | 内存 | Phase 1 |
| 5 | 0x01 | `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH` | `_IOW` | `struct gpu_pushbuffer_args` | 提交 | Phase 1 |
| 6 | 0x13 | `GPU_IOCTL_WAIT_FENCE` | `_IOW` | `struct gpu_wait_fence_args` | 同步 | Phase 1 |
| 7 | 0x40 | `GPU_IOCTL_CREATE_QUEUE` | `_IOWR` | `struct gpu_queue_args` | 队列 | Phase 2 |
| 8 | 0x41 | `GPU_IOCTL_DESTROY_QUEUE` | `_IOW` | `gpu_queue_handle_t` | 队列 | Phase 2 |
| 9 | 0x42 | `GPU_IOCTL_MAP_QUEUE_RING` | `_IOWR` | `struct gpu_queue_map_ring_args` | 队列 | Phase 2 |
| 10 | 0x43 | `GPU_IOCTL_QUERY_QUEUE` | `_IOWR` | `struct gpu_queue_info_args` | 队列 | Phase 2 |
| 11 | 0x30 | `GPU_IOCTL_CREATE_VA_SPACE` | `_IOWR` | `struct gpu_va_space_args` | VA Space | Phase 2 |
| 12 | 0x31 | `GPU_IOCTL_DESTROY_VA_SPACE` | `_IOW` | `gpu_va_space_handle_t` | VA Space | Phase 2 |
| 13 | 0x32 | `GPU_IOCTL_REGISTER_GPU` | `_IOW` | `struct gpu_register_gpu_args` | VA Space | Phase 2 |

补充 IOCTL (不在 kTable 中, 通过回调路径注册):

| 编号 | 宏 | 方向 | 用途 |
|------|----|------|------|
| 0x02 | `GPU_IOCTL_REGISTER_MMU_EVENT_CB` | `_IOW` | 注册 MMU 页迁移事件回调 |
| 0x03 | `GPU_IOCTL_REGISTER_FIRMWARE_CB` | `_IOW` | 注册固件回调 (OP_LAUNCH_CPU_TASK 路径) |

### 8.3 关键结构体示例

```c
/* GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH 参数 */
struct gpu_pushbuffer_args {
  u32 stream_id;
  u64 entries_addr;     /* 用户态 GPFIFO entries 数组地址 */
  u32 count;
  u32 flags;
  u64 fence_id;         /* OUT: 关联的 fence */
};

/* GPU_IOCTL_ALLOC_BO 参数 */
struct gpu_alloc_bo_args {
  u64 size;             /* IN */
  u32 domain;           /* IN: GPU_MEM_DOMAIN_VRAM/GTT/CPU */
  u32 flags;            /* IN: GPU_BO_DEVICE_LOCAL/HOST_VISIBLE/CXL_SHARED */
  u32 handle;           /* OUT: GEM handle */
  u64 gpu_va;           /* OUT: GPU 虚拟地址 */
};

/* GPU_IOCTL_CREATE_QUEUE 参数 */
struct gpu_queue_args {
  gpu_va_space_handle_t va_space_handle;  /* IN: 所属 VA Space */
  u32 queue_type;       /* IN: COMPUTE=0, COPY=1, GRAPHICS=2 */
  u32 priority;         /* IN: 0-100 */
  u64 ring_buffer_size; /* IN: entry 数 (max 1024) */
  gpu_queue_handle_t queue_handle;        /* OUT: 队列句柄 */
  u64 doorbell_pgoff;   /* OUT: Doorbell mmap page offset */
};

/* GPU_IOCTL_CREATE_VA_SPACE 参数 */
struct gpu_va_space_args {
  u32 page_size;        /* IN: 0=4KB, 1=64KB */
  u32 flags;            /* IN: VA Space 标志 */
  gpu_va_space_handle_t va_space_handle;  /* OUT: 句柄 */
};
```

### 8.4 与真实驱动的兼容性

- IOCTL 编号 (`0x01` ~ `0x43`) 在仿真器与未来真实 `.ko` 中**完全相同**。
- 所有结构体使用 `u32` / `u64` 固定宽度类型，无 padding 歧义。
- `gpu_gpfifo_entry` 使用 `__attribute__((packed))`，确保跨平台布局一致。

零改动迁移: TaskRunner 在用户态调用仿真器，把仿真器换成真实 `.ko` 后不需要重新编译。

---

## §9 关键数据流

### 9.1 Pushbuffer 提交完整流 (Phase 2)

```
User ioctl(fd, GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, args)
   ↓
GpgpuDevice::ioctl() → getIoctlTablePtr() table dispatch
   ↓
handlePushbufferSubmitBatch(args)
   ├─→ validate VA Space exists       (Phase 2 强制)
   ├─→ validate Queue belongs to VA Space
   ├─→ HAL.fence_create(&fence_id)    ← 异步跟踪
   ├─→ HardwarePullerEmu::submitBatch(gpfifo_addr, count)
   │     └─→ FSM: IDLE → FETCH (HAL.mem_read)
   │             → DECODE → SCHEDULE → DISPATCH → COMPLETE
   ├─→ GlobalScheduler::enqueue(entry, selectEngine(entry))
   │     └─→ GpfifoToLaunchParamsTranslator::translate()
   │         └─→ LaunchParamsCallback (kernel_name, grid, block)
   └─→ HAL.doorbell_ring(queue_id)
   ↓
返回 args.fence_id 给用户
```

### 9.2 Ring Buffer 快速路径 (ADR-024)

```
TaskRunner
   ↓ 写 Ring Buffer (mmap 共享内存, 零 syscall)
   ring_ptr[write_idx++] = gpfifo_entry
   __sync_synchronize()                 (内存屏障)
   ↓ 写 Doorbell (mmap'd BAR)
   *doorbell_ptr = queue_id
   ↓
DoorbellEmu::write(queue_id)           (本地函数, 弹射式)
   ↓ callback
HardwarePullerEmu::onDoorbell(queue_id)
   ↓
FSM: IDLE → FETCH_SOURCE_SELECT
   ├─→ SHARED_RING: GpuQueueEmu::dequeue(entry)
   └─→ IOCTL_DMA:  HAL.mem_read(dev_addr, ...)
   ↓
后续 DECODE / SCHEDULE / DISPATCH / COMPLETE 与 §9.1 相同
   ↓
完成后: Puller 写 gpu_ring_header.fence_value
   ↓
TaskRunner 轮询 fence_value, 收到完成信号
```

### 9.3 VA Space 创建与 Queue 附加

```
ioctl(fd, GPU_IOCTL_CREATE_VA_SPACE, &va_args)
   ↓
GpgpuDevice::handleCreateVASpace
   ├─→ page_size: 0=4KB / 1=64KB
   ├─→ 分配 next_va_space_handle_++
   └─→ va_spaces_[handle] = VASpace{...}
   ↓
ioctl(fd, GPU_IOCTL_CREATE_QUEUE, &q_args)  // q_args.va_space_handle
   ↓
GpgpuDevice::handleCreateQueue
   ├─→ validate vaSpaceExists(q_args.va_space_handle)
   ├─→ HAL.mem_alloc(queue_state_size) → 内部状态
   ├─→ new GpuQueueEmu(queue_id, type, priority, ring_size)
   ├─→ puller->registerQueue(queue)   // 绑定到 Puller
   ├─→ 分配 doorbell_pgoff = DOORBELL_ALLOC_BASE + h * DOORBELL_ALLOC_STRIDE
   ├─→ va_spaces_[va].attached_queues.push_back(queue_handle)
   └─→ queues_[queue_handle] = queue
   ↓
返回 q_args.queue_handle + q_args.doorbell_pgoff
```

---

## §10 数据模型

### 10.1 实体关系

```
VASpace  (gpu_va_space_handle_t, u64)
   ├─ page_size (0=4KB, 1=64KB)
   ├─ flags
   └─ attached_queues: [gpu_queue_handle_t]
         ↓ 1:N
Queue  (gpu_queue_handle_t, u64)
   ├─ queue_type (COMPUTE=0 / COPY=1 / GRAPHICS=2)
   ├─ priority (0-100)
   ├─ ring_size (entry 数, max 1024)
   ├─ ring_buffer: gpu_ring_header (shm-backed)
   │     ├─ write_idx / read_idx (volatile u32)
   │     ├─ capacity (max 1024)
   │     ├─ flags
   │     ├─ fence_value
   │     └─ reserved[32]
   ├─ entries[]: gpu_gpfifo_entry[]  (Ring Buffer 主体)
   └─ doorbell: mmap offset 0x10000 + h * 0x1000  (4KB per queue)
```

### 10.2 `gpu_ring_header` 布局

```
┌──────────────────────────────────────────┐
│ gpu_ring_header (struct, 缓存行对齐)      │
│   write_idx    : u32 volatile             │
│   read_idx     : u32 volatile             │
│   capacity     : u32                      │
│   flags        : u32                      │
│   fence_value  : u64                      │
│   reserved[32] : u8[32]                   │
├──────────────────────────────────────────┤
│ entries[0]  : gpu_gpfifo_entry           │
│ entries[1]  : gpu_gpfifo_entry           │
│   ...                                     │
│ entries[capacity-1] : gpu_gpfifo_entry   │
└──────────────────────────────────────────┘
```

生产者 (用户态) 写 `write_idx` 推进，消费者 (`GpuQueueEmu::dequeue`) 读 `read_idx` 推进。两者通过 volatile 字段同步。

### 10.3 `gpu_gpfifo_entry` 字段

```
┌──────────────────────────────────────────┐
│  32-bit 控制字                              │
│    valid       : 1   (entry 是否有效)      │
│    priv        : 1   (特权 entry)          │
│    method      : 12  (op 编码)            │
│    subchannel  : 3                        │
│    _reserved   : 15                       │
├──────────────────────────────────────────┤
│  payload[7]  : u64[7]   (op 参数)          │
├──────────────────────────────────────────┤
│  semaphore_va  : u64    (信号量 GPU VA)    │
│  semaphore_val : u32    (期望/写入值)      │
│  release       : 1     (0=WAIT, 1=RELEASE)│
│  _pad          : 31                       │
└──────────────────────────────────────────┘
```

`semaphore_va` + `semaphore_value` + `release` 三字段在 Puller 的 SEMAPHORE 状态中处理 (ADR-021 决策 3)。

### 10.4 Doorbell 寻址

```
mmap offset = DOORBELL_ALLOC_BASE + queue_handle * DOORBELL_ALLOC_STRIDE
            = 0x10000 + h * 0x1000
```

每个 Queue 占用 4KB mmap 区域。`MAX_QUEUES = 1024` (DoorbellEmu 常量)。`gpu_drm_driver.cpp` 中还有 `DOORBELL_MMAP_OFFSET = 0x20000` 作为全局 doorbell 区域。

---

## §11 插件加载与生命周期

### 11.1 加载模式

GPU 驱动通过 `dlopen` + `dlsym(handle, "mod")` 加载。这是 ADR-018 决策 2 的结果: 不再使用 `PluginManager` 模式 (Phase 1 旧设计)。

```cpp
// plugin.cpp:93-98 — 导出的 module 符号
module mod = {
    .name   = "gpu_driver",
    .depends = nullptr,
    .init   = plugin_init_internal,
    .exit   = plugin_fini_internal,
};
```

`ModuleLoader::load_plugins("plugins")` 扫描 `plugins/*.so`，对每个文件执行 `dlopen` + `dlsym("mod")`，调用 `init()` 入口。

### 11.2 初始化链

```cpp
// plugin.cpp:30-78 (节选)
static int plugin_init_internal() {
  static HalHolder hal_holder;
  hal_user_init(&hal_holder.hal, &hal_holder.ctx);
  g_hal = &hal_holder;

  hal_holder.puller = std::make_shared<HardwarePullerEmu>(
      &hal_holder.hal, &hal_holder.doorbell, &hal_holder.scheduler);

  hal_holder.scheduler.registerKernel(0, "simple_kernel");
  hal_holder.scheduler.registerKernel(1, "matmul_kernel");

  hal_holder.scheduler.setLaunchCallback(
      [](const char* name, u32 gx, u32 gy, u32 gz, u32 bx, u32 by, u32 bz, u32 sm) {
        std::cout << "[GpuPlugin] LaunchCallback: kernel=" << name
                  << " grid=(" << gx << "," << gy << "," << gz << ")"
                  << " block=(" << bx << "," << by << "," << bz << ")" << std::endl;
      });

  int ret = hal_user_set_doorbell_cb(&hal_holder.ctx,
      [](void* cb_ctx, u32 queue_id) {
        auto* dh = static_cast<HalHolder*>(cb_ctx);
        dh->doorbell.write(queue_id);
      }, &hal_holder);

  hal_holder.puller->start();

  auto device = std::make_shared<GpgpuDevice>(&hal_holder.hal);
  device->setPuller(hal_holder.puller);

  VFS::instance().register_device(
      std::make_shared<Device>(device->name, 0, device, nullptr));

  return 0;
}
```

初始化顺序:

```
1. hal_user_init()           // 注入 hal_user 实现到 gpu_hal_ops
2. new HardwarePullerEmu     // 创建 Puller, 引用 DoorbellEmu + GlobalScheduler
3. scheduler.registerKernel  // 注册 kernel 名称表
4. scheduler.setLaunchCallback // 设置 translator 的 launch 回调
5. hal_user_set_doorbell_cb  // HAL 的 doorbell_ring → DoorbellEmu::write
6. puller->start()           // 启动 Puller 后台线程
7. new GpgpuDevice(hal)      // 构造设备, 持有 hal_ 指针
8. device->setPuller(puller) // 注入 Puller 引用
9. VFS.register_device       // 注册 /dev/gpgpu0
```

### 11.3 卸载链

```cpp
// plugin.cpp:80-91
static void plugin_fini_internal() {
  if (g_hal) {
    if (g_hal->puller) g_hal->puller->stop();
    g_hal->puller.reset();
    hal_user_destroy(&g_hal->ctx);
    g_hal = nullptr;
  }
  VFS::instance().unregister_device("gpgpu0");
}
```

卸载顺序与初始化相反。先停 Puller 线程，再销毁 HAL 上下文，最后从 VFS 注销设备。

### 11.4 CMake 集成

```cmake
# plugins/gpu_driver/CMakeLists.txt
add_library(gpu_driver_plugin MODULE plugin.cpp)
target_include_directories(gpu_driver_plugin PRIVATE
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/shared
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/hal
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/sim
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/sim/hardware
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/sim/scheduler
    ${PROJECT_SOURCE_DIR}/libgpu_core/include
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/kernel
)

add_subdirectory(drv)
add_subdirectory(hal)
add_subdirectory(sim)

target_link_libraries(gpu_driver_plugin PRIVATE
    kernel gpu_hal gpu_core gpu_drv gpu_sim)

set_target_properties(gpu_driver_plugin PROPERTIES
    PREFIX "" SUFFIX ".so" POSITION_INDEPENDENT_CODE ON)

# Post-build: 复制 .so 到 plugins/ 目录
add_custom_command(TARGET gpu_driver_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:gpu_driver_plugin>
        ${PROJECT_SOURCE_DIR}/plugins/plugin_gpu_driver.so)
```

`gpu_driver_plugin` 链入 `kernel` (SHARED) 与 `gpu_hal` / `gpu_core` / `gpu_drv` / `gpu_sim` 四个内部子库。`kernel` 必须是 SHARED，否则 VFS 单例被割裂 (Issue #11)。

---

## §12 ADR 索引

本节列出与 GPU 驱动直接相关的 ADR。完整列表见 [docs/00_adr/README.md](../00_adr/README.md)。

### 12.1 GPU 驱动核心 ADR

| ADR | 标题 | 状态 | 对应本文档章节 |
|-----|------|------|----------------|
| [ADR-015](../00_adr/adr-015-gpu-ioctl-unification.md) | GPU IOCTL 接口统一 (System C) | 已接受 | §5, §8 |
| [ADR-016](../00_adr/adr-016-gpu-memory-domain.md) | GPU Memory Domain 模型 | 已接受 | §5.3 |
| [ADR-017](../00_adr/adr-017-gpfifo-queue-abstraction.md) | GPFIFO/Queue 抽象 | 已接受 | §10 |
| [ADR-018](../00_adr/adr-018-driver-sim-separation.md) | 驱动/仿真代码分离策略 | 已接受 | §2, §3, §6 |
| [ADR-019](../00_adr/adr-019-drm-gem-ttm-alignment.md) | DRM/GEM/TTM 标准接口对齐 | 已接受 | §3.5 |
| [ADR-020](../00_adr/adr-020-libgpu-core-extraction.md) | libgpu_core 算法核心提取 | 已接受 | §7 |
| [ADR-021](../00_adr/adr-021-hardware-puller.md) | Hardware Puller 状态机 | 已接受 | §6.3 |
| [ADR-023](../00_adr/adr-023-hal-interface.md) | 仿真层接口契约 (HAL) | 已接受 | §4 |
| [ADR-024](../00_adr/adr-024-user-mode-queue-submission.md) | 用户态队列命令提交架构 | 提议中 | §6.6, §9.2 |

### 12.2 关键决策摘要

- **ADR-018** 把 `plugins/gpu_driver/` 拆为 `drv/hal/sim/shared/` 四层，强制依赖方向 `drv → hal → sim`，避免仿真代码污染可移植的驱动代码。
- **ADR-020** 把 `BuddyAllocator` 提取为 `libgpu_core/` 纯 C 库，零外部依赖，未来可直接复制到内核模块。
- **ADR-021** 用状态机替换"伪执行"循环，明确定义 IDLE → FETCH → DECODE → SCHEDULE → DISPATCH → SEMAPHORE → COMPLETE 七个状态。
- **ADR-023** 定义 11 个 HAL 函数指针 (`gpu_hal_ops`)，分 `int` 与 `void` 两种返回类型，构造注入到 `GpgpuDevice`。
- **ADR-024** 新增用户态队列提交路径 (mmap Ring Buffer + Doorbell)，作为 ioctl 提交的高性能备选。

---

## §13 验证与测试

### 13.1 单元测试位置

GPU 驱动各层都有独立测试，测试框架为 **Catch2** (vendored 单文件 `tests/catch_amalgamated.{hpp,cpp}`)。

```
tests/
├── test_gpu_ioctl_standalone.cpp        — IOCTL 派发表
├── test_va_space_standalone.cpp         — VA Space 创建/销毁
├── test_gpu_ringbuffer_standalone.cpp    — Ring Buffer 读写
├── test_hardware_puller_emu_standalone.cpp — Puller 状态机
├── test_module_load_and_vfs_standalone.cpp  — 插件加载 + VFS 注册
└── test_gpu_queue_emu_standalone.cpp    — Queue 消费者
```

### 13.2 测试矩阵

| 测试类别 | 验证目标 | 测试文件 |
|----------|---------|---------|
| **IOCTL 派发** | 13 个 IOCTL 全部命中正确 handler | `test_gpu_ioctl_standalone.cpp` |
| **VA Space** | 创建/销毁 + 4KB/64KB 页面 + Queue 附加 | `test_va_space_standalone.cpp` |
| **Ring Buffer** | `gpu_ring_header` 读写 + 容量边界 | `test_gpu_ringbuffer_standalone.cpp` |
| **Puller FSM** | 状态转换 + semaphore 等待/释放 | `test_hardware_puller_emu_standalone.cpp` |
| **Queue 消费者** | dequeue + pendingCount + doorbell 回调 | `test_gpu_queue_emu_standalone.cpp` |
| **插件加载** | dlopen + dlsym("mod") + VFS 注册 | `test_module_load_and_vfs_standalone.cpp` |
| **HAL 单元** | 11 fn-ptr 全部挂载 + 行为可注入 | `plugins/gpu_driver/hal/test_hal.cpp` |
| **libgpu_core** | 分配/释放/合并 + 多 record 追踪 | `libgpu_core/test/test_buddy.c` |

### 13.3 关键测试样例

**VA Space + Queue 集成测试**:

```cpp
TEST_CASE("VA Space 创建并附加 Queue") {
  GpgpuDevice dev(&g_user_hal);
  gpu_va_space_args va_args{0, 0, 0};
  dev.ioctl(0, GPU_IOCTL_CREATE_VA_SPACE, &va_args);
  REQUIRE(va_args.va_space_handle != 0);

  gpu_queue_args q_args{va_args.va_space_handle, GPU_QUEUE_COMPUTE, 50, 1024, 0, 0};
  dev.ioctl(0, GPU_IOCTL_CREATE_QUEUE, &q_args);
  REQUIRE(q_args.queue_handle != 0);
  REQUIRE(q_args.doorbell_pgoff != 0);

  // 同一 VA Space 可附加多个 Queue
  gpu_queue_args q2{va_args.va_space_handle, GPU_QUEUE_COPY, 30, 512, 0, 0};
  dev.ioctl(0, GPU_IOCTL_CREATE_QUEUE, &q2);
  REQUIRE(q2.queue_handle != q_args.queue_handle);
}
```

**Pushbuffer 完整流测试**:

```cpp
TEST_CASE("PUSHBUFFER_SUBMIT_BATCH 触发 Puller FSM") {
  GpgpuDevice dev(&g_user_hal);
  GlobalScheduler scheduler;
  DoorbellEmu doorbell;
  auto puller = std::make_shared<HardwarePullerEmu>(&g_user_hal, &doorbell, &scheduler);
  dev.setPuller(puller);
  puller->start();

  gpu_gpfifo_entry entries[1] = {{
    .valid = 1, .method = GPU_OP_LAUNCH_KERNEL,
    .payload = {0, 0, 0, 0, 0, 0, 0},
    .semaphore_va = 0, .semaphore_value = 0, .release = 1
  }};

  gpu_pushbuffer_args pb{0, (u64)entries, 1, 0, 0};
  dev.ioctl(0, GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, &pb);
  REQUIRE(pb.fence_id != 0);

  // 等待 Puller 处理完成
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  puller->stop();
}
```

### 13.4 端到端运行

```bash
# 从项目根目录运行 (插件路径是相对的)
cd /workspace/project/UsrLinuxEmu

# 构建
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j4

# 运行所有测试
make test

# 或单独跑某个 GPU 链路测试
./build/bin/test_gpu_ioctl_standalone
./build/bin/test_va_space_standalone
./build/bin/test_hardware_puller_emu_standalone
```

---

## §14 与 TaskRunner 的边界

### 14.1 零耦合保证

UsrLinuxEmu 与 TaskRunner 之间的交互只通过两个接口:

1. **IOCTL** (`/dev/gpgpu0` 设备节点, `GPU_IOCTL_*` 编号)
2. **mmap** (Ring Buffer + Doorbell 区域)

不允许的耦合:

- ❌ TaskRunner 直接调用 UsrLinuxEmu 内部函数
- ❌ UsrLinuxEmu 包含 TaskRunner 头文件
- ❌ 两项目共享 `*.cpp` 源文件
- ❌ 两项目之间存在 `target_link_libraries` 依赖

### 14.2 头文件共享机制

```
TaskRunner/
└── UsrLinuxEmu/  (符号链接 → ../../UsrLinuxEmu/)
    └── plugins/gpu_driver/shared/  (符号链接 → ../../UsrLinuxEmu/plugins/gpu_driver/shared)
        ├── gpu_ioctl.h
        ├── gpu_types.h
        ├── gpu_queue.h
        ├── gpu_events.h
        └── gpu_regs.h
```

TaskRunner 通过符号链接访问 canonical 头文件。任何 IOCTL 编号或结构体修改都立即对两项目可见。

### 14.3 协同开发工作流

1. **需求提出**: 任一项目通过 GitHub Issue 提出接口变更
2. **接口评审**: 双方共同评审 `shared/gpu_*.h` 变更
3. **同步实现**: UsrLinuxEmu 更新 IOCTL handler + sim 仿真；TaskRunner 更新调用点
4. **集成验证**: 端到端测试跨两项目运行

### 14.4 集成测试

集成测试位于 `external/TaskRunner/tests/integration/`，通过符号链接访问 `plugins/gpu_driver/shared/`，加载仿真器并提交真实 GPFIFO entries。

---

## §15 总结

### 15.1 交付物清单

| 层级 | 文件 | 行数 | 职责 |
|------|------|------|------|
| **drv/** | `gpgpu_device.{h,cpp}` | 852 | IOCTL handler + VA Space + Queue 状态 |
| **drv/** | `gpu_drm_driver.cpp` | 288 | DRM 风格 IOCTL 描述符表 |
| **hal/** | `gpu_hal.h` | 96 | 11 fn-ptr 接口契约 |
| **hal/** | `hal_user.{h,cpp}` | 206 | 用户态 HAL 真实实现 |
| **hal/** | `hal_mock.{h,cpp}` | 166 | 单元测试 mock |
| **hal/** | `test_hal.cpp` | — | HAL 独立测试 |
| **sim/** | `hardware_puller_emu.{h,cpp}` | 384 | Puller 状态机 |
| **sim/** | `gpu_queue_emu.{h,cpp}` | 192 | Ring Buffer 消费者 |
| **sim/** | `scheduler/global_scheduler.{h,cpp}` | 110 | 全局调度器 |
| **sim/** | `scheduler/translator/gpfifo_translator.{h,cpp}` | — | GPFIFO → launch params 翻译 |
| **sim/** | `hardware/doorbell_emu.{h,cpp}` | — | Doorbell 寄存器仿真 |
| **sim/** | `buddy_allocator.cpp`, `fence_sim.cpp`, `doorbell_emu.cpp` | — | 辅助仿真 |
| **shared/** | `gpu_ioctl.h` | 274 | 15 个 IOCTL 编号 + 参数结构 |
| **shared/** | `gpu_types.h` | 68 | 类型 + GPFIFO entry 格式 |
| **shared/** | `gpu_queue.h` | 83 | Ring Buffer 头部 + Queue 描述符 |
| **shared/** | `gpu_events.h` | 150 | MMU 事件 + 回调签名 |
| **shared/** | `gpu_regs.h` | 109 | 寄存器偏移 |
| **libgpu_core/** | `include/gpu_buddy.h` | 113 | Buddy allocator C 接口 |
| **libgpu_core/** | `src/buddy.c` | 371 | Buddy allocator 实现 |

总计约 **3700+ 行**实现代码 (不含测试)。

### 15.2 关键架构承诺

| 维度 | 承诺 | 验证方法 |
|------|------|----------|
| **零改动迁移** | TaskRunner 切到真实 `.ko` 不改一行代码 | `external/TaskRunner/tests/integration/` 端到端测试 |
| **驱动可移植** | `drv/` 不直接依赖 `sim/`，只通过 `hal_` 指针访问硬件 | `grep "sim/" plugins/gpu_driver/drv/` 应只命中头文件 |
| **算法可复用** | `libgpu_core/` 可直接复制到内核模块 | `cp libgpu_core drivers/gpu/your_gpu/` 后 `make` 即可 |
| **HAL 接口稳定** | 11 fn-ptr 签名不变，新增接口不破坏 ABI | `test_hal.cpp` 覆盖所有 11 个接口 |
| **IOCTL 一致性** | `shared/gpu_ioctl.h` 是 UsrLinuxEmu 与 TaskRunner 的唯一真理 | 任何 IOCTL 修改必须同步两项目 |

### 15.3 重构时间轴

| 阶段 | 时间 | 关键事件 |
|------|------|----------|
| Phase 0 | 2025-12 ~ 2026-02 | 单仓库布局: `drivers/gpu/` + `simulator/gpu/` |
| Phase 1 | 2026-04 | System C 引入 (`GPU_IOCTL_*`) |
| Phase 1.5 | 2026-05 上 | 设备信息扩展 + libgpu_core 提取 + namespace wrap |
| Phase 1.5 → 2 | 2026-05 中 | 驱动/仿真分离 + HAL 契约 + Puller 状态机 |
| Phase 2 | 2026-05-13 | Ring Buffer + GpuQueueEmu + 多队列 fetch + Doorbell 修复 + VA Space 抽象 + 旧 launch-callback 接口清理 |

### 15.4 后续规划

| 阶段 | 内容 |
|------|------|
| Phase 2.5 | ADR-024 完整实现 (mmap Ring Buffer + Doorbell 用户态直写) |
| Phase 3 | 真实驱动 `.ko` 移植 (drv/ + libgpu_core/) |
| Phase 4 | 多 GPU 场景 (GPU_IOCTL_REGISTER_GPU 已就位) |

### 15.5 进一步阅读

- SSOT: [post-refactor-architecture.md](../02_architecture/post-refactor-architecture.md)
- 顶层 README: [../../README.md](../../README.md)
- 开发指南: [../../AGENTS.md](../../AGENTS.md)
- ADR 列表: [../00_adr/README.md](../00_adr/README.md)
- 集成文档: [../07-integration/](../07-integration/)

---

**维护者**: UsrLinuxEmu Architecture Team
**最后更新**: 2026-06-16
**对应代码 commit**: `374d463`
