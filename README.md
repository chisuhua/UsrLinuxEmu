# UsrLinuxEmu - 用户态 Linux 内核模拟环境

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)]()
[![License](https://img.shields.io/badge/license-MIT-blue)]()
[![Version](https://img.shields.io/badge/version-v0.5%2B-blueviolet)]()
[![Phase](https://img.shields.io/badge/phase-2%20complete-success)]()
[![IOCTL](https://img.shields.io/badge/ioctl-System%20C-blue)]()
[![Tests](https://img.shields.io/badge/tests-Catch2-orange)]()

> **最后验证**: 2026-06-16 (commit `374d463`)
>
> **权威架构文档**: [AGENTS.md](AGENTS.md) + [docs/02_architecture/post-refactor-architecture.md](docs/02_architecture/post-refactor-architecture.md)
>
> 本 README 反映 Phase 2 完成后的状态。如发现与上述两文件冲突，以它们为准。

## 项目简介

UsrLinuxEmu 是一个**用户态 Linux 内核模拟环境**，专为设备驱动开发和测试而设计。它允许开发者在**无需 root 权限**、**无需内核编译**的情况下，开发、测试和调试设备驱动程序，特别是支持 GPGPU 等复杂设备的完整模拟。

项目目标是让驱动开发者能在笔记本上完整跑通 GPU 驱动栈，验证后再无痛迁移到真实硬件。TaskRunner（外部子模块）通过共享的 IOCTL 接口（System C）即可在模拟器与真机驱动之间零改动切换。

### 核心特性

- 🚀 **用户态运行** - 无需 root 权限或内核模块
- 🔌 **插件化架构** - 通过 `dlopen` + `dlsym("mod")` 动态加载设备插件
- 🎮 **GPU 支持** - 完整 GPGPU 驱动 + 仿真器 + VA Space + Queue + Ring Buffer
- 🏗️ **驱动/仿真分离** - 清晰分层：`drv/` + `hal/` + `sim/` + `shared/`
- 🔧 **Linux 兼容层** - `include/linux_compat/` 提供内核 API 的用户态实现
- 📊 **统一日志 + 配置 + 服务注册** - 便于调试与扩展
- 🧪 **Catch2 测试栈** - 30+ 独立测试覆盖 IOCTL / VA Space / Queue / Plugin 加载

### 支持的设备

- ✅ GPGPU 设备（VA Space、Queue、Pushbuffer、Doorbell、Fence、模拟执行）
- ✅ 内存设备（示例插件）
- ✅ 串口设备（示例插件）
- ✅ PCIe 设备（基础支持）
- 🔜 网络设备（规划中）
- 🔜 存储设备（规划中）

## 快速开始

### 系统要求

- Linux 环境（推荐 Ubuntu 20.04+）
- CMake ≥ 3.14
- GCC/Clang 支持 C++17
- 无需 root 权限

### 构建项目

```bash
# 克隆仓库
git clone <repository-url>
cd UsrLinuxEmu

# 初始化子模块（TaskRunner 集成用）
git submodule update --init --recursive

# 手动构建（推荐）
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
```

或者使用构建脚本：

```bash
./build.sh                 # 构建所有目标
./build.sh test            # 构建 + 运行测试
./build.sh clean           # 清理 build/ 目录
```

> ⚠️ **kernel 库必须为 SHARED**（Issue #11）。`src/CMakeLists.txt` 中 `add_library(kernel SHARED ...)` 不可改为 STATIC，否则 VFS 单例会被可执行文件与插件割裂。

### 运行测试

**重要**: 测试必须从项目根目录运行，插件路径是相对路径。

```bash
# 从项目根目录
cd /workspace/project/UsrLinuxEmu

# 运行所有测试（CTest）
cd build && ctest --output-on-failure && cd ..

# 或单独跑某个测试（典型 GPU 链路测试）
./build/bin/test_gpu_ioctl_standalone
./build/bin/test_va_space_standalone
./build/bin/test_gpu_ringbuffer_standalone
./build/bin/test_hardware_puller_emu_standalone
./build/bin/test_module_load_and_vfs_standalone
```

### 运行 CLI

```bash
# 二进制路径：build/bin/cli（不是 build/tools/cli/cli）
./build/bin/cli
```

CLI 用于交互式加载/卸载插件、查看已注册设备。详细子命令可用 `./build/bin/cli --help`。

## 项目结构

```
UsrLinuxEmu/
├── AGENTS.md                  # 权威开发指南（架构要点 + 构建 + 编码风格）
├── README.md                  # 本文件
├── CMakeLists.txt             # 顶层 CMake（project: user_kernel_emu）
├── build.sh, run_cli.sh       # 构建/运行脚本
│
├── src/                       # 内核框架实现（kernel SHARED 库）
│   └── kernel/                # VFS / ModuleLoader / Logger / WaitQueue ...
│
├── include/
│   ├── kernel/                # 框架头文件：vfs.h, device/, pcie/, file_ops.h ...
│   └── linux_compat/          # Linux 内核 API 用户态兼容层（u8/u32, ERR_PTR, _IOR, drm/）
│
├── drivers/                   # 示例设备插件源码
│   ├── sample_memory/         # 内存设备示例
│   └── sample_serial/         # 串口设备示例
│
├── plugins/                   # 动态加载的设备插件
│   ├── gpu_driver/            # GPU 驱动插件（核心）
│   │   ├── drv/               # GpgpuDevice（ioctl 派发表）+ DRM 驱动
│   │   ├── hal/               # struct gpu_hal_ops（11 个函数指针）+ hal_user / hal_mock
│   │   ├── sim/               # 硬件仿真：scheduler/、hardware/、gpu_queue_emu ...
│   │   ├── shared/            # 公共头：gpu_ioctl.h, gpu_types.h, gpu_queue.h ...
│   │   └── plugin.cpp         # 导出 `module mod` 符号
│   ├── plugin_gpu_driver.so   # 构建产物
│   └── plugins.json           # 插件清单
│
├── libgpu_core/               # 纯 C 库：buddy allocator（ADR-020 提取）
│
├── tests/                     # Catch2 测试（30+ 个 standalone 二进制）
│
├── tools/cli/                 # CLI 工具源码（构建产物 → build/bin/cli）
│
├── simulator/                 # 旧仿真代码已清空（迁移到 plugins/gpu_driver/sim/）
│
├── archive/                   # 历史代码归档
│   ├── system_b_drivers/gpu/  # 旧 GPGPU_*（System B）驱动
│   ├── system_b_examples/     # 旧 sample_gpu 插件
│   ├── orphaned_simulator/gpu/
│   ├── old_gpu_device/        # 旧 GpuDevice 基类
│   └── historical-plans-2026-06-15/
│
├── external/TaskRunner/       # 子模块：用户态驱动验证框架
│
├── docs/                      # 项目文档
│   ├── 02_architecture/
│   │   └── post-refactor-architecture.md   # 重构后架构 SSOT
│   ├── 00_adr/                # 架构决策记录
│   ├── 01-quickstart/         # 快速上手
│   ├── 03-development/        # 开发指南
│   ├── 04-building/           # 构建与测试
│   ├── 05-advanced/           # 高级主题
│   ├── 06-reference/          # API / IOCTL 参考
│   └── 07-integration/        # TaskRunner 集成
│
└── zpoline/                   # 实验性子项目（不在 CMake 中）
```

> 详细的目录演进与归档清单参见 [docs/02_architecture/post-refactor-architecture.md §1.5](docs/02_architecture/post-refactor-architecture.md)。

## 架构概览

UsrLinuxEmu 采用**四层架构**，驱动代码与仿真代码物理分离（Phase 1.5）：

```
┌─────────────────────────────────────────────┐
│        用户应用层 (User Space Apps)          │
│   tests/, external/TaskRunner, 用户驱动    │
└─────────────────────────────────────────────┘
                  ↓ ioctl(fd, GPU_IOCTL_*, ...)
┌─────────────────────────────────────────────┐
│       内核模拟框架层 (Kernel Framework)      │
│   src/kernel/ + include/kernel/ (SHARED)   │
│   VFS (Meyers singleton) | ModuleLoader    │
│   ServiceRegistry | Logger | WaitQueue     │
│   include/linux_compat/                     │
└─────────────────────────────────────────────┘
                  ↓ dlopen("plugins/*.so")
┌─────────────────────────────────────────────┐
│          设备驱动层 (Device Driver)          │
│   plugins/gpu_driver/                       │
│   • drv/    : GpgpuDevice（ioctl 派发表）   │
│   • hal/    : struct gpu_hal_ops + impl     │
│   • shared/ : gpu_ioctl.h 等公共头          │
└─────────────────────────────────────────────┘
                  ↓ HAL ops 调用
┌─────────────────────────────────────────────┐
│        硬件仿真层 (Hardware Simulation)     │
│   plugins/gpu_driver/sim/                   │
│   • sim/scheduler/  : GlobalScheduler       │
│   • sim/hardware/   : HardwarePullerEmu FSM │
│   • gpu_queue_emu   : Ring Buffer 消费者    │
│   libgpu_core/       : 纯 C buddy allocator │
└─────────────────────────────────────────────┘
```

### IOCTL 体系（System C）

| 系统 | 前缀 | 状态 |
|------|------|------|
| System A | `CUDA_*` | ❌ 已删除 |
| System B | `GPGPU_*` | ⚠️ 已归档到 `archive/system_b_drivers/gpu/` |
| **System C** | **`GPU_IOCTL_*`** | ✅ **当前使用**（TaskRunner 与 UsrLinuxEmu 共享头文件） |

System C 完整编号表与结构体定义见 [plugins/gpu_driver/shared/gpu_ioctl.h](plugins/gpu_driver/shared/gpu_ioctl.h) 或 [post-refactor-architecture.md 附录 A](docs/02_architecture/post-refactor-architecture.md)。

### GPU 内存分配流程（Phase 2）

```
用户程序
    ↓ VFS::instance().open("/dev/gpgpu0", O_RDWR)
VFS 查找设备
    ↓ dev->fops->ioctl(fd, GPU_IOCTL_ALLOC_BO, &args)
GpgpuDevice::ioctl 派发表
    ↓ handle_alloc_bo(args)
HAL.fence_create(&fence_id)        ← 异步跟踪
    ↓
buddy_alloc(size, domain)          ← libgpu_core/gpu_buddy
    ↓
mmap 映射到 GPU VA Space
    ↓
返回 args.handle + args.gpu_va 给用户
```

### GPU Pushbuffer 提交流程（Phase 2 完整版）

```
User ioctl(fd, GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, args)
    ↓
GpgpuDevice::ioctl → table dispatch
    ↓
handlePushbufferSubmitBatch(args)
    ├─→ validate VA Space exists
    ├─→ validate Queue belongs to VA Space
    ├─→ HAL.fence_create(&fence_id)
    ├─→ HardwarePullerEmu::submitBatch()  (FSM: IDLE→FETCH→DECODE→...)
    ├─→ GlobalScheduler::enqueue(entry, selectEngine(entry))
    │     └─→ GpfifoToLaunchParamsTranslator
    └─→ HAL.doorbell_ring(stream_id)
    ↓
返回 args.fence_id
```

## 示例代码

### 加载插件并打开设备

```cpp
#include "kernel/vfs.h"
#include "kernel/module_loader.h"
#include "gpu_driver/shared/gpu_ioctl.h"
#include <fcntl.h>

// 1. 加载 plugins/ 下所有插件（从项目根目录运行）
ModuleLoader::load_plugins("plugins");

// 2. 通过 VFS 单例打开设备（Meyers singleton，Issue #11 修复后）
auto dev = VFS::instance().open("/dev/gpgpu0", O_RDWR);

// 3. 查询设备信息
gpu_device_info info{};
dev->fops->ioctl(dev->fd, GPU_IOCTL_GET_DEVICE_INFO, &info);
printf("GPU: %s, VRAM=%llu MB\n", info.marketing_name,
       (unsigned long long)(info.vram_size / (1024 * 1024)));
```

### GPU 内存分配（BO）

```cpp
#include "gpu_driver/shared/gpu_types.h"

gpu_alloc_bo_args args{};
args.size  = 1024 * 1024;            // 1 MB
args.domain = GPU_MEM_DOMAIN_VRAM;   // 0x1: VRAM, 0x2: GTT, 0x4: CPU
args.flags  = GPU_BO_HOST_VISIBLE;   // 0x2

int ret = dev->fops->ioctl(dev->fd, GPU_IOCTL_ALLOC_BO, &args);
// ret == 0 时 args.handle / args.gpu_va 已填充
if (ret == 0) {
    // 通过 GPU_VA 访问，mmap 由 GPU_IOCTL_MAP_BO 完成
    gpu_map_bo_args map_args{};
    map_args.handle = args.handle;
    dev->fops->ioctl(dev->fd, GPU_IOCTL_MAP_BO, &map_args);
}

// 用完释放
dev->fops->ioctl(dev->fd, GPU_IOCTL_FREE_BO, &args.handle);
```

### 创建 VA Space 与 Queue

```cpp
// 1. 创建 VA Space
gpu_va_space_args va_args{};
va_args.page_size = 0;   // 0=4KB, 1=64KB
dev->fops->ioctl(dev->fd, GPU_IOCTL_CREATE_VA_SPACE, &va_args);
gpu_va_space_handle_t va_handle = va_args.va_space_handle;

// 2. 在 VA Space 内创建 Queue
gpu_queue_args q_args{};
q_args.va_space_handle   = va_handle;
q_args.queue_type        = 0;                       // 0=COMPUTE, 1=COPY, 2=GRAPHICS
q_args.priority          = 0;
q_args.ring_buffer_size  = 1024 * sizeof(gpu_gpfifo_entry);
dev->fops->ioctl(dev->fd, GPU_IOCTL_CREATE_QUEUE, &q_args);
gpu_queue_handle_t q_handle = q_args.queue_handle;

// 3. 提交 Pushbuffer Batch
gpu_pushbuffer_args pb{};
pb.stream_id    = 0;
pb.entries_addr = /* gpfifo entries 用户态地址 */;
pb.count        = 16;
dev->fops->ioctl(dev->fd, GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, &pb);
// pb.fence_id 可用于 GPU_IOCTL_WAIT_FENCE 同步
```

完整可编译示例参见 [tests/test_gpu_ioctl.cpp](tests/test_gpu_ioctl.cpp) 与 [tests/test_va_space.cpp](tests/test_va_space.cpp)。

## 开发状态

当前阶段：**post-Phase 2**（2026-05-13 重构窗口完成）。详细时间轴见 [post-refactor-architecture.md §1.1](docs/02_architecture/post-refactor-architecture.md)。

### Phase 2 已完成（2026-05-13）

- ✅ Ring Buffer + `GpuQueueEmu` 多队列 fetch
- ✅ Doorbell 修复 + 队列 ioctl 接线
- ✅ `LAUNCH_CB` 已删除
- ✅ GlobalScheduler 回调链 + `fence_id` 异步跟踪
- ✅ **VA Space 抽象**（`GPU_IOCTL_CREATE_VA_SPACE`）
- ✅ ADR-024 用户态队列提交

### Phase 1.5 已完成（2026-05 上）

- ✅ 驱动/仿真代码分离（`drv/hal/sim/shared/`）
- ✅ HAL 接口契约（`struct gpu_hal_ops`，11 个函数指针）
- ✅ Hardware Puller 状态机
- ✅ `libgpu_core/` 提取（纯 C buddy allocator）
- ✅ namespace wrap（`usr_linux_emu::`）

### Phase 1 已完成（2026-04）

- ✅ System C 引入（`GPU_IOCTL_*` 替代 `GPGPU_*`）
- ✅ TaskRunner 共享头文件（符号链接）

### 后续计划

- Phase 3：网络设备 / 存储设备插件
- Phase 4：稳定的 v1.0 发布
- 详见 [docs/02_architecture/post-refactor-architecture.md](docs/02_architecture/post-refactor-architecture.md) 与 `docs/ROADMAP.md`

## 文档

完整文档集请参阅 [docs/](docs/) 目录。

| 类别 | 入口 | 说明 |
|------|------|------|
| 快速开始 | [docs/01-quickstart/](docs/01-quickstart/) | 安装、构建、第一个示例 |
| 架构（SSOT） | [docs/02_architecture/post-refactor-architecture.md](docs/02_architecture/post-refactor-architecture.md) | 重构后权威架构说明 |
| 架构决策 | [docs/00_adr/](docs/00_adr/) | ADR 列表（001~024）|
| 开发指南 | [docs/03-development/](docs/03-development/) | 编码规范、添加设备 |
| 构建与测试 | [docs/04-building/](docs/04-building/) | CMake、Catch2、CI |
| 高级主题 | [docs/05-advanced/](docs/05-advanced/) | GPU 驱动架构、插件开发 |
| API 参考 | [docs/06-reference/](docs/06-reference/) | API 与 IOCTL |
| TaskRunner 集成 | [docs/07-integration/](docs/07-integration/) | 与 TaskRunner 子模块的对接 |

> ⚠️ docs/ 目录中部分子文档（`docs/02_architecture/architecture.md` 等）仍处于审计修复中。**架构与构建相关信息以本 README + AGENTS.md + post-refactor-architecture.md 为准**。

## 贡献指南

我们欢迎各种形式的贡献。

### 贡献方式

- 🐛 报告 bug（[GitHub Issues](https://github.com/chisuhua/UsrLinuxEmu/issues)）
- 💡 提出新功能建议
- 📝 改进文档
- 🔧 提交代码修复
- ✨ 开发新设备插件

### 开始贡献

1. Fork 本仓库
2. 创建功能分支（`git checkout -b feature/amazing-feature`）
3. 阅读 [AGENTS.md](AGENTS.md) 中的编码风格（snake_case + 尾下划线成员变量）
4. 编写测试（`tests/` 下 Catch2 风格；新功能需 ≥ 80% 覆盖）
5. 提交更改（遵循 Conventional Commits：`feat(gpu): ...`）
6. 推送到分支
7. 创建 Pull Request

详细流程见 [CONTRIBUTING.md](CONTRIBUTING.md)。

## 常见问题

### Q: 为什么需要用户态内核模拟？
A: 用户态模拟允许在不需要 root 权限、不需要内核编译的情况下开发和测试驱动程序，**大大降低了开发门槛和迭代风险**。驱动写完后，TaskRunner 通过共享 IOCTL 接口可以零改动切到真机驱动。

### Q: 性能如何？
A: 用户态模拟有性能开销（系统调用走兼容层，无真实 MMU/DMA），但对开发和回归测试场景足够。硬件仿真层（`plugins/gpu_driver/sim/`）的优化持续进行中。

### Q: 支持哪些 Linux API？
A: 通过 `include/linux_compat/` 提供基础兼容：`u8/u32/u64` 类型、`ERR_PTR`/`PTR_ERR` 宏、`_IOR/_IOW/_IOWR` ioctl 编码、`GFP_*` 分配标志、DRM 子集（`drm_ioctl.h` / `drm_gem.h` / `drm_driver.h`）。目标是覆盖 80% 常用 API。

### Q: 跑测试时提示 "Device not found" 怎么办？
A: 测试必须从**项目根目录**运行（`./build/bin/test_*`），因为 `ModuleLoader::load_plugins("plugins")` 使用相对路径。

### Q: ioctl 返回 `-EFAULT` 怎么办？
A: 检查传入的结构体是否完整初始化（所有必要成员赋值）。System C 的结构体定义在 `plugins/gpu_driver/shared/gpu_ioctl.h` 与 `gpu_queue.h`。

### Q: 可以运行真实的 CUDA 程序吗？
A: 尚不能完整支持 CUDA runtime，但 TaskRunner 已经可以基于 System C 接口提交 GPFIFO entries 并被模拟器消费。简单的 kernel 调度链路已经跑通。

### Q: `kernel` 库为什么必须是 SHARED？
A: `VFS::instance()` 等单例使用 Meyers 单例（函数内 `static` 局部变量）。如果 `kernel` 是 STATIC 库，可执行文件和插件各自会有独立的单例副本，导致 VFS 状态割裂（Issue #11）。**不要改 `src/CMakeLists.txt` 里的 `add_library(kernel SHARED ...)`**。

更多问题请查看 [docs/](docs/) 或提交 [Issue](https://github.com/chisuhua/UsrLinuxEmu/issues)。

## 许可证

本项目采用 MIT 许可证。详见 [LICENSE](LICENSE) 文件。

## 致谢

感谢所有为本项目做出贡献的开发者！

特别致谢 TaskRunner 子模块提供的用户态驱动验证框架，以及所有 IOCTL 接口（System C）的共同维护者。

## 联系方式

- 📧 Email: [项目邮箱]
- 🐛 Issues: [GitHub Issues](https://github.com/chisuhua/UsrLinuxEmu/issues)
- 💬 Discussions: [GitHub Discussions](https://github.com/chisuhua/UsrLinuxEmu/discussions)

---

**当前版本**: v0.5+（post-Phase 2）  
**最后验证**: 2026-06-16  
**对应 commit**: `374d463`  
**维护者**: UsrLinuxEmu Team
