# 架构对齐分析报告

**文档版本**: 1.1
**最后更新**: 2026-05-11
**维护者**: Sisyphus Analysis

---

## 一、概述

本报告基于 2026-05-11 对 UsrLinuxEmu 代码库的全面审查，对比架构文档与实际实现，识别架构债务、代码债务和冗余代码。

### 分析范围

| 领域 | 状态 |
|------|------|
| GPU ioctl 双系统 (System B / System C) | ⚠️ 需处理 |
| VFS 单例与 Issue #11 | ✅ 已修复 |
| 插件加载与设备注册 | ⚠️ 需处理 |
| 测试框架一致性 | ⚠️ 文档错误 |
| 文档与代码对齐 | ⚠️ 严重偏差 |
| 代码债务累积 | 🔴 高优先级 |

### 测试状态

- **通过率**: 24/24 测试 (100%)
- **框架**: Catch2 (非 Google Test)
- **测试覆盖**: kernel, GPU plugin, scheduler, translator

---

## 二、架构债务 (Architecture Debt)

### 2.1 双 GPU 驱动系统并存 — P1

项目存在 **两套完整 GPU 驱动实现**，而非迁移完成后清理旧代码：

| 层级 | System B (废弃) | System C (当前) |
|------|----------------|-----------------|
| 位置 | `drivers/gpu/` | `plugins/gpu_driver/` |
| ioctl 前缀 | `GPGPU_*` | `GPU_IOCTL_*` |
| 定义文件 | `drivers/gpu/ioctl_gpgpu.h` | `plugins/gpu_driver/shared/gpu_ioctl.h` |
| 插件输出 | `driver_gpu.so`, `plugin_gpu.so` | `gpu_driver_plugin.so` |
| HAL | 无（直接调用） | `hal/` 抽象层 |
| 模拟器 | `simulator/gpu/` (独立静态库) | `plugins/gpu_driver/sim/` (完整模拟引擎) |

**现状**：
- `drivers/gpu/ioctl_gpgpu.h` 第 7-9 行已标记 `@deprecated`
- 但 `drivers/gpu/gpu_driver.cpp`、`drivers/gpu/plugin_gpu.cpp`、`drivers/sample_gpu.cpp` 仍可编译
- 仍有 switch-case 处理 `GPGPU_*` 命令

**建议**：按 ADR-015 执行清理计划，删除 `drivers/gpu/` 下废弃代码或归档。

### 2.2 文档与代码严重不一致 — P1

`docs/02_architecture/architecture.md` 第 304-314 行定义的 ioctl 接口：

```cpp
// 文档描述的（废弃 System B）
#define GPGPU_ALLOC_MEM      _IOWR('G', 1, struct alloc_mem_args)
#define GPGPU_SUBMIT_COMMAND _IOW('G', 4, struct command_packet)
```

实际 `plugins/gpu_driver/shared/gpu_ioctl.h` 定义的是：

```cpp
// 实际运行的（当前 System C）
#define GPU_IOCTL_ALLOC_BO    _IOWR(GPU_IOCTL_BASE, 0x10, struct gpu_alloc_bo_args)
```

**影响**：
- `docs/06-reference/ioctl-commands.md` 大量引用 `GPGPU_*`（29+ 处），但测试和插件全部使用 `GPU_IOCTL_*`
- `docs/01-quickstart/first-example.md` 使用 `GPGPU_ALLOC_MEM`，与实际 API 不符
- `docs/04-building/testing_guide.md` 同样引用废弃接口

**建议**：
1. 更新所有文档引用 System C 接口
2. 在 `ioctl-commands.md` 顶部添加废弃警告

### 2.3 测试框架声明与实际不符 — P2

| 位置 | 声明 | 实际 |
|------|------|------|
| README.md | Google Test | Catch2 (`catch_amalgamated.hpp`) |
| AGENTS.md | Google Test | Catch2 |
| Copilot Instructions | Google Test | Catch2 |

`tests/` 目录下全部使用 Catch2 (`catch_amalgamated.hpp/cpp`)，无任何 GTest 依赖或配置。

### 2.4 文档路径引用错误 — P0/P1

README.md 和其他文档中存在大量路径引用错误：

