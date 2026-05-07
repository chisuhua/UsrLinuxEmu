# 代码质量改进计划

**计划 ID**: `code-quality-improvement`  
**优先级**: HIGH EFFORT（测试修复完成后执行）  
**预计工期**: 2-3 周  
**依赖**: `fix-test-infra.md`（测试全部通过后开始）  
**创建日期**: 2026-05-07  
**最后更新**: 2026-05-07

---

## 1. 概述

本计划系统性地提升 UsrLinuxEmu 代码库的质量和可维护性。当前代码存在以下主要问题：

1. **缺乏代码格式化配置** — 无 `.clang-format`、`.clang-tidy`，缩进混用 2 空格和 4 空格
2. **命名不一致** — `camelCase` 与 `snake_case` 混用，类名风格不统一
3. **命名空间缺失** — kernel 层全部在全局命名空间，存在符号污染风险
4. **单体文件** — `plugin.cpp` 635 行包含 BuddyAllocator、HandleManager、GpgpuDevice 三个类
5. **孤儿文件** — `src/kernel/device/cuda_compat_ioctl.cpp` 不在 CMakeLists.txt 中
6. **双重注册** — VFS::register_device() 同时注册到 ServiceRegistry，职责不清
7. **缺失功能** — `GPU_OP_LAUNCH_CPU_TASK` 在 handle_pushbuffer_submit_batch 中被忽略

### 预期产出

- 统一的代码风格（2 空格缩进、snake_case 函数/变量、CamelCase 类）
- kernel 层纳入 `usr_linux_emu` 命名空间
- `plugin.cpp` 拆分为 4 个独立文件（BuddyAllocator、HandleManager、GpgpuDevice、plugin 入口）
- 零孤儿文件、零废弃代码引用
- `.clang-tidy` 静态分析通过（无 warning）

### 与外部计划的关联

| 外部计划 | 关联关系 |
|---------|---------|
| `fix-test-infra.md` | **前置依赖** — Phase B~E 必须在测试全绿后开始（格式化变更无法验证）；Phase A（配置文件创建）可提前并行执行，不依赖测试通过 |
| `phase1_implementation_plan.md` | P1.1a 的物理拆分依赖本计划的 Phase D（plugin.cpp 拆分） |
| `gpu_driver_portability_plan.md` | 里程碑 3.0 要求代码通过 `test_portability.sh` 检查 |

---

## 2. 前置条件

在开始本计划前，确保：

```bash
# 1. 自动验证 fix-test-infra 已完成（测试全部通过）
cd /workspace/project/UsrLinuxEmu
ctest --test-dir build --output-on-failure 2>&1 | tail -5
# 必须输出：100% tests passed, 19/19
# 如果未通过，立即终止：
if ! ctest --test-dir build 2>&1 | grep -q "100% tests passed"; then
    echo "ERROR: fix-test-infra 未完成，测试未全部通过。请先执行 fix-test-infra 计划。"
    exit 1
fi
echo "✅ fix-test-infra precondition verified: 19/19 tests pass"

# 2. Git 工作区干净
git status  # 应无未提交的修改
git stash push -m "auto-backup-code-quality-$(date +%s)"

# 3. 安装 clang-format 和 clang-tidy
clang-format --version  # 期望 >= 14
clang-tidy --version    # 期望 >= 14
```

---

## 3. 实施阶段

### Phase A: 添加工具链配置

**目标**: 创建 `.clang-format`、`.clang-tidy`、`.editorconfig`，统一代码风格。

**风险级别**: 低

#### 步骤 A.1: 创建 .clang-format

**文件**: `/workspace/project/UsrLinuxEmu/.clang-format`

```yaml
---
Language: Cpp
BasedOnStyle: Google
IndentWidth: 2
TabWidth: 2
UseTab: Never
ColumnLimit: 100
BreakBeforeBraces: Attach
AllowShortFunctionsOnASingleLine: Empty
AllowShortLambdasOnASingleLine: Empty
AllowShortIfStatementsOnASingleLine: Never
AllowShortLoopsOnASingleLine: false
PointerAlignment: Left
ReferenceAlignment: Left
DerivePointerAlignment: false
SortIncludes: CaseSensitive
IncludeBlocks: Preserve
SpaceAfterCStyleCast: false
SpaceAfterTemplateKeyword: true
SpaceBeforeAssignmentOperators: true
SpaceBeforeParens: ControlStatements
SpacesInParentheses: false
SpacesInAngles: false
SpacesInCStyleCastParentheses: false
# 命名风格（clang-format 14+）
# 注意：命名风格规则需要 clang-format 14+，如果版本较低则省略以下配置
---
```

