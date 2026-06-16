# 架构设计文档

> ⚠️ **本文档最后验证: 2026-06-16 (commit `374d463`)**
>
> 本文档基于 SSOT [`post-refactor-architecture.md`](./post-refactor-architecture.md) 同步更新。涉及代码路径、API、ioctl 编号时，以 `plugins/gpu_driver/shared/gpu_ioctl.h` 和 SSOT 为准。
>
> **状态**: 🟢 与 commit `374d463` 对齐
> **适用 Phase**: 1.5 → 2（驱动/仿真分离 + VA Space 抽象）

---

## 系统概述

UsrLinuxEmu 是一个**用户态 Linux 内核模拟环境**，专为设备驱动开发和测试而设计。它让开发者在**无需 root 权限**、**无需内核编译**的情况下，编写、测试和调试设备驱动程序，尤其是 GPGPU 这类复杂设备的完整模拟。

系统采用分层架构设计。Phase 1.5 之后，驱动层和仿真层做了物理分离（`plugins/gpu_driver/{drv,hal,sim,shared}/`）；Phase 2 进一步引入了 VA Space、Queue、Ring Buffer 三层抽象，统一了"GPU 虚拟地址 → 用户提交队列 → 硬件拉取器消费"的完整数据通路。

## 整体架构

### 架构分层

系统采用四层架构设计：

```
┌─────────────────────────────────────────────────────────────┐
│                     用户应用层                                │
│  (User Applications: Test Programs, external/TaskRunner)    │
└─────────────────────────────────────────────────────────────┘
                            ↓ ioctl(fd, GPU_IOCTL_*, ...)
┌─────────────────────────────────────────────────────────────┐
│                   内核模拟框架层                              │
│  • VFS (Meyers singleton)        • File Operations          │
│  • ModuleLoader / PluginManager  • Service Registry         │
│  • ConfigManager                 • WaitQueue / PollWatcher  │
│  • Logger                                                  │
│  位置: src/kernel/ + include/kernel/ + include/linux_compat/│
│  链接: kernel SHARED lib（必须 SHARED，见 Issue #11）       │
└─────────────────────────────────────────────────────────────┘
                            ↓ dlopen("plugins/*.so")
┌─────────────────────────────────────────────────────────────┐
│                   设备驱动层                                   │
│  plugins/gpu_driver/                                          │
│    drv/      : GpgpuDevice（table ioctl via getIoctlTablePtr）│
│    hal/      : struct gpu_hal_ops + hal_user / hal_mock      │
│    shared/   : gpu_ioctl.h, gpu_types.h, gpu_queue.h, ...    │
│    plugin.cpp: extern "C" mod 符号（dlopen + dlsym）         │
│  drivers/   : sample_memory, sample_serial（示例）          │
└─────────────────────────────────────────────────────────────┘
                            ↓ HAL ops + sim subsystems
┌─────────────────────────────────────────────────────────────┐
│                   硬件仿真层                                  │
│  plugins/gpu_driver/sim/                                      │
│    sim/scheduler/   : GlobalScheduler + Translator           │
│    sim/hardware/    : HardwarePullerEmu (FSM), DoorbellEmu   │
│    sim/gpu_queue_emu.{h,cpp} : Ring buffer 消费者 (Phase 2)  │
│  libgpu_core/         : 纯 C buddy allocator（gpu_buddy.h）  │
└─────────────────────────────────────────────────────────────┘
```

## 核心组件架构

### 1. 内核模拟框架层

#### 核心框架组件

```
┌──────────────────────────────────────────────────────────┐
│              Kernel Emulation Framework                   │
│                                                           │
│  ┌────────────┐  ┌──────────────┐  ┌─────────────────┐ │
│  │    VFS     │  │  Module      │  │ Service Registry│ │
│  │  (Meyers)  │  │   Loader     │  │   (服务管理)     │ │
│  └────────────┘  └──────────────┘  └─────────────────┘ │
│                                                           │
│  ┌────────────┐  ┌──────────────┐  ┌─────────────────┐ │
│  │  Plugin    │  │  File Ops    │  │  Config Manager │ │
│  │  Manager   │  │  (fops)      │  │   (配置管理)     │ │
│  └────────────┘  └──────────────┘  └─────────────────┘ │
│                                                           │
│  ┌────────────┐  ┌──────────────┐  ┌─────────────────┐ │
│  │ Wait Queue │  │ Poll Watcher │  │     Logger      │ │
│  │ (等待队列)  │  │  (事件监听)   │  │   (日志系统)     │ │
│  └────────────┘  └──────────────┘  └─────────────────┘ │
└──────────────────────────────────────────────────────────┘
```

