# 测试指南

> **最后验证**: 2026-06-16 (commit `374d463`)
>
> **测试框架**: **Catch2**（vendored 单文件 `tests/catch_amalgamated.{hpp,cpp}`）。
> 系统包管理器**不需要**安装任何外部测试框架。
>
> 权威架构文档：[post-refactor-architecture.md §1.7](../02_architecture/post-refactor-architecture.md)

---

## 目录

- [1. 测试框架：Catch2](#1-测试框架catch2)
- [2. 三类测试与命名约定](#2-三类测试与命名约定)
- [3. 构建与运行](#3-构建与运行)
- [4. Catch2 语法速查](#4-catch2-语法速查)
- [5. 编写 GPU 测试](#5-编写-gpu-测试)
- [6. 编写插件 / VFS 测试](#6-编写-插件--vfs-测试)
- [7. SECTION 嵌套子例](#7-section-嵌套子例)
- [8. 资源管理最佳实践](#8-资源管理最佳实践)
- [9. 错误处理与边界](#9-错误处理与边界)
- [10. 性能测试](#10-性能测试)
- [11. 测试覆盖范围与策略](#11-测试覆盖范围与策略)
- [12. 故障排除](#12-故障排除)
- [13. 相关文档](#13-相关文档)

---

## 1. 测试框架：Catch2

UsrLinuxEmu 使用 **Catch2 v2.x amalgamated 单文件版**，vendored 在 `tests/`：

```
tests/
├── catch_amalgamated.hpp   # 单头文件（约 520 KB）
├── catch_amalgamated.cpp   # 单源文件（约 350 KB）
└── test_*.cpp              # 30+ 测试用例
```

测试套件通过 `#include <catch_amalgamated.hpp>` 直接使用，**完全不依赖任何系统包**。所有测试基础设施都已经 vendored 在源码树里。

### Catch2 关键 API

Catch2 是 header-only 风格的单元测试框架，本项目使用 vendored 的 v2.x amalgamated 单文件版。

| 维度 | Catch2（项目实际使用） |
|------|---------------------|
| 头文件 | `<catch_amalgamated.hpp>` |
| 测试用例 | `TEST_CASE("name", "[tag]")` |
| 致命断言 | `REQUIRE(expr)`（失败立即终止 TEST_CASE）|
| 非致命断言 | `CHECK(expr)`（失败继续后续断言）|
| 子例/嵌套 | `SECTION("name")` 自动重新执行外层 |
| 系统安装 | 无需安装（vendored）|

---

## 2. 三类测试与命名约定

[`tests/CMakeLists.txt`](../../tests/CMakeLists.txt) 把测试分成三组：

| 类别 | 列表变量 | 链接 | 命名 | 用途 |
|------|----------|------|------|------|
| **Standalone** | `STANDALONE_TESTS` | `kernel` | `<src>_standalone` | 自定义 `main()` 的端到端测试 |
| **Catch2** | `CATCH2_TESTS` | `kernel` + `catch_amalgamated.cpp` | `<src>` | Catch2 风格 `TEST_CASE` / `REQUIRE` |
| **SIM** | `SIM_TESTS` | `kernel` + `gpu_sim` | `<src>_standalone` | 仿真层（scheduler/puller/hardware）测试 |

完整文件列表参见 [`docs/04-building/build_system.md`](build_system.md) §6.1。

新增 Catch2 测试的步骤：

1. 在 `tests/test_<name>.cpp` 写测试（`#include <catch_amalgamated.hpp>` + `TEST_CASE` + `REQUIRE`）。
2. 把文件名加到 `tests/CMakeLists.txt` 的 `CATCH2_TESTS` 列表（不是 `STANDALONE_TESTS`）。
3. 重新 `cmake .. && make -j$(nproc)`，CMake 会自动链接 `catch_amalgamated.cpp` 并生成可执行文件 `build/bin/test_<name>`。

---

## 3. 构建与运行

### 构建

```bash
# 必须从项目根目录
cd /workspace/project/UsrLinuxEmu

# 配置 + 构建（顶层 enable_testing() 已开启）
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
```

### 运行所有测试（CTest）

```bash
# 同样从项目根目录运行
cd /workspace/project/UsrLinuxEmu
cd build && ctest --output-on-failure && cd ..
```

### 运行单个测试

```bash
# 直接执行二进制
./build/bin/test_gpu_ioctl_standalone
./build/bin/test_va_space_standalone
./build/bin/test_gpu_ringbuffer_standalone
./build/bin/test_hardware_puller_emu_standalone
./build/bin/test_module_load_and_vfs_standalone

# Catch2 二进制支持 tag 过滤
./build/bin/test_gpu_memory                          # 跑该测试的所有 TEST_CASE
./build/bin/test_gpu_memory "[alloc]"                # 只跑 tag 含 [alloc] 的
./build/bin/test_gpu_memory "GPU memory allocation"  # 按 TEST_CASE 名字过滤
./build/bin/test_gpu_memory -s                       # 打印通过/失败的测试名
```

> **必须从项目根目录运行**：测试通过 `ModuleLoader::load_plugins("plugins")` 用相对路径加载插件。如果从 `build/bin/` 直接跑，会报 `Device not found`。

---

## 4. Catch2 语法速查

### 4.1 最小测试

```cpp
#include <catch_amalgamated.hpp>

TEST_CASE("GPU device returns vendor ID", "[gpu][device]") {
    REQUIRE(gpu_vendor_id() == 0x1000);
}
```

`TEST_CASE(name, tags)`：第一个参数是测试名（推荐自然语言），第二个参数是 tag（用 `[tag1][tag2]` 形式，便于 `-t` / `[tag]` 过滤）。

### 4.2 REQUIRE vs CHECK

| 宏 | 失败行为 | 适用场景 |
|----|----------|----------|
| `REQUIRE(expr)` | **致命**：当前 TEST_CASE 立即终止 | 前置条件、不变量 |
| `CHECK(expr)` | **非致命**：继续执行后续断言 | 期望大量独立断言都跑一遍的场景 |

```cpp
TEST_CASE("Multiple buffer allocations", "[gpu][memory]") {
    auto buf1 = alloc_buffer(1024);
    REQUIRE(buf1.valid());              // 致命：buf1 失败后面所有断言都没意义

    auto buf2 = alloc_buffer(2048);
    CHECK(buf2.valid());                // 非致命：即使失败也继续记录

    auto buf3 = alloc_buffer(4096);
    CHECK(buf3.valid());
}
```

### 4.3 常用断言

Catch2 的 `REQUIRE(expr)` / `CHECK(expr)` 用 C++ 表达式原语。布尔判断直接写表达式，数值相等用 `==`：

```cpp
REQUIRE(x == 42);
CHECK(vec.size() == 3);
REQUIRE(ptr != nullptr);
CHECK(s == "hello");
REQUIRE(flag);

CHECK_THROWS(stmt);        // 期望抛异常
CHECK_THROWS_AS(stmt, std::out_of_range);
CHECK_NOTHROW(stmt);
```

---

## 5. 编写 GPU 测试

参考 [`tests/test_gpu_memory.cpp`](../../tests/test_gpu_memory.cpp)（标准 Catch2 模板）。

### 5.1 标准模板

```cpp
#include <catch_amalgamated.hpp>

#include "gpu_driver/shared/gpu_ioctl.h"
#include "gpu_driver/shared/gpu_types.h"
#include "kernel/file_ops.h"
#include "kernel/module_loader.h"
#include "kernel/vfs.h"

using namespace usr_linux_emu;

// 一次性加载插件：避免重复 dlopen/dlclose 触发动态链接器缓存问题
struct PluginLifecycle {
  PluginLifecycle() {
    ModuleLoader::load_plugins("plugins");
  }
  ~PluginLifecycle() {
    ModuleLoader::unload_plugins();
  }
};
static PluginLifecycle plugin_lifecycle;

TEST_CASE("GPU memory allocation and free", "[gpu][memory]") {
  auto dev = VFS::instance().open("/dev/gpgpu0", 0);
  REQUIRE(dev != nullptr);
  REQUIRE(dev->fops != nullptr);

  gpu_alloc_bo_args alloc_args{};
  alloc_args.size   = 1024 * 1024;
  alloc_args.domain = GPU_MEM_DOMAIN_VRAM;
  alloc_args.flags  = GPU_BO_DEVICE_LOCAL;

  long ret = dev->fops->ioctl(0, GPU_IOCTL_ALLOC_BO, &alloc_args);
  REQUIRE(ret == 0);
  REQUIRE(alloc_args.handle != 0);
  REQUIRE(alloc_args.gpu_va != 0);

  u32 handle = alloc_args.handle;
  ret = dev->fops->ioctl(0, GPU_IOCTL_FREE_BO, &handle);
  REQUIRE(ret == 0);
}
```

### 5.2 常见 GPU 测试场景

| 场景 | 测试源 | 关键 ioctl |
|------|--------|-----------|
| 设备信息查询 | `test_gpu_ioctl.cpp` | `GPU_IOCTL_GET_DEVICE_INFO` |
| 显存分配 / 释放 | `test_gpu_memory.cpp` | `GPU_IOCTL_ALLOC_BO` / `GPU_IOCTL_FREE_BO` |
| mmap + 提交 | `test_gpu_mmap_and_submit.cpp` | `GPU_IOCTL_MAP_BO` + `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH` |
| VA Space 创建 | `test_va_space.cpp` | `GPU_IOCTL_CREATE_VA_SPACE` / `DESTROY_VA_SPACE` |
| Queue 创建 | `test_gpu_ioctl.cpp` | `GPU_IOCTL_CREATE_QUEUE` / `DESTROY_QUEUE` |
| Fence 等待 | `test_gpu_fence_return.cpp` | `GPU_IOCTL_WAIT_FENCE` |
| Ring buffer | `test_gpu_ringbuffer.cpp` | 多队列 fetch 链路 |
| Hardware puller | `test_hardware_puller_emu.cpp` | FSM 状态机 |

---

## 6. 编写插件 / VFS 测试

参考 [`tests/test_module_load_and_vfs.cpp`](../../tests/test_module_load_and_vfs.cpp)。

```cpp
#include <catch_amalgamated.hpp>

#include "kernel/module_loader.h"
#include "kernel/vfs.h"

using namespace usr_linux_emu;

TEST_CASE("Plugin loads and registers device", "[plugin][vfs]") {
  ModuleLoader::load_plugins("plugins");

  SECTION("Device lookup by path") {
    auto dev = VFS::instance().open("/dev/gpgpu0", O_RDWR);
    REQUIRE(dev != nullptr);
    REQUIRE(dev->fops != nullptr);
  }

  SECTION("Missing device returns nullptr") {
    auto dev = VFS::instance().open("/dev/nonexistent", O_RDWR);
    REQUIRE(dev == nullptr);
  }

  ModuleLoader::unload_plugins();
}
```

---

## 7. SECTION 嵌套子例

Catch2 的 `SECTION("name")` 是嵌套子例。每个 `SECTION` 会在每次进入 `TEST_CASE` 时**从头重新执行**外层 `TEST_CASE` 的代码（catch 的"tree of sub-cases"语义）。这意味着每个 section 自动获得一份干净的初始化状态，无需手动 `SetUp/TearDown`。

### 7.1 基础嵌套

```cpp
TEST_CASE("VA Space lifecycle", "[va_space]") {
  // 这段代码每个 SECTION 都会跑一遍
  gpu_va_space_args args{};
  args.page_size = 0;  // 4KB
  REQUIRE(dev->fops->ioctl(0, GPU_IOCTL_CREATE_VA_SPACE, &args) == 0);
  u64 handle = args.va_space_handle;
  REQUIRE(handle != 0);

  SECTION("Destroy with no attached queues") {
    long ret = dev->fops->ioctl(0, GPU_IOCTL_DESTROY_VA_SPACE, &handle);
    REQUIRE(ret == 0);
  }

  SECTION("Destroy with attached queue fails") {
    // 先创建一个 queue，挂在 VA Space 上
    gpu_queue_args q_args{};
    q_args.va_space_handle = handle;
    REQUIRE(dev->fops->ioctl(0, GPU_IOCTL_CREATE_QUEUE, &q_args) == 0);

    // 再销毁 VA Space 应该返回 -EBUSY
    long ret = dev->fops->ioctl(0, GPU_IOCTL_DESTROY_VA_SPACE, &handle);
    REQUIRE(ret == -EBUSY);
  }

  // 清理（每个 SECTION 跑完后都会执行）
  dev->fops->ioctl(0, GPU_IOCTL_DESTROY_VA_SPACE, &handle);
}
```

### 7.2 嵌套 SECTION

```cpp
TEST_CASE("Multi-page VA Space with mixed pages", "[va_space][nested]") {
  SECTION("4KB pages") {
    gpu_va_space_args args{};
    args.page_size = 0;  // 4KB
    dev->fops->ioctl(0, GPU_IOCTL_CREATE_VA_SPACE, &args);

    SECTION("Allocates 4KB-aligned BOs") {
      // ... 测试 4KB 对齐 ...
    }

    SECTION("Allocates 64KB BOs (跨多个 4KB 页)") {
      // ... 测试跨页 ...
    }
  }

  SECTION("64KB pages") {
    gpu_va_space_args args{};
    args.page_size = 1;  // 64KB
    dev->fops->ioctl(0, GPU_IOCTL_CREATE_VA_SPACE, &args);

    SECTION("Allocates 64KB-aligned BOs") {
      // ...
    }
  }
}
```

---

## 8. 资源管理最佳实践

### 8.1 RAII 守卫

```cpp
struct DeviceGuard {
  std::shared_ptr<Device> dev;
  DeviceGuard() {
    dev = VFS::instance().open("/dev/gpgpu0", O_RDWR);
    REQUIRE(dev != nullptr);  // 失败会终止整个 TEST_CASE
  }
};

TEST_CASE("GPU IOCTL round-trip", "[gpu]") {
  DeviceGuard guard;
  // 测试结束自动释放 guard.dev
}
```

### 8.2 避免显式 `main()`

Catch2 测试**不要**自己写 `main()`：

```cpp
// ❌ 错误：Standalone 模式会绕过 Catch2 的自动注册
int main() {
  ModuleLoader::load_plugins("plugins");
  auto dev = VFS::instance().open("/dev/gpgpu0", 0);
  // ...
}

// ✅ 正确：放进 TEST_CASE，让 Catch2 来组织执行
TEST_CASE("...", "[tag]") {
  static PluginLifecycle plugin_lifecycle;  // 全局一次性加载
  auto dev = VFS::instance().open("/dev/gpgpu0", 0);
  REQUIRE(dev != nullptr);
  // ...
}
```

如果确实需要自定义 `main()`（例如读特殊参数），把文件加入 `STANDALONE_TESTS` 而不是 `CATCH2_TESTS`，并提供 `CATCH_CONFIG_MAIN` 的替代。

---

## 9. 错误处理与边界

```cpp
TEST_CASE("Invalid arguments are rejected", "[gpu][errors]") {
  auto dev = VFS::instance().open("/dev/gpgpu0", 0);
  REQUIRE(dev != nullptr);

  SECTION("Zero size allocation fails") {
    gpu_alloc_bo_args args{};
    args.size = 0;
    long ret = dev->fops->ioctl(0, GPU_IOCTL_ALLOC_BO, &args);
    REQUIRE(ret != 0);  // 非 0 表示失败
  }

  SECTION("Invalid domain flag fails") {
    gpu_alloc_bo_args args{};
    args.size = 4096;
    args.domain = 0xDEADBEEF;  // 非法 domain
    long ret = dev->fops->ioctl(0, GPU_IOCTL_ALLOC_BO, &args);
    REQUIRE(ret != 0);
  }

  SECTION("Free with invalid handle fails") {
    u32 bad_handle = 0xFFFFFFFF;
    long ret = dev->fops->ioctl(0, GPU_IOCTL_FREE_BO, &bad_handle);
    REQUIRE(ret != 0);
  }
}
```

---

## 10. 性能测试

```cpp
#include <chrono>

TEST_CASE("10000 GPU BO allocations complete in < 1s", "[gpu][perf]") {
  using clock = std::chrono::high_resolution_clock;

  auto start = clock::now();

  for (int i = 0; i < 10000; ++i) {
    gpu_alloc_bo_args args{};
    args.size = 4096;
    args.domain = GPU_MEM_DOMAIN_VRAM;
    auto ret = dev->fops->ioctl(0, GPU_IOCTL_ALLOC_BO, &args);
    if (ret == 0) {
      u32 h = args.handle;
      dev->fops->ioctl(0, GPU_IOCTL_FREE_BO, &h);
    }
  }

  auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(
      clock::now() - start).count();

  CHECK(duration_us < 1'000'000);  // 1 秒
}
```

CI 上默认会跑性能测试；本地开发可以用 `./build/bin/test_xxx "[!perf]"` 跳过带 `[perf]` tag 的测试。

---

## 11. 测试覆盖范围与策略

### 11.1 重要场景清单

1. **边界条件**：最小/最大值、空值/零值、整数溢出
2. **错误路径**：无效输入、资源耗尽、ioctl 失败注入
3. **并发**：多线程访问、竞态条件、死锁恢复
4. **资源泄漏**：RAII 守卫、mock 析构次数统计
5. **跨模块集成**：plugin 加载 → VFS 注册 → ioctl → HAL → sim 的完整链路

### 11.2 覆盖率

```bash
# 启用 gcov 覆盖率
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_FLAGS="--coverage -g" \
      -DCMAKE_C_FLAGS="--coverage -g" ..
make -j$(nproc)

# 运行测试
ctest --output-on-failure

# 生成报告
gcovr -r .. --html --html-details -o coverage.html
firefox coverage.html
```

### 11.3 新功能测试要求

- 每个新功能至少 1 个 `TEST_CASE`
- 新功能代码覆盖率 ≥ 80%
- Bug 修复必须包含回归测试（`[regression]` tag）

---

## 12. 故障排除

| 症状 | 原因 | 解决 |
|------|------|------|
| `undefined reference to Catch::TestRegistrar` | 文件没在 `CATCH2_TESTS` 列表里 | 把 `.cpp` 加到 `tests/CMakeLists.txt` 的 `CATCH2_TESTS` |
| `Device not found` | 没从项目根目录运行 | `cd /workspace/project/UsrLinuxEmu` 后再跑测试 |
| `ioctl 返回 -EFAULT` | 结构体字段没初始化 | 用 `{}` 零初始化，再设需要的字段 |
| `ctest` 一个测试都没跑 | `BUILD_TESTS` 没开 | 顶层 `CMakeLists.txt` 已开启 `enable_testing()`，不需要 flag |
| 测试二进制找不到 `catch_amalgamated.hpp` | include 路径没设 | 检查 `tests/CMakeLists.txt` 的 `add_catch_test` 是否加了 `${CMAKE_CURRENT_SOURCE_DIR}` |

> 不要尝试用系统包管理器安装其他测试框架来"修复"上述问题——本项目**完全不依赖**任何外部测试包。所有 Catch2 设施都已 vendored。

---

## 13. 相关文档

| 文档 | 作用 |
|------|------|
| [build_system.md](build_system.md) | 构建系统详解、测试分三类的原因 |
| [ci-cd.md](ci-cd.md) | CI 配置（基于 Catch2） |
| [AGENTS.md](../../AGENTS.md) | 开发指南 + 编码风格 + 测试覆盖目标 |
| [post-refactor-architecture.md §1.7](../02_architecture/post-refactor-architecture.md) | 测试框架"声称 vs 实际"审计 |
| [tests/test_gpu_memory.cpp](../../tests/test_gpu_memory.cpp) | 标准 Catch2 模板 |
| [tests/CMakeLists.txt](../../tests/CMakeLists.txt) | 测试构建配置 |

---

**最后更新**: 2026-06-16  
**对应代码 commit**: `374d463`