> **注意**: 若 clang-format 版本 < 14，命名风格配置不可用。此时依赖 `.clang-tidy` 的 `readability-identifier-naming` 检查 + 人工审查。

#### 步骤 A.2: 创建 .clang-tidy

**文件**: `/workspace/project/UsrLinuxEmu/.clang-tidy`

```yaml
---
Checks: >
  bugprone-*,
  cppcoreguidelines-*,
  -cppcoreguidelines-avoid-magic-numbers,
  -cppcoreguidelines-owning-memory,
  -cppcoreguidelines-pro-bounds-array-to-pointer-decay,
  -cppcoreguidelines-pro-type-reinterpret-cast,
  -cppcoreguidelines-avoid-non-const-global-variables,
  google-*,
  -google-runtime-references,
  -google-readability-todo,
  misc-*,
  -misc-non-private-member-variables-in-classes,
  modernize-*,
  -modernize-use-trailing-return-type,
  -modernize-avoid-c-arrays,
  performance-*,
  portability-*,
  readability-*,
  -readability-named-parameter,
  -readability-braces-around-statements,
  -readability-else-after-return,
  -readability-magic-numbers,
  -readability-uppercase-literal-suffix,
  -readability-function-cognitive-complexity,
  -cppcoreguidelines-special-member-functions,
  -cppcoreguidelines-avoid-c-arrays

CheckOptions:
  - key:   readability-identifier-naming.ClassCase
    value: CamelCase
  - key:   readability-identifier-naming.StructCase
    value: CamelCase
  - key:   readability-identifier-naming.FunctionCase
    value: lower_case
  - key:   readability-identifier-naming.VariableCase
    value: lower_case
  - key:   readability-identifier-naming.MemberCase
    value: lower_case
  - key:   readability-identifier-naming.MemberSuffix
    value: _
  - key:   readability-identifier-naming.MacroDefinitionCase
    value: UPPER_CASE
  - key:   readability-identifier-naming.EnumCase
    value: CamelCase
  - key:   readability-identifier-naming.EnumConstantCase
    value: UPPER_CASE
  - key:   readability-identifier-naming.NamespaceCase
    value: lower_case
  - key:   readability-identifier-naming.TemplateParameterCase
    value: CamelCase
  - key:   readability-identifier-naming.TemplateParameterSuffix
    value: _T
  - key:   cppcoreguidelines-special-member-functions.AllowSoleDefaultDtor
    value: '1'

WarningsAsErrors: ''
HeaderFilterRegex: '.*'
FormatStyle: file
---
```

#### 步骤 A.3: 创建 .editorconfig

**文件**: `/workspace/project/UsrLinuxEmu/.editorconfig`

```ini
root = true

[*]
charset = utf-8
end_of_line = lf
insert_final_newline = true
trim_trailing_whitespace = true

[*.{cpp,h,c,cmake}]
indent_style = space
indent_size = 2
max_line_length = 100

[*.md]
trim_trailing_whitespace = false

[CMakeLists.txt]
indent_style = space
indent_size = 2
```

#### 步骤 A.4: 验证配置有效

```bash
cd /workspace/project/UsrLinuxEmu
# 检查 clang-format 是否能解析配置
clang-format --dump-config > /dev/null

# 检查 clang-tidy 是否能解析配置
clang-tidy -p build/ --list-checks 2>/dev/null | head -5
```

---

### Phase B: 格式化所有源文件

**目标**: 运行 clang-format 统一代码风格，消除缩进混用。

**风险级别**: 低（纯格式化，无逻辑变更）

#### 步骤 B.1: 按目录批次格式化

**策略**: 按目录分批执行，每批后编译验证，避免一次性大规模变更导致问题难以定位。