**主要职责**：
- **VFS**: 设备节点的注册、查找和管理（Meyers 单例，`VFS::instance()`）
- **ModuleLoader**: 运行时批量加载 `plugins/*.so`（按 `plugins/plugins.json` 扫描）
- **PluginManager**: 静态 API，CLI 工具加载单个插件时使用
- **Service Registry**: 全局服务的注册和获取（单例模式）
- **File Operations**: 文件操作的抽象和分发
- **Config Manager**: 配置文件的解析和管理
- **Wait Queue**: 等待队列的实现（类似内核等待队列）
- **Poll Watcher**: 事件监听和通知（观察者模式）
- **Logger**: 统一的日志输出接口

### 2. 设备抽象层

#### 设备类层次结构

```
                    ┌───────────────┐
                    │    Device     │
                    │   (抽象基类)   │
                    │  ───────────  │
                    │ + open()      │
                    │ + close()     │
                    │ + ioctl()     │
                    │ + mmap()      │
                    │ + read()      │
                    │ + write()     │
                    └───────┬───────┘
                            │
            ┌───────────────┼───────────────┐
            │               │               │
    ┌───────▼──────┐ ┌─────▼──────┐ ┌─────▼────────┐
    │SerialDevice  │ │MemoryDevice│ │ GpgpuDevice  │
    │  (串口设备)   │ │ (内存设备)  │ │  (GPU设备)   │
    └──────────────┘ └────────────┘ └──────────────┘
                                    ↑ 位于 plugins/gpu_driver/drv/
                                      不再是 include/kernel/device/
```

**设计模式**：
- **抽象工厂模式**: Device 基类定义统一接口
- **策略模式**: 不同设备实现不同的操作策略
- **模板方法模式**: 基类定义算法框架，子类实现具体步骤

注意 `GpgpuDevice` 在 Phase 1.5 之后已经**从 `include/kernel/device/` 迁移到 `plugins/gpu_driver/drv/`**。框架层只提供 Device 基类，具体设备由各插件实现。

### 3. GPU 驱动架构（Phase 1.5 → 2）

#### GPGPU 驱动组件分层

```
┌─────────────────────────────────────────────────────────┐
│           plugins/gpu_driver/drv/  (GpgpuDevice)         │
│  ┌──────────────────────────────────────────────────┐  │
│  │              GpgpuDevice（设备类）                  │  │
│  │  • getIoctlTablePtr() → drm_ioctl_desc[] 表驱动   │  │
│  │  • handlePushbufferSubmitBatch / AllocBo / ...    │  │
│  │  • VA Space 集合 + Queue 集合（Phase 2）          │  │
│  └─────────┬───────────────────────────┬──────────┘  │
└────────────┼───────────────────────────┼─────────────┘
             │                           │
   ┌─────────▼─────────┐       ┌─────────▼─────────┐
   │  plugins/gpu_     │       │  plugins/gpu_     │
   │  driver/hal/      │       │  driver/sim/      │
   │                   │       │                   │
   │ struct gpu_hal_ops│       │ GlobalScheduler   │
   │  (11 个函数指针)  │       │ HardwarePullerEmu │
   │ + hal_user        │       │ GpuQueueEmu       │
   │ + hal_mock        │       │ DoorbellEmu       │
   └─────────┬─────────┘       └─────────┬─────────┘
             │                           │
             └───────────┬───────────────┘
                         │
                ┌────────▼─────────┐
                │   libgpu_core/   │
                │  gpu_buddy.h     │
                │  (纯 C buddy)    │
                └──────────────────┘
```

**Phase 1.5 关键变化**：
- **驱动/仿真物理分离**：`drv/` 不再包含仿真代码，仿真下沉到 `sim/`
- **HAL 接口契约**：`struct gpu_hal_ops`（11 个函数指针）作为驱动调用仿制的标准接口
- **libgpu_core 提取**（ADR-020）：buddy allocator 抽出为纯 C 库，可直接嵌入内核驱动

**Phase 2 新增**（VA Space / Queue / Ring Buffer 三层抽象）：
- **VASpace**: GPU 虚拟地址空间（`u64` 句柄）
- **Queue**: 命令队列（属于某个 VASpace，ring buffer 消费方）
- **Ring Buffer**: 用户空间 mmap 共享的环形命令缓冲

### 4. 硬件仿真层

#### GPU 仿真子系统

