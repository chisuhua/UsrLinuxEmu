# UsrLinuxEmu 项目概述

> ⚠️ **DEPRECATED**: 此文档最后验证于 2026-06-16 (commit `374d463`)，pre-v0.1.5 内容，**已显著 outdated**。
> **请使用 SSOT**: [`post-refactor-architecture.md`](post-refactor-architecture.md)（持续更新至 v0.1.7+，2026-06-23 H-4 governance cleanup 同步）。
> **迁移指南**: 见 H-4 [ADR-035](../../00_adr/adr-035-governance-policy.md) §Rule 4.3 — pre-v0.1.5 文档 deprecated 头标规则。

> **最后验证**: 2026-06-16 (commit `374d463`)
>
> **本文档角色**: 一页式项目介绍。详细架构看 [post-refactor-architecture.md](post-refactor-architecture.md)（SSOT），开发命令看 [AGENTS.md](../../AGENTS.md)，项目说明看 [README.md](../../README.md)。

---

## 一句话定义

UsrLinuxEmu 是**用户态 Linux 内核模拟环境**，让驱动开发者在**不需要 root 权限、不需要内核编译、不需要真实硬件**的情况下开发、测试和调试设备驱动（特别是 GPGPU 驱动），并通过 TaskRunner 子模块在模拟器与真机驱动之间**零改动切换**。

---

## 项目目标

为驱动开发者提供一条**"模拟器先跑通 → 真机再验证"**的默认工作流：

1. 在普通用户身份下开发完整驱动逻辑（ioctl handler、内存管理、命令提交、调度）
2. 用真实 GPU 工作流测试（GPFIFO 提交、VA Space、Queue、fence 同步）
3. 把通过验证的代码零改动部署到真实内核驱动（TaskRunner 共享 IOCTL 头文件）
4. Phase 2 起，模拟器支持**用户态队列提交**（mmap doorbell + 共享 ring buffer），对齐真实硬件行为

---

## 关键特性

- 🚀 **用户态运行** —— 不需要 root，不编译内核模块。驱动 bug 不会让系统 panic。
- 🔌 **插件化架构** —— 设备作为 `.so` 通过 `dlopen` + `dlsym("mod")` 动态加载。`ModuleLoader::load_plugins("plugins")` 一行启动。
- 🎮 **完整 GPU 支持** —— GPGPU 驱动 + 硬件仿真 + **VA Space** + **Queue** + **Ring Buffer** + Doorbell + Fence + Hardware Puller 状态机 + GlobalScheduler。
- 🏗️ **驱动 / 仿真分离** —— `drv/`（可移植到真实内核）、`hal/`（接口契约）、`sim/`（仅用户态）、`shared/`（与 TaskRunner 共享的 ABI）。
- 🔧 **Linux 兼容层** —— `include/linux_compat/` 提供 `u8/u32/u64`、`_IOR/_IOW/_IOWR`、`ERR_PTR`、DRM 子集等用户态实现。
- 🧪 **Catch2 测试栈** —— `tests/catch_amalgamated.{hpp,cpp}` vendored 单文件，30+ 个独立测试二进制覆盖 IOCTL / VA Space / Queue / 插件加载。
- 📊 **统一日志 / 配置 / 服务注册** —— 框架级组件（`Logger`、`ServiceRegistry`、`ConfigManager`、`WaitQueue`、`PollWatcher`）跨设备复用。

---

## 支持的设备

| 设备 | 状态 | 路径 |
|------|------|------|
| GPGPU（VA Space / Queue / Pushbuffer / Doorbell / Fence / 模拟执行）| ✅ 完整 | `plugins/gpu_driver/` |
| 内存设备（示例）| ✅ 示例 | `drivers/sample_memory/` |
| 串口设备（示例）| ✅ 示例 | `drivers/sample_serial/` |
| PCIe 设备 | ✅ 基础 | `include/kernel/pcie/` |
| 网络设备 | 🔜 规划 | — |
| 存储设备 | 🔜 规划 | — |

---

## 10 行上手示例

```cpp
#include "kernel/vfs.h"
#include "kernel/module_loader.h"
#include "gpu_driver/shared/gpu_ioctl.h"
#include <fcntl.h>

// 1. 加载 plugins/ 下所有插件（必须从项目根目录运行）
ModuleLoader::load_plugins("plugins");

// 2. 打开设备（VFS 单例）
auto dev = VFS::instance().open("/dev/gpgpu0", O_RDWR);

// 3. 查询设备信息
gpu_device_info info{};
dev->fops->ioctl(dev->fd, GPU_IOCTL_GET_DEVICE_INFO, &info);
printf("GPU: %s, VRAM=%llu MB\n", info.marketing_name,
       (unsigned long long)(info.vram_size / (1024 * 1024)));
```