```bash
cd /workspace/project/UsrLinuxEmu

# 批次 1: include/ 头文件（最上层接口，变更影响最大）
find include/ -name "*.h" -o -name "*.hpp" | xargs clang-format -i
make -C build -j4
ctest --output-on-failure
# 全部通过 → 提交此批次

# 批次 2: src/kernel/ 实现
find src/kernel/ -name "*.cpp" | xargs clang-format -i
make -C build -j4
ctest --output-on-failure

# 批次 3: drivers/
find drivers/ -name "*.cpp" -o -name "*.h" | xargs clang-format -i
make -C build -j4
ctest --output-on-failure

# 批次 4: plugins/
find plugins/ -name "*.cpp" -o -name "*.h" | xargs clang-format -i
make -C build -j4
ctest --output-on-failure

# 批次 5: tests/
find tests/ -name "*.cpp" -o -name "*.h" | xargs clang-format -i
make -C build -j4
ctest --output-on-failure

# 批次 6: tools/
find tools/ -name "*.cpp" -o -name "*.h" | xargs clang-format -i
make -C build -j4
ctest --output-on-failure

# 批次 7: simulator/ 和 libgpu_core/
find simulator/ libgpu_core/ -name "*.cpp" -o -name "*.h" -o -name "*.c" | xargs clang-format -i
make -C build -j4
ctest --output-on-failure
```

#### 步骤 B.2: 运行 clang-tidy 检查

```bash
# 生成 compile_commands.json（如果尚未生成）
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -B build .

# 对核心文件运行 clang-tidy
cd /workspace/project/UsrLinuxEmu
clang-tidy -p build/ src/kernel/vfs.cpp src/kernel/module_loader.cpp plugins/gpu_driver/plugin.cpp
# 记录所有 warning，分类处理（本阶段不强制修复，仅记录）
```

#### 验证

```bash
# 构建零错误
make -C build -j4
# 测试全通过
ctest --output-on-failure
# Git diff 仅包含空白和格式变更，无逻辑变更
git diff --stat
```

---

### Phase C: 添加命名空间 `usr_linux_emu`

**目标**: 将 kernel 层代码纳入 `usr_linux_emu` 命名空间，消除全局命名空间污染。

**风险级别**: 中（影响所有 include 引用）

#### 步骤 C.1: 添加命名空间到 kernel 头文件

**文件列表**:
- `/workspace/project/UsrLinuxEmu/include/kernel/vfs.h`
- `/workspace/project/UsrLinuxEmu/include/kernel/file_ops.h`
- `/workspace/project/UsrLinuxEmu/include/kernel/device.h`
- `/workspace/project/UsrLinuxEmu/include/kernel/device/*.h`
- `/workspace/project/UsrLinuxEmu/include/kernel/module_loader.h`
- `/workspace/project/UsrLinuxEmu/include/kernel/service_registry.h`
- `/workspace/project/UsrLinuxEmu/include/kernel/pcie_device.h`
- `/workspace/project/UsrLinuxEmu/include/kernel/plugin_manager.h`

**修改模式**:

```cpp
#pragma once
// ... 现有 includes ...

namespace usr_linux_emu {

// ... 现有类定义 ...

} // namespace usr_linux_emu
```

#### 步骤 C.2: 添加命名空间到 kernel 实现文件

**文件列表**:
- `/workspace/project/UsrLinuxEmu/src/kernel/vfs.cpp`
- `/workspace/project/UsrLinuxEmu/src/kernel/module_loader.cpp`
- `/workspace/project/UsrLinuxEmu/src/kernel/service_registry.cpp`
- `/workspace/project/UsrLinuxEmu/src/kernel/plugin_manager.cpp`
- `/workspace/project/UsrLinuxEmu/src/kernel/pcie_device.cpp`

**修改模式**:

```cpp
#include "vfs.h"
// ... 其他 includes ...

namespace usr_linux_emu {

// ... 现有实现 ...

} // namespace usr_linux_emu
```

#### 步骤 C.3: 更新所有引用点

**策略**: 使用 `using namespace usr_linux_emu;` 作为过渡（避免一次性修改所有文件导致冲突）。

在以下位置添加 `using namespace`：

```cpp
// 在插件和驱动文件的顶部（紧接 includes 后）
using namespace usr_linux_emu;
```

**需要添加的文件**:
- `plugins/gpu_driver/plugin.cpp`
- `plugins/sample_memory/plugin.cpp`
- `plugins/sample_serial/plugin.cpp`
- `drivers/gpu/*.cpp`
- `drivers/gpu/*.h`
- `tests/*.cpp`
- `tools/cli/*.cpp`
- `simulator/**/*.cpp`

> **注意**: 后续计划（Phase 3+）中，应将 `using namespace` 替换为显式限定（`usr_linux_emu::VFS`），但在本阶段使用 `using` 过渡以降低变更量。

