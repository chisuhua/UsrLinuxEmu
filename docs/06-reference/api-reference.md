# API 参考文档

> **最后验证**: 2026-06-16 (commit `374d463`)
>
> **架构 SSOT**: [`docs/02_architecture/post-refactor-architecture.md`](../02_architecture/post-refactor-architecture.md)
> **状态**: 已重写，对齐 Phase 1.5 / Phase 2 重构后的三层架构

## 目录

- [§1 架构总览](#1-架构总览)
- [§2 Layer 1 - 框架层 (Framework)](#2-layer-1---框架层-framework)
- [§3 Layer 2 - 驱动层 (Driver)](#3-layer-2---驱动层-driver)
- [§4 Layer 3 - 仿真层 (Simulation)](#4-layer-3---仿真层-simulation)
- [§5 完整使用示例](#5-完整使用示例)
- [§6 跨文档索引](#6-跨文档索引)

---

## §1 架构总览

UsrLinuxEmu 自 Phase 2 起采用**三层分离**架构，API 按层组织：

| 层 | 路径 | 职责 |
|----|------|------|
| **Layer 1 - 框架层** | `include/kernel/`, `include/linux_compat/` | 进程内 VFS、设备抽象、Linux 兼容 |
| **Layer 2 - 驱动层** | `plugins/gpu_driver/drv/` | 设备特定 ioctl 表、HAL 契约 |
| **Layer 3 - 仿真层** | `plugins/gpu_driver/sim/`, `libgpu_core/` | 硬件行为模拟、内存分配算法 |

**关键边界**：框架层不依赖驱动层和仿真层；驱动层通过 `gpu_hal_ops` 函数指针表与仿真层解耦；仿真层可独立替换（用户态仿真 → 内核态硬件）。详见 SSOT §1.2 架构图。

---

## §2 Layer 1 - 框架层 (Framework)

框架层实现进程内单例 VFS、设备抽象、插件加载机制与 Linux 兼容 API。代码位于 `src/kernel/`（SHARED 库）和 `include/kernel/`、`include/linux_compat/`。

> 框架层**不**包含 `GpgpuDevice`（属于驱动层，见 §3）。

### 2.1 VFS - 虚拟文件系统

**头文件**: `include/kernel/vfs.h`  **命名空间**: `usr_linux_emu::`  **模式**: Meyers 单例

```cpp
class VFS {
 public:
  static VFS& instance();
  int register_device(const std::shared_ptr<Device>& dev);
  std::shared_ptr<Device> lookup_device(const std::string& name);
  std::shared_ptr<Device> open(const std::string& path, int flags);
  std::vector<std::string> list_devices() const;
  int unregister_device(const std::string& name);
  void clear_devices();
  static void shutdown();
};
```

**`instance()`**: 获取全局唯一 VFS 实例（线程安全）。`kernel` 库必须是 SHARED 编译产物，否则主程序和插件会各自构造一份独立实例（参见 `AGENTS.md` Issue #11）。

**`register_device(dev)`**: 注册设备，`dev->name` 字段为 VFS 路径（如 `/dev/gpgpu0`）。返回 0 = 成功，负值 = 错误码。

**`open(path, flags)`**: 按路径打开设备，返回 `shared_ptr<Device>`，失败返回 `nullptr`。

**`lookup_device(name)`**: 仅查找，不触发 `fops->open`。

**`unregister_device(name)` / `clear_devices()` / `shutdown()`**: 注销与清理。

示例：

```cpp
auto gpu = std::make_shared<GpgpuDevice>(hal_ops);
gpu->name = "/dev/gpgpu0";
usr_linux_emu::VFS::instance().register_device(gpu);
auto dev = usr_linux_emu::VFS::instance().open("/dev/gpgpu0", O_RDWR);
```

### 2.2 Device - 设备基类

**头文件**: `include/kernel/device/device.h`  **命名空间**: `usr_linux_emu::`

Device 类本身**不**实现 `open/close/ioctl`，只持有元数据和 `FileOperations` 共享指针：

```cpp
class Device {
 public:
  Device(const std::string& name, dev_t id,
         std::shared_ptr<FileOperations> ops, void* handle);
  std::string name;        // VFS 路径，如 /dev/gpgpu0
  dev_t dev_id;            // 设备号
  void* plugin_handle;     // dlopen 句柄
  std::shared_ptr<FileOperations> fops;  // 操作句柄
};
```

**调用约定**：`dev->fops->ioctl(fd, GPU_IOCTL_GET_DEVICE_INFO, &info)`。

### 2.3 FileOperations - 文件操作抽象

**头文件**: `include/kernel/file_ops.h`

```cpp
class FileOperations {
 public:
  virtual ~FileOperations() = default;
  virtual int open(const char* path, int flags);
  virtual int close(int fd);
  virtual ssize_t read(int fd, void* buf, size_t count);
  virtual ssize_t write(int fd, const void* buf, size_t count);
  virtual long ioctl(int fd, unsigned long request, void* argp) = 0;  // 纯虚
  virtual void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset);
  virtual int munmap(void* addr, size_t length);
 protected:
  WaitQueue wait_queue_;
  bool has_data_ = false;
};
```

`ioctl` 是纯虚函数，**所有**设备必须实现。成功返回 0 或正数；错误返回负 Linux 错误码（`-EINVAL`、`-ENOMEM` 等）。

### 2.4 ModuleLoader - 运行时插件加载

**头文件**: `include/kernel/module_loader.h`  **使用场景**: 进程运行时（**首选** API）

C 风格插件加载接口，依赖 `module mod` 符号（`dlopen` + `dlsym`）：

```c
extern "C" {
typedef struct module {
  const char* name;       // 插件名
  const char** depends;   // 依赖列表（NULL 结尾）
  int (*init)(void);      // 初始化
  void (*exit)(void);     // 卸载
} module;
}
```

```cpp
class ModuleLoader {
 public:
  static int load_plugins(const std::string& dir_path);  // 扫描目录并加载全部
  static void unload_plugins();
  static int load_plugin(const std::string& path);      // 加载单个 .so
  static int unload_plugin(const std::string& name);
  static int resolve_dependencies(module* mod);
  static void list_plugins();
};
```

示例（与 `AGENTS.md` GPU 插件使用模式一致）：

```cpp
usr_linux_emu::ModuleLoader::load_plugins("plugins");
```

### 2.5 PluginManager - CLI 工具专用

**头文件**: `include/kernel/plugin_manager.h`  **使用场景**: **仅 CLI 工具**（`tools/cli/`）

> **重要区别**：运行时调用应使用 `ModuleLoader::load_plugins()`（§2.4）。`PluginManager` 是 CLI 模式下手动管理插件生命周期的单例。

```cpp
class PluginManager {
 public:
  static PluginManager& instance();
  int load_plugin(const std::string& path);
  int unload_plugin(const std::string& name);
  void list_plugins() const;
};
```

### 2.6 ServiceRegistry / ConfigManager / Logger

**ServiceRegistry**（`include/kernel/service_registry.h`）: 全局服务注册中心，VFS 把 `Device` 句柄按字符串名注册到 `ServiceRegistry::instance()`。

```cpp
class ServiceRegistry {
 public:
  static ServiceRegistry& instance();
  void register_service(const std::string& name, const std::shared_ptr<Device>& dev);
  void unregister_service(const std::string& name);
  void clear_services();
};
```

**ConfigManager**（`include/kernel/config_manager.h`）: 解析 `plugins/plugins.json`。

```cpp
struct PluginConfig { std::string name, path; std::vector<std::string> depends; };
class ConfigManager {
 public:
  static int load_from_file(const std::string& filename);
  static const std::unordered_map<std::string, PluginConfig>& get_configs();
};
```

**Logger**（`include/kernel/logger.h`）: 进程内统一日志接口。

```cpp
class Logger {
 public:
  enum Level { DEBUG, INFO, WARN, ERROR };
  static void set_level(Level level);
  static void log(Level level, const std::string& msg);
  static void debug(const std::string& msg);
  static void info(const std::string& msg);
  static void warn(const std::string& msg);
  static void error(const std::string& msg);
};
```

### 2.7 WaitQueue / PollWatcher

**WaitQueue**（`include/kernel/wait_queue.h`）: 基于 `std::condition_variable` 的等待队列（`FileOperations::wait_queue_` 用）。

```cpp
class WaitQueue {
 public:
  void wait();                    // 无限期等待
  void wake_up();                 // 唤醒一个等待者
  bool wait_for(int timeout_ms);  // 超时等待
};
```

**PollWatcher**（`include/kernel/poll_watcher.h`）: 单例 fd 事件订阅器，模拟 `epoll`。

```cpp
enum class EventType { Readable, Writable };
using EventCallback = std::function<void(int fd)>;
class PollWatcher {
 public:
  static PollWatcher& instance();
  void add_event(int fd, EventType type, EventCallback callback);
  void remove_event(int fd, EventType type);
  void trigger_event(int fd, EventType type);
};
```

### 2.8 linux_compat - Linux 兼容层

**统一入口**: `include/linux_compat/compat.h`（含 `types.h`、`ioctl.h`、`memory.h`、`macros.h` 和 `drm/` 子目录）。

**基础类型**（`types.h`）:

```cpp
using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using s8  = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;

void* ERR_PTR(long error);    // 错误码 → 指针
long  PTR_ERR(const void* ptr);
bool  IS_ERR(const void* ptr);
bool  IS_ERR_OR_NULL(const void* ptr);
```

**ioctl 宏**（`ioctl.h` 和 `include/kernel/ioctl.h`）:

```cpp
_IO(type, nr)
_IOR(type, nr, size) / _IOW(type, nr, size) / _IOWR(type, nr, size)
_IOC_DIR(nr) / _IOC_TYPE(nr) / _IOC_NR(nr) / _IOC_SIZE(nr)
```

通用 ioctl: `FIONREAD 0x541B`、`TIOCINQ FIONREAD`、`FIONBIO 0x5421`、`FIOASYNC 0x5452`。

**内存分配**（`memory.h`）:

```cpp
void* kmalloc(size_t size, int flags);
void* kzalloc(size_t size, int flags);
void* kcalloc(size_t n, size_t size, int flags);
void  kfree(const void* ptr);
void* vmalloc(unsigned long size);
void  vfree(const void* ptr);
```

GFP 标志: `GFP_KERNEL`、`GFP_ATOMIC`、`GFP_USER`、`GFP_DMA`、`GFP_HIGHUSER`。
页面宏: `PAGE_SHIFT 12`、`PAGE_SIZE (1UL << PAGE_SHIFT)`、`PAGE_MASK`。

**常用宏**（`macros.h`）: `BIT(x)`、`ARRAY_SIZE(x)`、`ALIGN(x, a)`、`PAGE_ALIGN(addr)`、`likely(x)/unlikely(x)`、`min/max/clamp`、`swap(a, b)`（C only）、`__packed`、`__aligned(x)`、`__maybe_unused`。

**DRM 子目录**（`linux_compat/drm/`）:

| 头文件 | 作用 | 移植替换 |
|--------|------|----------|
| `drm_ioctl.h` | `struct drm_ioctl_desc`、`drm_ioctl_compat()` 线性扫描派发 | `<drm/drm_ioctl.h>` |
| `drm_driver.h` | `struct drm_driver` 骨架（`ioctls`、`num_ioctls`、`fops`、`gem_create_object`） | `<drm/drm_drv.h>` |
| `drm_gem.h` | `struct drm_gem_object`、句柄管理 `drm_gem_handle_create/delete` | `<drm/drm_gem.h>` |

---

## §3 Layer 2 - 驱动层 (Driver)

驱动层实现设备特定的 ioctl 表分发与 HAL 接口契约。代码位于 `plugins/gpu_driver/drv/`、`hal/`、`shared/`。

### 3.1 GpgpuDevice - GPGPU 设备驱动

**头文件**: `plugins/gpu_driver/drv/gpgpu_device.h`  **继承**: `usr_linux_emu::FileOperations`

> **重要**: `GpgpuDevice` 属于**驱动层**，不是框架层。框架层（§2）不依赖此类型。

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
  size_t dispatchCount() const { return kNumIoctls; }
};
```

**关键常量**：

```cpp
static constexpr off_t QUEUE_RING_MMAP_BASE  = 0x10000;
static constexpr off_t DOORBELL_MMAP_OFFSET  = 0x20000;
static constexpr uint64_t DOORBELL_ALLOC_BASE   = 0x10000;
static constexpr uint64_t DOORBELL_ALLOC_STRIDE = 0x1000;
```

**ioctl 分发机制**：`GpgpuDevice::ioctl` 通过**静态表**分派到 13 个 handler：

```cpp
struct IoctlEntry {
  unsigned long request;
  const char* name;
  long (GpgpuDevice::*handler)(void*);
};
// 注意：`getIoctlTable()` 已于 commit `cb2f386` (2026-06-16) 移除。
// 当前唯一入口是 `getIoctlTablePtr()`（PR #19 audit 修复）。
static const IoctlEntry* getIoctlTablePtr();
```

**新增 ioctl 必须**：① 在 `gpu_ioctl.h` 定义宏；② 在 `gpgpu_device.h` 声明 `handleXxx`；③ 在 `getIoctlTablePtr()` 加一行；④ `kNumIoctls +1`。

### 3.2 IOCTL 表与 13 个命令

**头文件**: `plugins/gpu_driver/shared/gpu_ioctl.h`  **魔数**: `'G'`  **共享**: TaskRunner 通过 `TaskRunner/UsrLinuxEmu` 符号链接访问

| 编号 | 宏 | 方向 | 参数结构 |
|------|------|------|----------|
| 0x01 | `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH` | `_IOW` | `struct gpu_pushbuffer_args` |
| 0x02 | `GPU_IOCTL_REGISTER_MMU_EVENT_CB` | `_IOW` | `struct gpu_mmu_event_cb_args` |
| 0x03 | `GPU_IOCTL_REGISTER_FIRMWARE_CB` | `_IOW` | `struct gpu_firmware_cb_args` |
| 0x10 | `GPU_IOCTL_ALLOC_BO` | `_IOWR` | `struct gpu_alloc_bo_args` |
| 0x11 | `GPU_IOCTL_FREE_BO` | `_IOW` | `u32` (handle) |
| 0x12 | `GPU_IOCTL_MAP_BO` | `_IOWR` | `struct gpu_map_bo_args` |
| 0x13 | `GPU_IOCTL_WAIT_FENCE` | `_IOW` | `struct gpu_wait_fence_args` |
| 0x20 | `GPU_IOCTL_GET_DEVICE_INFO` | `_IOR` | `struct gpu_device_info` |
| 0x30 | `GPU_IOCTL_CREATE_VA_SPACE` | `_IOWR` | `struct gpu_va_space_args` |
| 0x31 | `GPU_IOCTL_DESTROY_VA_SPACE` | `_IOW` | `gpu_va_space_handle_t` |
| 0x32 | `GPU_IOCTL_REGISTER_GPU` | `_IOW` | `struct gpu_register_gpu_args` |
| 0x40 | `GPU_IOCTL_CREATE_QUEUE` | `_IOWR` | `struct gpu_queue_args` |
| 0x41 | `GPU_IOCTL_DESTROY_QUEUE` | `_IOW` | `gpu_queue_handle_t` |
| 0x42 | `GPU_IOCTL_MAP_QUEUE_RING` | `_IOWR` | `struct gpu_queue_map_ring_args` |
| 0x43 | `GPU_IOCTL_QUERY_QUEUE` | `_IOWR` | `struct gpu_queue_info_args` |

> **注**: `kNumIoctls = 13`（GpgpuDevice 表注册的 handler 数；0x02/0x03 走 callback 注册，不进表）。

**关键结构体示例**：

```cpp
struct gpu_pushbuffer_args {
  u32 stream_id;  u64 entries_addr;  u32 count;  u32 flags;  u64 fence_id;
};
struct gpu_alloc_bo_args {
  u64 size;       // 输入
  u32 domain;     // GPU_MEM_DOMAIN_VRAM=0x1, GTT=0x2, CPU=0x4
  u32 flags;      // GPU_BO_DEVICE_LOCAL=0x1, HOST_VISIBLE=0x2
  u32 handle;     // 输出
  u64 gpu_va;     // 输出
};
struct gpu_device_info {
  u32 vendor_id, device_id;
  u64 vram_size, bar0_size;
  u32 max_channels, compute_units, gpfifo_capacity, cache_line_size;
  /* Phase 1.5 扩展 */ u32 warp_size, max_clock_frequency, driver_version,
                       firmware_version, simd_count, max_memory_clock_frequency,
                       memory_bus_width, peak_fp32_gflops, pcie_bandwidth, architecture_id;
  char marketing_name[64];
};
```

完整 15 个结构体定义见 [`plugins/gpu_driver/shared/gpu_ioctl.h`](../../plugins/gpu_driver/shared/gpu_ioctl.h)。

### 3.3 gpu_hal_ops - HAL 接口契约

**头文件**: `plugins/gpu_driver/hal/gpu_hal.h`  **位置**: drv/ 与 sim/ 之间的 C ABI 边界（`extern "C"`）

11 个函数指针定义驱动与仿真之间的硬件抽象层。**移植到内核时只替换实现，不改接口。**

```c
struct gpu_hal_ops {
  void *ctx;
  /* 可能失败 → 返回 Linux 错误码 */
  int (*register_read)(void *ctx, uint64_t offset, uint64_t *out_val);
  int (*register_write)(void *ctx, uint64_t offset, uint64_t val);
  int (*mem_read)(void *ctx, uint64_t dev_addr, void *host_buf, uint64_t size);
  int (*mem_write)(void *ctx, uint64_t dev_addr, const void *host_buf, uint64_t size);
  int (*mem_alloc)(void *ctx, uint64_t size, uint64_t *out_dev_addr);
  int (*mem_free)(void *ctx, uint64_t dev_addr);
  int (*fence_create)(void *ctx, uint64_t *out_fence_id);
  int (*fence_read)(void *ctx, uint64_t fence_id, uint64_t *out_val);
  /* 弹射式操作 → void */
  void (*doorbell_ring)(void *ctx, uint32_t queue_id);
  void (*interrupt_raise)(void *ctx, uint32_t vector);
  void (*time_wait)(void *ctx, uint64_t us);
};
```

**预置实现**：

| 实现 | 头文件 | 用途 |
|------|--------|------|
| `hal_user` | `plugins/gpu_driver/hal/hal_user.h` | 真实用户态仿真（mmap heap + buddy + fences） |
| `hal_mock` | `plugins/gpu_driver/hal/hal_mock.h` | 单元测试 mock（调用计数 + 注入返回值） |

> 注：`hal_user.h` 注释旧版说"10 个回调"，代码实际 **11 个**（参见 SSOT §5 关键洞察 3）。

### 3.4 VA Space 管理 (Phase 2)

**头文件**: `plugins/gpu_driver/drv/gpgpu_device.h`

Phase 2 引入的 GPU 虚拟地址空间抽象，所有 BO 分配和 Queue 创建必须先创建 VA Space。

```cpp
struct VASpace {
  uint64_t handle;
  uint32_t page_size;     // 0=4KB, 1=64KB
  uint32_t flags;
  uint64_t created_at;
  std::vector<uint64_t> attached_queues;
};
```

| 方法 | 说明 |
|------|------|
| `createVASpace(page_size, flags, out_handle)` | 创建，输出 `gpu_va_space_handle_t` |
| `destroyVASpace(handle)` | 销毁（必须先分离所有 Queue） |
| `vaSpaceExists(handle)` | 查询 |
| `attachQueueToVASpace(va_h, q_h)` | 关联 Queue |
| `detachQueueFromVASpace(va_h, q_h)` | 分离 Queue |

handle 类型（`shared/gpu_types.h`）: `typedef u64 gpu_va_space_handle_t;` `typedef u64 gpu_queue_handle_t;`

### 3.5 Queue 管理 (Phase 2)

**头文件**: `plugins/gpu_driver/shared/gpu_queue.h`（结构）+ `drv/gpgpu_device.h`（方法）

Queue 隶属于 VA Space，用于命令提交（AMD UMQ / NVIDIA GPFIFO 模式）。

```cpp
struct gpu_ring_header {
  volatile uint32_t write_idx;   // Producer（用户态写）
  volatile uint32_t read_idx;    // Consumer（Puller 读）
  uint32_t capacity;             // 最大 1024
  uint32_t flags;
  uint64_t fence_value;
  uint8_t  reserved[32];
};
#define GPU_MAX_RING_ENTRIES 1024
struct gpu_create_queue_args {
  uint32_t queue_type;   // GPU_QUEUE_COMPUTE=0 / COPY=1
  uint32_t priority;     // 0-100
  uint32_t ring_size;
  uint32_t reserved;
  uint64_t queue_handle;
  uint64_t doorbell_pgoff;
};
enum gpu_queue_type { GPU_QUEUE_COMPUTE = 0, GPU_QUEUE_COPY = 1 };
```

**GpgpuDevice 方法**: `getQueue(queue_handle)` 返回 `shared_ptr<GpuQueueEmu>`；`removeQueue(queue_handle)` 注销 Queue。完整数据流见 SSOT §1.3。

---

## §4 Layer 3 - 仿真层 (Simulation)

仿真层实现 GPU 硬件行为模拟（拉取器、调度器、门铃、队列消费者）和底层内存分配器。代码位于 `plugins/gpu_driver/sim/` 和 `libgpu_core/`。

### 4.1 GlobalScheduler - 全局调度器

**头文件**: `plugins/gpu_driver/sim/scheduler/global_scheduler.h`

FIFO 队列 + 引擎路由（COMPUTE / COPY / FIRMWARE）。

```cpp
enum class EngineType { COMPUTE, COPY, FIRMWARE };
struct WorkItem { gpu_gpfifo_entry entry; EngineType engine; void* user_data; };

class GlobalScheduler {
 public:
  using EngineDispatchFn = std::function<void(const gpu_gpfifo_entry&, EngineType)>;
  GlobalScheduler();  ~GlobalScheduler();
  void setDispatchCallback(EngineDispatchFn fn);
  void setLaunchCallback(GpfifoToLaunchParamsTranslator::LaunchParamsCallback cb);
  void registerKernel(uint32_t kernel_idx, const char* kernel_name);
  void enqueue(const gpu_gpfifo_entry& entry, EngineType engine);
  bool dequeue(WorkItem* out_item);
  size_t queueSize() const;
  void flush();
  EngineType selectEngine(const gpu_gpfifo_entry& entry);
};
```

**回调链**（Phase 2）: `Puller → GlobalScheduler::enqueue() → GpfifoToLaunchParamsTranslator::translate() → LaunchParamsCallback(kernel_name, grid, block, shared_mem)`。

### 4.2 GpfifoToLaunchParamsTranslator - GPFIFO 翻译器

**头文件**: `plugins/gpu_driver/sim/scheduler/translator/gpfifo_translator.h`  **命名空间**: `usr_linux_emu::`

将 `gpu_gpfifo_entry` 编码转换为 TaskRunner `LaunchParams` 格式（kernel 名 + grid/block 维度）。

```cpp
class GpfifoToLaunchParamsTranslator {
 public:
  using LaunchParamsCallback = std::function<void(const char* kernel_name,
                                                  uint32_t grid_x, uint32_t grid_y, uint32_t grid_z,
                                                  uint32_t block_x, uint32_t block_y, uint32_t block_z,
                                                  uint32_t shared_mem)>;
  GpfifoToLaunchParamsTranslator();
  void setLaunchCallback(LaunchParamsCallback cb);
  void registerKernel(uint32_t kernel_idx, const char* kernel_name);
  bool translate(const gpu_gpfifo_entry& entry);
};
```

**编码约定**: `payload[1]` = grid_dim（`grid_x | (grid_y<<16) | (grid_z<<24)`），`payload[2]` = block_dim（`block_x | (block_y<<8) | (block_z<<16)`）。

### 4.3 HardwarePullerEmu - 硬件拉取器仿真

**头文件**: `plugins/gpu_driver/sim/hardware/hardware_puller_emu.h`

模拟 GPU 硬件拉取器。**有限状态机**: `IDLE → FETCH → DECODE → SCHEDULE → DISPATCH → SEMAPHORE → COMPLETE`。

```cpp
class HardwarePullerEmu {
 public:
  enum class State { IDLE, FETCH, DECODE, SCHEDULE, DISPATCH, SEMAPHORE, COMPLETE };
  HardwarePullerEmu(struct gpu_hal_ops* hal, DoorbellEmu* doorbell, GlobalScheduler* scheduler);
  ~HardwarePullerEmu();
  void start();
  void stop();
  State currentState() const;
  const char* stateName() const;
  void submitBatch(u64 gpfifo_gpu_addr, u32 entry_count);   // ioctl 路径
  void onDoorbell(u32 queue_id);                            // doorbell 路径
  void registerQueue(GpuQueueEmu* queue);
  void unregisterQueue(uint32_t queue_id);
  int  getInterruptCount() const;
  void signalSemaphore(u64 addr, u32 value);
};
```

### 4.4 DoorbellEmu - 门铃仿真

**头文件**: `plugins/gpu_driver/sim/hardware/doorbell_emu.h`

每队列门铃寄存器模拟，最多支持 1024 个队列。

```cpp
class DoorbellEmu {
 public:
  static constexpr u32 MAX_QUEUES = 1024;
  using DoorbellCallback = std::function<void(u32 queue_id)>;
  void write(u32 queue_id);          // 触发门铃
  bool poll(u32 queue_id) const;    // 查询 pending
  void acknowledge(u32 queue_id);   // 清 pending
  void setCallback(DoorbellCallback cb);
  u64  getRingCount(u32 queue_id) const;
};
```

**触发链**: `mmap 写 doorbell 偏移 → GpuQueueEmu::ringDoorbell() → DoorbellEmu::write() → HardwarePullerEmu::onDoorbell()`。

### 4.5 GpuQueueEmu - 队列仿真

**头文件**: `plugins/gpu_driver/sim/gpu_queue_emu.h`

模拟 GPU 硬件队列：管理共享内存 Ring Buffer，响应 Doorbell 触发。**不可拷贝**。

```cpp
class GpuQueueEmu {
 public:
  using DoorbellCallback = std::function<void(uint32_t queue_id)>;
  GpuQueueEmu(uint32_t queue_id, uint32_t queue_type, uint32_t priority, uint32_t ring_size);
  ~GpuQueueEmu();
  uint32_t queueId() const;
  uint32_t queueType() const;   // GPU_QUEUE_COMPUTE / COPY
  uint32_t doorbellId() const;  // 同 queueId
  uint32_t priority() const;
  uint32_t ringSize() const;
  void setDoorbellCallback(DoorbellCallback cb);
  bool dequeue(gpu_gpfifo_entry* out_entry);   // Puller 调用
  bool hasPending() const;
  uint32_t pendingCount() const;
  int attachSharedMemory(void* shm_addr, size_t size);
  struct gpu_ring_header* ringHeader() const;
  void ringDoorbell();
};
```

### 4.6 libgpu_core - 纯 C buddy allocator

**头文件**: `libgpu_core/include/gpu_buddy.h`  **实现**: `libgpu_core/buddy.c`  **接口**: **纯 C**（`extern "C"`），不是 C++ 类

零依赖 buddy allocator，可直接嵌入 Linux 内核驱动使用。**调用者负责外部同步**（无内部锁）。

```c
#define GPU_BUDDY_MIN_BLOCK_SHIFT 12   /* 4KB */
#define GPU_BUDDY_MAX_ORDER         21  /* 2^21 * 4KB = 8GB */
#define GPU_BUDDY_MAX_RECORDS       4096

struct gpu_buddy {
  uint64_t base_addr, pool_size;
  int max_order;
  struct gpu_buddy_block *free_lists[GPU_BUDDY_MAX_ORDER + 1];
  struct gpu_buddy_block block_pool[GPU_BUDDY_MAX_RECORDS + GPU_BUDDY_MAX_ORDER + 1];
  int block_pool_used;
  struct gpu_buddy_record records[GPU_BUDDY_MAX_RECORDS];
  int record_count;
};

void  gpu_buddy_init(struct gpu_buddy *buddy, uint64_t base, uint64_t size);
int   gpu_buddy_alloc(struct gpu_buddy *buddy, uint64_t size, uint64_t *out_addr);
int   gpu_buddy_free(struct gpu_buddy *buddy, uint64_t addr);
void  gpu_buddy_reset(struct gpu_buddy *buddy);
uint64_t gpu_buddy_free_size(const struct gpu_buddy *buddy);
int   gpu_buddy_allocated_count(const struct gpu_buddy *buddy);
```

示例（被 `hal_user` 用于设备内存堆）:

```c
struct gpu_buddy buddy;
gpu_buddy_init(&buddy, heap_base, heap_size);
uint64_t dev_addr;
gpu_buddy_alloc(&buddy, 4096, &dev_addr);  // 0=ok, -ENOMEM/-EINVAL
gpu_buddy_free(&buddy, dev_addr);
```

> 历史背景: `BuddyAllocator` C++ 类已**重写为 C 接口**（ADR-020）。旧类归档在 `archive/system_b_drivers/gpu/buddy_allocator.{h,cpp}`，仅作历史参考。

---

## §5 完整使用示例

下面示例展示三层 API 的协同使用流程（VA Space → BO 分配 → Queue 创建 → Pushbuffer 提交）。

```cpp
#include "kernel/vfs.h"
#include "kernel/module_loader.h"
#include "gpu_driver/shared/gpu_ioctl.h"
#include "gpu_driver/shared/gpu_types.h"
#include "gpu_driver/shared/gpu_queue.h"

int main() {
  // === Layer 1: 加载插件并打开设备 ===
  usr_linux_emu::ModuleLoader::load_plugins("plugins");
  auto dev = usr_linux_emu::VFS::instance().open("/dev/gpgpu0", O_RDWR);
  int fd = 0;  // 简化的 fd
  auto* fops = dev->fops.get();

  // === Layer 2: 查询设备信息 ===
  struct gpu_device_info info = {};
  fops->ioctl(fd, GPU_IOCTL_GET_DEVICE_INFO, &info);

  // === Phase 2: 创建 VA Space ===
  struct gpu_va_space_args va_args = {.page_size = 0, .flags = 0};
  fops->ioctl(fd, GPU_IOCTL_CREATE_VA_SPACE, &va_args);
  gpu_va_space_handle_t va_h = va_args.va_space_handle;

  // === Layer 2: 分配 BO ===
  struct gpu_alloc_bo_args bo_args = {
    .size = 4 * 1024 * 1024,        // 4MB
    .domain = GPU_MEM_DOMAIN_VRAM,  // 0x1
    .flags = GPU_BO_DEVICE_LOCAL,   // 0x1
  };
  fops->ioctl(fd, GPU_IOCTL_ALLOC_BO, &bo_args);
  u32 bo_handle = bo_args.handle;
  u64 gpu_va = bo_args.gpu_va;

  // === Phase 2: 创建 Queue ===
  struct gpu_create_queue_args q_args = {
    .queue_type = GPU_QUEUE_COMPUTE, .priority = 50, .ring_size = 256,
  };
  fops->ioctl(fd, GPU_IOCTL_CREATE_QUEUE, &q_args);
  gpu_queue_handle_t q_h = q_args.queue_handle;
  u64 doorbell_pgoff = q_args.doorbell_pgoff;

  // === Layer 2: 提交 pushbuffer 并等 fence ===
  struct gpu_pushbuffer_args pb_args = {
    .stream_id = 0, .entries_addr = gpu_va, .count = 1, .flags = 0,
  };
  fops->ioctl(fd, GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, &pb_args);
  struct gpu_wait_fence_args wf_args = {.fence_id = pb_args.fence_id, .timeout_ms = 5000};
  fops->ioctl(fd, GPU_IOCTL_WAIT_FENCE, &wf_args);
  return 0;
}
```

**底层调用链**（仿真层）:

```
ioctl → GpgpuDevice::ioctl() → getIoctlTablePtr() → handlePushbufferSubmitBatch()
  → HardwarePullerEmu::submitBatch() (FSM: IDLE→FETCH→...)
  → GlobalScheduler::enqueue() → GpfifoToLaunchParamsTranslator::translate()
  → LaunchParamsCallback（TaskRunner 接收）
  → HAL.doorbell_ring() → DoorbellEmu::write() → fence 信号
```

---

## §6 跨文档索引

| 主题 | 文档 |
|------|------|
| 架构 SSOT（含 32 项修复建议） | [`docs/02_architecture/post-refactor-architecture.md`](../02_architecture/post-refactor-architecture.md) |
| IOCTL 编号完整表 | 同上，附录 A |
| 仓库物理布局 | 同上，§1.5 |
| 关键数据流 | 同上，§1.3 |
| VA Space / Queue 数据模型 | 同上，§1.4 |
| 架构决策记录 | [`docs/00_adr/`](../00_adr/README.md)（ADR-018, 020, 021, 023, 024） |
| 框架层开发指南 | `AGENTS.md`（项目根） |
| IOCTL 命令详解 | [`docs/06-reference/ioctl-commands.md`](ioctl-commands.md) |
| 术语表 | [`docs/06-reference/glossary.md`](glossary.md) |

---

**维护者**: UsrLinuxEmu Architecture Team
**最后更新**: 2026-06-16
**对应代码 commit**: `374d463`
**状态**: 已对齐 Phase 1.5 / Phase 2 重构