```
┌──────────────────────────────────────────────────────┐
│            plugins/gpu_driver/sim/                    │
│                                                       │
│  ┌──────────────┐         ┌──────────────────────┐  │
│  │  Hardware    │────────▶│   Global Scheduler   │  │
│  │   Puller     │         │  (FIFO + engine      │  │
│  │   (FSM)      │         │   routing)           │  │
│  └──────┬───────┘         └──────────┬───────────┘  │
│         │                            │              │
│         ▼                            ▼              │
│  ┌──────────────┐         ┌──────────────────────┐  │
│  │  Doorbell    │         │   GpuQueueEmu        │  │
│  │   Emu        │         │  (Ring Buffer 消费)  │  │
│  └──────────────┘         └──────────────────────┘  │
│         │                            │              │
│         ▼                            ▼              │
│  ┌──────────────────────────────────────────────┐   │
│  │          libgpu_core/  (gpu_buddy.h)          │   │
│  │     纯 C buddy allocator，零依赖              │   │
│  └──────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────┘
```

## 数据流设计

### GPU 内存分配流程

```
┌──────────────┐
│ User Program │
└──────┬───────┘
       │ open("/dev/gpgpu0")
       ▼
┌──────────────────────┐
│  VFS::instance()     │
│  .lookup_device()    │
└──────┬───────────────┘
       │ shared_ptr<GpgpuDevice>
       ▼
┌──────────────────────┐
│  GpgpuDevice         │
│  (drv/gpgpu_device)  │
└──────┬───────────────┘
       │ ioctl(fd, GPU_IOCTL_ALLOC_BO, &args)
       ▼
┌──────────────────────┐
│  GpgpuDevice::ioctl  │
│  → table dispatch    │
│  (getIoctlTablePtr)  │
└──────┬───────────────┘
       │ handleAllocBo(args)
       ▼
┌──────────────────────┐
│  HAL ops 路由         │
│  hal.alloc_mem /     │
│  hal.buddy_alloc     │
└──────┬───────────────┘
       │
       ▼
┌──────────────────────┐
│  libgpu_core/        │
│  gpu_buddy_alloc()   │
│  (纯 C buddy 分配器)  │
└──────┬───────────────┘
       │ return gpu_va + handle
       ▼
┌──────────────────────┐
│  GPU_IOCTL_MAP_BO    │
│  (mmap 映射)          │
└──────┬───────────────┘
       │ return user_ptr
       ▼
┌──────────────────────┐
│  User Program        │
│  (获得 GPU 虚拟地址)  │
└──────────────────────┘
```

### GPU 命令执行流程（Phase 2 完整版）

```
┌──────────────┐
│ User Program │
└──────┬───────┘
       │ 1. GPU_IOCTL_CREATE_VA_SPACE  → va_handle
       │ 2. GPU_IOCTL_CREATE_QUEUE      → queue_handle, doorbell_pgoff
       │ 3. mmap(ring_buffer)           → 写入 gpu_gpfifo_entry[]
       │ 4. ioctl(fd, GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, args)
       ▼
┌──────────────────────┐
│  GpgpuDevice         │
│  (drv)               │
└──────┬───────────────┘
       │ handlePushbufferSubmitBatch(args)
       ▼
┌──────────────────────────────────────┐
│  校验 VASpace / Queue 存在（强制）    │
│  HAL.fence_create(&fence_id)          │
└──────┬───────────────────────────────┘
       │ gpfifo_addr, count
       ▼
┌──────────────────────┐
│  HardwarePullerEmu   │
│  (sim/hardware)      │
│  FSM: IDLE→FETCH→    │
│  DECODE→SCHEDULE→   │
│  DISPATCH→COMPLETE  │
└──────┬───────────────┘
       │ enqueue(entry, selectEngine(entry))
       ▼
┌──────────────────────┐
│  GlobalScheduler     │
│  (sim/scheduler)     │
│  + Translator        │
└──────┬───────────────┘
       │ LaunchParamsCallback
       ▼
┌──────────────────────┐
│  GpuQueueEmu         │
│  (sim/gpu_queue_emu) │
│  Ring Buffer 消费者  │
└──────┬───────────────┘
       │ access memory
       ▼
┌──────────────────────┐
│  System Memory       │
│  (via buddy)         │
└──────────────────────┘
       │ HAL.doorbell_ring(stream_id)
       ▼
┌──────────────────────┐
│  返回 fence_id       │
│  给用户               │
└──────────────────────┘
```

## 接口设计

### 核心框架层