#### 验证

```bash
make -C build -j4
ctest --output-on-failure
# 零编译错误，测试全通过
```

---

### Phase D: 重构 plugin.cpp（拆分单体文件）

**目标**: 将 635 行的 `plugin.cpp` 拆分为 4 个独立文件，每个类一个文件。

**风险级别**: 高（核心文件重构）

#### 步骤 D.1: 提取 BuddyAllocator

**新建文件**:
- `/workspace/project/UsrLinuxEmu/plugins/gpu_driver/buddy_allocator.h`
- `/workspace/project/UsrLinuxEmu/plugins/gpu_driver/buddy_allocator.cpp`

**buddy_allocator.h**:

```cpp
#pragma once

#include "shared/gpu_types.h"
#include <atomic>
#include <map>
#include <mutex>

namespace usr_linux_emu {

class BuddyAllocator {
 public:
  static constexpr u64 kMinBlockSize = 4 * 1024ULL;
  static constexpr u64 kMaxBlockSize = 8ULL * 1024 * 1024 * 1024;  // 8GB
  static constexpr u32 kMaxOrder = 21;

  BuddyAllocator(u64 base, u64 size);

  u64 allocate(u64 size);
  void free(u64 addr);
  void reset();
  void dump() const;

 private:
  struct Block {
    u64 addr;
    Block* next;
    Block* prev;
  };

  struct AllocInfo {
    u64 addr;
    u64 size;
    u32 order;
  };

  Block* allocate_block(u64 addr, u64 size);
  void insert_free(Block* block, u32 order);
  Block* remove_free(Block* block, u32 order);
  Block* remove_free(u32 order);
  void coalesce(u32 order);
  Block* find_buddy(u64 buddy_addr, u32 order);
  bool are_adjacent(Block* a, Block* b, u64 block_size) const;
  u64 get_buddy_addr(u64 addr, u64 block_size) const;
  static u64 round_up_to_power_of_2(u64 size);
  u32 order_for_size(u64 size) const;

  u64 base_;
  u64 size_;
  u32 max_order_;
  Block* free_lists_[kMaxOrder + 1];
  std::map<u64, AllocInfo> allocated_blocks_;
  mutable std::mutex mutex_;
};

}  // namespace usr_linux_emu
```

**buddy_allocator.cpp**: 将 `plugin.cpp` 中的 `BuddyAllocator` 类实现完整迁移，重命名私有方法为 `snake_case`。

#### 步骤 D.2: 提取 HandleManager

**新建文件**:
- `/workspace/project/UsrLinuxEmu/plugins/gpu_driver/handle_manager.h`
- `/workspace/project/UsrLinuxEmu/plugins/gpu_driver/handle_manager.cpp`

**handle_manager.h**:

```cpp
#pragma once

#include "shared/gpu_types.h"
#include <map>
#include <mutex>

namespace usr_linux_emu {

class HandleManager {
 public:
  u32 allocate();
  bool free(u32 handle);
  bool valid(u32 handle) const;

 private:
  static constexpr u32 kMaxHandles = 65535;
  std::map<u32, bool> handles_;
  mutable std::mutex mutex_;
};

}  // namespace usr_linux_emu
```

#### 步骤 D.3: 提取 GpgpuDevice

**新建文件**:
- `/workspace/project/UsrLinuxEmu/plugins/gpu_driver/gpgpu_device.h`
- `/workspace/project/UsrLinuxEmu/plugins/gpu_driver/gpgpu_device.cpp`

**gpgpu_device.h**:

```cpp
#pragma once

#include "kernel/file_ops.h"
#include "buddy_allocator.h"
#include "handle_manager.h"
#include "shared/gpu_ioctl.h"
#include "shared/gpu_types.h"
#include <atomic>
#include <map>
#include <string>

namespace usr_linux_emu {

struct BoInfo {
  u64 gpu_va;
  u64 size;
  u32 domain;
  u32 flags;
};

struct FenceInfo {
  std::atomic<bool> signaled{false};
};

class GpgpuDevice : public FileOperations {
 public:
  GpgpuDevice();

  long ioctl(int fd, unsigned long request, void* argp) override;
  int open(const char* path, int flags) override;
  int close(int fd) override;

  std::string name = "gpgpu0";

 private:
  long handle_get_device_info(void* argp);
  long handle_alloc_bo(void* argp);
  long handle_free_bo(void* argp);
  long handle_map_bo(void* argp);
  long handle_pushbuffer_submit_batch(void* argp);
  long handle_wait_fence(void* argp);

  BuddyAllocator buddy_;
  HandleManager handles_;
  std::map<u32, BoInfo> bo_map_;
  std::map<u64, FenceInfo> fences_;
  std::atomic<u64> fence_counter_{1};
  std::map<std::string, u32> registered_kernels_;
};

}  // namespace usr_linux_emu
```