完整流程示例（VA Space + Queue + Pushbuffer 提交）见 [README.md](../../README.md#示例代码) 与 [post-refactor-architecture.md](post-refactor-architecture.md) §1.3。

---

## 30 秒架构图

```
用户应用层
   tests/, external/TaskRunner, 用户驱动
       ↓ ioctl(fd, GPU_IOCTL_*, ...)
内核模拟框架层 (SHARED 库)
   src/kernel/  + include/kernel/
   VFS (Meyers singleton) | ModuleLoader
   ServiceRegistry | Logger | WaitQueue
   include/linux_compat/
       ↓ dlopen("plugins/*.so")
设备驱动层
   plugins/gpu_driver/
   ├── drv/    GpgpuDevice (ioctl 派发表)
   ├── hal/    struct gpu_hal_ops (11 函数指针)
   ├── sim/    scheduler/ hardware/ gpu_queue_emu
   └── shared/ gpu_ioctl.h, gpu_types.h, gpu_queue.h
       ↓ HAL ops 调用
硬件仿真层
   plugins/gpu_driver/sim/  +  libgpu_core/ (纯 C buddy allocator)
```

---

## 当前阶段

**post-Phase 2**（2026-05-13 重构窗口完成）：

- ✅ System C IOCTL 体系（`GPU_IOCTL_*` 0x01-0x43）
- ✅ VA Space 抽象（`GPU_IOCTL_CREATE_VA_SPACE` 0x30）
- ✅ Queue / Ring Buffer / Doorbell（0x40-0x43）
- ✅ Hardware Puller FSM（IDLE → FETCH → DECODE → SCHEDULE → DISPATCH → COMPLETE）
- ✅ GlobalScheduler 路由到 compute / copy / firmware 引擎
- ✅ 用户态队列提交双路径（mmap doorbell 快速路径 + ioctl 回退路径，ADR-024）
- ✅ drv/hal/sim/shared 物理分离（ADR-018/023）
- ✅ libgpu_core 纯 C 提取（ADR-020）
- ✅ Catch2 测试栈（30+ 独立测试）

后续计划：网络设备 / 存储设备插件（Phase 3）→ 稳定 v1.0（Phase 4）。详见 [post-refactor-architecture.md](post-refactor-architecture.md) §1.1 时间轴。

---

## 该读哪些文档

| 你是谁 | 路径 |
|--------|------|
| **新用户** | [README.md](../../README.md) → [AGENTS.md](../../AGENTS.md)（构建命令） |
| **架构理解者** | [post-refactor-architecture.md](post-refactor-architecture.md)（SSOT）→ [architecture_design.md](architecture_design.md)（设计原理） |
| **驱动开发者** | [AGENTS.md](../../AGENTS.md) → `plugins/gpu_driver/shared/gpu_ioctl.h` |
| **贡献者** | [AGENTS.md](../../AGENTS.md)（编码风格）→ `docs/00_adr/`（架构决策） |
| **维护者** | [post-refactor-architecture.md](post-refactor-architecture.md) §3 修复清单（32 项） |

---

## 关键路径速查

| 组件 | 路径 |
|------|------|
| 框架入口 | `src/kernel/vfs.cpp` + `include/kernel/vfs.h` |
| GPU 插件入口 | `plugins/gpu_driver/plugin.cpp` |
| IOCTL 定义（System C） | `plugins/gpu_driver/shared/gpu_ioctl.h` |
| 数据类型 | `plugins/gpu_driver/shared/gpu_types.h` |
| Queue 定义 | `plugins/gpu_driver/shared/gpu_queue.h` |
| HAL 接口 | `plugins/gpu_driver/hal/gpu_hal.h` |
| 设备类 | `plugins/gpu_driver/drv/gpgpu_device.h` |
| 纯 C 库 | `libgpu_core/include/gpu_buddy.h` |
| Linux 兼容 | `include/linux_compat/` |
| 权威架构说明 | `docs/02_architecture/post-refactor-architecture.md` |

---

## IOCTL 速查（System C）

| 编号 | 宏 | 方向 | 作用 |
|------|------|------|------|
| 0x01 | `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH` | `_IOW` | 提交 GPFIFO entries |
| 0x10 | `GPU_IOCTL_ALLOC_BO` | `_IOWR` | 分配 GPU buffer object |
| 0x11 | `GPU_IOCTL_FREE_BO` | `_IOW` | 释放 buffer object |
| 0x12 | `GPU_IOCTL_MAP_BO` | `_IOWR` | 映射 BO 到 GPU VA |
| 0x13 | `GPU_IOCTL_WAIT_FENCE` | `_IOW` | 等待 fence 完成 |
| 0x20 | `GPU_IOCTL_GET_DEVICE_INFO` | `_IOR` | 查询设备能力 |
| 0x30 | `GPU_IOCTL_CREATE_VA_SPACE` | `_IOWR` | 创建 VA Space（Phase 2）|
| 0x31 | `GPU_IOCTL_DESTROY_VA_SPACE` | `_IOW` | 销毁 VA Space |
| 0x40 | `GPU_IOCTL_CREATE_QUEUE` | `_IOWR` | 创建命令队列（Phase 2）|
| 0x42 | `GPU_IOCTL_MAP_QUEUE_RING` | `_IOWR` | mmap 共享 ring buffer |
| 0x43 | `GPU_IOCTL_QUERY_QUEUE` | `_IOWR` | 查询队列状态 |

> System A 和 System B（前两代 IOCTL 体系）已删除或归档；**当前唯一活跃的是 System C（`GPU_IOCTL_*`）**。新代码请直接使用 System C。

---

## 常见问题（一句话版）

- **跑测试要"Device not found"** → 从项目根目录运行，插件路径是相对的（详见 [AGENTS.md](../../AGENTS.md)）
- **ioctl 返回 `-EFAULT`** → 检查结构体是否完整初始化
- **为什么 `kernel` 库必须是 SHARED** → `VFS::instance()` 等单例的链接模型决定（Issue #11）
- **TaskRunner 怎么接** → 共享 `plugins/gpu_driver/shared/` 头文件，子模块符号链接访问
- **可以跑真实 CUDA 程序吗** → 当前不能完整支持 CUDA runtime；TaskRunner 可基于 System C 提交 GPFIFO，模拟器已跑通基本调度链路

详细问答见 [README.md](../../README.md#常见问题) 与 [AGENTS.md](../../AGENTS.md)。

---

**维护者**: UsrLinuxEmu Team
**最后验证**: 2026-06-16
**对应代码 commit**: `374d463`