| 组件 | 头文件 | 说明 |
|------|--------|------|
| Device 基类 | `include/kernel/device.h` | 统一设备接口（`open/close/ioctl/mmap/read/write`）|
| File Operations | `include/kernel/file_ops.h` | 文件操作抽象与分发 |
| VFS | `include/kernel/vfs.h` | **Meyers 单例**（`VFS::instance()`），设备注册/查找 |
| Module Loader | `include/kernel/module_loader.h` | 运行时加载 `plugins/` 目录下所有 .so |
| Plugin Manager | `include/kernel/plugin_manager.h` | 静态 API，CLI 单插件加载用 |
| Service Registry | `include/kernel/service_registry.h` | 全局服务注册与获取 |
| Linux 兼容层 | `include/linux_compat/` | `types.h` / `memory.h` / `ioctl.h` / `drm/`（u8/u32、_IOR、ERR_PTR、GFP_*）|

### 设备实现层

| 设备 | 位置 |
|------|------|
| 串口设备（示例）| `drivers/sample_serial/` |
| 内存设备（示例）| `drivers/sample_memory/` |
| GPGPU 设备 | **`plugins/gpu_driver/drv/gpgpu_device.{h,cpp}`** |

### 驱动实现层（`plugins/gpu_driver/`）

| 子目录 | 内容 |
|--------|------|
| `drv/` | `GpgpuDevice`（设备类，table ioctl via `getIoctlTablePtr()`）+ `gpu_drm_driver.cpp`（drm_ioctl_desc[] 表）|
| `hal/` | `struct gpu_hal_ops`（11 个函数指针契约）+ `hal_user`（mmap heap + buddy + fences）+ `hal_mock` |
| `sim/` | `scheduler/`（GlobalScheduler + Translator）+ `hardware/`（HardwarePullerEmu + DoorbellEmu）+ `gpu_queue_emu.{h,cpp}` + `buddy_allocator.cpp` / `fence_sim.cpp`（shadow 编译）|
| `shared/` | **canonical 头**：`gpu_ioctl.h`、`gpu_types.h`、`gpu_queue.h`、`gpu_events.h`、`gpu_regs.h`（也通过 symlink 被 TaskRunner 引用）|
| `plugin.cpp` | `extern "C" { ... mod ... }` 符号（dlopen + dlsym 加载）|

### 模拟器层

| 组件 | 位置 |
|------|------|
| 全局调度器 | `plugins/gpu_driver/sim/scheduler/global_scheduler.{h,cpp}` |
| GPFIFO → LaunchParams 转换器 | `plugins/gpu_driver/sim/scheduler/translator/` |
| 硬件拉取器（FSM）| `plugins/gpu_driver/sim/hardware/hardware_puller_emu.{h,cpp}` |
| Doorbell 模拟 | `plugins/gpu_driver/sim/hardware/doorbell_emu.{h,cpp}` |
| Ring Buffer 消费者 | `plugins/gpu_driver/sim/gpu_queue_emu.{h,cpp}` |
| 纯 C Buddy Allocator | `libgpu_core/include/gpu_buddy.h` + `libgpu_core/src/buddy.c`（**ADR-020**，可被内核驱动直接嵌入）|

### Device 接口定义

```cpp
// include/kernel/device.h
class Device {
public:
  virtual int open(int flags) = 0;
  virtual int close() = 0;
  virtual int ioctl(unsigned long cmd, void *arg) = 0;
  virtual void* mmap(void* addr, size_t length, int prot,
                     int flags, off_t offset) = 0;
  virtual ssize_t read(void* buf, size_t count) = 0;
  virtual ssize_t write(const void* buf, size_t count) = 0;
};
```

### VFS 接口（Meyers 单例）

```cpp
// include/kernel/vfs.h
namespace usr_linux_emu {
class VFS {
 public:
  static VFS& instance();   // Meyers singleton（Issue #11 修复）

  int register_device(const std::shared_ptr<Device>& dev);
  std::shared_ptr<Device> lookup_device(const std::string& name);
  std::shared_ptr<Device> open(const std::string& path, int flags);

  std::vector<std::string> list_devices() const;
  int unregister_device(const std::string& name);
  void clear_devices();
  static void shutdown();

 private:
  VFS() = default;
  ~VFS() = default;
  std::unordered_map<std::string, std::shared_ptr<Device>> devices_;
};
}  // namespace usr_linux_emu
```

> **关键约束**：`kernel` 库必须是 SHARED（`src/CMakeLists.txt`）。若为 STATIC，进程和插件各自持有一份静态局部变量副本，`VFS::instance()` 单例会被割裂（Issue #11）。

### 插件加载 API