#### 步骤 D.4: 精简 plugin.cpp 为薄入口

**修改后文件**: `/workspace/project/UsrLinuxEmu/plugins/gpu_driver/plugin.cpp`

```cpp
/*
 * plugin.cpp - GPU 驱动插件入口（薄入口）
 */
#include "gpgpu_device.h"
#include "kernel/vfs.h"
#include "kernel/module_loader.h"
#include <iostream>
#include <memory>

namespace usr_linux_emu {

static int plugin_init_internal() {
  std::cout << "[GpuPlugin] Initializing...\n";

  auto device = std::make_shared<GpgpuDevice>();

  VFS& vfs = VFS::instance();
  auto dev = std::make_shared<Device>(device->name, 0, device, nullptr);
  vfs.register_device(dev);

  std::cout << "[GpuPlugin] Registered /dev/gpgpu0\n";
  return 0;
}

static void plugin_fini_internal() {
  std::cout << "[GpuPlugin] Shutting down...\n";
  VFS::instance().unregister_device("gpgpu0");
}

}  // namespace usr_linux_emu

extern "C" {

module mod = {
    .name = "gpu_driver",
    .depends = nullptr,
    .init = usr_linux_emu::plugin_init_internal,
    .exit = usr_linux_emu::plugin_fini_internal,
};

}  // extern "C"
```

#### 步骤 D.5: 更新 plugins/CMakeLists.txt

**文件**: `/workspace/project/UsrLinuxEmu/plugins/CMakeLists.txt`

**修改**: 将 `plugin.cpp` 替换为多文件编译：

```cmake
# 假设现有配置类似：
# add_library(gpu_driver_plugin MODULE plugin.cpp)

# 修改为：
add_library(gpu_driver_plugin MODULE
    gpu_driver/plugin.cpp
    gpu_driver/buddy_allocator.cpp
    gpu_driver/handle_manager.cpp
    gpu_driver/gpgpu_device.cpp
)
```

> **注意**: 需要根据实际的 `plugins/CMakeLists.txt` 内容调整。

#### 验证

```bash
make -C build -j4
ctest --output-on-failure
# 构建通过，测试全绿
# ldd build/plugins/gpu_driver/libgpu_driver_plugin.so 无异常依赖
```

---

### Phase E: 清理孤儿文件和冗余代码

**目标**: 移除未使用的文件，修复已知代码缺陷。

**风险级别**: 中

#### 步骤 E.1: 处理 cuda_compat_ioctl.cpp 孤儿文件

**文件**: `/workspace/project/UsrLinuxEmu/src/kernel/device/cuda_compat_ioctl.cpp`

**决策**: 根据 `sync-plan.md` Phase 0 的任务，此文件已废弃。

**操作**:

```bash
# 确认文件不在任何 CMakeLists.txt 中
grep -r "cuda_compat_ioctl" /workspace/project/UsrLinuxEmu/src/CMakeLists.txt /workspace/project/UsrLinuxEmu/CMakeLists.txt
# 预期：无匹配

# 删除文件
git rm /workspace/project/UsrLinuxEmu/src/kernel/device/cuda_compat_ioctl.cpp
```

#### 步骤 E.2: 解决 VFS/ServiceRegistry 双重注册

**文件**:
- `/workspace/project/UsrLinuxEmu/src/kernel/vfs.cpp`
- `/workspace/project/UsrLinuxEmu/include/kernel/vfs.h`

**决策**: VFS 不应负责注册到 ServiceRegistry。ServiceRegistry 的注册应在调用方（如 plugin_init_internal）显式完成，或完全移除双重注册。

**影响评估**（审计结果 2026-05-07）:
```bash
# 检查 ServiceRegistry 的使用情况
grep -r "lookup_service" src/ tests/ drivers/ plugins/
# 结果：仅 service_registry.h/.cpp 自身。无外部调用者。
```
结论：`lookup_service` 无外部调用者，ServiceRegistry 是一个"写后即弃"的 sink。

