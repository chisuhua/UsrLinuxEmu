# AGENTS.md - UsrLinuxEmu 开发指南

## 项目概述

UsrLinuxEmu 是用户态 Linux 内核模拟环境，用于设备驱动开发（特别是 GPGPU 驱动）。无需 root 权限或内核编译即可开发和测试驱动。

## 构建命令

```bash
# 必须从项目根目录运行
cd /workspace/project/UsrLinuxEmu

# 配置和构建
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j4

# 运行所有测试
make test

# 运行特定测试（二进制在 build/bin/）
./bin/test_gpu_ioctl_standalone
```

**注意**: 测试必须从项目根目录运行（不是 build/bin/），因为插件路径是相对路径。

## 关键架构决策

### kernel 库必须是 SHARED

`src/CMakeLists.txt` 中 kernel 必须是 SHARED：
```cmake
add_library(kernel SHARED ...)
```

**原因**: VFS::instance() 等单例使用静态局部变量。当 kernel 是 STATIC 时，可执行文件和插件各有独立的静态变量副本，导致 VFS 单例被割裂（Issue #11）。

### 双 ioctl 系统

| 系统 | 前缀 | 状态 | 位置 |
|------|------|------|------|
| System B | `GPGPU_*` | **已废弃** | `drivers/gpu/ioctl_gpgpu.h` |
| System C | `GPU_IOCTL_*` | **当前使用** | `plugins/gpu_driver/shared/gpu_ioctl.h` |

TaskRunner 应使用 System C 接口。

## 关键路径

| 组件 | 路径 |
|------|------|
| GPU 插件 | `plugins/gpu_driver/plugin.cpp` |
| GPU ioctl 定义 | `plugins/gpu_driver/shared/gpu_ioctl.h` |
| GPU 类型定义 | `plugins/gpu_driver/shared/gpu_types.h` |
| VFS 实现 | `src/kernel/vfs.cpp` |
| Device 类 | `include/kernel/device/device.h` |
| 集成文档 | `docs/07-integration/` |

## GPU 插件使用

```cpp
#include "kernel/vfs.h"
#include "kernel/module_loader.h"
#include "gpu_driver/shared/gpu_ioctl.h"

// 加载插件（从项目根目录的 plugins/ 目录）
ModuleLoader::load_plugins("plugins");

// 打开设备
auto dev = VFS::instance().open("/dev/gpgpu0", O_RDWR);

// 调用 ioctl
dev->fops->ioctl(fd, GPU_IOCTL_GET_DEVICE_INFO, &info);
dev->fops->ioctl(fd, GPU_IOCTL_ALLOC_BO, &args);
```

## 编码风格

- **缩进**: 2 空格（禁止 Tab）
- **类名**: CamelCase (`GpgpuDevice`)
- **函数/变量**: snake_case (`allocate_memory`)
- **成员变量**: snake_case_ 后缀 (`buffer_size_`)
- **宏**: SCREAMING_SNAKE_CASE (`MAX_BUFFER_SIZE`)

## TaskRunner 集成

TaskRunner 作为子模块在 `external/TaskRunner/`。UsrLinuxEmu 定义 canonical 接口，TaskRunner 通过符号链接访问：

```
TaskRunner/UsrLinuxEmu → ../../UsrLinuxEmu/
```

GPU 插件接口同步确认：
- Issue #3: GPU_IOCTL_ALLOC_BO 参数
- Issue #4: GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH 格式
- Issue #5: Phase 1 实现清单
- Issue #9: GPU_IOCTL_GET_DEVICE_INFO 参数
- Issue #11: VFS 单例问题（已修复）

## 常见问题

### "Device not found" after plugin loads

检查是否从正确目录运行。插件加载路径是相对于当前工作目录的 `plugins/`。

### ioctl 返回 -EFAULT

确保结构体已正确初始化，包含所有必要成员。

### 编译找不到头文件

`include_directories` 在 CMakeLists.txt 中配置。项目根目录已自动添加。

<!-- code-review-graph MCP tools -->
## MCP Tools: code-review-graph

**IMPORTANT: This project has a knowledge graph. ALWAYS use the
code-review-graph MCP tools BEFORE using Grep/Glob/Read to explore
the codebase.** The graph is faster, cheaper (fewer tokens), and gives
you structural context (callers, dependents, test coverage) that file
scanning cannot.

### When to use graph tools FIRST

- **Exploring code**: `semantic_search_nodes` or `query_graph` instead of Grep
- **Understanding impact**: `get_impact_radius` instead of manually tracing imports
- **Code review**: `detect_changes` + `get_review_context` instead of reading entire files
- **Finding relationships**: `query_graph` with callers_of/callees_of/imports_of/tests_for
- **Architecture questions**: `get_architecture_overview` + `list_communities`

Fall back to Grep/Glob/Read **only** when the graph doesn't cover what you need.

### Key Tools

| Tool | Use when |
| ------ | ---------- |
| `detect_changes` | Reviewing code changes — gives risk-scored analysis |
| `get_review_context` | Need source snippets for review — token-efficient |
| `get_impact_radius` | Understanding blast radius of a change |
| `get_affected_flows` | Finding which execution paths are impacted |
| `query_graph` | Tracing callers, callees, imports, tests, dependencies |
| `semantic_search_nodes` | Finding functions/classes by name or keyword |
| `get_architecture_overview` | Understanding high-level codebase structure |
| `refactor_tool` | Planning renames, finding dead code |

### Workflow

1. The graph auto-updates on file changes (via hooks).
2. Use `detect_changes` for code review.
3. Use `get_affected_flows` to understand impact.
4. Use `query_graph` pattern="tests_for" to check coverage.