```cpp
// 运行时（推荐）：批量加载 plugins/ 目录下所有 .so
#include "kernel/module_loader.h"
ModuleLoader::load_plugins("plugins");

// 静态加载（CLI 用）：加载单个 .so
#include "kernel/plugin_manager.h"
PluginManager::load_plugin("plugins/plugin_gpu_driver.so");
```

### GPU IOCTL 命令（System C）

完整定义见 [`plugins/gpu_driver/shared/gpu_ioctl.h`](../../plugins/gpu_driver/shared/gpu_ioctl.h)。这里列出主命令集：

```cpp
// 命令提交（0x01 ~ 0x03）
#define GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH _IOW('G', 0x01, struct gpu_pushbuffer_args)
#define GPU_IOCTL_REGISTER_MMU_EVENT_CB  _IOW('G', 0x02, struct gpu_mmu_event_cb_args)
#define GPU_IOCTL_REGISTER_FIRMWARE_CB   _IOW('G', 0x03, struct gpu_firmware_cb_args)

// 内存管理（0x10 ~ 0x13）
#define GPU_IOCTL_ALLOC_BO     _IOWR('G', 0x10, struct gpu_alloc_bo_args)
#define GPU_IOCTL_FREE_BO      _IOW ('G', 0x11, u32)
#define GPU_IOCTL_MAP_BO       _IOWR('G', 0x12, struct gpu_map_bo_args)
#define GPU_IOCTL_WAIT_FENCE   _IOW ('G', 0x13, struct gpu_wait_fence_args)

// 设备信息（0x20）
#define GPU_IOCTL_GET_DEVICE_INFO _IOR('G', 0x20, struct gpu_device_info)

// VA Space 管理（0x30 ~ 0x32，Phase 2 强制）
#define GPU_IOCTL_CREATE_VA_SPACE  _IOWR('G', 0x30, struct gpu_va_space_args)
#define GPU_IOCTL_DESTROY_VA_SPACE _IOW ('G', 0x31, gpu_va_space_handle_t)
#define GPU_IOCTL_REGISTER_GPU     _IOW ('G', 0x32, struct gpu_register_gpu_args)

// Queue 管理（0x40 ~ 0x43，Phase 2 / ADR-024）
#define GPU_IOCTL_CREATE_QUEUE    _IOWR('G', 0x40, struct gpu_queue_args)
#define GPU_IOCTL_DESTROY_QUEUE   _IOW ('G', 0x41, gpu_queue_handle_t)
#define GPU_IOCTL_MAP_QUEUE_RING  _IOWR('G', 0x42, struct gpu_queue_map_ring_args)
#define GPU_IOCTL_QUERY_QUEUE     _IOWR('G', 0x43, struct gpu_queue_info_args)
```

> **历史说明**：Phase 1 之前的 System B（以 `GPGPU` 为前缀的 ioctl 命名空间，编号 0x01-0x06）已整体归档到 `archive/system_b_drivers/`，被 System C（`GPU_IOCTL_*` 命名空间，编号 0x01-0x43）取代。System A（`CUDA_*`）在更早阶段已删除。

## 模块详细设计

### 1. 设备抽象模块

#### Device 基类
**文件**: `include/kernel/device.h`

**功能**:
- 定义所有设备的通用接口
- 提供设备生命周期管理
- 实现设备基础功能

**关键方法**:
- `open()`: 设备打开操作，初始化设备状态
- `close()`: 设备关闭操作，释放资源
- `ioctl()`: 设备控制操作，处理各种设备命令
- `mmap()`: 内存映射操作，将设备内存映射到用户空间
- `read()`: 数据读取操作
- `write()`: 数据写入操作

#### 设备实现类

**SerialDevice**: 串口设备
- 模拟串行通信设备
- 支持基本的读写操作
- 实现流式数据传输

**MemoryDevice**: 内存设备
- 模拟内存访问设备
- 提供直接内存访问接口
- 支持内存映射

**GpgpuDevice**（位置: `plugins/gpu_driver/drv/`）:
- 完整的 GPU 功能模拟
- 内存管理、命令提交、VA Space / Queue 管理
- 通过 `getIoctlTablePtr()` 返回的 `drm_ioctl_desc[]` 表做 ioctl 分发
- 集成 HAL ops + sim 子系统

### 2. 文件操作模块

**文件**: `include/kernel/file_ops.h`

**功能**:
- 封装不同设备的文件操作
- 提供统一的接口调度机制
- 管理文件描述符和设备的映射

### 3. 虚拟文件系统模块

**文件**: `include/kernel/vfs.h`、`src/kernel/vfs.cpp`

**功能**:
- 设备节点的注册和查找
- 路径解析和管理
- 设备生命周期管理

