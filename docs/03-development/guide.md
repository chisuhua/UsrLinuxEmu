# 开发指南

> **最后验证**: 2026-06-16 (commit `374d463`)
>
> **架构 SSOT**: [`docs/02_architecture/post-refactor-architecture.md`](../02_architecture/post-refactor-architecture.md)
> **状态**: 已重写，对齐 Phase 1.5 / Phase 2 重构后的三层架构

## 目录

- [§1 阅读对象与文档定位](#1-阅读对象与文档定位)
- [§2 环境搭建](#2-环境搭建)
- [§3 构建项目](#3-构建项目)
- [§4 运行测试](#4-运行测试)
- [§5 项目结构（重构后）](#5-项目结构重构后)
- [§6 写第一个设备插件](#6-写第一个设备插件)
- [§7 编写 Catch2 测试](#7-编写-catch2-测试)
- [§8 调试技巧](#8-调试技巧)
- [§9 常见问题](#9-常见问题)
- [§10 跨文档索引](#10-跨文档索引)

---

## §1 阅读对象与文档定位

本文面向需要**修改、扩展、调试** UsrLinuxEmu 的开发者。读者应熟悉 C++17 与基本 Linux 设备驱动概念。

| 角色 | 推荐阅读路径 |
|------|--------------|
| 首次贡献 | §2 → §3 → §4 → §6（sample_memory 走通）|
| 新增设备 | §6 → [adding-devices.md](adding-devices.md) |
| 调试 / 移植 | §8 + [`docs/06-reference/api-reference.md`](../06-reference/api-reference.md) |
| GPU 驱动 | §6 → [docs/05-advanced/gpu_driver_architecture.md](../05-advanced/gpu_driver_architecture.md) |

**重要边界**：本文只描述**当前**的代码形态（`374d463` 之后）。任何与 `archive/` 下历史代码的对比都不在本文档讨论范围内。

---

## §2 环境搭建

### 2.1 系统要求

- Linux（Ubuntu 20.04+ 推荐；其他发行版需确保 glibc ≥ 2.31）
- CMake ≥ 3.14
- GCC ≥ 9 或 Clang ≥ 10（C++17 支持）
- 无 root 权限要求

### 2.2 安装依赖

```bash
# Ubuntu
sudo apt update
sudo apt install build-essential cmake git

# 验证版本
cmake --version          # ≥ 3.14
g++ --version            # ≥ 9
```

> **测试框架**：项目使用 **Catch2**（vendored 单文件，路径 `tests/catch_amalgamated.{hpp,cpp}`），**不**需要安装 `libgtest-dev`。详见 [§7](#7-编写-catch2-测试)。

### 2.3 获取源码

```bash
git clone <repository-url>
cd UsrLinuxEmu
git submodule update --init --recursive   # 拉取 external/TaskRunner
```

---

## §3 构建项目

### 3.1 推荐：构建脚本

```bash
./build.sh                 # 配置 + 编译所有目标
./build.sh test            # 构建 + ctest
./build.sh clean           # 清理 build/
```

### 3.2 手动构建

```bash
# 必须从项目根目录运行
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
```

构建产物布局：

| 路径 | 内容 |
|------|------|
| `build/bin/cli` | CLI 工具 |
| `build/bin/test_*` | Catch2 / standalone 测试 |
| `build/drivers/*.so` | 示例插件（plugin_sample_memory.so 等）|
| `build/plugins/*.so` | GPU 驱动插件（plugin_gpu_driver.so）|
| `build/libkernel.so` | 框架 SHARED 库 |

### 3.3 kernel 必须是 SHARED（Issue #11）

`src/CMakeLists.txt` 中必须保持：

```cmake
add_library(kernel SHARED ...)
```

**不要改成 STATIC**。`VFS::instance()`、`ModuleLoader` 等使用 Meyers 单例（函数内 `static` 局部变量）；STATIC 库会让可执行文件与插件各自持有独立的单例副本，设备注册/查找完全断裂。修改前请阅读 [post-refactor-architecture.md §4 关键执行原则](../02_architecture/post-refactor-architecture.md)。

---

## §4 运行测试

### 4.1 从项目根目录运行

**插件路径是相对路径**。所有测试必须从项目根目录（不是 `build/bin/`）执行。

```bash
cd /workspace/project/UsrLinuxEmu

# 跑所有测试
cd build && ctest --output-on-failure && cd ..

# 单独跑某个二进制
./build/bin/test_gpu_ioctl_standalone
./build/bin/test_va_space_standalone
./build/bin/test_module_load_and_vfs_standalone
./build/bin/test_gpu_ringbuffer_standalone
./build/bin/test_hardware_puller_emu_standalone
```

### 4.2 常见测试场景

| 想验证什么 | 跑哪个 |
|------------|--------|
| VFS + 插件加载链路 | `test_module_load_and_vfs_standalone` |
| GPU ioctl 派发表 | `test_gpu_ioctl_standalone` |
| VA Space 抽象 | `test_va_space_standalone` |
| Ring Buffer 消费者 | `test_gpu_ringbuffer_standalone` |
| Hardware Puller FSM | `test_hardware_puller_emu_standalone` |
| 插件隔离（重复 dlopen）| `test_module_loader_isolation`（Catch2）|

完整测试列表见 `tests/CMakeLists.txt`（`STANDALONE_TESTS` / `CATCH2_TESTS` / `SIM_TESTS` 三组）。

---

## §5 项目结构（重构后）

Phase 1.5 / Phase 2 之后，仓库布局如 [`post-refactor-architecture.md` §1.5](../02_architecture/post-refactor-architecture.md) 所述。开发者最关心的几个目录：

```
UsrLinuxEmu/
├── src/kernel/                # 框架实现（SHARED lib, 14 cpp）
├── include/kernel/            # 框架头：vfs.h, device/, file_ops.h, module_loader.h ...
├── include/linux_compat/      # Linux API 用户态兼容层（u8/u32, _IOR, drm/）
├── drivers/                   # 示例插件源码
│   ├── sample_memory.{h,cpp}
│   ├── sample_memory_plugin.cpp
│   ├── sample_serial.{h,cpp}
│   └── CMakeLists.txt         # 生成 plugin_sample_*.so
├── plugins/gpu_driver/        # GPU 驱动插件（核心）
│   ├── drv/                   # GpgpuDevice（ioctl 派发表）
│   ├── hal/                   # struct gpu_hal_ops + hal_user / hal_mock
│   ├── sim/                   # 硬件仿真：scheduler/, hardware/, gpu_queue_emu
│   ├── shared/                # 公共头：gpu_ioctl.h, gpu_types.h ...
│   └── plugin.cpp             # 导出 `module mod` 符号
├── libgpu_core/               # 纯 C buddy allocator（ADR-020）
├── tests/                     # Catch2 + standalone 测试
├── tools/cli/                 # CLI 工具源码
└── archive/                   # 历史代码（不要引用）
```

### 5.1 已删除的旧路径

下列路径在 Phase 1 之前存在，**已不存在**，不要引用：

- ~~`drivers/gpu/`~~（旧 System B 驱动）→ `archive/system_b_drivers/gpu/`
- ~~`simulator/gpu/`~~（已清空）→ `archive/orphaned_simulator/gpu/`
- ~~`examples/`~~、~~`benchmarks/`~~ → 不存在
- ~~旧的 IOCTL 兼容层源文件对~~ → 已删除（Phase 1 之前存在）
- ~~`include/kernel/device/gpgpu_device.h`~~ → `plugins/gpu_driver/drv/gpgpu_device.h`
- ~~已废弃的插件宏~~ → 不存在；改用 `module mod` 符号（dlsym 入口）
- ~~`GpuDevice` / `GpuDriver`~~ → `GpgpuDevice`

---

## §6 写第一个设备插件

下面的示例基于 `drivers/sample_memory.cpp` + `drivers/sample_memory_plugin.cpp`，可编译、可运行。

### 6.1 实现 `FileOperations` 子类

`include/kernel/file_ops.h` 定义了 7 个虚方法（open / close / read / write / ioctl / mmap / munmap）。`ioctl` 是纯虚，其他默认空实现。

```cpp
// drivers/my_device.h
#pragma once
#include "kernel/file_ops.h"
#include "kernel/ioctl.h"

#define MY_IOC_MAGIC 'M'
#define MY_SET_MODE   _IOW(MY_IOC_MAGIC, 1, int)
#define MY_GET_STATUS _IOR(MY_IOC_MAGIC, 2, int)

namespace usr_linux_emu {

class MyDevice : public FileOperations {
 public:
  MyDevice() = default;
  ~MyDevice() override = default;

  // 必须实现
  long ioctl(int fd, unsigned long request, void* argp) override;

  // 按需 override
  int open(const char* path, int flags) override;
  ssize_t read(int fd, void* buf, size_t count) override;
  ssize_t write(int fd, const void* buf, size_t count) override;

 private:
  int mode_ = 0;
  int status_ = 0;
};

}  // namespace usr_linux_emu
```

```cpp
// drivers/my_device.cpp
#include "my_device.h"
#include <cstring>
#include <iostream>

using namespace usr_linux_emu;

long MyDevice::ioctl(int fd, unsigned long request, void* argp) {
  switch (request) {
    case MY_SET_MODE:
      mode_ = *(int*)argp;
      return 0;
    case MY_GET_STATUS:
      *(int*)argp = status_;
      return 0;
    default:
      return -ENOTTY;
  }
}

int MyDevice::open(const char* path, int flags) {
  std::cout << "[MyDevice] open path=" << path << " flags=0x" << std::hex << flags << std::endl;
  return 0;
}

ssize_t MyDevice::read(int fd, void* buf, size_t count) {
  const char* msg = "hello";
  size_t n = std::min(count, strlen(msg));
  std::memcpy(buf, msg, n);
  return n;
}

ssize_t MyDevice::write(int fd, const void* buf, size_t count) {
  std::cout << "[MyDevice] wrote " << count << " bytes\n";
  return count;
}
```

### 6.2 写插件入口：`module mod` 符号

插件通过 `module mod` 符号注册到 `ModuleLoader`。`module` 结构定义在 [`include/kernel/module_loader.h`](../02_architecture/post-refactor-architecture.md)：

```c
// include/kernel/module_loader.h（节选）
extern "C" {
typedef struct module {
  const char* name;      // 插件名
  const char** depends;  // 依赖项（NULL 结尾，可为 nullptr）
  int (*init)(void);     // 初始化
  void (*exit)(void);    // 卸载
} module;
}
```

**注意**：

- `module` 必须**是 C 链接**结构（`extern "C"`）。这是 dlopen + dlsym 查找的入口符号。
- **不要使用 `__attribute__((constructor))`**。项目加载机制走 `dlsym(handle, "mod")`。
- **没有"插件宏"或"注册函数"调用**。插件导出 = 暴露 C 链接的 `module mod` 符号。

插件入口示例：

```cpp
// drivers/my_device_plugin.cpp
#include <iostream>
#include <memory>
#include "kernel/device/device.h"
#include "kernel/module_loader.h"
#include "kernel/vfs.h"
#include "my_device.h"

using namespace usr_linux_emu;

module mod = {
    .name = "my_device",
    .depends = nullptr,
    .init =
        []() -> int {
          auto dev = std::make_shared<Device>("my0", 0x9000,
                                               std::make_shared<MyDevice>(), nullptr);
          VFS::instance().register_device(dev);
          std::cout << "[MyDevice] registered /dev/my0\n";
          return 0;
        },
    .exit = []() {
      VFS::instance().unregister_device("my0");
      std::cout << "[MyDevice] unregistered\n";
    }};
```

注册到 VFS 的 `Device` 是个**轻量包装**（4 字段：`name`, `dev_id`, `fops`, `plugin_handle`），真正的设备行为在 `FileOperations` 子类里。

### 6.3 加入 CMake

`drivers/CMakeLists.txt`：

```cmake
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

### 6.4 运行时加载插件

**Runtime 路径**用 `ModuleLoader`：

```cpp
// 用户应用 / 测试
#include "kernel/module_loader.h"
#include "kernel/vfs.h"

int main() {
  // 从项目根目录运行；plugins/ 是相对路径
  ModuleLoader::load_plugins("plugins");

  // 或单独加载某个 .so
  // ModuleLoader::load_plugin("build/drivers/plugin_my_device.so");

  auto dev = VFS::instance().open("/dev/my0", O_RDWR);
  if (!dev) return 1;

  int mode = 42;
  dev->fops->ioctl(0, MY_SET_MODE, &mode);

  int status = 0;
  dev->fops->ioctl(0, MY_GET_STATUS, &status);

  ModuleLoader::unload_plugins();
  return 0;
}
```

> `PluginManager` 仍存在于 `include/kernel/plugin_manager.h`，**仅供 CLI 工具使用**。新代码请用 `ModuleLoader::load_plugins("plugins")`。

### 6.5 完整可编译示例

完整可编译示例参见 `tests/test_module_load_and_vfs.cpp`（用 `sample_memory` 插件走完整链路）。GPU 插件入口模式见 [`plugins/gpu_driver/plugin.cpp`](../../plugins/gpu_driver/plugin.cpp)。

---

## §7 编写 Catch2 测试

测试文件**已经**自带 Catch2（`tests/catch_amalgamated.{hpp,cpp}`），不要走 `apt install libgtest-dev`。

### 7.1 最小可运行测试

```cpp
// tests/test_my_device.cpp
#include <catch_amalgamated.hpp>

#include "kernel/vfs.h"
#include "my_device.h"

using namespace usr_linux_emu;

TEST_CASE("MyDevice ioctl SET/GET", "[my_device]") {
  MyDevice dev;

  int mode = 7;
  long ret = dev.ioctl(0, MY_SET_MODE, &mode);
  REQUIRE(ret == 0);

  int status = 0;
  ret = dev.ioctl(0, MY_GET_STATUS, &status);
  REQUIRE(ret == 0);
  REQUIRE(status == 0);  // GET_STATUS 返回的是 status_，与 mode_ 独立
}

TEST_CASE("MyDevice ioctl unknown", "[my_device]") {
  MyDevice dev;
  long ret = dev.ioctl(0, 0xDEADBEEF, nullptr);
  REQUIRE(ret == -ENOTTY);
}
```

### 7.2 用 SECTION 拆分

```cpp
TEST_CASE("MyDevice state machine", "[my_device]") {
  MyDevice dev;
  int val = 0;

  SECTION("set then get") {
    val = 1; dev.ioctl(0, MY_SET_MODE, &val);
    val = 0; dev.ioctl(0, MY_GET_STATUS, &val);
    REQUIRE(val == 0);
  }
  SECTION("unknown returns ENOTTY") {
    REQUIRE(dev.ioctl(0, 0x1234, nullptr) == -ENOTTY);
  }
  // 每个 SECTION 在新 TEST_CASE 实例上运行（隔离状态）
}
```

### 7.3 VFS + 插件生命周期

参考 [`tests/test_gpu_memory.cpp`](../../tests/test_gpu_memory.cpp) 的 RAII 包装：

```cpp
#include <catch_amalgamated.hpp>
#include "kernel/module_loader.h"
#include "kernel/vfs.h"

using namespace usr_linux_emu;

struct PluginLifecycle {
  PluginLifecycle() { ModuleLoader::load_plugins("plugins"); }
  ~PluginLifecycle() { ModuleLoader::unload_plugins(); }
};

// 全局 RAII：避免每个 TEST_CASE 都 dlopen/dlclose
static PluginLifecycle plugin_lifecycle;

TEST_CASE("GPU device info", "[gpu]") {
  auto dev = VFS::instance().open("/dev/gpgpu0", 0);
  REQUIRE(dev != nullptr);
  REQUIRE(dev->fops != nullptr);

  struct gpu_device_info info {};
  long ret = dev->fops->ioctl(0, GPU_IOCTL_GET_DEVICE_INFO, &info);
  REQUIRE(ret == 0);
  REQUIRE(info.vendor_id == 0x1000);
}
```

### 7.4 注册到 tests/CMakeLists.txt

新测试加入 `CATCH2_TESTS` 列表（`tests/CMakeLists.txt`）：

```cmake
set(CATCH2_TESTS
    test_gpu_memory.cpp
    test_gpu_mmap_bar.cpp
    test_gpu_plugin.cpp
    test_module_loader_isolation.cpp
    test_my_device.cpp          # ← 加这里
)
```

`add_catch_test()` 会自动链接 Catch2 单文件 + kernel 库，并注册到 ctest。

### 7.5 不要写 GTest 风格

下面是 **错误** 的写法（项目实际不用 GTest）：

```text
// ❌ 错误风格：项目用 Catch2，不安装 GTest。
//    包含 gtest 头、用 fixture 宏、链式断言的写法都不要照搬。
//    正确做法见下方 §7.1（TEST_CASE / REQUIRE）。
```

正确风格见 [§7.1](#71-最小可运行测试)。完整 API/IOCTL 速查见 [`docs/06-reference/api-reference.md`](../06-reference/api-reference.md)。

---

## §8 调试技巧

### 8.1 日志

`include/kernel/logger.h` 提供 `Logger::debug/info/warn/error`，**是 std::string 风格**：

```cpp
#include "kernel/logger.h"

Logger::set_level(Logger::DEBUG);
Logger::debug("Processing request=0x" + std::to_string(request));
Logger::info("Device initialized");
Logger::error("alloc failed: size=" + std::to_string(size));
```

`LOG_INFO << "..."` 流式语法**当前不支持**（参考的 `docs/06-reference/api-reference.md` §日志章节会更新）。

### 8.2 GDB 调试

```bash
# 1. Debug 构建
cd build && cmake -DCMAKE_BUILD_TYPE=Debug .. && make -j$(nproc) && cd ..

# 2. 附加到测试
gdb --args ./build/bin/test_gpu_ioctl_standalone
(gdb) break GpgpuDevice::ioctl
(gdb) run
(gdb) print request
(gdb) bt
```

### 8.3 打印插件加载细节

```bash
# ModuleLoader 加载过程在 stderr 输出
./build/bin/test_module_load_and_vfs_standalone 2>&1 | head -50
```

### 8.4 用 CLI 检查已注册设备

```bash
./build/bin/cli list
./build/bin/cli --help
```

`tools/cli/` 编译产物在 `build/bin/cli`。

### 8.5 异步路径：fence 与 doorbell

GPU 链路是异步的（`GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH` 立即返回 `fence_id`）。要等到 GPU 真正"完成"，用 `GPU_IOCTL_WAIT_FENCE`：

```cpp
struct gpu_wait_fence_args f{ .fence_id = submit_args.fence_id, .timeout_ms = 1000, .status = 0 };
dev->fops->ioctl(0, GPU_IOCTL_WAIT_FENCE, &f);
REQUIRE(f.status == 0);
```

### 8.6 排查 "Device not found"

1. **路径**：必须从项目根目录运行（`cd /workspace/project/UsrLinuxEmu && ./build/bin/test_*`）。
2. **插件路径**：`ModuleLoader::load_plugins("plugins")` 是相对当前 cwd。
3. **插件编译**：`build/plugins/plugin_gpu_driver.so` 是否生成？没有就检查 `plugins/gpu_driver/CMakeLists.txt`。
4. **符号导出**：`nm -D build/plugins/plugin_gpu_driver.so | grep mod` 应该看到 `mod` 符号。

---

## §9 常见问题

### Q: kernel 库能改成 STATIC 吗？

**不能**。`VFS::instance()` 用 Meyers 单例；STATIC 库会让进程内两份单例，VFS 状态割裂（Issue #11）。

### Q: ioctl 返回 -EFAULT？

99% 是结构体没完整初始化。System C 结构体定义在 `plugins/gpu_driver/shared/gpu_ioctl.h` 和 `gpu_queue.h`，所有成员都要赋值。

### Q: PluginManager 还是 ModuleLoader？

**Runtime 一律用 `ModuleLoader::load_plugins("plugins")`**。`PluginManager` 仅 CLI 内部使用，不再扩展。

### Q: 我的设备类应该继承 Device 还是 FileOperations？

**继承 `FileOperations`**（`open/read/write/ioctl/mmap/munmap`），然后用 `Device` 包装一层注册到 VFS。`Device` 自身只持有 `name` / `dev_id` / `fops` / `plugin_handle`。

### Q: 怎样支持多个设备实例？

```cpp
auto d0 = std::make_shared<Device>("my0", 0x9000, std::make_shared<MyDevice>(), nullptr);
auto d1 = std::make_shared<Device>("my1", 0x9001, std::make_shared<MyDevice>(), nullptr);
VFS::instance().register_device(d0);
VFS::instance().register_device(d1);
```

### Q: GPU 驱动怎么开始？

读 [`plugins/gpu_driver/plugin.cpp`](../../plugins/gpu_driver/plugin.cpp) 入口（90 行，结构最清晰），再看 [`plugins/gpu_driver/drv/gpgpu_device.h`](../../plugins/gpu_driver/drv/gpgpu_device.h) 的 ioctl 派发表（13 项），最后看 [`post-refactor-architecture.md` 附录 A](../02_architecture/post-refactor-architecture.md) 完整 IOCTL 编号表。

### Q: 测试应该写到 tests/ 还是 plugins/gpu_driver/test/？

`plugins/gpu_driver/test/` **不存在**。所有测试都进 `tests/`，按 `STANDALONE_TESTS` / `CATCH2_TESTS` / `SIM_TESTS` 三组分流。

---

## §10 跨文档索引

| 想了解什么 | 看哪里 |
|------------|--------|
| 三层架构总览 / 仓库布局 / IOCTL 编号表 | [`docs/02_architecture/post-refactor-architecture.md`](../02_architecture/post-refactor-architecture.md) |
| Device / VFS / ModuleLoader 完整 API | [`docs/06-reference/api-reference.md`](../06-reference/api-reference.md) |
| 15 个 GPU_IOCTL_* 的参数与示例 | [`docs/06-reference/ioctl-commands.md`](../06-reference/ioctl-commands.md) |
| 如何写一个新设备类（Device 完整步骤）| [adding-devices.md](adding-devices.md) |
| GPU 驱动内部（GpgpuDevice / HAL / sim）| [`docs/05-advanced/gpu_driver_architecture.md`](../05-advanced/gpu_driver_architecture.md) |
| TaskRunner 对接 | [`docs/07-integration/taskrunner-index.md`](../07-integration/taskrunner-index.md) |
| 编码风格（snake_case + 尾下划线成员变量）| [`AGENTS.md`](../../AGENTS.md) + [`docs/03-development/coding-style.md`](coding-style.md) |
| 架构决策（ADR-018~024 驱动/HAL/Queue）| [`docs/00_adr/`](../00_adr/) |

---

**最后更新**: 2026-06-16
**对应 commit**: `374d463`
**维护者**: UsrLinuxEmu Team
