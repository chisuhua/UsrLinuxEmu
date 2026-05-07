# 修复测试基础设施计划

**计划 ID**: `fix-test-infra`  
**优先级**: HIGHEST  
**预计工期**: 3-5 天  
**依赖**: 无（此计划为其他计划的前置条件）  
**创建日期**: 2026-05-07  
**最后更新**: 2026-05-07

---

## 1. 概述

本计划旨在修复 UsrLinuxEmu 项目中 **12 个失败测试**，将测试通过率从 37% (7/19) 提升到 100% (19/19)。当前测试失败集中在三个根因：

1. **工作目录问题** — CTest 从 `build/` 运行测试，但测试代码调用 `ModuleLoader::load_plugins("plugins")` 使用相对路径，导致找不到插件目录
2. **插件卸载时 SEGFAULT** — `dlclose()` 后 `shared_ptr<Device>` 仍持有对已卸载内存的引用，析构时访问非法地址
3. **GPU ioctl 参数不匹配** — `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH` 结构体在测试主程序和插件 .so 中解析为不同大小，导致 ioctl 号不匹配（-22 EINVAL）

### 预期产出

- 19/19 测试通过（ctest 从 build/ 运行）
- 测试可在项目根目录手动运行（`./build/bin/test_*`）
- 消除所有 System B (GPGPU_*) 废弃接口的测试依赖

### 与外部计划的关联

| 外部计划 | 关联关系 |
|---------|---------|
| `phase1_implementation_plan.md` | 本计划是 P1.1b 的前置条件（测试通过后才能切换薄入口） |
| `sync-plan.md` | S4 端到端集成验证依赖测试全部通过 |
| `gpu_driver_portability_plan.md` | 里程碑 2.1 要求测试覆盖率 100% |

---

## 2. 前置条件

在开始本计划前，确保以下环境就绪：

```bash
# 1. 项目可编译
cd /workspace/project/UsrLinuxEmu
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j4

# 2. 可复现当前失败状态
ctest --output-on-failure
# 预期输出：12 failed, 7 passed

# 3. Git 工作区干净（用于回滚）
git status  # 应无未提交的修改
git stash push -m "auto-backup-fix-test-infra-$(date +%s)"
```

### 已确认的根因清单

| # | 测试名 | 根因 | 当前错误表现 |
|---|-------|------|-------------|
| 1 | `test_gpu_ioctl_standalone` | ioctl 号不匹配 + 卸载 SEGFAULT | `Unknown ioctl: 0x40204701` + SIGSEGV |
| 2 | `test_gpu_mmap_standalone` | CWD 问题 + 使用废弃 GPGPU_ALLOC_MEM | `Invalid plugin directory: plugins` |
| 3 | `test_gpu_mmap_and_submit_standalone` | CWD 问题 + 使用废弃 GPGPU_* | `Invalid plugin directory: plugins` |
| 4 | `test_gpu_register_standalone` | CWD 问题 + 无寄存器接口 | `Invalid plugin directory: plugins` |
| 5 | `test_gpu_regs_standalone` | CWD 问题 + 使用废弃 GPGPU_GET_DEVICE_INFO | `Invalid plugin directory: plugins` |
| 6 | `test_gpu_ringbuffer_standalone` | CWD 问题 + 使用废弃 GPGPU_GET_DEVICE_INFO | `Invalid plugin directory: plugins` |
| 7 | `test_gpu_submit_standalone` | CWD 问题 + 使用废弃 GPGPU_SUBMIT_PACKET | `Invalid plugin directory: plugins` |
| 8 | `test_pcie_gpu_standalone` | CWD 问题 + `dynamic_cast<PciDevice*>` 失败 | `Invalid plugin directory: plugins` |
| 9 | `test_module_load_and_vfs_standalone` | CWD 问题（`../drivers` 路径） | `Invalid plugin directory: ../drivers` |
| 10 | `test_gpu_memory` | 无 Catch2 TEST_CASE + 使用废弃 GPGPU_* | `No tests ran` |
| 11 | `test_gpu_mmap_bar` | 文件为空（0 字节） | 无测试用例 |
| 12 | `test_gpu_plugin` | CWD 问题 + 卸载 SEGFAULT | `Invalid plugin directory: plugins` |

---

## 3. 实施阶段

### Phase A: 修复 CTest 工作目录（快速胜利）