**决策树**:
```
Q: lookup_service 是否有外部调用者？
├── 否 (审计确认) → ServiceRegistry 可安全移除或保留为空壳
│   ├── 选项 A: 完全删除 ServiceRegistry 类
│   │   ├── 删除 service_registry.h, service_registry.cpp
│   │   ├── 从 src/CMakeLists.txt 中移除 service_registry.cpp
│   │   └── 从 vfs.cpp 中移除 ServiceRegistry::instance() 调用
│   └── 选项 B: 保留 ServiceRegistry 但移除 VFS 中的自动注册
│       ├── 从 vfs.cpp 中移除 register_service() 调用
│       └── ServiceRegistry 保留供将来使用（不删除文件）
└── 是 → 保留 ServiceRegistry，仅从 VFS 移除自动注册调用

推荐：选项 B（保留文件但去耦合，降低风险）
```

**修改**（选项 B 实现）:

```cpp
// vfs.cpp
int VFS::register_device(const std::shared_ptr<Device>& dev) {
    if (devices_.find(dev->name) != devices_.end()) {
        std::cerr << "[VFS] Device already exists: " << dev->name << std::endl;
        return -1;
    }
    devices_[dev->name] = dev;
    std::cout << "[VFS] Registered device: /dev/" << dev->name << std::endl;
    // 移除以下行：ServiceRegistry::instance().register_service(dev->name, dev);
    return 0;
}
```

**验证**:
```bash
make -j4 && ctest --output-on-failure
# 构建通过，测试全绿，ServiceRegistry 不再被 VFS 调用

#### 步骤 E.3: 实现 GPU_OP_LAUNCH_CPU_TASK 处理

**文件**: `/workspace/project/UsrLinuxEmu/plugins/gpu_driver/gpgpu_device.cpp`（拆分后的文件）

**当前问题**: `handle_pushbuffer_submit_batch` 的 switch 中未处理 `GPU_OP_LAUNCH_CPU_TASK`：

```cpp
switch (e.method) {
    case GPU_OP_LAUNCH_KERNEL: ... break;
    case GPU_OP_MEMCPY: ... break;
    case GPU_OP_MEMSET: ... break;
    case GPU_OP_FENCE: ... break;
    // 缺少 GPU_OP_LAUNCH_CPU_TASK
    default: ... break;
}
```

**修改**: 添加处理分支（Phase 1 骨架实现）：

```cpp
case GPU_OP_LAUNCH_CPU_TASK: {
    u64 task_desc_addr = e.payload[0];
    std::cout << "[GpgpuDevice] LAUNCH_CPU_TASK: desc=0x" << std::hex << task_desc_addr << std::dec << "\n";
    // Phase 1: 仅打印日志，Phase 2 实现回调调用
    break;
}
```

#### 步骤 E.4: 清理 build.sh

**文件**: `/workspace/project/UsrLinuxEmu/build.sh`

**检查**: 确认脚本内容是否过时（如硬编码路径、废弃参数）。

```bash
cat /workspace/project/UsrLinuxEmu/build.sh
```

**常见清理项**:
- 删除 `set -e` 后的冗余检查
- 统一使用 `cmake --build` 而非直接 `make`
- 添加 `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`

#### 步骤 E.5: 修复 test_plugin.cpp 的硬编码路径

**文件**: `/workspace/project/UsrLinuxEmu/tests/test_plugin.cpp`

**问题**: 硬编码 `drivers/sample_memory_plugin.so` 路径。

**修改**: 使用 `PROJECT_SOURCE_DIR` 或相对项目根的路径。

#### 验证

```bash
make -C build -j4
ctest --output-on-failure
# 测试全绿
# 确认 cuda_compat_ioctl.cpp 已删除
git status
```

---

## 4. 风险评估

| 风险 | 影响 | 概率 | 缓解措施 |
|------|------|------|---------|
| Phase D 拆分 plugin.cpp 引入符号链接/可见性问题 | 高 | 中 | 每次提取后编译验证；确保 CMakeLists.txt 正确更新 |
| Phase C 命名空间导致外部项目（TaskRunner）编译失败 | 高 | 低 | TaskRunner 通过符号链接访问头文件，头文件中的 `using` 或 `#ifdef` 可控制命名空间暴露；或者保留头文件在全局命名空间 |
| clang-format 变更导致 git blame 信息丢失 | 低 | 高 | 使用 `.git-blame-ignore-revs` 记录格式化 commit，GitHub 支持忽略 |
| 删除 cuda_compat_ioctl.cpp 后发现仍有引用 | 中 | 低 | 删除前全局 grep 确认；如有引用则保留并标记 `[[deprecated]]` |
| ServiceRegistry 移除影响其他插件 | 中 | 中 | 全局搜索 `lookup_service` 和 `register_service` 确认影响范围 |

