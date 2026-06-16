# 构建系统文档

> **最后验证**: 2026-06-16 (commit `374d463`)
>
> **权威架构文档**: [AGENTS.md](../../AGENTS.md) + [docs/02_architecture/post-refactor-architecture.md](../02_architecture/post-refactor-architecture.md)
>
> 本文档描述 UsrLinuxEmu 的实际 CMake 结构（Phase 2 完成、2026-05~06 重构之后）。如发现与上述两份权威文档冲突，以它们为准。

---

## 目录

- [1. 构建系统概述](#1-构建系统概述)
- [2. 顶层 CMakeLists.txt](#2-顶层-cmakeliststxt)
- [3. 六个构建子目录](#3-六个构建子目录)
  - [3.1 `src/`（kernel SHARED 库）](#31-srckernel-shared-库)
  - [3.2 `drivers/`（示例设备插件源码）](#32-drivers示例设备插件源码)
  - [3.3 `plugins/`（动态加载的设备插件）](#33-plugins动态加载的设备插件)
  - [3.4 `tests/`（Catch2 测试）](#34-testscatch2-测试)
  - [3.5 `tools/cli/`（命令行工具）](#35-toolscli命令行工具)
  - [3.6 `libgpu_core/`（纯 C buddy allocator）](#36-libgpu_core纯-c-buddy-allocator)
- [4. ⚠️ kernel 库必须为 SHARED（Issue #11）](#4-⚠️kernel-库必须为-sharedissue-11)
- [5. ⚠️ `include_directories(simulator)` 隐藏问题](#5-⚠️include_directoriessimulator-隐藏问题)
- [6. 测试框架：Catch2（vendored 单文件）](#6-测试框架catch2vendored-单文件)
  - [6.1 三类测试与命名约定](#61-三类测试与命名约定)
  - [6.2 运行测试](#62-运行测试)
- [7. 构建命令](#7-构建命令)
  - [7.1 标准构建（推荐）](#71-标准构建推荐)
  - [7.2 一键脚本](#72-一键脚本)
  - [7.3 调试 / 发布变体](#73-调试--发布变体)
- [8. 构建输出结构](#8-构建输出结构)
- [9. 添加新模块](#9-添加新模块)
- [10. 依赖](#10-依赖)
- [11. 常见问题](#11-常见问题)
- [12. 相关文档](#12-相关文档)

---

## 1. 构建系统概述

UsrLinuxEmu 使用 CMake 作为构建系统，最低要求 CMake 3.14。整体目标是：

- **跨平台**：Linux 为主目标（其他 Unix 系统理论可行，未验证）
- **模块化**：每个顶层目录自带 `CMakeLists.txt`，独立维护
- **可重定位**：所有头文件路径通过 `${PROJECT_SOURCE_DIR}` 解析，源码树内任意位置都可构建
- **插件化**：设备插件通过 `dlopen` + `dlsym("mod")` 动态加载（详见 §3.3）
- **无 root 依赖**：全部链接 `pthread` 和 `dl`，不依赖内核模块

整个项目编译产物（可执行文件 + 共享库）统一输出到 `${PROJECT_BINARY_DIR}/bin`。

---

## 2. 顶层 CMakeLists.txt

顶层 [`CMakeLists.txt`](../../CMakeLists.txt) 位于项目根目录，定义全局编译选项、头文件路径和子目录入口。**完整实际内容**如下（30 行，无遗漏）：

```cmake
cmake_minimum_required(VERSION 3.14)
project(user_kernel_emu)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 输出路径
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

# 启用测试
enable_testing()

# 包含头文件目录
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/include/kernel)
include_directories(${PROJECT_SOURCE_DIR}/include/kernel/device)
include_directories(${PROJECT_SOURCE_DIR}/drivers)
include_directories(${PROJECT_SOURCE_DIR}/external/json)
include_directories(${PROJECT_SOURCE_DIR}/simulator)

# 添加子模块
add_subdirectory(src)
add_subdirectory(drivers)
add_subdirectory(plugins)
add_subdirectory(tests)
add_subdirectory(tools/cli)
add_subdirectory(libgpu_core)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
```

### 关键观察

| 行 | 含义 |
|----|------|
| L8 | 所有运行时产物（可执行文件 + 共享库）输出到 `build/bin/` |
| L11 | 顶层 `enable_testing()` 让 `ctest` 在 `build/` 目录可用 |
| L14-19 | 6 个全局 include 路径（含 §5 描述的 stale `simulator/`） |
| L22-27 | **6 个 add_subdirectory**，对应 §3 描述的子模块 |
| L29 | `compile_commands.json` 启用，供 clangd / IDE 索引 |

注意：**include 路径并非模块化**（用顶层 `include_directories` 而非 `target_include_directories`）。这是历史遗留，新代码建议迁移到 per-target include，但目前不要改。

---

## 3. 六个构建子目录

顶层 `CMakeLists.txt` 通过 6 个 `add_subdirectory` 引入以下模块（顺序与 CMakeLists.txt 一致）：

```
CMakeLists.txt
├── src/               → kernel SHARED 库（框架实现）
├── drivers/           → 示例设备插件源码（sample_memory, sample_serial）
├── plugins/           → 动态加载的设备插件（核心 GPU 驱动插件）
├── tests/             → Catch2 + standalone 测试
├── tools/cli/         → 命令行工具 cli
└── libgpu_core/       → 纯 C buddy allocator（ADR-020）
```

`include/` 目录不通过 `add_subdirectory` 引入（因为它只有头文件，没有 `CMakeLists.txt`），但通过顶层 `include_directories` 全局可见。

### 3.1 `src/`（kernel SHARED 库）

[`src/CMakeLists.txt`](../../src/CMakeLists.txt) 定义唯一的核心库：

```cmake
add_library(kernel SHARED
    kernel/types.cpp
    kernel/module.cpp
    kernel/vfs.cpp
    kernel/file_ops.cpp
    kernel/wait_queue.cpp
    kernel/plugin_manager.cpp
    kernel/module_loader.cpp
    kernel/config_manager.cpp
    kernel/service_registry.cpp
    kernel/logger.cpp
    kernel/device.cpp
    kernel/poll_watcher.cpp
    kernel/device/serial_device.cpp
    kernel/device/memory_device.cpp
)

include_directories(${PROJECT_SOURCE_DIR}/include)
target_link_libraries(kernel PRIVATE dl pthread)
set_target_properties(kernel PROPERTIES POSITION_INDEPENDENT_CODE ON)
```

14 个 cpp 文件提供框架核心：

| 组件 | 职责 |
|------|------|
| `vfs.cpp` | VFS 单例（Meyers singleton，Issue #11 修复） |
| `module_loader.cpp` / `plugin_manager.cpp` | 插件加载（CLI 用 `plugin_manager`，测试用 `module_loader`） |
| `device.cpp` + `device/serial_device.cpp` + `device/memory_device.cpp` | Device 基类 + 两个示例设备 |
| `wait_queue.cpp` / `poll_watcher.cpp` | 阻塞 / poll 支持 |
| `logger.cpp` / `config_manager.cpp` / `service_registry.cpp` | 日志、配置、服务注册 |
| `file_ops.cpp` / `ioctl.h` | file_operations 与 ioctl 派发 |
| `types.cpp` / `module.cpp` / `sync_utils.h` | 基础类型与同步原语 |

**链接依赖**: `dl`（`dlopen`/`dlsym`）+ `pthread`。**位置无关代码**开启，因为 SHARED 库需要。

### 3.2 `drivers/`（示例设备插件源码）

[`drivers/`](../../drivers) 目录包含两个**示例**设备（不是构建目标本身）：

- `drivers/sample_memory/`：内存设备示例
- `drivers/sample_serial/`：串口设备示例

每个子目录有自己的 `CMakeLists.txt`，把示例源文件编译为**独立插件 `.so`**。这些是开发者参考模板，不是核心插件。核心 GPU 插件在 `plugins/gpu_driver/`（见 §3.3）。

**顶层 include 路径已经包含 `drivers/`**，所以示例插件可以直接 `#include "sample_memory/sample_memory_plugin.h"`。

### 3.3 `plugins/`（动态加载的设备插件）

[`plugins/CMakeLists.txt`](../../plugins/CMakeLists.txt) 本身只有一行：

```cmake
cmake_minimum_required(VERSION 3.14)
add_subdirectory(gpu_driver)
```

真正的插件构建在 [`plugins/gpu_driver/CMakeLists.txt`](../../plugins/gpu_driver/CMakeLists.txt)：

```cmake
cmake_minimum_required(VERSION 3.14)

# GPU 驱动仿真插件（现有活跃入口）
add_library(gpu_driver_plugin MODULE
    plugin.cpp
)

# 添加 shared 目录（canonical 接口）
target_include_directories(gpu_driver_plugin PRIVATE
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/shared
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/hal
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/sim
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/sim/hardware
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/sim/scheduler
    ${PROJECT_SOURCE_DIR}/libgpu_core/include
)

# 添加 kernel 框架头文件
target_include_directories(gpu_driver_plugin PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/kernel
)

# drv/ 和 hal/ 总是编译（plugin.cpp 依赖它们）
add_subdirectory(drv)
add_subdirectory(hal)

# sim/ 总是编译（gpu_sim 提供 doorbell/puller/scheduler 仿真）
add_subdirectory(sim)

target_link_libraries(gpu_driver_plugin PRIVATE kernel gpu_hal gpu_core gpu_drv gpu_sim)

set_target_properties(gpu_driver_plugin PROPERTIES
    PREFIX ""
    SUFFIX ".so"
    POSITION_INDEPENDENT_CODE ON
)

# ── Post-build: 同步插件到 plugins/ 目录 ─────────────────────────
add_custom_command(TARGET gpu_driver_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:gpu_driver_plugin>
        ${PROJECT_SOURCE_DIR}/plugins/plugin_gpu_driver.so
    COMMENT "Copying gpu_driver_plugin.so to plugins/ directory"
)
```

要点：

- **MODULE 库**（非 SHARED）：用 `dlopen` 加载，不需要导出符号表全加载
- **PREFIX = ""**：去掉 `lib` 前缀，产物名为 `gpu_driver_plugin.so`
- **Post-build 复制**：CMake 编译产物在 `build/bin/`，但 `ModuleLoader` 加载的路径是 `plugins/`，所以编译后自动复制到 `plugins/plugin_gpu_driver.so`。这样源码树和构建目录保持一致，测试可以相对路径加载
- **子目录编译**：`drv/`、`hal/`、`sim/` 各有自己的 `CMakeLists.txt`，定义 `gpu_drv`、`gpu_hal`、`gpu_sim` 三个 STATIC 库供 `gpu_driver_plugin` 链接

### 3.4 `tests/`（Catch2 测试）

[`tests/CMakeLists.txt`](../../tests/CMakeLists.txt) 定义三类测试，目标约 30 个独立可执行文件。详见 §6。

### 3.5 `tools/cli/`（命令行工具）

[`tools/cli/CMakeLists.txt`](../../tools/cli/CMakeLists.txt)：

```cmake
# CLI工具可执行文件
add_executable(cli 
    main.cpp
)

target_link_libraries(cli PRIVATE kernel)
target_include_directories(cli PRIVATE 
    ${PROJECT_SOURCE_DIR}/include 
    ${PROJECT_SOURCE_DIR}/include/kernel
    ${PROJECT_SOURCE_DIR}/include/linux_compat)
```

产物名是 `cli`（不是 `cli_tool`），输出到 `build/bin/cli`。CLI 用于交互式加载/卸载插件、查看已注册设备。子命令可用 `./build/bin/cli --help`。

### 3.6 `libgpu_core/`（纯 C buddy allocator）

[`libgpu_core/CMakeLists.txt`](../../libgpu_core/CMakeLists.txt)：

```cmake
cmake_minimum_required(VERSION 3.14)

project(libgpu_core C)

# 纯 C 静态库
add_library(gpu_core STATIC
    src/buddy.c
)

target_include_directories(gpu_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 要求 C99 标准
set_target_properties(gpu_core PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

# 测试
option(BUILD_TESTS "Build libgpu_core tests" ON)

if(BUILD_TESTS)
    add_executable(test_buddy
        test/test_buddy.c
    )
    target_link_libraries(test_buddy PRIVATE gpu_core)
    target_include_directories(test_buddy PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    add_test(NAME test_buddy COMMAND test_buddy)
endif()
```

ADR-020 提取的纯 C buddy allocator（`gpu_buddy.h` + `buddy.c`）。与 C++ kernel 框架**完全解耦**：使用方只需 `#include "gpu_buddy.h"` 并链接 `-lgpu_core`。`POSITION_INDEPENDENT_CODE` 开启，保证可被 SHARED 库（kernel、gpu_driver_plugin）链接。

---

## 4. ⚠️ kernel 库必须为 SHARED（Issue #11）

`src/CMakeLists.txt` 中 `add_library(kernel SHARED ...)` **不可改为 STATIC**。

### 为什么

`VFS::instance()` 等单例使用 Meyers 单例模式（函数内 `static` 局部变量）：

```cpp
VFS& VFS::instance() {
    static VFS vfs;  // Meyers singleton
    return vfs;
}
```

如果 `kernel` 是 STATIC 库，每个使用它的可执行文件 / 插件都会**链接一份独立的 `VFS` 单例副本**：

- 测试可执行文件有自己的 `VFS` 实例
- `gpu_driver_plugin.so` 有自己的 `VFS` 实例
- `cli` 又有自己的 `VFS` 实例

结果是：测试通过 `ModuleLoader::load_plugins("plugins")` 加载的设备**注册到测试进程的 `VFS`**，与插件内部的 `VFS` 完全隔离。插件里 `VFS::instance().open(...)` 找不到测试进程注册的设备，导致 `Device not found` 错误（Issue #11）。

### 修复后行为（SHARED）

`kernel` 是 SHARED 库时，运行时通过动态链接器（`ld.so`）共享同一份 `.so` 文件，Meyers 单例的 `static VFS vfs` 在进程全局只有一份。插件加载时调用 `dlsym(handle, "mod")` 拿到符号，符号解析指向同一份 `kernel.so` 的 `VFS::instance()`，返回唯一的全局实例。

### 验证

如果怀疑 VFS 单例被割裂，临时改回 STATIC 重现问题即可。但**不要提交**这个临时改动。

---

## 5. ⚠️ `include_directories(simulator)` 隐藏问题

顶层 CMakeLists.txt L19 包含一个**陈旧的 include 路径**：

```cmake
include_directories(${PROJECT_SOURCE_DIR}/simulator)
```

但 `simulator/` 目录在 Phase 1.5 重构时已被清空（旧仿真代码迁移到 `plugins/gpu_driver/sim/`）。当前 `simulator/` 目录为空：

```bash
$ ls simulator/
# (空)
```

这个 include 路径当前**不会导致编译错误**（因为没有任何源文件 `#include <simulator/...>`），但属于隐藏的债务：

- 误导读者以为 `simulator/` 仍在使用
- 如果未来有人向 `simulator/` 添加文件，可能掩盖实际架构变化
- 与 README 中"`simulator/` 已清空"的描述冲突

### 建议清理

在未来的清理 commit 中：

```cmake
# 删除这一行
- include_directories(${PROJECT_SOURCE_DIR}/simulator)
```

并考虑删除空的 `simulator/` 目录（保留也行，作为架构演进的"墓碑"）。

**当前文档明确标注此问题**，不阻塞构建。

---

## 6. 测试框架：Catch2（vendored 单文件）

UsrLinuxEmu 使用 **Catch2 v2.x amalgamated 单文件版**，vendored 在 `tests/`：

```
tests/
├── catch_amalgamated.hpp   (522 KB，单头文件)
├── catch_amalgamated.cpp   (348 KB，单源文件)
└── test_*.cpp              (30+ 测试用例)
```

测试套件通过 `#include "catch_amalgamated.hpp"` 直接使用，**完全不依赖任何 Google Test 风格的 apt 包**。换句话说：Ubuntu 上不要跑 `apt install` 装 Google Test 的开发包，那些与本项目无关。所有测试基础设施都已经在源码树里了。

### 6.1 三类测试与命名约定

[`tests/CMakeLists.txt`](../../tests/CMakeLists.txt) 把测试分为三组：

| 类别 | 列表变量 | 链接 | 命名 | 用途 |
|------|----------|------|------|------|
| **Standalone** | `STANDALONE_TESTS` | `kernel` | `<src>_standalone` | 自定义 main，框架接口单测 |
| **Catch2** | `CATCH2_TESTS` | `kernel` + `catch_amalgamated.cpp` | `<src>` | Catch2 风格 `TEST_CASE` / `REQUIRE` |
| **SIM** | `SIM_TESTS` | `kernel` + `gpu_sim` | `<src>_standalone` | 仿真层（scheduler/puller/hardware）测试 |

完整文件列表（截至 2026-06-16）：

**STANDALONE_TESTS**（17 个）：

```
test_compat_memory           test_compat_types
test_doorbell_emu            test_gpu_ioctl
test_gpu_ioctl_number        test_gpu_fence_return
test_gpu_mmap                test_gpu_mmap_and_submit
test_gpu_register            test_gpu_regs
test_gpu_submit              test_ioctl
test_logger                  test_pcie_gpu
test_module_loader           test_module_load_and_vfs
test_plugin                  test_va_space
```

**CATCH2_TESTS**（4 个）：

```
test_gpu_memory              test_gpu_mmap_bar
test_gpu_plugin              test_module_loader_isolation
```

**SIM_TESTS**（6 个）：

```
test_gpu_ringbuffer          test_queue_puller_integration
test_hardware_puller_emu     test_global_scheduler
test_gpfifo_translator       test_gpu_callback_integration
```

CMake 用三个 helper 函数包装：

```cmake
function(add_standalone_test TEST_NAME TEST_SOURCE) ... endfunction()
function(add_catch_test TEST_NAME) ... endfunction()
function(add_sim_test TEST_NAME TEST_SOURCE) ... endfunction()
```

每个测试都用 `set_tests_properties(... WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})` 设置工作目录为项目根，**因为 `ModuleLoader::load_plugins("plugins")` 使用相对路径**，必须在项目根运行。

### 6.2 运行测试

**必须从项目根目录运行**（不是 `build/bin/`），插件路径是相对路径。

```bash
# 推荐：从项目根运行所有测试
cd /workspace/project/UsrLinuxEmu
cd build && ctest --output-on-failure && cd ..

# 单个测试（典型 GPU 链路）
./build/bin/test_gpu_ioctl_standalone
./build/bin/test_va_space_standalone
./build/bin/test_gpu_ringbuffer_standalone
./build/bin/test_hardware_puller_emu_standalone
./build/bin/test_module_load_and_vfs_standalone

# Catch2 测试可以直接传 filter
./build/bin/test_gpu_memory "[alloc]"
```

如果遇到 `Device not found` 错误，99% 是因为没在项目根目录运行。

---

## 7. 构建命令

详细命令与故障排除见 [AGENTS.md](../../AGENTS.md)。本节给出最小可复现流程。

### 7.1 标准构建（推荐）

```bash
# 从项目根目录
cd /workspace/project/UsrLinuxEmu

# 初始化子模块（仅首次）
git submodule update --init --recursive

# 配置 + 构建
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
```

构建产物在 `build/bin/`：

```
build/bin/
├── cli                       # 命令行工具
├── gpu_driver_plugin.so      # GPU 驱动插件（同时复制到 plugins/）
├── plugin_gpu_driver.so      # 插件副本（ModuleLoader 加载此路径）
└── test_*_standalone         # 30+ 测试可执行文件
```

### 7.2 一键脚本

项目提供 [`build.sh`](../../build.sh)：

```bash
./build.sh                  # 构建所有目标
./build.sh test             # 构建 + 运行 ctest
./build.sh clean            # 清理 build/ 目录
./build.sh gpu_driver_plugin  # 只构建 GPU 插件
```

[`run_cli.sh`](../../run_cli.sh) 启动 CLI（确保 build/ 已生成）。

### 7.3 调试 / 发布变体

```bash
# 调试版本（推荐开发用，含调试符号、关闭优化）
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)

# 发布版本（最高优化，无调试符号）
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# 带调试符号的发布版本（性能分析用）
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
make -j$(nproc)

# 最小体积版本
cmake -DCMAKE_BUILD_TYPE=MinSizeRel ..
make -j$(nproc)
```

| 构建类型 | 优化 | 调试符号 | 典型用途 |
|----------|------|----------|----------|
| Debug | `-O0 -g` | 完整 | 日常开发、断点调试 |
| Release | `-O3 -DNDEBUG` | 无 | 性能基准、最终交付 |
| RelWithDebInfo | `-O2 -g -DNDEBUG` | 完整 | perf + gdb 联动分析 |
| MinSizeRel | `-Os -DNDEBUG` | 无 | 嵌入式场景（目前无此场景） |

---

## 8. 构建输出结构

```
build/
├── bin/                              # 所有运行时产物
│   ├── cli                           # CLI 工具（来自 tools/cli/）
│   ├── gpu_driver_plugin.so          # GPU 插件（来自 plugins/gpu_driver/）
│   ├── plugin_gpu_driver.so          # 插件副本（post-build 复制）
│   ├── test_gpu_ioctl_standalone     # 测试（30+ 个 *_standalone）
│   ├── test_va_space_standalone
│   ├── ...
│   └── catch_amalgamated.cpp.o       # Catch2 静态副本（被多个测试链接）
│
├── CMakeFiles/                       # CMake 中间产物（每目标一个子目录）
├── Testing/                          # CTest 临时文件
├── Makefile                          # 主 Makefile
├── cmake_install.cmake
└── compile_commands.json             # 给 clangd / IDE 用
```

输出目录命名约定：

- **可执行文件**：原始名字（如 `cli`、`test_gpu_ioctl_standalone`）
- **共享库**：`lib<name>.so` 默认，但插件用 `set_target_properties(PREFIX "" SUFFIX ".so")` 去掉 `lib` 前缀
- **静态库**：`lib<name>.a`（如 `libgpu_core.a`）

---

## 9. 添加新模块

### 添加新的设备插件

1. 在 `plugins/<plugin_name>/` 创建目录
2. 写源文件，导出 `module mod` 符号（参考 `plugins/gpu_driver/plugin.cpp`）
3. 创建 `plugins/<plugin_name>/CMakeLists.txt`，声明 `add_library(<plugin>_plugin MODULE ...)`
4. 在 [`plugins/CMakeLists.txt`](../../plugins/CMakeLists.txt) 加 `add_subdirectory(<plugin_name>)`
5. 重新 `cmake ..` 生成（顶层 CMakeLists.txt 不需要改）

### 添加新的 C++ 子模块（如未来 `libfoo/`）

1. 创建 `libfoo/CMakeLists.txt`，定义 `add_library(foo STATIC ...)` 或 `SHARED`
2. 在顶层 [`CMakeLists.txt`](../../CMakeLists.txt) 加 `add_subdirectory(libfoo)`
3. 在调用方用 `target_link_libraries(<target> PRIVATE foo)`

### 添加新的顶层 include 路径

只在确实需要时添加（**当前不要清理历史 include**）。在顶层 CMakeLists.txt 的 `include_directories` 块加一行。注意：尽量用 `target_include_directories` 而非全局 `include_directories`，新代码遵守此约定。

---

## 10. 依赖

### 编译依赖

| 依赖 | 版本 | 说明 |
|------|------|------|
| CMake | ≥ 3.14 | 项目硬性要求 |
| GCC | ≥ 7（或 Clang ≥ 5）| C++17 支持 |
| pthread | - | POSIX 线程（隐式链接 kernel 库） |
| dl | - | 动态加载（隐式链接 kernel 库） |
| Catch2 | vendored | `tests/catch_amalgamated.{hpp,cpp}`，无外部依赖 |

**不需要安装任何 Google Test 的 apt 开发包**。本项目的测试套件完全自包含（与早期文档声明相反：那些文档让你装 Google Test 的开发包，但代码层面从来没有真正依赖它）。

### 构建端依赖

无需 Ninja / Make 之外的工具，但 Ninja 能加速增量构建：

```bash
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug ..
ninja
```

### 运行端依赖

- Linux 用户态（无内核模块、无 root）
- `external/json/` 头文件（vendored，已通过顶层 include 暴露）

---

## 11. 常见问题

### Q: 编译时报 `undefined reference to VFS::instance()`

`kernel` 库被错误地改为 STATIC。检查 [`src/CMakeLists.txt`](../../src/CMakeLists.txt) L1，必须是 `add_library(kernel SHARED ...)`。详见 §4。

### Q: 运行时 `Device not found`

测试 / CLI 没有从项目根目录运行。`ModuleLoader::load_plugins("plugins")` 是相对路径，需要 CWD = 项目根。详见 §6.2。

### Q: ioctl 返回 `-EFAULT`

检查传入结构体是否完整初始化。System C 的结构体定义在 [`plugins/gpu_driver/shared/gpu_ioctl.h`](../../plugins/gpu_driver/shared/gpu_ioctl.h) 与 `gpu_queue.h`。

### Q: 编译时找不到 Google Test 风格的测试头文件

正常，**本项目根本不安装 Google Test 那一套**。如果某个旧文档让你装 Google Test 的开发包，那是历史错误，以本文档为准。所有测试基础设施都 vendored 在源码树里（`tests/catch_amalgamated.{hpp,cpp}`）。

### Q: 顶层 CMake 配置了错误的测试目录名

历史错误。当前顶层 CMakeLists.txt 使用的是 `add_subdirectory(tests)`（带 s，目录名复数）。如果还有文档写单数形式不带 s 的版本，那是在描述已废弃的布局，请报告为 doc bug。

### Q: 能不能改成 Ninja / 别的 generator？

可以，`cmake -G Ninja ..` 即可。但 Makefile 是默认且 CI 用的，不建议换。

### Q: 怎么只编译一个目标？

```bash
make gpu_driver_plugin   # 只编译 GPU 插件
make test_gpu_ioctl_standalone  # 只编译一个测试
make cli                 # 只编译 CLI
```

---

## 12. 相关文档

| 文档 | 作用 |
|------|------|
| [AGENTS.md](../../AGENTS.md) | 开发指南 + 架构要点 + 构建命令速查 |
| [docs/02_architecture/post-refactor-architecture.md](../02_architecture/post-refactor-architecture.md) | 重构后架构 SSOT（必读） |
| [docs/01-quickstart/building.md](../01-quickstart/building.md) | 新人快速上手构建 |
| [docs/04-building/testing_guide.md](testing_guide.md) | 测试编写指南（同样基于 Catch2） |
| [docs/04-building/ci-cd.md](ci-cd.md) | CI 配置（基于 Catch2） |
| [docs/00_adr/adr-020-libgpu-core-extraction.md](../00_adr/) | ADR-020：libgpu_core 提取 |
| [README.md](../../README.md) | 项目主页 |

---

**维护者**: UsrLinuxEmu Team  
**最后更新**: 2026-06-16  
**对应代码 commit**: `374d463`