| 文档位置 | 错误引用 | 实际路径 | 严重度 |
|---------|---------|---------|--------|
| README.md 第 22 行 | `docs/02-core/overview.md` | `docs/02_architecture/overview.md` | 🔴 P0 |
| README.md 第 22 行 | `docs/02-core/architecture.md` | `docs/02_architecture/architecture.md` | 🔴 P0 |
| README.md 第 24 行 | `docs/05-advanced/gpu-driver-architecture.md` | `docs/05-advanced/gpu_driver_architecture.md` | 🔴 P0 |
| README.md 第 24 行 | `docs/04-building/build-system.md` | `docs/04-building/build_system.md` | 🟡 P1 |
| README.md 第 24 行 | `docs/04-building/testing-guide.md` | `docs/04-building/testing_guide.md` | 🟡 P1 |
| docs/03-development/index.md | `docs/04-building/testing_guide.md` | 同上 | 🟡 P1 |

**影响**: 新开发者无法根据文档找到实际文件，影响入职效率。

**修复**: 更新 README.md 和相关文档中的路径引用。

---

## 三、代码债务 (Code Debt)

### 3.1 `decrease_ref` 清除所有设备 — P0 严重 bug

**文件**: `src/kernel/module_loader.cpp:38-51`

```cpp
void ModuleLoader::decrease_ref(const char* name) {
  auto it = loaded_plugins_.find(name);
  if (it != loaded_plugins_.end()) {
    if (--it->second->ref_count <= 0) {
      module* mod = it->second->mod;
      if (mod->exit)
        mod->exit();
      VFS::instance().clear_devices();   // ← 清除 ALL 设备
      VFS::shutdown();                    // ← 关闭 VFS 单例
      dlclose(it->second->handle);
      loaded_plugins_.erase(it);
    }
  }
}
```

**问题**：卸载任意一个插件会清除**所有设备**，而不是只清除该插件注册的设备。

**场景**：加载了 GPU + Serial 两个插件 → 卸载 Serial → GPU 设备丢失。

**修复建议**：按设备注册的 plugin_handle 过滤，只清除目标插件的设备。

### 3.2 缺失文件导致构建损坏 — P0

**文件**: `simulator/gpu/CMakeLists.txt` 第 4 行

```cmake
add_library(pcie_gpu_simulator STATIC pcie_gpu_simulator.cpp)
```

但 `simulator/gpu/pcie_gpu_simulator.cpp` 文件不存在，会导致该 target 构建失败。

**修复**：创建空实现文件或从 CMakeLists.txt 移除该 target。

### 3.3 冗余的插件系统 — P2

| 类 | 类型 | 功能 |
|----|------|------|
| `ModuleLoader` | 静态类 | 扫描目录 + 加载插件 |
| `PluginManager` | 单例模式 | 加载单个插件 |

两者做同样的事：`dlopen` → `dlsym("mod")` → `mod->init()`。Consumer 混乱，不知该用哪个。

### 3.4 VFS 无线程安全 — P2

**文件**: `src/kernel/vfs.cpp`

```cpp
std::unordered_map<std::string, std::shared_ptr<Device>> devices_;  // 无锁
```

但 `PollWatcher` 使用 `std::mutex`，`ServiceRegistry` 无锁 — 三者线程安全策略不一致。

### 3.5 消息前缀错误 — P3

**文件**: `src/kernel/module_loader.cpp:144`

```cpp
std::cerr << "[PluginManager] Plugin not found: " << name << std::endl;
```

`ModuleLoader::unload_plugin` 打印的是 `"[PluginManager]"`，消息前缀错误。

### 3.6 空目录 — P2

`include/usr_linux_emu/` 完全为空（0 文件），但存在于 include 路径中。

### 3.7 代码质量债务 — P1

#### 3.7.1 代码重复 (~90%) — P1

| 文件对 | 重复内容 | 影响 |
|--------|---------|------|
| `drv/gpgpu_device.cpp` vs `drv/gpu_drm_driver.cpp` | ~90% ioctl 处理逻辑 | 维护成本高，违反 DRY 原则 |

两套文件处理相同的 `GPGPU_*` / `GPU_IOCTL_*` 命令，switch-case 逻辑几乎完全相同。

#### 3.7.2 C 风格类型转换 — P1

多个文件使用不安全的基础类型转换：

| 文件 | 行 | 模式 | 风险 |
|------|-----|------|------|
| `hal/hal_user.cpp` | 多处 | `*(int*)argp` | 类型未定义，破坏严格别名规则 |
| `src/kernel/device/serial_device.cpp` | 多处 | `*(int*)argp` | 同上 |
| `src/kernel/file_ops.cpp` | 多处 | `(void*)` 强制转换 | 类型安全缺失 |

#### 3.7.3 魔数硬编码 — P1

| 文件 | 行 | 数值 | 说明 |
|------|-----|------|------|
| `hal/hal_user.cpp` | 多处 | `-22`, `-12` | errno 魔数，应使用 EFAULT 等常量 |

#### 3.7.4 全局静态状态 — P2