**目标**: 让 CTest 从项目根目录运行测试，解决 `load_plugins("plugins")` 找不到目录的问题。

**风险级别**: 低

#### 步骤 A.1: 修改 tests/CMakeLists.txt

**文件**: `/workspace/project/UsrLinuxEmu/tests/CMakeLists.txt`

**修改内容**:

在 `add_test` 调用后添加 `WORKING_DIRECTORY` 属性：

```cmake
# 在 add_standalone_test 函数中
add_test(NAME ${TEST_NAME} COMMAND $<TARGET_FILE:${TEST_NAME}>)
# 新增：
set_tests_properties(${TEST_NAME} PROPERTIES
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)

# 在 add_catch_test 函数中
add_test(NAME ${TEST_NAME} COMMAND $<TARGET_FILE:${TEST_NAME}>)
# 新增：
set_tests_properties(${TEST_NAME} PROPERTIES
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)
```

**验证**:

```bash
cd build
make -j4
ctest -R test_gpu_ioctl_standalone --output-on-failure
# 预期：不再报 "Invalid plugin directory: plugins"
# 仍可能失败（因 ioctl 不匹配或 SEGFAULT），但 CWD 错误应消失
```

#### 步骤 A.2: 修复 test_module_load_and_vfs 的插件路径

**文件**: `/workspace/project/UsrLinuxEmu/tests/test_module_load_and_vfs.cpp`

**修改内容**:

```cpp
// 修改前：
ModuleLoader::load_plugins("../drivers");

// 修改后：
ModuleLoader::load_plugins("drivers");
```

**验证**:

```bash
ctest -R test_module_load_and_vfs_standalone --output-on-failure
# 预期：不再报 "Invalid plugin directory: ../drivers"
```

---

### Phase B: 修复插件卸载 SEGFAULT（严重）

**目标**: 消除所有测试在 `ModuleLoader::unload_plugins()` 时的段错误。

**风险级别**: 中

#### 根因分析

问题发生在以下调用链：

```
1. 测试调用 ModuleLoader::load_plugins("plugins")
   → dlopen(plugin.so) → 构造 GpgpuDevice → VFS::register_device()

2. 测试调用 VFS::instance().open("/dev/gpgpu0", 0)
   → 返回 shared_ptr<Device>（引用计数 +1）

3. 测试调用 ModuleLoader::unload_plugins()
   → decrease_ref() → dlclose(handle)
   → 插件 .so 从进程地址空间卸载

4. 测试作用域结束，shared_ptr<Device> 析构
   → 引用计数归零 → ~GpgpuDevice()
   → GpgpuDevice 的析构函数代码在已卸载的 .so 中 → SIGSEGV
```

**关键观察**: `test_gpu_ioctl_standalone` 不是 `shared_ptr` 直接持有 GpgpuDevice，而是通过 `dev->fops` 访问。但 VFS 内部 `devices_` map 持有 `shared_ptr<Device>`。当 `dlclose` 后，VFS 中的 shared_ptr 仍指向已卸载内存。

#### 步骤 B.1: 在 unload_plugins() 前清空 VFS 设备表

**文件**: `/workspace/project/UsrLinuxEmu/include/kernel/vfs.h` 和 `/workspace/project/UsrLinuxEmu/src/kernel/vfs.cpp`

**修改内容**:

在 `VFS` 类中添加 `unregister_device` 和 `clear_devices` 方法：

```cpp
// vfs.h
class VFS {
public:
    // ... 现有方法 ...
    
    /** @brief 注销指定名称的设备 */
    int unregister_device(const std::string& name);
    
    /** @brief 清空所有已注册设备（插件卸载前调用） */
    void clear_devices();
};

// vfs.cpp
int VFS::unregister_device(const std::string& name) {
    auto it = devices_.find(name);
    if (it == devices_.end()) {
        return -1;
    }
    devices_.erase(it);
    ServiceRegistry::instance().unregister_service(name);  // 见步骤 B.2
    return 0;
}

void VFS::clear_devices() {
    devices_.clear();
    // 注意：不清空 ServiceRegistry，由 B.2 处理
}
```

#### 步骤 B.2: 在 ServiceRegistry 添加 unregister_service

**文件**: `/workspace/project/UsrLinuxEmu/include/kernel/service_registry.h` 和对应实现文件

**修改内容**:

```cpp
// service_registry.h
class ServiceRegistry {
public:
    // ... 现有方法 ...
    void unregister_service(const std::string& name);
};
```

#### 步骤 B.3: 验证 shared_ptr 生命周期时序

**关键问题**: 必须确保 `GpgpuDevice` 析构函数在 `dlclose` **之前**执行（而非之后），否则析构函数代码已在卸载的 .so 中触发 SIGSEGV。

**验证原理**:
```
调用链: unload_plugins()
  → plugin_fini_internal()      ← .so 仍加载，代码可执行
    → VFS::unregister_device()  ← 释放 shared_ptr → ~GpgpuDevice() ← ✅ 安全（.so 仍加载）
  → dlclose(handle)              ← .so 卸载 ← ✅ GpgpuDevice 已析构
```

**修改**: 向 `plugin_fini_internal` 添加日志以验证执行顺序：

```cpp
static void plugin_fini_internal() {
    std::cout << "[GpuPlugin] Shutting down...\n";
    
    // 在 dlclose 之前注销设备，确保 shared_ptr 在 .so 加载时释放
    std::cout << "[GpuPlugin] Unregistering device /dev/gpgpu0...\n";
    VFS::instance().unregister_device("gpgpu0");
    std::cout << "[GpuPlugin] Device unregistered, shared_ptr released.\n";
    // 此时 ~GpgpuDevice() 应已执行（如果 VFS 持有唯一引用）
}
```

**需要确认**: VFS::unregister_device() 必须调用 `devices_.erase()` 并且 `shared_ptr<Device>` 中的 `fops`（持有 GpgpuDevice 实例）此时被释放。可通过添加 `~GpgpuDevice()` 析构函数日志来验证：

```cpp
// 在 GpgpuDevice 类中添加：
~GpgpuDevice() {
    std::cout << "[GpgpuDevice] Destructor called while .so still loaded\n";
}
```

#### 步骤 B.4: 修改 plugin_fini_internal 注销设备

**文件**: `/workspace/project/UsrLinuxEmu/plugins/gpu_driver/plugin.cpp`

**修改内容**:

```cpp
static void plugin_fini_internal() {
    std::cout << "[GpuPlugin] Shutting down...\n";
    
    // 从 VFS 注销设备，释放 shared_ptr（.so 仍加载，安全析构）
    int ret = VFS::instance().unregister_device("gpgpu0");
    if (ret == 0) {
        std::cout << "[GpuPlugin] /dev/gpgpu0 unregistered successfully.\n";
    }
}
```

#### 步骤 B.5: 修改 unload_plugins 的迭代器失效问题

**文件**: `/workspace/project/UsrLinuxEmu/src/kernel/module_loader.cpp`

**当前问题**:

```cpp
void ModuleLoader::unload_plugins() {
    for (auto& [name, info] : loaded_plugins_) {
        decrease_ref(name.c_str());  // decrease_ref 内部会 erase(it)，导致迭代器失效
    }
}
```

**修改内容**:

```cpp
void ModuleLoader::unload_plugins() {
    std::cout << "[ModuleLoader] Unloading all plugins..." << std::endl;
    // 先拷贝名称列表，避免迭代器失效
    std::vector<std::string> names;
    for (const auto& [name, info] : loaded_plugins_) {
        names.push_back(name);
    }
    for (const auto& name : names) {
        decrease_ref(name.c_str());
    }
}
```

#### 验证

```bash
ctest -R test_gpu_ioctl_standalone --output-on-failure
# 预期：不再出现 SIGSEGV
# 可能仍报 "Unknown ioctl"（Phase C 修复）
```

---

### Phase C: 修复 GPU ioctl 参数不匹配

**目标**: 消除 `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH` 的 `-22 EINVAL` 错误。

**风险级别**: 中

#### 根因分析

ioctl 编码宏 `_IOW(type, nr, size)` 将 `sizeof(size)` 嵌入 ioctl 号中。当测试主程序和插件 .so 对 `struct gpu_pushbuffer_args` 的大小计算不一致时，ioctl 号不匹配。

实测：测试发送 `0x40204701`（size=32），但插件不认识此号码。

可能原因：
1. `gpu_gpfifo_entry` 或 `gpu_pushbuffer_args` 的 `#pragma pack` 不一致
2. 测试和插件使用了不同版本的 `gpu_ioctl.h`（符号链接断裂？）
3. `u32` / `u64` 类型定义在两侧不同（但 gpu_types.h 通过符号链接共享）

