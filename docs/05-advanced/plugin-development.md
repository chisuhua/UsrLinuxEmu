# 插件开发指南

> **最后验证**: 2026-06-16 (commit `758b39c` — PR #16 后)
>
> **SSOT**: 本文档与 [docs/02_architecture/post-refactor-architecture.md §1.5](../02_architecture/post-refactor-architecture.md) 和 [AGENTS.md](../../AGENTS.md) 对齐。如冲突，以 SSOT 为准。
>
> **状态**: ✅ 与 Phase 1.5 / Phase 2 重构后代码一致
>
> **重要更新 (2026-06-16)**:
> - §2.1 / §2.2 / §4 Step 4 / §7.4：**修正 plugins.json 误导**——`ModuleLoader` **不读** `plugins/plugins.json`（PR #15 证实），它是手工维护的 dead documentation。loader 只扫目录里的 `plugin_*.so`。
> - §3.4 新增「**dlclose + shared_ptr 陷阱**」——每个 plugin author **必须**在 `mod->exit()` 里撤销 `init()` 注册的所有 kernel-side state（PR #16 fix）。

本文介绍如何在 UsrLinuxEmu 中开发、构建、加载一个设备插件。读完之后，你应该能：

- 解释 `module mod` 符号的 ABI 契约
- 用 `ModuleLoader::load_plugins("plugins")` 加载插件目录
- 用 `VFS::instance().register_device(...)` 注册一个设备
- **安全地写 `mod->exit()`**（避免 dlclose 后的 SEGFAULT）
- 写一个最小可编译的插件并接入 Catch2 测试

---

## 1. 概述

UsrLinuxEmu 用 `dlopen` + `dlsym` 把设备驱动作为共享库动态加载。内核模拟框架和驱动代码之间通过两个 C ABI 符号耦合：

- `module mod` 符号：插件的元数据 + 生命周期回调（`init` / `exit`）
- `VFS::instance()` 单例：插件把设备对象挂到全局文件系统

运行时**只有一条加载路径**：`ModuleLoader::load_plugins(dir)` 扫描 `dir` 子目录、找所有 `plugin_*.so`、逐个 `dlopen`、`dlsym(handle, "mod")`、调用 `mod->init()`。**注意：`plugins/plugins.json` 不是 loader 输入**——它是手工维护的 dead documentation（loader 不读它），只用于给开发者/工具查 plugin 路径。

`PluginManager` 是 **CLI 工具**（`tools/cli/`）用来手动加载/卸载单个 `.so` 的接口。它不是运行时 API，不要在生产代码或测试里使用。

---

## 2. 架构

### 2.1 加载流程

```
main() / test fixture
    ↓ ModuleLoader::load_plugins("plugins")        (静态 API, dir 参数任意)
扫描 dir/ 子目录（filesystem::directory_iterator）
    ↓
对每个匹配 plugin_*.so 的文件:
    ├── dlopen(path)
    ├── dlsym(handle, "mod") → struct module*
    ├── 校验 mod != nullptr（否则 dlclose + continue）
    └── load_plugin(path)：
         ├── resolve_dependencies(mod)
         ├── mod->init()
         └── 记录到 ModuleLoader::loaded_plugins_

注：loader 完全不读 plugins/plugins.json。该文件可保留作为
    documentation，但删掉也不会影响加载行为。
```

### 2.2 核心组件

| 组件 | 头文件 | 角色 |
|------|--------|------|
| `ModuleLoader` | `include/kernel/module_loader.h` | 静态 API 加载/卸载插件；`load_plugins(dir)` 是入口 |
| `module` | `include/kernel/module_loader.h` | 插件 ABI 契约（C 结构体）|
| `VFS` | `include/kernel/vfs.h` | Meyers 单例，存放 `name → Device` 映射 |
| `Device` | `include/kernel/device/device.h` | 设备容器，持有 `FileOperations` 智能指针 |
| `FileOperations` | `include/kernel/file_ops.h` | 驱动实现 `open/close/read/write/ioctl/mmap` |
| `plugins.json` | `plugins/plugins.json` | **dead documentation**（loader 不读）。保留以方便查阅插件路径，但新增插件不需要改它 |

### 2.3 VFS 路径约定

`VFS::register_device(dev)` 把 `dev->name` 存进哈希表，调用 `VFS::open("/dev/<name>", flags)` 时查找。名字里不要带 `/dev/` 前缀。

```cpp
auto dev = std::make_shared<Device>("gpgpu0", 0, my_fops, nullptr);
VFS::instance().register_device(dev);
// 用户调用：VFS::instance().open("/dev/gpgpu0", O_RDWR)
```

---

## 3. `module mod` 符号契约

每个插件 `.so` 必须导出 C 链接的 `module` 符号。这是 `ModuleLoader` 唯一识别的入口。

### 3.1 `module` 结构体

定义在 `include/kernel/module_loader.h`：

```cpp
extern "C" {
typedef struct module {
  const char* name;      // 插件名（VFS 唯一标识）
  const char** depends;  // 依赖列表，NULL 结尾；无依赖写 nullptr
  int (*init)(void);     // 加载时调用；返回 0 成功，负数 errno
  void (*exit)(void);    // 卸载时调用
} module;
}
```

### 3.2 生命周期

| 阶段 | 触发 | 调用 |
|------|------|------|
| 加载 | `ModuleLoader::load_plugins()` → `dlopen` | `mod->init()` |
| 卸载 | `ModuleLoader::unload_plugins()` → `dlclose` | `mod->exit()` |

`init` 在 `dlopen` 返回后**同步**调用。它负责：

- 创建 `FileOperations` 子类实例
- 包装成 `std::shared_ptr<Device>`
- 调用 `VFS::instance().register_device(dev)`

`exit` 负责反向操作：注销设备、释放资源。`init` 失败时必须返回负数 errno，ModuleLoader 会 `dlclose` 句柄并跳过该插件。

### 3.3 与旧模式的区别

| 旧（已废弃） | 新（当前） |
|--------------|------------|
| `__attribute__((constructor)) void init_module()` | `module mod = {.init = lambda}` |
| 旧声明式注册宏（`REGISTER_*_PLUGIN` 系列） | `extern "C" module mod` 全局符号 |
| `PluginManager`（CLI 工具的单插件加载方法）| `ModuleLoader::load_plugins("plugins")` |
| `VFS::register_device("/dev/foo", dev)` 静态 | `VFS::instance().register_device(dev)` 实例 |

### 3.4 ⚠️ dlclose + shared_ptr 陷阱（**必读**）

**这是所有 plugin author 都必须踩过的坑**。`mod->exit()` 不是装饰，它**必须**撤销 `init()` 在 kernel-side 注册的所有 state。否则**进程退出时 SEGFAULT**。

#### 3.4.1 现象

在 `mod->exit()` 返回后，`ModuleLoader::decrease_ref` 会执行 `dlclose(handle)`。如果有任何 `shared_ptr` 仍指向 plugin 内的代码（C++ 对象、vtable 等），进程退出时它们的析构会跳转到已 unmap 的内存：

```
Program received signal SIGSEGV, Segmentation fault.
#0  std::_Sp_counted_base<...>::_M_release
#1  std::__shared_count<...>::~__shared_count
#2  std::__shared_ptr<Device, ...>::~__shared_ptr
#3  std::shared_ptr<Device>::~shared_ptr
#4  main () at test_my_plugin.cpp:47
```

#### 3.4.2 根因

`VFS::instance().register_device(dev)` 把 `dev` 的 shared_ptr 存进 `devices_` map。`Device` 持有 `shared_ptr<FileOperations> fops`，`FileOperations` 子类（如 `SampleMemory`）的虚函数表指向 **plugin .so 内的代码**。

```
Device (kernel lib)        fops shared_ptr
    │
    └── FileOperations (kernel lib, 虚函数)
            │
            └── SampleMemory / SampleSerialDriver / etc. (plugin .so)
                   ^
                   └── vtable entry → plugin .so code
```

`init()` 之后 `dlclose()` 会 unmap plugin .so。**任何**指向 plugin 代码的对象（包括 vtable 中的函数指针、虚函数调用）都会跳转到 unmapped 内存。

#### 3.4.3 解药

**plugin 侧**（`mod->exit()` 必须**撤销** `init()` 注册的所有 kernel-side state）：

```cpp
// drivers/my_plugin/plugin.cpp
module mod = {
    .name = "my_plugin",
    .init = []() -> int {
      auto dev = std::make_shared<Device>(
          "mydev0", 0, std::make_shared<MyDriver>(), nullptr);
      VFS::instance().register_device(dev);
      return 0;
    },
    .exit = []() {
      // 必须调用！撤销 init() 注册的设备，否则 dlclose 后 VFS 的析构
      // 会通过 fops shared_ptr → plugin .so 的 vtable 跳到 unmapped 内存。
      VFS::instance().unregister_device("mydev0");
    },
};
```

**测试侧**（plugin 的 `shared_ptr<Device>` 必须**先于** `unload_plugins()` 析构）：

```cpp
TEST_CASE("...", "[plugin][my_plugin]") {
  ModuleLoader::load_plugins("plugins");

  {
    auto dev = VFS::instance().open("/dev/mydev0", 0);
    REQUIRE(dev);
    // ... use dev ...
  }  // ⬅ dev 在这里析构；.so 还加载着，~fops 安全

  ModuleLoader::unload_plugins();  // 现在安全：没人引用 plugin 代码
}
```

#### 3.4.4 通用规则

任何在 `init()` 里调用过、`影响 kernel-side 状态`的操作，**必须在 `exit()` 里反向调用一次**：

| init() 调用 | exit() 必须反向调用 |
|------------|--------------------|
| `VFS::instance().register_device(dev)` | `VFS::instance().unregister_device(name)` |
| `ServiceRegistry::instance().register_service(...)` | `ServiceRegistry::instance().unregister_service(...)` |
| 启动后台线程 | `pthread_join` 或标记 stop 并等待 |
| `mmap` shared memory | `munmap` |

#### 3.4.5 真实案例

PR #16（commit `8fb4f49`）就是这个 bug 的修复。`drivers/sample_serial.cpp` 和 `drivers/sample_memory_plugin.cpp` 的 `mod->exit()` 之前是 no-op：

```cpp
.exit = []() { std::cout << "[SampleSerial] Module exited.\n"; }
// 没有 unregister_device → 测试退出时 SEGFAULT
```

修复后 `tests/test_serial_ioctl_standalone` 和 `tests/test_poll_standalone` 从 SEGFAULT 变成干净退出（33/33 tests pass）。

---

## 4. 开发流程

### 步骤 1: 实现 `FileOperations` 子类

`FileOperations` 是插件的核心。继承它，实现你的设备方法：

```cpp
// drivers/my_plugin/my_device.h
#pragma once

#include "kernel/device/memory_device.h"   // 或自定义基类
#include "kernel/file_ops.h"

namespace usr_linux_emu {

class MyDevice : public MemoryDevice {  // 或直接继承 FileOperations
 public:
  explicit MyDevice(size_t size = 4096);
  ~MyDevice() override = default;

  int open(const char* path, int flags) override;
  ssize_t read(int fd, void* buf, size_t count) override;
  ssize_t write(int fd, const void* buf, size_t count) override;
  long ioctl(int fd, unsigned long request, void* argp) override;

 private:
  int state_ = 0;
};

}  // namespace usr_linux_emu
```

注意签名细节（与 `docs/03-development/adding-devices.md` 旧版不同）：

- `open(const char* path, int flags)`，**不是** `open(int fd, int flags)`
- `read/write` 的 `fd` 来自 VFS 调用，**不要**把它当 `this`
- `ioctl` 返回 Linux 风格负数 errno；`0` = 成功

### 步骤 2: 写插件入口（`module mod` 符号）

```cpp
// drivers/my_plugin/plugin.cpp
#include <iostream>
#include <memory>
#include "kernel/device/device.h"
#include "kernel/module_loader.h"
#include "kernel/vfs.h"
#include "my_device.h"

using namespace usr_linux_emu;

extern "C" {

module mod = {
    .name    = "my_plugin",
    .depends = nullptr,
    .init    = []() -> int {
      std::cout << "[MyPlugin] init\n";
      auto mydev = std::make_shared<Device>(
          "mydev0",           // VFS 名字（无 /dev/ 前缀）
          0x1234,             // dev_t id
          std::make_shared<MyDevice>(),
          nullptr);           // dlopen handle（插件内通常留空）
      VFS::instance().register_device(mydev);
      return 0;
    },
    .exit    = []() {
      std::cout << "[MyPlugin] exit\n";
      VFS::instance().unregister_device("mydev0");
    },
};

}  // extern "C"
```

要点：

- `module mod` 必须是 `extern "C"` 块里的全局变量，`dlsym` 才能按名字找到
- 不要再用 `__attribute__((constructor))`，ModuleLoader 不依赖它
- 不要再用旧的声明式注册宏（`REGISTER_*_PLUGIN` 系列）
- `init` 内的 lambda **捕获要小心**：不要捕获本地栈变量的指针

### 步骤 3: `CMakeLists.txt`

插件产物是 `SHARED` 库，输出到 `build/bin/plugins/<name>/<name>.so`：

```cmake
# drivers/my_plugin/CMakeLists.txt
add_library(my_plugin SHARED
    my_device.cpp
    plugin.cpp
)

target_include_directories(my_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/drivers
)

target_link_libraries(my_plugin PRIVATE kernel)

set_target_properties(my_plugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/plugins/my_plugin
    PREFIX ""                # 去掉 lib 前缀
)
```

`kernel` 库必须为 `SHARED`（参见 `src/CMakeLists.txt`）。原因：`VFS::instance()` 是函数内 `static` 局部变量（Meyers 单例），如果 kernel 是 `STATIC`，可执行文件和插件会各持有一份独立副本，导致 VFS 状态割裂（Issue #11）。

### 步骤 4: （可选）在 `plugins/plugins.json` 加条目

**注意：loader 不读这个文件**（PR #15 证实），它是手工维护的 dead documentation。加条目仅供开发者/工具查询插件路径。**新增插件不强制更新它**。

如果要更新，格式：

```json
{
  "_note": "Documentation only — ModuleLoader does not read this file.",
  "plugins": [
    {
      "name": "my_plugin",
      "path": "build/drivers/my_plugin.so",
      "depends": []
    }
  ]
}
```

字段说明：

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `name` | string | ✅ | 插件逻辑名，用于日志/CLI 显示 |
| `path` | string | ✅ | build 产物的相对路径（**不是** plugin 源路径） |
| `depends` | array of string | ❌ | 文档化用，loader 不读 |

### 步骤 5: 写 Catch2 测试

测试用 Catch2（vendored 单文件，路径 `tests/catch_amalgamated.hpp`）。每个测试 binary 是一个独立可执行文件。

```cpp
// tests/test_my_plugin.cpp
#include "catch_amalgamated.hpp"
#include "kernel/module_loader.h"
#include "kernel/vfs.h"

using namespace usr_linux_emu;

namespace {
struct PluginLifecycle {
  PluginLifecycle() { ModuleLoader::load_plugins("plugins"); }
  ~PluginLifecycle() { ModuleLoader::unload_plugins(); }
};
PluginLifecycle g_keep_alive;  // 进程期内只加载一次
}  // namespace

TEST_CASE("MyPlugin registers /dev/mydev0", "[plugin][my_plugin]") {
  auto dev = VFS::instance().open("/dev/mydev0", 0);
  REQUIRE(dev != nullptr);
  REQUIRE(dev->name == "mydev0");
  REQUIRE(dev->fops != nullptr);
}

TEST_CASE("MyPlugin read returns expected payload", "[plugin][my_plugin]") {
  auto dev = VFS::instance().open("/dev/mydev0", 0);
  REQUIRE(dev != nullptr);

  char buf[64] = {0};
  ssize_t n = dev->fops->read(0, buf, sizeof(buf));
  REQUIRE(n > 0);
  REQUIRE(std::string(buf, n) == "hello from my plugin");
}
```

运行测试时**必须从项目根目录**启动：

```bash
cd /workspace/project/UsrLinuxEmu
./build/bin/test_my_plugin_standalone
```

`ModuleLoader::load_plugins("plugins")` 用相对路径。如果从 `build/bin/` 跑，路径解析会失败。

**Plugin 生命周期管理**：参考 `tests/test_serial_ioctl.cpp` 和 `tests/test_poll.cpp`（PR #16 后的正确模式）：

```cpp
TEST_CASE("plugin lifecycle", "[plugin][my_plugin]") {
  ModuleLoader::load_plugins("plugins");  // load once per test

  {
    auto dev = VFS::instance().open("/dev/mydev0", 0);
    REQUIRE(dev);
    // ... use dev ...
  }  // dev shared_ptr destroyed HERE — BEFORE unload_plugins

  ModuleLoader::unload_plugins();  // safe: no shared_ptrs into plugin code
}
```

**绝对不要**：让 `dev` (或其他 plugin shared_ptr) 跨越 `unload_plugins()` 边界。这会导致**进程退出时的 SEGFAULT**（详见 §3.4 dlclose + shared_ptr 陷阱）。

---

## 5. 完整示例 1：最小可编译插件（`sample_memory`）

这是项目里的参考实现，位于 `drivers/sample_memory.cpp` + `drivers/sample_memory_plugin.cpp`。

### 5.1 设备类

```cpp
// drivers/sample_memory.h
#pragma once
#include "kernel/device/memory_device.h"
#include "kernel/ioctl.h"

#define SAMPLE_IOC_MAGIC 'k'
#define SAMPLE_SET_MODE  _IOW(SAMPLE_IOC_MAGIC, 1, int)
#define SAMPLE_GET_STATUS _IOR(SAMPLE_IOC_MAGIC, 2, int)

namespace usr_linux_emu {

class SampleMemory : public MemoryDevice {
 public:
  explicit SampleMemory(size_t size = 4096);
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

`MemoryDevice` 提供了 `WaitQueue` 阻塞读模型的默认实现，`SampleMemory` 在此基础上加 ioctl 支持。

### 5.2 插件入口

```cpp
// drivers/sample_memory_plugin.cpp
#include <iostream>
#include <memory>
#include "kernel/device/device.h"
#include "kernel/module_loader.h"
#include "kernel/vfs.h"
#include "sample_memory.h"

using namespace usr_linux_emu;

module mod = {.name = "sample",
              .depends = nullptr,
              .init    = []() -> int {
                auto mydev = std::make_shared<Device>(
                    "sample", 12345,
                    std::make_shared<SampleMemory>(), nullptr);
                VFS::instance().register_device(mydev);
                std::cout << "[SampleMemory] Module initialized.\n";
                return 0;
              },
              .exit    = []() {
                std::cout << "[SampleMemory] Module exited.\n";
              }};
```

注意 `module mod` **没有**包在 `extern "C"` 块里也能编译通过（C++ 全局变量对 C ABI 已经是 name-mangled，但 `module` 类型本身是 `extern "C"` 声明的，所以类型 OK；这里靠 `module` typedef 的 C-linkage 解决）。但 GPU 插件用了 `extern "C"` 包裹以更明确，推荐也用同样写法。

### 5.3 测试

```cpp
// tests/test_sample_memory_plugin.cpp
#include "catch_amalgamated.hpp"
#include "kernel/module_loader.h"
#include "kernel/vfs.h"

using namespace usr_linux_emu;

namespace {
struct Lifecycle {
  Lifecycle() { ModuleLoader::load_plugins("plugins"); }
  ~Lifecycle() { ModuleLoader::unload_plugins(); }
};
Lifecycle keep;
}

TEST_CASE("sample_memory registers /dev/sample", "[plugin][sample]") {
  auto dev = VFS::instance().open("/dev/sample", 0);
  REQUIRE(dev);
  REQUIRE(dev->fops);
}
```

### 5.4 CMake 片段

`drivers/CMakeLists.txt` 中的对应部分：

```cmake
add_library(sample_memory SHARED
    sample_memory.cpp
    sample_memory_plugin.cpp
)
target_include_directories(sample_memory PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/drivers
)
target_link_libraries(sample_memory PRIVATE kernel)
set_target_properties(sample_memory PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/drivers
    PREFIX ""
)
```

`plugins/plugins.json` 的对应 entry：

```json
{
  "name": "memory",
  "path": "drivers/sample_memory.so",
  "depends": []
}
```

---

## 6. 完整示例 2：GPU 驱动插件（四层架构）

GPU 插件是项目里**最完整**的参考实现。它展示了 `FileOperations` 之上的多层抽象：`GpgpuDevice`（drv/）调用 `gpu_hal_ops`（hal/）调度 `HardwarePullerEmu` 和 `GlobalScheduler`（sim/）。

完整源码在 `plugins/gpu_driver/plugin.cpp`。下面是简化版，只保留插件结构相关的部分：

```cpp
// plugins/gpu_driver/plugin.cpp（简化）
#include <iostream>
#include <memory>
#include "kernel/device/device.h"
#include "kernel/module_loader.h"
#include "kernel/vfs.h"
#include "drv/gpgpu_device.h"
#include "hal/hal_user.h"
#include "sim/hardware/doorbell_emu.h"
#include "sim/hardware/hardware_puller_emu.h"
#include "sim/scheduler/global_scheduler.h"

using namespace usr_linux_emu;

namespace {
// 仿真子系统持有者：HAL 表 + 上下文 + 调度器 + 拉取器
struct HalHolder {
  struct gpu_hal_ops hal;
  struct hal_user_context ctx;
  DoorbellEmu doorbell;
  GlobalScheduler scheduler;
  std::shared_ptr<HardwarePullerEmu> puller;
};
HalHolder* g_hal = nullptr;  // 进程级单例
}  // namespace

extern "C" {

static int plugin_init_internal() {
  static HalHolder hal_holder;
  hal_user_init(&hal_holder.hal, &hal_holder.ctx);
  g_hal = &hal_holder;

  // 拉取器 + 调度器装配
  hal_holder.puller = std::make_shared<HardwarePullerEmu>(
      &hal_holder.hal, &hal_holder.doorbell, &hal_holder.scheduler);

  hal_holder.scheduler.registerKernel(0, "simple_kernel");
  hal_holder.scheduler.registerKernel(1, "matmul_kernel");
  hal_holder.puller->start();

  // 设备对象：FileOperations 子类 + 仿真子系统
  auto gpgpu = std::make_shared<GpgpuDevice>(&hal_holder.hal);
  gpgpu->setPuller(hal_holder.puller);

  auto dev = std::make_shared<Device>(gpgpu->name, 0, gpgpu, nullptr);
  VFS::instance().register_device(dev);

  std::cout << "[GpuPlugin] Registered /dev/" << gpgpu->name << "\n";
  return 0;
}

static void plugin_fini_internal() {
  if (g_hal && g_hal->puller) g_hal->puller->stop();
  g_hal = nullptr;
  VFS::instance().unregister_device("gpgpu0");
}

module mod = {
    .name    = "gpu_driver",
    .depends = nullptr,
    .init    = plugin_init_internal,
    .exit    = plugin_fini_internal,
};

}  // extern "C"
```

### 6.1 与最小示例的差异

| 维度 | sample_memory | gpu_driver |
|------|---------------|------------|
| `FileOperations` 子类 | `SampleMemory : MemoryDevice` | `GpgpuDevice : FileOperations`（自定义）|
| 仿真子系统 | 无 | HAL + DoorbellEmu + HardwarePullerEmu + GlobalScheduler |
| 持有者 | lambda 直构 | `HalHolder` 命名空间 + `static` 局部 |
| ioctl 协议 | 自定义（`SAMPLE_*`）| System C（`GPU_IOCTL_*`，见 `plugins/gpu_driver/shared/gpu_ioctl.h`）|
| 注册名 | `sample` | `gpgpu0` |
| 退出时额外动作 | 无 | `puller->stop()`、清仿真子系统 |

### 6.2 测试形态

GPU 插件的 Catch2 测试用 `TEST_CASE_METHOD` + RAII fixture，参考 `tests/test_gpu_plugin.cpp`：

```cpp
class GpuPluginFixture {
 public:
  GpuPluginFixture() : fd_(0) {
    device_ = VFS::instance().open("/dev/gpgpu0", 0);
  }
  long ioctl(unsigned long request, void* arg) {
    return device_->fops->ioctl(fd_, request, arg);
  }
  std::shared_ptr<Device> device_;
  int fd_;
};

TEST_CASE_METHOD(GpuPluginFixture, "GPU_IOCTL_GET_DEVICE_INFO", "[gpu][ioctl]") {
  struct gpu_device_info info = {};
  REQUIRE(ioctl(GPU_IOCTL_GET_DEVICE_INFO, &info) == 0);
  REQUIRE(info.vendor_id == 0x1000);
  REQUIRE(info.vram_size == 8ULL * 1024 * 1024 * 1024);
}
```

注意：所有 GPU ioctl 编号与结构体都来自 `plugins/gpu_driver/shared/gpu_ioctl.h`（System C）。**不要**用旧的 System B 前缀（`GPG` 开头的宏，已归档到 `archive/system_b_drivers/gpu/`）或 `LAUNCH_CB`（已删除，commit `b78edc9`）。

---

## 7. 调试

### 7.1 验证符号导出

```bash
# 检查 mod 符号
nm -D build/bin/plugins/gpu_driver/gpu_driver_plugin.so | grep " mod"
# 期望输出：0000... B mod  （B = uninitialized data segment）

# 检查 init/exit 函数指针是否正确填充
objdump -d build/bin/plugins/gpu_driver/gpu_driver_plugin.so | grep plugin_init_internal
```

### 7.2 检查动态依赖

```bash
ldd build/bin/plugins/gpu_driver/gpu_driver_plugin.so
```

确保 `libkernel.so` 指向 `build/bin/libkernel.so`。如果显示 `not found`，说明 `LD_LIBRARY_PATH` 没设，或 `kernel` 库没编成 SHARED。

### 7.3 用 lldb 跟踪 `dlopen`

```bash
lldb -- build/bin/test_gpu_plugin_standalone
(lldb) break set -n plugin_init_internal
(lldb) run
(lldb) bt   # 加载时的调用栈
```

### 7.4 排查 "Device not found"

按顺序检查：

1. `.so` 文件是否在调用 `load_plugins(dir)` 传入的目录里（**loader 不查 JSON**）
2. `.so` 文件名是否以 `plugin_` 开头（loader 用这个前缀过滤）
3. `.so` 是否是 MODULE 库（`add_library(... MODULE ...)`）
4. `ModuleLoader::load_plugins` 是否被调用（在 `main` 或 fixture）
5. 测试是从**项目根目录**启动的（`WORKING_DIRECTORY` 默认是项目根）
6. `mod->init()` 是否返回 0（用 `std::cerr` 加日志；返回负数 errno 会被 loader 跳过）

### 7.5 排查"进程退出时 SEGFAULT"

按 §3.4 检查：

1. `mod->exit()` 是否撤销了 `init()` 在 kernel-side 注册的所有 state
2. 测试代码中 plugin `shared_ptr`（`auto dev = ...`）是否在 `ModuleLoader::unload_plugins()` **之前**就被析构（用 scope block）
3. 用 gdb 在 SEGFAULT 时 `bt` 看 `_M_release` → `~shared_ptr` 链，确认是 plugin vtable 调用

---

## 8. 最佳实践

### 8.1 错误处理

`init` 失败时返回负数 errno，ModuleLoader 会跳过该插件：

```cpp
.init = []() -> int {
  if (!hal_available()) {
    std::cerr << "[MyPlugin] HAL not ready\n";
    return -ENODEV;
  }
  // ... 注册
  return 0;
}
```

`exit` 不应抛异常或返回错误（签名是 `void`）。如果清理失败，用日志记录。

### 8.2 资源所有权

- 设备对象用 `std::shared_ptr` 持有，自动随 VFS 释放
- 仿真子系统（GPU 的 `HalHolder`）用 `static` 局部变量或 `g_hal` 指针，避免 `init`/`exit` 之间的栈逃逸
- 不要把 `this` 指针捕获到 `init` lambda 里

### 8.3 线程安全

`init` 在主线程同步执行，但设备方法（`read/write/ioctl`）可能被多线程调用。共享状态加 `std::mutex`：

```cpp
class ThreadSafeDevice : public MemoryDevice {
 private:
  std::mutex mu_;
  std::atomic<bool> open_{false};
 public:
  int open(const char* path, int flags) override {
    std::lock_guard<std::mutex> lock(mu_);
    if (open_) return -EBUSY;
    open_ = true;
    return 0;
  }
};
```

### 8.4 命名

- 插件名（小写，下划线）：`gpu_driver`, `sample_memory`
- 设备名（小写，无前缀）：`gpgpu0`, `sample`
- VFS 路径：自动补 `/dev/`，**不要**自己写

### 8.5 避免的写法

- ❌ `__attribute__((constructor))`（ModuleLoader 不会触发它）
- ❌ 旧的声明式注册宏（`REGISTER_*_PLUGIN` 系列，已删除）
- ❌ `VFS::register_device("/dev/foo", dev)` 静态调用（接口变了）
- ❌ `PluginManager` 的单插件加载方法在测试/生产代码里（CLI only）
- ❌ 在 `init` lambda 里捕获本地栈指针

---

## 9. 常见问题

### Q: `ModuleLoader::load_plugins` 返回非零，但 `dlerror` 没显示？

`load_plugins` 只返回第一个失败的插件的 errno（负数）。具体哪个 `.so` 失败，加日志：

```cpp
// 临时调试
int rc = ModuleLoader::load_plugins("plugins");
std::cerr << "load_plugins returned " << rc << "\n";
```

### Q: 两个插件都注册同名设备会怎样？

后注册的覆盖前者。ModuleLoader 不做去重检查；命名要保证唯一。

### Q: 插件之间如何共享代码？

把共享代码放到 `libgpu_core/`（参考 ADR-020）或 `drivers/common/`，在两个插件的 `CMakeLists.txt` 里 `target_link_libraries` 同一个库。**不要**让插件 A `dlopen` 插件 B 拿符号，ABI 容易破坏。

### Q: 可以在插件里 `fork()` 或启动线程吗？

可以，但注意：

- 子进程会继承 VFS，但 dlopen 句柄可能失效
- 后台线程对 VFS 的写入需要加锁（VFS 内部已用 `std::mutex` 保护 `devices_`）
- 测试结束前 `ModuleLoader::unload_plugins()` 会 `dlclose`，确保线程已退出

### Q: `kernel` 库必须是 SHARED 吗？

是的。这是 Issue #11 修复后的硬约束。`src/CMakeLists.txt` 中 `add_library(kernel SHARED ...)` 不可改为 STATIC，否则 VFS 单例在可执行文件与插件之间割裂。

---

## 10. 相关文档

- 架构 SSOT：[docs/02_architecture/post-refactor-architecture.md §1.5](../02_architecture/post-refactor-architecture.md)
- 开发指南：[AGENTS.md](../../AGENTS.md)
- 设备开发：[docs/03-development/adding-devices.md](../03-development/adding-devices.md)
- 测试框架：[docs/04-building/testing_guide.md](../04-building/testing_guide.md)
- IOCTL 编号：[docs/02_architecture/post-refactor-architecture.md 附录 A](../02_architecture/post-refactor-architecture.md)
- GPU 插件源码：[plugins/gpu_driver/plugin.cpp](../../plugins/gpu_driver/plugin.cpp)
- 最小插件源码：[drivers/sample_memory_plugin.cpp](../../drivers/sample_memory_plugin.cpp)
- 插件清单：[plugins/plugins.json](../../plugins/plugins.json)

---

**最后验证**: 2026-06-16 (commit `374d463`)