---

## 5. 成功标准

以下标准**全部满足**时，本计划视为完成：

### 5.1 构建与测试

```bash
cd /workspace/project/UsrLinuxEmu/build
make -j4
ctest --output-on-failure
```

- [ ] 27/27 构建目标成功（无新增编译错误）
- [ ] 19/19 测试通过
- [ ] 零 LSP 错误/警告

### 5.2 代码风格

```bash
cd /workspace/project/UsrLinuxEmu
# 检查所有 .cpp/.h 文件是否已格式化
find include src drivers plugins tests tools simulator libgpu_core -name "*.cpp" -o -name "*.h" -o -name "*.c" | xargs clang-format --dry-run --Werror
```

- [ ] `clang-format --dry-run` 无输出（所有文件已格式化）
- [ ] `clang-tidy` 对 `src/kernel/*.cpp`、`plugins/gpu_driver/*.cpp` 无 warning（或仅有已记录的误报）

### 5.3 架构质量

```bash
grep -r "^class \|^struct " include/kernel/ src/kernel/ | grep -v "namespace usr_linux_emu" | grep -v "extern \"C\""
```

- [ ] kernel 层所有类和函数在 `usr_linux_emu` 命名空间内
- [ ] `plugin.cpp` 行数 < 100 行（薄入口）
- [ ] 无 `cuda_compat_ioctl.cpp` 文件
- [ ] VFS 中无 `ServiceRegistry` 交叉调用
- [ ] `handle_pushbuffer_submit_batch` 包含 `GPU_OP_LAUNCH_CPU_TASK` 分支

---

## 6. 回滚计划

如果实施过程中出现无法解决的问题：

```bash
cd /workspace/project/UsrLinuxEmu

# 1. 放弃所有修改
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
| Phase A | 删除 `.clang-format`、`.clang-tidy`、`.editorconfig` |
| Phase B | `git checkout -- .` 还原所有格式化变更 |
| Phase C | 移除所有 `namespace usr_linux_emu {` 和 `}` 包装，删除 `using namespace` |
| Phase D | 删除新文件，还原 `plugin.cpp`，还原 `plugins/CMakeLists.txt` |
| Phase E | 恢复 `cuda_compat_ioctl.cpp`（如有删除），还原 VFS 双重注册 |

---

## 7. 附录

### 7.1 当前代码质量指标基线

| 指标 | 当前值 | 目标值 |
|------|--------|--------|
| 测试通过率 | 37% (7/19) | 100% (19/19) |
| 单体文件最大行数 | 635 (plugin.cpp) | < 200 |
| 全局命名空间类数量 | ~15 | 0（kernel 层）|
| 孤儿文件数量 | 1 (cuda_compat_ioctl.cpp) | 0 |
| .clang-format | 无 | 有 |
| .clang-tidy | 无 | 有 |
| 命名风格一致性 | 混合 | 统一 |

### 7.2 命名规范速查表

| 类型 | 规范 | 示例 |
|------|------|------|
| 类/结构体 | CamelCase | `BuddyAllocator`, `GpgpuDevice` |
| 函数 | snake_case | `allocate_memory()`, `handle_alloc_bo()` |
| 变量 | snake_case | `buffer_size`, `gpu_va` |
| 成员变量 | snake_case_ | `buffer_size_`, `fence_counter_` |
| 宏常量 | SCREAMING_SNAKE_CASE | `MAX_BUFFER_SIZE`, `GPU_IOCTL_BASE` |
| 模板参数 | CamelCase_T | `Allocator_T` |
| 命名空间 | snake_case | `usr_linux_emu` |

### 7.3 相关文档

- `docs/03-development/coding-style.md` — 项目编码规范
- `AGENTS.md` — Agent 开发指南（含 LSP 规则）
- `plans/phase1_implementation_plan.md` — P1.1a 物理拆分依赖本计划 Phase D