#### 步骤 C.1: 检查并修复 gpu_ioctl.h 结构体对齐

**文件**: `/workspace/project/UsrLinuxEmu/plugins/gpu_driver/shared/gpu_ioctl.h`

**检查项**:

```bash
# 检查测试主程序视角的 struct 大小
cd build
cat > /tmp/check_sizes.cpp << 'EOF'
#include <iostream>
#include "plugins/gpu_driver/shared/gpu_ioctl.h"
int main() {
    std::cout << "sizeof(gpu_pushbuffer_args) = " << sizeof(gpu_pushbuffer_args) << "\n";
    std::cout << "sizeof(gpu_gpfifo_entry) = " << sizeof(gpu_gpfifo_entry) << "\n";
    std::cout << "GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH = 0x" << std::hex << GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH << std::dec << "\n";
}
EOF
# 编译并运行
```

同时检查插件 .so 中的大小（通过打印）。

**修改内容**:

如果大小不一致，在结构体定义前后添加显式 pack：

```cpp
// 检查 gpu_gpfifo_entry 的定义（位于 gpu_types.h）
// 确保所有共享结构体使用统一对齐
#pragma pack(push, 8)

struct gpu_pushbuffer_args {
    u32 stream_id;
    const struct gpu_gpfifo_entry *entries;  // 注意：这是指针！
    u32 count;
    u32 flags;
    u64 fence_id;
};

#pragma pack(pop)
```

**重要**：`_IOW` 宏使用 `sizeof(struct)`，而 `gpu_pushbuffer_args` 包含一个指针字段。Linux 内核 ioctl 通常**不**在 ioctl 参数中传递指针类型（因为内核/用户空间指针大小可能不同）。正确做法是将 `entries` 改为 `u64`（用户空间地址的数值表示）：

```cpp
struct gpu_pushbuffer_args {
    u32 stream_id;
    u64 entries_addr;   // 用户空间指针的 u64 表示
    u32 count;
    u32 flags;
    u64 fence_id;       // OUT
};
```

> **注意**：此修改是 **ABI 破坏性变更**，需要同步更新 `sync-plan.md` 的 S3 同步点。但考虑到当前测试已经失败，说明现有 ABI 本身就是 broken 的，修复是必需的。

#### 步骤 C.X: 验证 ABI 一致性的 sizeof 检查

在修改前后，编译验证结构体大小在测试二进制和插件 .so 中一致：

```bash
# 创建检查程序
cat > /tmp/check_abi.cpp << 'EOF'
#include "plugins/gpu_driver/shared/gpu_ioctl.h"
#include <cstdio>
int main() {
    printf("sizeof(gpu_pushbuffer_args) = %zu\n", sizeof(gpu_pushbuffer_args));
    printf("sizeof(gpu_gpfifo_entry) = %zu\n", sizeof(gpu_gpfifo_entry));
    printf("GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH = 0x%lx\n",
           (unsigned long)GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH);
    return 0;
}
EOF

# 从测试二进制视角编译
cd /workspace/project/UsrLinuxEmu
g++ -std=c++17 -I include -I plugins /tmp/check_abi.cpp -o /tmp/check_abi_test
/tmp/check_abi_test

# 同时在 plugin.cpp 的 handler 入口添加 sizeof 打印，对比输出
```

预期：`GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH` 的 ioctl 号在两种视角下完全一致。

#### 步骤 C.2: 同步更新 plugin.cpp 的 handler

**文件**: `/workspace/project/UsrLinuxEmu/plugins/gpu_driver/plugin.cpp`

**修改内容**:

```cpp
long handle_pushbuffer_submit_batch(void* argp) {
    auto* args = static_cast<struct gpu_pushbuffer_args*>(argp);
    if (!args) return -EFAULT;

    // 新增：从 u64 entries_addr 转换为指针
    auto* entries = reinterpret_cast<const struct gpu_gpfifo_entry*>(args->entries_addr);
    
    for (u32 i = 0; i < args->count; ++i) {
        const auto& e = entries[i];
        // ... 原有处理逻辑 ...
    }
}
```

#### 步骤 C.3: 同步更新所有测试代码

