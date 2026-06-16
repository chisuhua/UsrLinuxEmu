# 添加新设备指南

> **最后验证**: 2026-06-16 (commit `374d463`)
>
> **架构 SSOT**: [`docs/02_architecture/post-refactor-architecture.md`](../02_architecture/post-refactor-architecture.md)
> **状态**: 已重写，对齐 Phase 1.5 / Phase 2 重构后的三层架构

本文档说明如何在 UsrLinuxEmu 框架里**从零添加一种新设备类型**。下面的示例基于 `drivers/sample_memory.cpp` + `drivers/sample_memory_plugin.cpp`，代码**真实可编译可运行**。

## 目录

- [§1 设备架构（重构后）](#1-设备架构重构后)
- [§2 Device 与 FileOperations 的关系](#2-device-与-fileoperations-的关系)
- [§3 步骤 1：实现 FileOperations 子类](#3-步骤-1实现-fileoperations-子类)
- [§4 步骤 2：定义 IOCTL 命令](#4-步骤-2定义-ioctl-命令)
- [§5 步骤 3：写插件入口（`module mod` 符号）](#5-步骤-3写插件入口module-mod-符号)
- [§6 步骤 4：加入 CMake 构建](#6-步骤-4加入-cmake-构建)
- [§7 步骤 5：编写 Catch2 测试](#7-步骤-5编写-catch2-测试)
- [§8 步骤 6：运行时加载与使用](#8-步骤-6运行时加载与使用)
- [§9 常见陷阱（来自 GpgpuDevice 的经验）](#9-常见陷阱来自-gpgpudevice-的经验)
- [§10 跨文档索引](#10-跨文档索引)

---

## §1 设备架构（重构后）

UsrLinuxEmu 采用**两层结构**：

```
┌─────────────────────────────────────────────┐
│              Device（VFS 注册项）             │
│   • name, dev_id, fops, plugin_handle       │
│   • 极薄包装，不定义设备行为                  │
└─────────────────────────────────────────────┘
                  ↑ shared_ptr<FileOperations>
┌─────────────────────────────────────────────┐
│         FileOperations（行为抽象）            │
│   • 7 个虚方法：open/close/read/write/       │
│     ioctl/mmap/munmap                        │
│   • 实际设备逻辑都在这里                     │
│   • MemoryDevice / SerialDevice / GpgpuDevice│
│     都继承 FileOperations                    │
└─────────────────────────────────────────────┘
```

**关键点**：

1. **不要**直接继承 `Device`（它没有可 override 的虚方法，只是 VFS 的句柄）。
2. 设备行为在 `FileOperations` 子类里；`Device` 只把它包一层注册到 VFS。
3. 注册到 VFS 的 `Device` 名字（`name`）就是 `/dev/<name>` 的 basename。例如 `Device("my0", ...)` → `/dev/my0`。

**继承层次**（实际代码）：

```
FileOperations (include/kernel/file_ops.h)
├── MemoryDevice      (include/kernel/device/memory_device.h)
│   └── SampleMemory  (drivers/sample_memory.h)
├── SerialDevice      (include/kernel/device/serial_device.h)
│   └── SampleSerialDriver (drivers/sample_serial.h)
└── GpgpuDevice       (plugins/gpu_driver/drv/gpgpu_device.h)  ← 驱动层
```

`GpgpuDevice` **不属于框架层**。它是 GPU 驱动插件的一部分（`plugins/gpu_driver/drv/`），不在 `include/kernel/` 里。

---

## §2 Device 与 FileOperations 的关系

### 2.1 `Device`（VFS 句柄）

[`include/kernel/device/device.h`](../../include/kernel/device/device.h)：

```cpp
namespace usr_linux_emu {

class FileOperations;

class Device {
 public:
  Device(const std::string& name, dev_t dev_id,
         std::shared_ptr<FileOperations> ops, void* handle);

  virtual ~Device() = default;

  std::string name;
  dev_t dev_id;
  void* plugin_handle = nullptr;

  std::shared_ptr<FileOperations> fops;
};

}  // namespace usr_linux_emu
```

**字段说明**：

| 字段 | 用途 |
|------|------|
| `name` | VFS 中的设备名（`/dev/<name>` 的 basename）|
| `dev_id` | Linux `dev_t`（主+次设备号），仅做标识用 |
| `fops` | 真正的行为；`FileOperations` 子类的 `shared_ptr` |
| `plugin_handle` | dlopen 句柄（`ModuleLoader` 内部使用，设备代码不用碰）|

### 2.2 `FileOperations`（7 个虚方法）

[`include/kernel/file_ops.h`](../../include/kernel/file_ops.h)：

```cpp
class FileOperations {
 public:
  virtual ~FileOperations() = default;

  // 默认空实现
  virtual int open(const char* path, int flags);
  virtual int close(int fd);

  // 必须实现
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

| 方法 | 是否纯虚 | 典型用途 |
|------|----------|----------|
| `open(path, flags)` | 否 | 初始化设备状态 |
| `close(fd)` | 否 | 清理资源 |
| `read(fd, buf, count)` | 否 | 阻塞读；用 `wait_queue_` 等待数据 |
| `write(fd, buf, count)` | 否 | 写入并 `wait_queue_.wake_up()` |
| `ioctl(fd, request, argp)` | **是** | 命令派发 |
| `mmap(...)` | 否 | 设备内存映射（如 GPU BO 映射）|
| `munmap(...)` | 否 | 取消映射 |

`ioctl` 是唯一**必须**实现的方法。其他按需 override。

---

## §3 步骤 1：实现 FileOperations 子类

完整示例基于 `drivers/sample_memory.cpp`（在仓库里可编译可运行）。

### 3.1 头文件

```cpp
// drivers/sample_memory.h
#pragma once
#include "kernel/device/memory_device.h"
#include "kernel/ioctl.h"

#define SAMPLE_IOC_MAGIC 'k'
#define SAMPLE_SET_MODE   _IOW(SAMPLE_IOC_MAGIC, 1, int)
#define SAMPLE_GET_STATUS _IOR(SAMPLE_IOC_MAGIC, 2, int)

namespace usr_linux_emu {

class SampleMemory : public MemoryDevice {
 public:
  SampleMemory(size_t size = 4096);
  ~SampleMemory() override = default;

  int open(const char* path, int flags) override;
  ssize_t read(int fd, void* buf, size_t count) override;
  ssize_t write(int fd, const void* buf, size_t count) override;
  long ioctl(int fd, unsigned long request, void* argp) override;

 private:
  int mode_ = 0;
  int status_ = 0;
};

}  // namespace usr_linux_emu
```

继承自 `MemoryDevice`（已有 `buffer_` 存储），只 override 需要的几个方法。

### 3.2 源文件

```cpp
// drivers/sample_memory.cpp
#include "sample_memory.h"
#include <iostream>
#include "kernel/device/device.h"
#include "kernel/module_loader.h"
#include "kernel/vfs.h"

using namespace usr_linux_emu;

SampleMemory::SampleMemory(size_t size) : MemoryDevice(size) {}

int SampleMemory::open(const char* path, int flags) {
  std::cout << "[SampleMemory] Open on path: " << path << std::endl;
  MemoryDevice::open(path, flags);
  return 0;
}

ssize_t SampleMemory::read(int fd, void* buf, size_t count) {
  std::cout << "[SampleMemory] Read (blocking/nonblocking)." << std::endl;
  return MemoryDevice::read(fd, buf, count);
}

ssize_t SampleMemory::write(int fd, const void* buf, size_t count) {
  std::cout << "[SampleMemory] Write, waking reader." << std::endl;
  return MemoryDevice::write(fd, buf, count);
}

long SampleMemory::ioctl(int fd, unsigned long request, void* argp) {
  switch (request) {
    case SAMPLE_SET_MODE:
      mode_ = *(int*)argp;
      return 0;
    case SAMPLE_GET_STATUS:
      *(int*)argp = status_;
      return 0;
    default:
      return -ENOTTY;
  }
}
```

**注意**：

- `MemoryDevice` 已经实现 `read`/`write`（带 `wait_queue_` 阻塞语义）。子类调用 `Base::read(...)` 复用。
- 不要直接继承 `Device`。**必须**继承 `FileOperations`（或它的子类）。

---

## §4 步骤 2：定义 IOCTL 命令

### 4.1 复用内核 `_IO`/`_IOR`/`_IOW` 宏

`include/kernel/ioctl.h` 已提供：

```cpp
#define _IO(type, nr)         _IOC(_IOC_NONE,  (type), (nr), 0)
#define _IOR(type, nr, size)  _IOC(_IOC_READ,  (type), (nr), sizeof(size))
#define _IOW(type, nr, size)  _IOC(_IOC_WRITE, (type), (nr), sizeof(size))
#define _IOWR(type, nr, size) _IOC(_IOC_READ|_IOC_WRITE, (type), (nr), sizeof(size))
```

### 4.2 选择 IOCTL magic number

- `'k'` (0x6B)、`'s'` (0x73)、`'M'` (0x4D) 等 ASCII 单字符。
- **不要与 System C GPU 冲突**：`GPU_IOCTL_BASE` 是 `'G'`。
- 避免与 Linux 内核通用 magic (`'k'` 已被部分内核驱动占用，但项目级无所谓)。

### 4.3 GPU 设备用 System C

GPU 设备**不要**自造 magic；统一用 `plugins/gpu_driver/shared/gpu_ioctl.h` 的 `GPU_IOCTL_*`（15 个）。完整编号表见 [`post-refactor-architecture.md` 附录 A](../02_architecture/post-refactor-architecture.md)。

---

## §5 步骤 3：写插件入口（`module mod` 符号）

**这是关键步骤。** UsrLinuxEmu 不使用任何"插件注册宏"或"插件注册函数调用"。插件入口是一个 C 链接的 `module` 结构。

### 5.1 真实 `module` 定义

`include/kernel/module_loader.h`：

```cpp
extern "C" {
typedef struct module {
  const char* name;      // 插件名
  const char** depends;  // 依赖（NULL 结尾，nullptr 表示无依赖）
  int (*init)(void);     // 加载时调用，返回 0 = 成功
  void (*exit)(void);    // 卸载时调用
} module;
}
```

### 5.2 插件入口示例

```cpp
// drivers/sample_memory_plugin.cpp
#include <iostream>
#include <memory>
#include "kernel/device/device.h"
#include "kernel/module_loader.h"
#include "kernel/vfs.h"
#include "sample_memory.h"

using namespace usr_linux_emu;

module mod = {
    .name = "sample",
    .depends = nullptr,
    .init =
        []() -> int {
          auto dev = std::make_shared<Device>("sample", 12345,
                                              std::make_shared<SampleMemory>(), nullptr);
          VFS::instance().register_device(dev);
          std::cout << "[SampleMemory] registered" << std::endl;
          return 0;
        },
    .exit =
        []() {
          std::cout << "[SampleMemory] unloaded" << std::endl;
        }};
```

**关键点**：

1. **`module` 是 C 链接结构**（`extern "C"`），所以插件源码**必须**用 `module mod = {...};` 全局对象（不能在 `extern "C"` 块外或命名空间里）。
2. **`ModuleLoader` 怎么找到它**：内部走 `dlsym(handle, "mod")` 查找 `module` 类型的 `mod` 符号。
3. **不要**用 `__attribute__((constructor))`。这是另一个不存在的机制。
4. **`init` 必须返回 0**，否则 `ModuleLoader::load_plugin` 失败。
5. **`name` 字段**：VFS 注册设备的 basename（如 `"sample"` → `/dev/sample`）。`register_device` 内部拼前缀。

### 5.3 GPU 插件的真实入口

参考 [`plugins/gpu_driver/plugin.cpp`](../../plugins/gpu_driver/plugin.cpp)（100 行），结构最清晰：

```cpp
// 节选
static int plugin_init_internal() {
  static HalHolder hal_holder;
  hal_user_init(&hal_holder.hal, &hal_holder.ctx);
  // ... HAL 初始化、scheduler 注册 kernel、设置 launch callback ...

  auto device = std::make_shared<GpgpuDevice>(&hal_holder.hal);
  device->setPuller(hal_holder.puller);

  VFS& vfs = VFS::instance();
  auto dev = std::make_shared<Device>(device->name, 0, device, nullptr);
  vfs.register_device(dev);
  return 0;
}

static void plugin_fini_internal() {
  VFS::instance().unregister_device("gpgpu0");
}

module mod = {
    .name = "gpu_driver",
    .depends = nullptr,
    .init = plugin_init_internal,
    .exit = plugin_fini_internal,
};
```

---

## §6 步骤 4：加入 CMake 构建

`drivers/CMakeLists.txt`：

```cmake
# 你的新设备插件
add_library(plugin_my_device MODULE
    my_device.cpp
    my_device_plugin.cpp
)

set_target_properties(plugin_my_device PROPERTIES
    PREFIX "" SUFFIX ".so"
    POSITION_INDEPENDENT_CODE ON
)

target_include_directories(plugin_my_device PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(plugin_my_device PRIVATE kernel)
```

`MODULE` 关键字让 CMake 生成 dlopen 友好的 .so（不链接 main，不强制 SONAME 唯一）。`PREFIX ""` + `SUFFIX ".so"` 控制输出文件名为 `plugin_my_device.so`（不是 `libplugin_my_device.so`）。

构建产物在 `build/drivers/plugin_my_device.so`。

---

## §7 步骤 5：编写 Catch2 测试

### 7.1 单元测试（不依赖 VFS）

```cpp
// tests/test_my_device.cpp
#include <catch_amalgamated.hpp>
#include "my_device.h"

using namespace usr_linux_emu;

TEST_CASE("MyDevice ioctl", "[my_device]") {
  MyDevice dev;
  int val = 0;

  SECTION("SET_MODE") {
    val = 42;
    REQUIRE(dev.ioctl(0, MY_SET_MODE, &val) == 0);
  }
  SECTION("GET_STATUS returns 0") {
    REQUIRE(dev.ioctl(0, MY_GET_STATUS, &val) == 0);
    REQUIRE(val == 0);
  }
  SECTION("unknown returns -ENOTTY") {
    REQUIRE(dev.ioctl(0, 0xDEADBEEF, nullptr) == -ENOTTY);
  }
}
```

### 7.2 集成测试（VFS + 插件）

```cpp
// tests/test_my_device_vfs.cpp
#include <catch_amalgamated.hpp>
#include "kernel/module_loader.h"
#include "kernel/vfs.h"
#include "my_device.h"

using namespace usr_linux_emu;

namespace {
struct PluginLifecycle {
  PluginLifecycle() { ModuleLoader::load_plugins("build/drivers"); }
  ~PluginLifecycle() { ModuleLoader::unload_plugins(); }
};
PluginLifecycle lifecycle;  // 全局 RAII
}

TEST_CASE("MyDevice via VFS", "[my_device][vfs]") {
  auto dev = VFS::instance().open("/dev/my0", 0);
  REQUIRE(dev != nullptr);
  REQUIRE(dev->fops != nullptr);

  int mode = 7;
  long ret = dev->fops->ioctl(0, MY_SET_MODE, &mode);
  REQUIRE(ret == 0);
}
```

### 7.3 注册到 `tests/CMakeLists.txt`

新测试加入 `CATCH2_TESTS`：

```cmake
set(CATCH2_TESTS
    test_gpu_memory.cpp
    test_gpu_mmap_bar.cpp
    test_gpu_plugin.cpp
    test_module_loader_isolation.cpp
    test_my_device.cpp          # ← 加这里
    test_my_device_vfs.cpp
)
```

`add_catch_test()`（`tests/CMakeLists.txt`）会自动链接 Catch2 单文件 + kernel 库，并注册到 ctest。

### 7.4 **不要**写 GTest 风格

```text
// ❌ 错误风格：项目用 Catch2，不安装 GTest。
//    包含 gtest 头、用 fixture / 链式断言的写法都不要照搬。
//    正确做法见上方 §7.1（TEST_CASE / REQUIRE / SECTION）。
```

Catch2 速查：`TEST_CASE`（替代 GTest 的 `TEST`/fixture 宏）、`SECTION`（隔离子例）、`REQUIRE`（致命断言）、`CHECK`（非致命断言）。

---

## §8 步骤 6：运行时加载与使用

### 8.1 加载插件

**Runtime 永远用 `ModuleLoader::load_plugins("plugins")`**（从项目根目录跑）。

```cpp
#include "kernel/module_loader.h"
#include "kernel/vfs.h"

int main() {
  // 加载项目根 plugins/ 下的所有 .so
  ModuleLoader::load_plugins("plugins");

  // 或单个 .so
  // ModuleLoader::load_plugin("build/drivers/plugin_my_device.so");

  auto dev = VFS::instance().open("/dev/my0", O_RDWR);
  // ... 使用 dev->fops->ioctl/read/write ...

  ModuleLoader::unload_plugins();
  return 0;
}
```

> **`PluginManager` 已废弃**：仅 CLI 工具使用，新代码不要调用。`PluginManager::load_plugin(path)` 与 `ModuleLoader::load_plugin(path)` 行为类似，但 CLI 之外没有维护。

### 8.2 卸载插件

```cpp
// 全部卸载
ModuleLoader::unload_plugins();

// 单个卸载
ModuleLoader::unload_plugin("my_device");  // 传 mod.name
```

卸载会调用 `mod.exit()`，你可以在那里 `VFS::instance().unregister_device("my0")`。

### 8.3 多个设备实例

```cpp
auto d0 = std::make_shared<Device>("my0", 0x9000, std::make_shared<MyDevice>(), nullptr);
auto d1 = std::make_shared<Device>("my1", 0x9001, std::make_shared<MyDevice>(), nullptr);
VFS::instance().register_device(d0);   // /dev/my0
VFS::instance().register_device(d1);   // /dev/my1
```

`dev_id` 是标识用，不强制唯一，但建议区分主+次。

---

## §9 常见陷阱（来自 GpgpuDevice 的经验）

下面是从 [`plugins/gpu_driver/drv/gpgpu_device.cpp`](../../plugins/gpu_driver/drv/gpgpu_device.cpp) 抽出的真实踩坑点。

### 9.1 ioctl 派发表必须查 `argp` 为空

```cpp
long GpgpuDevice::ioctl(int fd, unsigned long request, void* argp) {
  if (!argp) return -EINVAL;        // ← 必须检查
  // ... 派发表 ...
}
```

`ioctl` 第三个参数是用户态指针，**永远**不能信任。`nullptr` 直接 `-EINVAL`。

### 9.2 结构体成员**全部**初始化

GPU 链路踩坑：忘记把 `gpu_alloc_bo_args.handle` 设为 0，导致 `ioctl` 返回成功但 `handle` 是栈垃圾。

```cpp
struct gpu_alloc_bo_args args = {};   // ← 必须全 0 初始化（C++ value-init）
args.size   = 1024 * 1024;
args.domain = GPU_MEM_DOMAIN_VRAM;
args.flags  = 0;
args.handle = 0;                      // 即便 value-init 也要显式写
args.gpu_va = 0;
```

`= {}` 触发 C++ 值初始化（清零结构体）。这是防御性编码的最佳实践。

### 9.3 VA Space 必须先于 Queue 创建（Phase 2 强制）

GPU 链路在 Phase 2 之后要求：

```cpp
// 1. 先创建 VA Space
gpu_va_space_args va = {};
va.page_size = 0;
dev->fops->ioctl(0, GPU_IOCTL_CREATE_VA_SPACE, &va);
gpu_va_space_handle_t va_handle = va.va_space_handle;

// 2. 才能在该 VA Space 内创建 Queue
gpu_queue_args q = {};
q.va_space_handle = va_handle;       // ← 必填
dev->fops->ioctl(0, GPU_IOCTL_CREATE_QUEUE, &q);
```

不创建 VA Space 直接 `CREATE_QUEUE` 会得到 `-EINVAL`。

### 9.4 Fence 是异步的，提交后立即返回

`GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH` 返回时 GPU 还没执行完。要等完成用 `GPU_IOCTL_WAIT_FENCE`：

```cpp
gpu_pushbuffer_args pb = { /* ... */ };
dev->fops->ioctl(0, GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, &pb);
// ↑ 返回时 pb.fence_id 已填充，但 GPU 可能还在跑

gpu_wait_fence_args wait = { .fence_id = pb.fence_id, .timeout_ms = 1000, .status = 0 };
dev->fops->ioctl(0, GPU_IOCTL_WAIT_FENCE, &wait);
REQUIRE(wait.status == 0);
```

### 9.5 资源配对：分配 → 释放

GPU 链路强制每对 `ALLOC_BO` 必须配对 `FREE_BO`，否则下次 `dlopen` 加载插件时 buddy allocator 状态错乱（libgpu_core 没有持久化）：

```cpp
gpu_alloc_bo_args alloc = { .size = 1024*1024, .domain = GPU_MEM_DOMAIN_VRAM };
dev->fops->ioctl(0, GPU_IOCTL_ALLOC_BO, &alloc);
{
  // ... 使用 alloc.handle / alloc.gpu_va ...
}
u32 h = alloc.handle;
dev->fops->ioctl(0, GPU_IOCTL_FREE_BO, &h);
```

**测试里的 RAII 包装**：

```cpp
struct BoGuard {
  u32 handle;
  std::shared_ptr<Device> dev;
  BoGuard(std::shared_ptr<Device> d, u32 h) : handle(h), dev(d) {}
  ~BoGuard() { u32 h = handle; dev->fops->ioctl(0, GPU_IOCTL_FREE_BO, &h); }
};
```

### 9.6 插件只 dlopen 一次

`tests/test_gpu_memory.cpp` 用全局 `PluginLifecycle` RAII 避免每个 `TEST_CASE` 都 dlopen/dlclose。重复 dlopen 在某些 glibc 版本上会触发动态链接器缓存问题。

```cpp
struct PluginLifecycle {
  PluginLifecycle() { ModuleLoader::load_plugins("plugins"); }
  ~PluginLifecycle() { ModuleLoader::unload_plugins(); }
};
static PluginLifecycle lifecycle;   // ← 整个测试进程一次
```

### 9.7 plugin_handle 不要自己填

```cpp
// ❌ 错误
auto dev = std::make_shared<Device>("my0", 0x9000, fops, /*plugin_handle=*/this);

// ✅ 正确：传 nullptr
auto dev = std::make_shared<Device>("my0", 0x9000, fops, /*plugin_handle=*/nullptr);
```

`plugin_handle` 由 `ModuleLoader` 在 `dlopen` 时填充。插件代码里传 `nullptr`。

### 9.8 命名约定（与 `AGENTS.md` 对齐）

- 类：`PascalCase`（`MyDevice`）
- 函数 / 变量：`snake_case`（`open_device`、`buffer_size`）
- 成员变量：`snake_case_` 尾下划线（`mode_`、`status_`）
- 宏 / IOCTL：`UPPER_SNAKE_CASE`（`MY_SET_MODE`）
- 命名空间：`usr_linux_emu::`
- 头文件守卫：`#pragma once`
- 错误码：Linux 风格（`-EINVAL` / `-ENOTTY` / `-ENOMEM`）

---

## §10 跨文档索引

| 想了解什么 | 看哪里 |
|------------|--------|
| 三层架构 / Device / VFS / ModuleLoader 完整 API | [`docs/06-reference/api-reference.md`](../06-reference/api-reference.md) |
| 15 个 `GPU_IOCTL_*` 命令的参数 / 结构体 / 示例 | [`docs/06-reference/ioctl-commands.md`](../06-reference/ioctl-commands.md) |
| 从环境搭建到跑通第一个测试的完整步骤 | [`guide.md`](guide.md) |
| 架构 SSOT（三层图、IOCTL 编号表、Phase 时间轴）| [`docs/02_architecture/post-refactor-architecture.md`](../02_architecture/post-refactor-architecture.md) |
| GPU 驱动内部（`GpgpuDevice` / HAL / sim / scheduler）| [`docs/05-advanced/gpu_driver_architecture.md`](../05-advanced/gpu_driver_architecture.md) |
| 编码风格详细规范 | [`docs/03-development/coding-style.md`](coding-style.md) + [`AGENTS.md`](../../AGENTS.md) |
| TaskRunner 对接 | [`docs/07-integration/taskrunner-index.md`](../07-integration/taskrunner-index.md) |
| 架构决策（ADR-018~024 驱动/HAL/Queue/VA Space）| [`docs/00_adr/`](../00_adr/) |

**canonical 示例文件**：

| 想看 | 路径 |
|------|------|
| 最简设备（10 行插件）| [`drivers/sample_memory_plugin.cpp`](../../drivers/sample_memory_plugin.cpp) |
| 内存设备（FileOperations 子类）| [`drivers/sample_memory.cpp`](../../drivers/sample_memory.cpp) + [`.h`](../../drivers/sample_memory.h) |
| 串口设备 | [`drivers/sample_serial.cpp`](../../drivers/sample_serial.cpp) + [`.h`](../../drivers/sample_serial.h) |
| GPU 设备（最完整示例）| [`plugins/gpu_driver/plugin.cpp`](../../plugins/gpu_driver/plugin.cpp) + [`drv/gpgpu_device.h`](../../plugins/gpu_driver/drv/gpgpu_device.h) |
| Catch2 测试 | [`tests/test_gpu_memory.cpp`](../../tests/test_gpu_memory.cpp) |
| Standalone 测试 | [`tests/test_module_load_and_vfs.cpp`](../../tests/test_module_load_and_vfs.cpp) |

---

**最后更新**: 2026-06-16
**对应 commit**: `374d463`
**维护者**: UsrLinuxEmu Team