**核心实现**: Meyers 单例（`VFS::instance()` 返回引用），所有设备注册都走这个实例。

### 4. GPU 驱动模块（Phase 1.5 → 2）

#### 4.1 Buddy Allocator（纯 C 库）

**文件**: `libgpu_core/include/gpu_buddy.h` + `libgpu_core/src/buddy.c`（**ADR-020**）

**特性**:
- **纯 C 实现**，零依赖（不上锁、不分配内存）
- 可直接嵌入 Linux 内核驱动使用
- 调用者负责外部同步
- 最大支持 8GB（2^21 × 4KB）

**API**（C 接口）:
```c
struct gpu_buddy* gpu_buddy_init(uint64_t base, uint64_t size);
int  gpu_buddy_alloc(struct gpu_buddy* b, uint64_t size, uint64_t* addr);
void gpu_buddy_free (struct gpu_buddy* b, uint64_t addr, uint64_t size);
void gpu_buddy_destroy(struct gpu_buddy* b);
```

#### 4.2 HAL 接口契约

**文件**: `plugins/gpu_driver/hal/gpu_hal.h`

```cpp
struct gpu_hal_ops {
  // 内存（11 个函数指针之一示例）
  int (*alloc_mem)(void* hal_ctx, uint64_t size, uint64_t* gpu_va, uint32_t* handle);
  int (*free_mem) (void* hal_ctx, uint32_t handle);
  int (*map_mem)  (void* hal_ctx, uint32_t handle, uint64_t* gpu_va);

  // fence
  int (*fence_create)(void* hal_ctx, uint64_t* fence_id);
  int (*fence_wait)  (void* hal_ctx, uint64_t fence_id, uint32_t timeout_ms);

  // 内存读 / doorbell
  int (*mem_read)    (void* hal_ctx, uint64_t gpu_va, void* dst, uint64_t size);
  void (*doorbell_ring)(void* hal_ctx, uint32_t stream_id);
  // ... 共 11 个 ops
};
```

#### 4.3 Hardware Puller（FSM）

**文件**: `plugins/gpu_driver/sim/hardware/hardware_puller_emu.{h,cpp}`

- IDLE → FETCH（`HAL.mem_read`）→ DECODE → SCHEDULE → DISPATCH → COMPLETE
- 消费 `gpu_gpfifo_entry[]`，调度到 `GlobalScheduler`

#### 4.4 VA Space / Queue / Ring Buffer（Phase 2）

**VA Space**（虚拟地址空间）
- `u64` 句柄（`gpu_va_space_handle_t`）
- `page_size`（0=4KB, 1=64KB）
- `flags`
- 关联的 Queue 列表

**Queue**（命令队列）
- `u64` 句柄（`gpu_queue_handle_t`）
- `queue_type`（COMPUTE / COPY / GRAPHICS）
- `priority`（0-100）
- 关联一个 `ring_buffer`（`gpu_ring_header`，shm-backed）
- 关联一个 `doorbell`（mmap 偏移 `0x10000 + h*0x1000`）

**Ring Buffer**（环形缓冲）
- 共享内存后端（mmap 到用户空间）
- `write_idx` / `read_idx`（volatile）
- `capacity`（最大 1024）
- `entries[]`：`gpu_gpfifo_entry` 数组

#### 4.5 GPU 驱动入口

**文件**: `plugins/gpu_driver/drv/gpu_drm_driver.cpp`

- 维护 `drm_ioctl_desc[]` 表，把每个 `GPU_IOCTL_*` 映射到对应的 handler
- `GpgpuDevice::ioctl()` 通过 `getIoctlTablePtr()` 查表分发
- 与 HAL ops + sim 子系统协作

### 5. 硬件仿真模块（`plugins/gpu_driver/sim/`）

| 组件 | 文件 |
|------|------|
| GlobalScheduler | `sim/scheduler/global_scheduler.{h,cpp}` |
| GpfifoToLaunchParamsTranslator | `sim/scheduler/translator/` |
| HardwarePullerEmu | `sim/hardware/hardware_puller_emu.{h,cpp}` |
| DoorbellEmu | `sim/hardware/doorbell_emu.{h,cpp}` |
| GpuQueueEmu（Ring Buffer 消费）| `sim/gpu_queue_emu.{h,cpp}` |
| BuddyAllocator（shadow 编译，libgpu_core 的 C++ 封装）| `sim/buddy_allocator.cpp` |
| FenceSim | `sim/fence_sim.cpp` |

## 并发和同步设计

### 线程模型