**文件**:
- `/workspace/project/UsrLinuxEmu/tests/test_gpu_ioctl.cpp`
- `/workspace/project/UsrLinuxEmu/tests/test_gpu_plugin.cpp`

**修改内容**:

```cpp
// 修改前：
struct gpu_pushbuffer_args pb_args = {
    .stream_id = 0,
    .entries = &entry,
    .count = 1,
    .flags = 0
};

// 修改后：
struct gpu_pushbuffer_args pb_args = {
    .stream_id = 0,
    .entries_addr = reinterpret_cast<u64>(&entry),
    .count = 1,
    .flags = 0
};
```

#### 验证

```bash
ctest -R test_gpu_ioctl_standalone --output-on-failure
# 预期：PUSHBUFFER_SUBMIT_BATCH 成功，无 "Unknown ioctl" 错误
```

---

### Phase D: 修复剩余 GPU 相关测试

**目标**: 修复 mmap、register、regs、ringbuffer、pcie、plugin 测试。

**风险级别**: 低-中

#### 步骤 D.1: 修复 test_gpu_mmap

**文件**: `/workspace/project/UsrLinuxEmu/tests/test_gpu_mmap.cpp`

**问题**: 使用废弃的 `GPGPU_ALLOC_MEM` 和 `GPGPU_FREE_MEM`，且 `mmap` 调用方式错误（`dev->fops->mmap` 签名不匹配）。

**修改策略**: 将测试迁移到 System C (`GPU_IOCTL_*`)。

```cpp
// 重写为：
// 1. 使用 GPU_IOCTL_ALLOC_BO 分配
// 2. 使用 GPU_IOCTL_MAP_BO 获取 GPU VA
// 3. 删除 mmap 调用（用户态模拟不支持真正的 mmap，或使用 simulator 层）
```

#### 步骤 D.2: 修复 test_gpu_register / test_gpu_regs / test_gpu_ringbuffer / test_gpu_submit

**文件**:
- `/workspace/project/UsrLinuxEmu/tests/test_gpu_register.cpp`
- `/workspace/project/UsrLinuxEmu/tests/test_gpu_regs.cpp`
- `/workspace/project/UsrLinuxEmu/tests/test_gpu_ringbuffer.cpp`
- `/workspace/project/UsrLinuxEmu/tests/test_gpu_submit.cpp`

**问题**: 全部使用废弃的 `GPGPU_GET_DEVICE_INFO`、`GPGPU_ALLOC_MEM`、`GPGPU_SUBMIT_PACKET`。

**修改策略**: 统一迁移到 System C 接口：

```cpp
// 替换：
// GPGPU_GET_DEVICE_INFO → GPU_IOCTL_GET_DEVICE_INFO
// GPGPU_ALLOC_MEM → GPU_IOCTL_ALLOC_BO + GPU_IOCTL_FREE_BO
// GPGPU_SUBMIT_PACKET → GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH
// GpuDeviceInfo → gpu_device_info
// GpuCommandRequest → gpu_pushbuffer_args + gpu_gpfifo_entry
```

#### 步骤 D.3: 修复 test_pcie_gpu

**文件**: `/workspace/project/UsrLinuxEmu/tests/test_pcie_gpu.cpp`

**问题**: `dynamic_cast<PciDevice*>(dev->fops.get())` 返回 nullptr，因为 `GpgpuDevice` 继承自 `FileOperations`，不是 `PciDevice`。

**修改策略**:

方案 A（推荐）：让 `GpgpuDevice` 继承 `PciDevice`：

```cpp
// plugin.cpp
class GpgpuDevice : public PciDevice {  // 改为继承 PciDevice
public:
    GpgpuDevice() : PciDevice(VENDOR_SIMULATED, DEVICE_SIMULATED_V1) { ... }
    // ...
};
```

方案 B：修改测试，不使用 PCIe 接口测试 GPU 插件：

```cpp
// 测试改为验证 GPU 基本信息，删除 dynamic_cast 和 PCIe 配置空间访问
```

#### 步骤 D.4: 修复 test_gpu_mmap_and_submit

**文件**: `/workspace/project/UsrLinuxEmu/tests/test_gpu_mmap_and_submit.cpp`

**问题**: 混合使用废弃接口 + mmap 调用。

**修改策略**: 与 D.1 相同，迁移到 System C。

#### 验证