| 文件 | 变量 | 问题 |
|------|------|------|
| `plugins/gpu_driver/plugin.cpp` | `g_hal` | 全局静态，违反模块化原则 |
| `tests/test_hardware_puller_emu.cpp` | `g_next_entry_release_bit` | 已改为 atomic 但仍为静态 |

#### 3.7.5 using namespace 在公共头文件 — P1

`drv/gpgpu_device.h` 中包含 `using namespace usr_linux_emu`，污染公共 API 边界。

---

## 四、冗余/过时代码清单

| 文件 | 位置 | 状态 | 说明 |
|------|------|------|------|
| `ioctl_gpgpu.h` | `drivers/gpu/` | 已标记 `@deprecated` | System B，仅旧插件使用 |
| `gpu_driver.cpp` | `drivers/gpu/` | 废弃 | switch-case on `GPGPU_*` |
| `plugin_gpu.cpp` | `drivers/gpu/` | 废弃 | 注册 GpuDriver 设备 |
| `sample_gpu.cpp` | `drivers/` | 过时 | 简单示例，无 HAL |
| `pcie_gpu_simulator.cpp` | `simulator/gpu/` | **缺失** | CMake 引用但文件不存在 |

---

## 五、测试覆盖分析

### 5.1 测试统计

| 指标 | 值 |
|------|---|
| 测试文件总数 | 27 个 |
| 总代码行 | 2,494 行 |
| 测试框架 | Catch2 (`catch_amalgamated.hpp`) |
| ctest 注册测试 | 24 个 |
| 通过率 | 24/24 (100%) |

### 5.2 测试覆盖映射

| 源模块 | 测试覆盖 | 质量 | 备注 |
|--------|---------|------|------|
| `src/kernel/vfs.cpp` | ✅ `test_module_load_and_vfs.cpp` | 集成测试 | 覆盖 VFS + ModuleLoader |
| `src/kernel/module_loader.cpp` | ✅ `test_module_loader.cpp` | TDD 风格 | 插件加载/卸载 |
| `src/kernel/device.cpp` | ✅ `test_serial_device.cpp` | 基本 | 仅 serial 设备 |
| `src/kernel/logger.cpp` | ✅ `test_logger.cpp` | 基本 | 日志输出检查 |
| `src/kernel/poll_watcher.cpp` | ✅ `test_poll.cpp` | TDD 风格 | 异步等待测试 |
| `plugins/gpu_driver/hal/` | ✅ `test_gpu_ioctl.cpp` | 完整 | HAL ioctl |
| `plugins/gpu_driver/plugin.cpp` | ✅ `test_gpu_plugin.cpp` | 高 (43 assertions) | GPU 完整流程 |
| `plugins/gpu_driver/sim/scheduler/` | ✅ `test_global_scheduler.cpp` | TDD (8 tests) | FIFO + engine 路由 |
| `plugins/gpu_driver/sim/scheduler/translator/` | ✅ `test_gpfifo_translator.cpp` | TDD (6 tests) | GPFIFO→LaunchParams |
| `plugins/gpu_driver/sim/hardware_puller_emu.cpp` | ✅ `test_hardware_puller_emu.cpp` | TDD (11 tests) | 状态机 + race condition |
| `src/kernel/service_registry.cpp` | ❌ 无 | — | 缺失 |
| `src/kernel/config_manager.cpp` | ❌ 无 | — | 缺失 |
| `src/kernel/wait_queue.cpp` | ❌ 无 | — | 缺失 |

### 5.3 测试质量分级

| 级别 | 文件 | 特征 |
|------|------|------|
| **高** | `test_gpu_plugin.cpp` | 300 行，43 REQUIRE，16 TEST_CASE |
| **高** | `test_global_scheduler.cpp` | 8 个 TDD 测试，完全覆盖 |
| **高** | `test_hardware_puller_emu.cpp` | 11 tests，含 atomic race 检测 |
| **中** | `test_gpfifo_translator.cpp` | 6 tests，边界条件覆盖 |
| **低** | `test_gpu_mmap_bar.cpp` | 仅设备打开，无实际验证 |

### 5.4 缺失测试 (P1 优先级)

以下核心模块**没有对应测试**，需新增：

1. **service_registry.cpp** — 服务注册表，无单元测试
2. **config_manager.cpp** — 配置管理器，无测试
3. **wait_queue.cpp** — 等待队列，无测试

---

## 六、架构合规性检查

### 6.1 ✅ 正确的设计决策