```
┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│ User Thread  │     │ User Thread  │     │ User Thread  │
└──────┬───────┘     └──────┬───────┘     └──────┬───────┘
       │                    │                    │
       └────────────────────┼────────────────────┘
                            │
                   ┌────────▼────────┐
                   │  VFS + Devices  │
                   │   (Thread-Safe) │
                   │  (Meyers 单例)  │
                   └────────┬────────┘
                            │
                   ┌────────▼────────┐
                   │   GpuQueueEmu   │
                   │   (Lock-Free    │
                   │    Ring Buffer) │
                   └────────┬────────┘
                            │
                   ┌────────▼────────┐
                   │  Hardware       │
                   │  Puller + Sim   │
                   │   (Worker)      │
                   └─────────────────┘
```

### 同步机制

**文件**: `include/kernel/sync_utils.h`

**提供的同步原语**:
- Mutex (互斥锁)
- Spinlock (自旋锁)
- Semaphore (信号量)
- Condition Variable (条件变量)

**使用场景**:
- VFS 设备注册使用互斥锁
- Ring Buffer 使用原子操作
- Wait Queue 使用条件变量
- Buddy allocator（libgpu_core）**不上锁**，由调用者同步

## 扩展性设计

### 插件化架构

#### 插件加载流程

```
┌────────────────┐
│  plugins/      │
│  *.so +        │
│  plugins.json  │
└───────┬────────┘
        │
        ▼
┌────────────────────┐
│ ModuleLoader       │
│ .load_plugins()    │
└───────┬────────────┘
        │ dlopen + dlsym(handle, "mod")
        ▼
┌────────────────┐
│  mod 符号      │
│  (extern "C")  │
└───────┬────────┘
        │ 调用 register_device
        ▼
┌────────────────┐
│  GpgpuDevice   │
│  实例          │
└───────┬────────┘
        │
        ▼
┌────────────────────────┐
│ VFS::instance()        │
│  .register_device()    │
│  (Meyers 单例)         │
└────────────────────────┘
```

#### 添加新设备类型

1. **定义设备类**（继承 `Device`，实现 `open/close/ioctl/mmap/...`）
2. **在插件入口导出 `mod` 符号**（`extern "C"`），内部调用 `VFS::instance().register_device(...)`
3. **编译为动态库**（`.so`），放到 `plugins/` 目录
4. **可选**: 在 `plugins/plugins.json` 中登记

### Linux 兼容层扩展

**目录**: `include/linux_compat/`

**目标**: 提供 Linux 内核 API 的用户态实现

**已实现**:
- 基础类型定义 (`types.h` —— `u8` / `u32` / `u64` / `ERR_PTR` 等)
- 内存管理函数 (`memory.h`)
- IOCTL 宏定义 (`ioctl.h` —— `_IOR` / `_IOW` / `_IOWR`)
- DRM 子集 (`drm/drm_ioctl.h`、`drm/drm_gem.h`、`drm/drm_driver.h`)

**计划实现**:
- 字符设备 API (`cdev.h`)
- 设备模型 API (`device.h`)
- 同步原语 (`sync.h`)
- 中断处理 (`interrupt.h`)
- PCI 设备 API (`pci.h`)

## 性能优化设计

### 内存管理优化

1. **零拷贝 mmap**: BO 通过 `GPU_IOCTL_MAP_BO` 直接 mmap 到用户空间
2. **共享内存 Ring Buffer**: Queue 的 ring buffer 由用户 mmap，仿真侧零拷贝消费
3. **延迟释放**: fence 信号后才释放临时结构
4. **buddy 池化**: libgpu_core 的纯 C buddy 减少碎片

### 并发优化

1. **无锁 Ring Buffer**: GpuQueueEmu 用 volatile `write_idx` / `read_idx`
2. **HAL ops 间接调用**: 11 个函数指针，避免虚函数开销
3. **细粒度锁**: VFS 用全局锁，HAL 内部按需
4. **fence 异步跟踪**: `HAL.fence_create` 返回 fence_id，硬件完成后通过 doorbell 通知

### 缓存优化

1. **内存对齐**: `gpu_buddy.h` 中 4KB 对齐
2. **批量操作**: `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH` 一次提交多个 entry
3. **预取**: Hardware Puller 的 FETCH 阶段
4. **局部性**: `gpu_gpfifo_entry` 紧凑布局

## 错误处理和调试

### 错误处理策略

1. **返回值检查**: 所有函数返回 Linux 风格错误码（`-EINVAL`、`-ENOMEM` 等）
2. **异常安全**: 使用 RAII 管理资源
3. **错误传播**: 明确的错误传播链（ioctl handler → HAL ops → libgpu_core）
4. **日志记录**: 通过 `Logger` 统一记录