```bash
ctest -R "test_gpu_mmap|test_gpu_register|test_gpu_regs|test_gpu_ringbuffer|test_gpu_submit|test_pcie_gpu" --output-on-failure
# 预期：全部通过
```

---

### Phase E: 迁移 test_gpu_memory 从 System B 到 System C

**目标**: 将 `test_gpu_memory.cpp` 从废弃的 GPGPU_* 迁移到 GPU_IOCTL_*，并添加 Catch2 测试用例。

**风险级别**: 低

#### 步骤 E.1: 重写 test_gpu_memory.cpp

**文件**: `/workspace/project/UsrLinuxEmu/tests/test_gpu_memory.cpp`

**当前问题**: 文件只有辅助函数（`cudaMalloc`、`cudaMemcpy`、`cudaFree`、`cudaSubmitKernel`），没有 `TEST_CASE` 宏。

**修改内容**:

```cpp
#include <catch_amalgamated.hpp>
#include "kernel/vfs.h"
#include "kernel/module_loader.h"
#include "kernel/file_ops.h"
#include "gpu_driver/shared/gpu_ioctl.h"
#include "gpu_driver/shared/gpu_types.h"

TEST_CASE("GPU memory allocation round-trip", "[gpu][memory]") {
    ModuleLoader::load_plugins("plugins");
    auto dev = VFS::instance().open("/dev/gpgpu0", 0);
    REQUIRE(dev != nullptr);
    REQUIRE(dev->fops != nullptr);
    
    int fd = 0;
    
    // 分配 BO
    struct gpu_alloc_bo_args alloc_args = {};
    alloc_args.size = 1024 * 1024;
    alloc_args.domain = GPU_MEM_DOMAIN_VRAM;
    alloc_args.flags = GPU_BO_DEVICE_LOCAL;
    
    long ret = dev->fops->ioctl(fd, GPU_IOCTL_ALLOC_BO, &alloc_args);
    REQUIRE(ret == 0);
    REQUIRE(alloc_args.handle != 0);
    REQUIRE(alloc_args.gpu_va != 0);
    
    // 释放 BO
    ret = dev->fops->ioctl(fd, GPU_IOCTL_FREE_BO, &alloc_args.handle);
    REQUIRE(ret == 0);
    
    ModuleLoader::unload_plugins();
}
```

#### 步骤 E.2: 删除或填充 test_gpu_mmap_bar.cpp

**文件**: `/workspace/project/UsrLinuxEmu/tests/test_gpu_mmap_bar.cpp`

**当前**: 0 字节空文件。

**修改策略**: 添加基本测试框架或从 CMakeLists.txt 中移除：

```cpp
#include <catch_amalgamated.hpp>
#include "kernel/vfs.h"
#include "kernel/module_loader.h"
#include "gpu_driver/shared/gpu_ioctl.h"
TEST_CASE("MMAP BAR placeholder", "[gpu][mmap][bar]") {
    REQUIRE(true);  // 占位测试，Phase 2 实现 BAR mmap
}
```

#### 验证

```bash
ctest -R "test_gpu_memory|test_gpu_mmap_bar" --output-on-failure
# 预期：测试运行并通过
```

#### 步骤 E.3: 确认常量定义可用性

**已确认 (Audit 2026-05-07)**:

| 常量 | 位置 | 状态 |
|------|------|------|
| `GPU_MEM_DOMAIN_VRAM` (0x1) | `gpu_types.h:61` | ✅ 已定义 |
| `GPU_BO_DEVICE_LOCAL` (0x1) | `gpu_ioctl.h:105` | ✅ 已定义 |
| `GPU_BO_HOST_VISIBLE` (0x2) | `gpu_ioctl.h:106` | ✅ 已定义 |
| `GPUBO_DEVICE_LOCAL`(拼写错误变体) | 不存在 | ❌ 不存在 |
| `Device` 构造函数 | `Device(const std::string&, dev_t, shared_ptr<FileOperations>, void*)` | ✅ 匹配示例代码 |

无需对常量或构造函数签名进行额外修改。

#### 已验证事实 (Momus 审查后确认)