| 决策 | 状态 | 证据 |
|------|------|------|
| kernel 库必须是 SHARED | ✅ | `src/CMakeLists.txt:1` `add_library(kernel SHARED ...)` |
| System C 是当前系统 | ✅ | `plugins/gpu_driver/shared/gpu_ioctl.h` canonical 定义 |
| HAL 分层 | ✅ | `plugins/gpu_driver/hal/` 硬件抽象层 |
| libgpu_core C 库 | ✅ | `libgpu_core/` 伙伴分配器提取为纯 C |

### 6.2 ⚠️ 需要修复的合规性问题

| 问题 | 风险 | 建议 |
|------|------|------|
| kernel 可能被误改回 STATIC | 高 | 在 CMakeLists.txt 添加注释防止误改 |
| TaskRunner symlink 循环 | 中 | 文档化或修复 symlink |
| `decrease_ref` 清除所有设备 | 高 | 重构为按 plugin_handle 过滤 |

---

## 七、优先级排序与修复计划

### P0 — 立即修复

| # | 问题 | 影响 | 估计工时 |
|---|------|------|---------|
| 1 | `pcie_gpu_simulator.cpp` 缺失 | 构建失败 | 10 分钟 |
| 2 | `decrease_ref` 清除所有设备 | 运行时崩溃 | 2 小时 |
| 3 | 文档路径错误 (`02-core/`, `gpu-driver-architecture.md`) | 新开发者无法找到文件 | 2 小时 |

### P1 — 本周修复

| # | 问题 | 影响 | 估计工时 |
|---|------|------|---------|
| 4 | 双驱动并存未清理 | 技术债累积 | 4 小时 |
| 5 | 文档引用废弃 System B | 开发者误导 | 6 小时 |
| 6 | 代码重复 (gpgpu_device.cpp vs gpu_drm_driver.cpp) | 维护成本高 | 4 小时 |
| 7 | C 风格类型转换 | 类型安全风险 | 2 小时 |
| 8 | 魔数硬编码 (-22, -12) | 可读性差 | 1 小时 |
| 9 | `using namespace` 在公共头文件 | 命名空间污染 | 30 分钟 |
| 10 | 缺失单元测试 (service_registry, config_manager, wait_queue) | 回归风险 | 4 小时 |

### P2 — 计划修复

| # | 问题 | 影响 | 估计工时 |
|---|------|------|---------|
| 11 | 测试框架声明错误 (GTest vs Catch2) | 文档错误 | 1 小时 |
| 12 | 空目录 `include/usr_linux_emu/` | 未使用代码 | 10 分钟 |
| 13 | VFS 无线程安全 | 并发风险 | 4 小时 |
| 14 | ModuleLoader/PluginManager 冗余 | 代码复杂度 | 2 小时 |
| 15 | 全局静态状态 (g_hal, g_next_entry_release_bit) | 违反模块化 | 2 小时 |

### P3 — 后续处理

| # | 问题 | 影响 | 估计工时 |
|---|------|------|---------|
| 16 | TaskRunner symlink 循环 | 工具报错 | 1 小时 |
| 17 | 消息前缀错误 | 调试困惑 | 10 分钟 |

---

## 八、快速修复步骤

### 8.1 修复缺失的 pcie_gpu_simulator.cpp

```bash
# 创建空实现文件
touch simulator/gpu/pcie_gpu_simulator.cpp
```

或在 `simulator/gpu/CMakeLists.txt` 中移除该 target。

### 8.2 修复 decrease_ref 设备清除问题

将 `VFS::instance().clear_devices()` + `VFS::shutdown()` 改为：
1. 遍历 VFS 中设备
2. 仅移除 plugin_handle 匹配目标的设备
3. 不调用 shutdown()

### 8.3 更新文档路径引用

```bash
# 修复 README.md 中的路径
sed -i 's|docs/02-core/|docs/02_architecture/|g' README.md docs/03-development/index.md
sed -i 's|gpu-driver-architecture.md|gpu_driver_architecture.md|g' README.md
sed -i 's|build-system.md|build_system.md|g' README.md
sed -i 's|testing-guide.md|testing_guide.md|g' README.md
```

---

## 九、相关 ADR

- **ADR-015**: GPU ioctl 统一 — System C 迁移
- **ADR-016**: GPU memory domain 支持
- **ADR-019**: DRM GEM/TTM 对齐
- **ADR-020**: libgpu_core C 库提取

---

## 十、变更日志

| 日期 | 版本 | 变更 |
|------|------|------|
| 2026-05-09 | 1.0 | 初始版本 — 全面架构审查 |
| 2026-05-11 | 1.1 | 新增：测试覆盖分析、代码债务详细条目、文档路径错误、P1 优先级修复清单 |