### 调试支持

1. **日志系统**: 分级日志输出（`include/kernel/logger.h`）
2. **断言检查**: 开发期检查
3. **性能分析**: 支持性能统计
4. **内存检查**: 集成内存泄漏检测

## 安全性设计

### 权限控制

1. **设备访问控制**: 限制 `/dev/gpgpu0` 访问权限
2. **内存保护**: VA Space 隔离进程 GPU 虚拟地址
3. **参数验证**: ioctl handler 严格校验 `args`
4. **资源限制**: `ring_size` 最大 1024，`max_channels` 限制

### 数据保护

1. **VA Space 隔离**: 不同进程的 GPU 虚拟地址互不影响
2. **安全的内存操作**: HAL.mem_read/write 带 size 校验
3. **输入验证**: 防止 ioctl 参数注入
4. **输出过滤**: `gpu_device_info.marketing_name` 固定长度 64 字节

---

## Phase 2 新增概念（VA Space → Queue → Ring Buffer）

Phase 2 引入了三层 GPU 抽象，统一了"用户提交 → 驱动分发 → 硬件消费"的完整数据通路。

### 数据模型

```
VASpace  (u64 handle, 内部 GpgpuDevice::VASpace)
   ├─ page_size (0=4KB, 1=64KB)
   ├─ flags
   └─ attached_queues: [queue_handle]
         ↓
Queue  (u64 handle, 内部 GpuQueueEmu shared_ptr)
   ├─ queue_type (COMPUTE/COPY/GRAPHICS)
   ├─ priority
   ├─ ring_size
   ├─ ring_buffer: gpu_ring_header (shm-backed)
   │     ├─ write_idx / read_idx (volatile)
   │     ├─ capacity (max 1024)
   │     └─ entries[] (gpu_gpfifo_entry)
   └─ doorbell: mmap offset 0x10000 + h*0x1000
```

### 完整生命周期（用户视角）

1. **创建 VA Space**：`ioctl(fd, GPU_IOCTL_CREATE_VA_SPACE, &args)` → `args.va_space_handle`
2. **创建 Queue**：`ioctl(fd, GPU_IOCTL_CREATE_QUEUE, &args)` → `args.queue_handle`、`args.doorbell_pgoff`
3. **映射 Ring Buffer**：`ioctl(fd, GPU_IOCTL_MAP_QUEUE_RING, &args)` → 用户获得共享内存指针
4. **提交命令**：
   - 写入 `gpu_gpfifo_entry[]` 到 ring buffer
   - `ioctl(fd, GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, &args)` → `args.fence_id`
5. **等待完成**：`ioctl(fd, GPU_IOCTL_WAIT_FENCE, &wait_args)`
6. **销毁 Queue** / **销毁 VA Space**（反向）

### 强制校验

`handlePushbufferSubmitBatch` 强制要求：
- 指定的 VASpace 存在
- 指定的 Queue 属于该 VASpace
- 否则返回 `-EINVAL`

这条校验在 Phase 2 之前不存在（Phase 1 用隐式默认 Queue），Phase 2 起成为强制契约。

### 关键设计权衡

- **VA Space 解耦 GPU 与进程**: 支持多 GPU peer-to-peer
- **Queue 解耦命令流与硬件**: 支持 COMPUTE / COPY / GRAPHICS 多引擎
- **Ring Buffer 用户态 mmap**: 零拷贝，但需要同步（volatile idx）
- **fence_id 异步跟踪**: S3.5 起，提交立即返回 fence_id，完成后 doorbell 通知

---

## 相关文档

- **SSOT（权威架构）**：[`post-refactor-architecture.md`](./post-refactor-architecture.md) —— 重构后架构总览 + 32 项 docs 修复建议
- **API 参考**：[`docs/06-reference/api-reference.md`](../06-reference/api-reference.md)
- **IOCTL 命令**：[`docs/06-reference/ioctl-commands.md`](../06-reference/ioctl-commands.md)
- **GPU 驱动高级架构**：[`docs/05-advanced/gpu_driver_architecture.md`](../05-advanced/gpu_driver_architecture.md)
- **开发指南**：[`AGENTS.md`](../../AGENTS.md)

---

**文档版本**: 3.0  
**最后验证**: 2026-06-16 (commit `374d463`)  
**维护者**: UsrLinuxEmu Architecture Team  
**历史版本**: 2.0（2026-02-10，严重过期，已被 3.1 重构抛下）→ 3.0（2026-06-16，对齐 Phase 1.5 → 2）