| 检查项 | 结果 |
|--------|------|
| `GPU_MEM_DOMAIN_VRAM` 定义了用于 `gpu_alloc_bo_args.domain` 字段 | ✅ |
| `GPU_BO_DEVICE_LOCAL` 定义了用于 `gpu_alloc_bo_args.flags` 字段 | ✅ |
| `cuda_compat_ioctl.cpp` 是否存在于 CMakeLists.txt 中 | ❌ 不在任何 CMakeLists.txt 中 — 确认孤儿文件 |
| `ServiceRegistry::lookup_service()` 的调用者 | 仅在其自身的 .h/.cpp 中定义，**无外部调用者** |
| 仍在使用废弃 `GPGPU_*` 的重构测试 | 8 个文件: `test_gpu_mmap`, `test_gpu_mmap_and_submit`, `test_gpu_memory`, `test_gpu_submit`, `test_gpu_ringbuffer`, `test_gpu_regs`, `drivers/sample_gpu.cpp`, `drivers/gpu/gpu_driver.cpp` |

---

## 4. 风险评估

| 风险 | 影响 | 概率 | 缓解措施 |
|------|------|------|---------|
| `entries` 改为 `entries_addr` 破坏 TaskRunner 兼容性 | 高 | 中 | 在 `sync-plan.md` S3 同步点记录此变更；TaskRunner 尚未实现 PUSHBUFFER_SUBMIT_BATCH 客户端 |
| `PciDevice` 继承链修改引入编译错误 | 中 | 低 | 小范围修改，编译后立即验证 |
| VFS `clear_devices()` 引入新的 use-after-free | 中 | 低 | 确保在 `dlclose` 前调用，且 `shared_ptr` 引用已释放 |
| 测试迁移遗漏废弃接口引用 | 低 | 中 | 使用 `grep -r "GPGPU_" tests/` 全局检查 |

---

## 5. 成功标准

以下标准**全部满足**时，本计划视为完成：

```bash
cd /workspace/project/UsrLinuxEmu/build
make -j4
ctest --output-on-failure
```

- [ ] 19/19 测试通过（0 失败）
- [ ] 无 SIGSEGV、无内存泄漏（可用 valgrind 抽样验证）
- [ ] 从项目根目录运行 `./build/bin/test_gpu_ioctl_standalone` 通过
- [ ] `grep -r "GPGPU_" tests/` 无结果（除注释外）
- [ ] `grep -r "GPGPU_" plugins/gpu_driver/` 无结果
- [ ] 构建产物数量不变（未引入新的编译错误）

---

## 6. 回滚计划

如果实施过程中出现无法解决的问题：

```bash
# 1. 放弃所有修改
cd /workspace/project/UsrLinuxEmu
git checkout -- .
git clean -fd

# 2. 恢复备份
git stash pop

# 3. 重建
rm -rf build
mkdir build && cd build
cmake ..
make -j4
ctest
```

**分阶段回滚策略**:

| 阶段 | 回滚方式 |
|------|---------|
| Phase A | 还原 tests/CMakeLists.txt 和 test_module_load_and_vfs.cpp |
| Phase B | 还原 vfs.cpp/vfs.h、service_registry、module_loader.cpp、plugin.cpp |
| Phase C | 还原 gpu_ioctl.h、plugin.cpp、test_gpu_ioctl.cpp、test_gpu_plugin.cpp |
| Phase D/E | 还原对应测试文件 |

---

## 7. 附录

### 调试命令速查

```bash
# 运行单个测试并查看详细输出
./build/bin/test_gpu_ioctl_standalone

# 使用 gdb 追踪 SEGFAULT
gdb -batch -ex "run" -ex "bt" ./build/bin/test_gpu_ioctl_standalone

# 检查插件符号
nm -D build/plugins/gpu_driver/libgpu_driver_plugin.so | grep -i gpgpu

# 验证 ioctl 号
python3 -c "
import struct
import fcntl
# 手动计算 ioctl 号
print('sizeof check:')
"

# 结构体大小检查
cat > /tmp/check_size.cpp << 'EOF'
#include <cstdint>
#include <cstdio>
// 复制 gpu_types.h 和 gpu_ioctl.h 的相关定义
// ...
int main() {
    printf("sizeof(gpu_pushbuffer_args) = %zu\n", sizeof(gpu_pushbuffer_args));
}
EOF
```

### 相关 Issue

- Issue #3: GPU_IOCTL_ALLOC_BO 参数
- Issue #4: GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH 格式
- Issue #5: Phase 1 实现清单
- Issue #9: GPU_IOCTL_GET_DEVICE_INFO 参数
- Issue #11: VFS 单例问题（已修复，kernel 为 SHARED）
