# UsrLinuxEmu 架构设计：决策与原理

> ⚠️ **DEPRECATED**: 此文档最后验证于 2026-06-16 (commit `374d463`)，pre-v0.1.5 内容，**已显著 outdated**。
> **请使用 SSOT**: [`post-refactor-architecture.md`](post-refactor-architecture.md)（持续更新至 v0.1.7+，2026-06-23 H-4 governance cleanup 同步）。
> **迁移指南**: 见 H-4 [ADR-035](../../00_adr/adr-035-governance-policy.md) §Rule 4.3 — pre-v0.1.5 文档 deprecated 头标规则。

> **最后验证**: 2026-06-16 (commit `374d463`)
>
> **作者**: UsrLinuxEmu Architecture Team
> **作用**: 本文是 `architecture.md` 的**姊妹文档**，聚焦**设计决策背后的原理**而非结构本身。
> **配套阅读**:
> - [architecture.md](architecture.md) —— 当前架构的**结构性总览**（分层、目录、API）
> - [post-refactor-architecture.md](post-refactor-architecture.md) —— **SSOT**（权威架构说明 + docs 同步方案）
> - [AGENTS.md](../../AGENTS.md) —— 开发指南与构建命令
>
> **引用规范**: 所有 ADR 引用遵循 `adr-XXX-title.md` 格式，路径为 `docs/00_adr/`。

---

## 目录

- [§0 文档定位与读者指南](#0-文档定位与读者指南)
- [§1 顶层决策：为什么是用户态模拟](#1-顶层决策为什么是用户态模拟)
- [§2 三大架构支柱](#2-三大架构支柱)
  - [2.1 插件化架构（ADR-003）](#21-插件化架构adr-003)
  - [2.2 分层架构（ADR-006）](#22-分层架构adr-006)
  - [2.3 设备发现与 Linux 兼容（ADR-008 / ADR-009）](#23-设备发现与-linux-兼容adr-008--adr-009)
- [§3 核心：drv / hal / sim / shared 四分（ADR-018 / ADR-023）](#3-核心drv--hal--sim--shared-四分adr-018--adr-023)
  - [3.1 分离的动机](#31-分离的动机)
  - [3.2 依赖方向：drv → hal → sim](#32-依赖方向drv--hal--sim)
  - [3.3 HAL 接口契约的形式选择](#33-hal-接口契约的形式选择)
  - [3.4 构造注入而不是单例](#34-构造注入而不是单例)
- [§4 算法核心提取：为什么 libgpu_core 是纯 C（ADR-020）](#4-算法核心提取为什么-libgpu_core-是纯-cadr-020)
  - [4.1 纯度的代价与回报](#41-纯度的代价与回报)
  - [4.2 完全无锁：调用者责任](#42-完全无锁调用者责任)
  - [4.3 与 HAL 的边界](#43-与-hal-的边界)
- [§5 命令处理的状态机：Hardware Puller（ADR-021）](#5-命令处理的状态机hardware-pulleradr-021)
  - [5.1 为什么必须是状态级仿真](#51-为什么必须是状态级仿真)
  - [5.2 Doorbell 触发而非直接处理](#52-doorbell-触发而非直接处理)
  - [5.3 Global Scheduler 的责任分配](#53-global-scheduler-的责任分配)
- [§6 用户态队列提交：双路径架构（ADR-024）](#6-用户态队列提交双路径架构adr-024)
  - [6.1 真实硬件的参照](#61-真实硬件的参照)
  - [6.2 为什么是"快速路径 + 回退路径"而不是替换](#62-为什么是快速路径--回退路径而不是替换)
  - [6.3 VA Space 作为 Queue 的归属](#63-va-space-作为-queue-的归属)
- [§7 跨切关注：namespace、错误码、测试框架](#7-跨切关注namespace错误码测试框架)
  - [7.1 `usr_linux_emu` 命名空间（ADR-002 衍生）](#71-usr_linux_emu-命名空间adr-002-衍生)
  - [7.2 Linux 错误码统一](#72-linux-错误码统一)
  - [7.3 测试框架：Catch2（ADR-010 实际结果）](#73-测试框架catch2adr-010-实际结果)
- [§8 与真实硬件的语义对齐](#8-与真实硬件的语义对齐)
- [§9 已识别的设计张力与开放问题](#9-已识别的设计张力与开放问题)
- [附录 A：ADR 索引（本文引用）](#附录-aadr-索引本文引用)
- [附录 B：变更记录](#附录-b变更记录)

---

## §0 文档定位与读者指南

`architecture.md` 回答的是"**长什么样**"：分层、目录、API、数据流。本文回答的是"**为什么这样**"：每一个看似武断的目录划分、接口签名、依赖方向背后，源自哪条 ADR，权衡了什么代价。

**新读者路径**：先读 `architecture.md` 拿到"地图"，再读本文理解"路线为什么这么走"。

**架构师 / 维护者**：当收到"这个设计是否合理"的质疑时，引用本文对应的 ADR 章节回应。

**贡献者**：在写新代码前，先看相关章节确认"这代码属于哪一层、用哪种依赖方向"。

---

## §1 顶层决策：为什么是用户态模拟

> 详见 [ADR-001](../00_adr/adr-001-user-mode-emulation.md)

设备驱动开发传统上要求 root 权限、内核编译、模块加载、硬件可达，**开发门槛极高**。UsrLinuxEmu 用一个激进的前提换掉了这套模型：

**决策**：整套 Linux 内核设备模型在用户态模拟。开发者用普通用户身份跑一个可执行文件，就能开发、测试、调试驱动（特别是 GPU 驱动）。

**原理层面的代价**：

| 维度 | 收益 | 代价 |
|------|------|------|
| 安全性 | 驱动 bug 不会让内核 panic | 真实内核态竞态无法仿真 |
| 迭代速度 | 改代码 → 重链接 → 重跑，无需重启 | 需要适配层 |
| 可调试性 | GDB、Valgrind、AddressSanitizer 直接用 | 性能轨迹与真实驱动不完全一致 |
| 跨平台潜力 | 理论上 macOS 也能跑（已部分实现） | Linux 内核 API 兼容性是天花板 |

这个决策**不是为生产环境部署驱动**准备的；它是为了**缩短驱动开发者从"想清楚"到"验证过"的距离**。TaskRunner（外部子模块）通过共享的 `plugins/gpu_driver/shared/` 头文件在模拟器与真实内核驱动之间零改动切换，**让"先在模拟器跑通，再到真机验证"成为默认工作流**。

---

## §2 三大架构支柱

### 2.1 插件化架构（ADR-003）

> 详见 [ADR-003](../00_adr/adr-003-plugin-architecture.md)

设备作为**动态库（`.so`）**动态加载，由 `ModuleLoader::load_plugins("plugins")` 触发（`include/kernel/module_loader.h`）。

**关键细节**：

- **加载机制**：标准 `dlopen` + `dlsym(handle, "mod")` 模式，插件必须导出名为 `mod` 的 `struct module` 符号（见 [AGENTS.md](../../AGENTS.md) "kernel 库必须是 SHARED" 段）
- **配置载体**：`plugins/plugins.json` 列出所有插件
- **生命周期**：VFS 在插件加载时注册设备节点，卸载时反注册

**原理**：

1. **核心框架零依赖设备类型** —— 加新设备不改 `src/kernel/` 一行代码
2. **并行开发** —— 多个开发者可同时开发不同设备插件，无合并冲突
3. **故障隔离** —— 某个插件 bug 不影响其他插件和框架
4. **可选加载** —— 资源受限场景下只加载需要的插件

**为什么不直接静态链接？** 静态链接会让每个测试二进制都带一份内核框架和所有设备，浪费空间且无法独立部署设备。

### 2.2 分层架构（ADR-006）

> 详见 [ADR-006](../00_adr/adr-006-layered-architecture.md)

四层：用户应用层 / 框架层 / 设备驱动层 / 硬件模拟层。**这层的物理载体在不同阶段经历过重组**——Phase 1.5 之后，驱动层进一步被拆分为 `drv/hal/sim/shared/`（见 §3），分层没有变化，但物理分离更细。

### 2.3 设备发现与 Linux 兼容（ADR-008 / ADR-009）

> 详见 [ADR-008](../00_adr/adr-008-linux-api-compat.md) / [ADR-009](../00_adr/adr-009-singleton-pattern.md)

**Linux 兼容层**（`include/linux_compat/`）是用户态模拟能"假装是内核"的桥梁：

- `u8/u16/u32/u64` 类型
- `ERR_PTR/PTR_ERR` 宏
- `_IOR/_IOW/_IOWR` ioctl 编码
- `GFP_*` 分配标志
- DRM 子集（`drm_ioctl.h` / `drm_gem.h` / `drm_driver.h`）

**单例模式**（Meyers singleton）用于 `VFS::instance()`、`ServiceRegistry::instance()` 等全局服务点。**Issue #11** 的修复（`kernel` 库必须 SHARED）直接源自此处——单例的链接模型决定了框架必须 SHARED 而非 STATIC（见 [AGENTS.md](../../AGENTS.md) 关键架构决策段）。

---

## §3 核心：drv / hal / sim / shared 四分（ADR-018 / ADR-023）

> 详见 [ADR-018](../00_adr/adr-018-driver-sim-separation.md) / [ADR-023](../00_adr/adr-023-hal-interface.md)
>
> **这是 Phase 1.5 的核心架构重组**，将原本塞在 635 行 `plugin.cpp` 里的代码按"是否可移植到真实内核"重新分类。

### 3.1 分离的动机

原来 `plugin.cpp` 混着两类**本质不同**的代码：

- **驱动代码**（移植目标）：`handle_pushbuffer_submit_batch`、`handle_alloc_bo`、GEM object 生命周期
- **仿真代码**（仅用户态环境）：`BuddyAllocator`、fence map、`std::cout` 调试日志

混在一起导致三个灾难：

1. **不可移植**：驱动代码里 `std::mutex`、`std::cout` 直接用，进内核得全部重写
2. **职责不清**：新人不知道哪些代码能动、哪些是"神圣的"
3. **不可独立测试**：仿真逻辑和驱动逻辑绑死，没法拆开测

**决策**：物理拆为四个目录：

| 目录 | 角色 | 命运 |
|------|------|------|
| `plugins/gpu_driver/drv/` | GpgpuDevice 派发表、ioctl handler、fence tracker、VA Space 管理 | **移植到真实内核** |
| `plugins/gpu_driver/hal/` | `struct gpu_hal_ops`（11 个函数指针）+ `hal_user` + `hal_mock` | 接口两边都编译，实现两边不同 |
| `plugins/gpu_driver/sim/` | `GlobalScheduler`、`HardwarePullerEmu`、`DoorbellEmu`、`SimBuddyAllocator` | **仅用户态**，不移植 |
| `plugins/gpu_driver/shared/` | `gpu_ioctl.h` / `gpu_types.h` / `gpu_queue.h` / `gpu_events.h` / `gpu_regs.h` | **ABI 契约**，与 TaskRunner 共享 |

### 3.2 依赖方向：drv → hal → sim

```
drv/  ──►  hal/  ──►  sim/
  │         │           │
  │         │           └─  BuddyAllocator, Puller, Scheduler
  │         └─ gpu_hal.h 定义 11 个硬件访问函数指针
  └─ ioctl handler 调用 HAL 接口
```

**铁律**：

- `drv/` **不**直接调 `sim/`，所有硬件访问走 `hal/`
- `sim/` **不**依赖 `drv/`，可独立编译和测试
- `shared/` **不**属于任何一边，是 ABI 契约

**移植时的映射**：

```
用户态：            drv/ → hal_user.cpp → sim/（函数调用）
真实内核：         drv/ → 真实 hal/ 实现 → 硬件寄存器（MMIO）
单元测试：         drv/ → hal_mock.cpp → 桩函数
```

**为什么必须有这一层？** 没有 HAL，`drv/` 要么直接调 `sim/`（不可移植），要么满屏 `#ifdef CONFIG_USERMODE`（不可维护）。HAL 让"用户态函数调用"和"内核态 MMIO"在 `drv/` 看来是同一种东西。

### 3.3 HAL 接口契约的形式选择

**为什么是 `struct gpu_hal_ops` 函数指针表 + `static inline` 包装函数**，而不是 C++ 纯虚类？

- Linux 内核模块是 C 编译，**不**支持 C++ 虚函数
- 内核标准模式就是 `struct xxx_ops`（`struct file_operations`、`struct pci_driver_ops`、`struct drm_driver`）
- 移植时直接替换实现函数，**零适配成本**

11 个函数指针按"是否可能失败"分两类：

- **`int` 返回**（失败 → Linux 错误码）：`register_read/write`、`mem_read/write/alloc/free`、`fence_create/read`
- **`void` 返回**（弹射式）：`doorbell_ring`、`interrupt_raise`、`time_wait`

**为什么这样分？** 不可能失败的操作不强迫调用方写无意义的错误检查路径，错误处理语义更干净。

### 3.4 构造注入而不是单例

```cpp
class GpgpuDevice {
  explicit GpgpuDevice(struct gpu_hal_ops *hal) : hal_(hal) {}
private:
  struct gpu_hal_ops *hal_;
};
```

**为什么不是单例？** HAL 注入让单元测试可以用 `hal_mock` 隔离硬件依赖，**单例无法多实例隔离**。GpgpuDevice 本身在 VFS 中也只有一个实例，但构造注入让"硬件是谁"这个事在构造时决定，对测试友好。

---

## §4 算法核心提取：为什么 libgpu_core 是纯 C（ADR-020）

> 详见 [ADR-020](../00_adr/adr-020-libgpu-core-extraction.md)

`libgpu_core/` 目录里只有两个文件：`include/gpu_buddy.h` + `src/buddy.c`。**不是 C++ 包装，不是 STL，没有依赖**。

### 4.1 纯度的代价与回报

**零依赖清单**：

| ✅ 允许 | ❌ 禁止 |
|---------|---------|
| 纯 C（C99/C11） | `malloc`/`free` |
| 传入缓冲区操作 | 系统调用 |
| 位运算 | STL |
| `assert()` | 锁/原子操作 |
| `memcpy/memset` | 日志输出 |
| `bool`/`uint32_t` | `errno` |

**为什么这样狠？** 因为内核模块是 C 编译的，**不能带任何 C++ runtime**。`libgpu_core/*.c` 要做到"复制到任何 Linux 内核驱动的子目录下，加进该驱动的 `Makefile` 就能编译"。这就是"**可移植性门禁**"——一旦混进 STL，整个库就废了。

### 4.2 完全无锁：调用者责任

```c
void     gpu_buddy_init(struct gpu_buddy *buddy, void *memory, u64 size);
u64      gpu_buddy_alloc(struct gpu_buddy *buddy, u64 size);
void     gpu_buddy_free(struct gpu_buddy *buddy, u64 addr);
```

BuddyAllocator 只操作自身数据结构，**不**做锁。**调用者必须做外部同步**。

**为什么？** 锁策略取决于调用场景：用户态仿真可以用 `std::mutex`，内核态用 `spinlock` 或 `mutex`，固化在 libgpu_core 里反而绑死了使用方式。

### 4.3 与 HAL 的边界

`libgpu_core` 是**纯地址运算**，HAL 的 `mem_alloc/mem_free` 调用它完成实际的 VRAM 划分。**libgpu_core 不感知 HAL**，HAL 也不感知 libgpu_core 的存在——`sim/buddy_allocator.cpp` 把 libgpu_core 的 C API 包装为 `SimBuddyAllocator` 类，HAL 在 `hal_user.cpp` 里调用这个包装器。

**为什么这样切？** 关注点分离：算法是 C，仿真设备是 C++，HAL 是契约。

---

## §5 命令处理的状态机：Hardware Puller（ADR-021）

> 详见 [ADR-021](../00_adr/adr-021-hardware-puller.md)

`HardwarePullerEmu`（在 `plugins/gpu_driver/sim/hardware/`）模拟真实 GPU 的命令处理单元（NVIDIA PBF/PE、AMD ACE）。**它不是简单 switch 循环**，是完整的状态机：

```
IDLE → FETCH_SOURCE_SELECT → [FETCH_FROM_SHARED_RING | FETCH_FROM_DEVICE_MEMORY]
     → DECODE → [SCHEDULE | SEMAPHORE]
     → DISPATCH → COMPLETE → NEXT → IDLE
```

### 5.1 为什么必须是状态级仿真

真实硬件 Puller 是状态机：**从 ring buffer DMA 读 entry → 解码 method → 处理 semaphore → 分发引擎 → 中断通知**。**只仿真"打印日志然后返回成功"根本不是命令处理单元**。

**为什么不是行为级或周期级？**

- 行为级：只关心"做了什么"，无法验证状态转换正确性
- 周期级：仿真时序，对驱动验证过度工程

状态级是"够用"和"不冗余"之间的甜点。

### 5.2 Doorbell 触发而非直接处理

`handle_pushbuffer_submit_batch` 不再就地处理命令，而是：

1. 将 GPFIFO entries 写入模拟的设备内存（drv 通过 HAL 写入）
2. 触发模拟 doorbell 写入（`hal_doorbell_ring`）
3. Puller 状态机从 IDLE → FETCH 开始完整流程

**为什么绕一圈走 doorbell？** 真实 GPU 就是这么工作的：用户态写 doorbell 寄存器，硬件 Puller 检测到 doorbell 变化才开始拉命令。**直接处理会让仿真无法验证"doorbell 触发是否能正确唤醒 Puller"这种真实硬件的关键行为**。

### 5.3 Global Scheduler 的责任分配

Puller 状态机责任范围是 `FETCH → DECODE → ISSUE`，**不**做引擎分发。`GlobalScheduler`（在 `plugins/gpu_driver/sim/scheduler/`）接管：

- 维护待执行命令队列
- 按引擎类型路由（compute → `gpu_core_emu`、copy → `memcpy`、firmware → `cpu_core_emu`）
- 支持优先级调度（Phase 2 实现）
- 支持并发执行（多 entry 可同时在不同引擎上执行）

**为什么 Puller 和 Scheduler 拆开？** 任务编排和执行引擎路由是**两个关注点**。拆开之后，Puller 的 FSM 复杂度被压住，Scheduler 可以独立做调度策略实验。

---

## §6 用户态队列提交：双路径架构（ADR-024）

> 详见 [ADR-024](../00_adr/adr-024-user-mode-queue-submission.md)
>
> Phase 2 引入，是 UsrLinuxEmu 当前**最有架构性**的设计决策。

### 6.1 真实硬件的参照

| 架构 | 用户态提交机制 |
|------|----------------|
| AMD UMQ (GFX11+/CDNA3) | 用户态直接写 Ring Buffer (GPU VRAM) + 写 Doorbell (PCIe BAR MMIO mmap) |
| NVIDIA GPFIFO | 用户态写 GPFIFO ring + 写 `GP_PUT` 或 userd doorbell |
| Intel Gen12+/Xe | 用户态写 ring + doorbell (GuC 调度) |

**共同点**：用户态**直接写共享内存 + 写 doorbell**，**零 syscall**。kernel 只在队列创建/异常时介入。

**UsrLinuxEmu 改造前**的状态：每次命令提交都走 `ioctl(GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH)`，**每次都是 syscall**。这与真实硬件语义不一致，也让 TaskRunner 迁移到真机时必须改提交代码。

### 6.2 为什么是"快速路径 + 回退路径"而不是替换

ADR-024 决策是**双路径**而不是替换：

```
快速路径（新增）:                  回退路径（现有）:

TaskRunner                         TaskRunner
    │                                   │
    │ 写 Ring Buffer (共享内存)           │ ioctl(SUBMIT_BATCH)
    │ *(volatile u32*)doorbell           │
    ▼                                   ▼
GPU 模拟                            GpgpuDevice
    │                                   │
    │ Hardware Puller 读取               │ HAL DMA write + doorbell_ring
    ▼                                   ▼
       HardwarePullerEmu runLoop()  ←  共用执行路径
```

**为什么不全替换？**

1. **架构对齐**：真实硬件就是这样，模拟器对齐
2. **性能潜力**：模拟器里 syscall 占 30-50% 模拟时间
3. **渐进迁移**：现有 ioctl 路径完整保留，TaskRunner 可以逐场景切换
4. **测试兼容**：不影响现有测试

ioctl 回退路径保留三个用途：

- **兼容回退**：TaskRunner 在不支持用户态队列时使用
- **调试路径**：开发测试时的替代提交方式
- **小批量**：单次命令提交的开销在模拟器中可接受

**HAL doorbell 的双层模型**（ADR-023 v2 修订）：

```
用户态 MMIO 直写（快速路径）:
  TaskRunner → *(volatile u32*)doorbell_ptr = queue_id
       └─ mmap'd BAR 地址 → DoorbellEmu::write()

内核 HAL 调用（回退路径）:
  drv/ → hal_doorbell_ring(hal_, queue_id)
       └─ 函数指针 → hal->doorbell_ring(ctx, qid) → DoorbellEmu::write()
```

**两边最终都到 `DoorbellEmu::write()`**，执行路径统一。

### 6.3 VA Space 作为 Queue 的归属

Phase 2 之前，Queue 是隐式默认的。Phase 2 把 Queue 显式化，**且必须属于某个 VA Space**：

```cpp
struct gpu_queue_args {
  gpu_va_space_handle_t va_space_handle;  // 必须
  u32 queue_type;
  u32 priority;
  u64 ring_buffer_size;
  gpu_queue_handle_t queue_handle;        // OUT
  u64 doorbell_pgoff;                     // OUT: 给用户态 mmap
};
```

**为什么 Queue 必须挂在 VA Space 下？** 真实 GPU 的 GPU VA 是进程地址空间，VA Space 是这个抽象的容器。Queue 在哪个 VA Space 下创建，决定了它的 Ring Buffer 物理地址映射对哪个进程可见。Phase 2 强制 `validate VA Space exists` + `validate Queue belongs to VA Space` 是为了把"进程隔离"这件事在模拟器层就做对。

---

## §7 跨切关注：namespace、错误码、测试框架

### 7.1 `usr_linux_emu` 命名空间（ADR-002 衍生）

所有公共类型在 `usr_linux_emu` 命名空间下（`include/usr_linux_emu/`，目前是空目录，预留）。**原因**：避免与真实 Linux 内核头文件中的同名类型冲突，特别是在 `include/linux_compat/` 的过渡期。

### 7.2 Linux 错误码统一

所有可能失败的函数返回 `int`，`0` 成功，**负值**是 Linux 错误码（`-EINVAL`、`-ENOMEM`、`-EFAULT`、`-ENOTTY`）。HAL 返回值**直接穿透**到 ioctl 返回值，不做转换：

```cpp
long handle_pushbuffer_submit_batch(void* argp) {
  int ret = hal_mem_write(hal_, DEV_MEM_BASE, entries, size);
  if (ret < 0) return ret;  // -EFAULT 直接到用户
  hal_doorbell_ring(hal_, queue_id);
  return 0;
}
```

**为什么统一负值？** 让用户态程序**复用内核态的错误处理习惯**——`if (ioctl(...) < 0) { perror(...); }` 的代码可以直接搬。

### 7.3 测试框架：Catch2（ADR-010 实际结果）

> ADR-010 原提案"迁移到 GTest"**未实施**。实际项目用 Catch2（vendored 单文件，零外部依赖）。

**为什么 Catch2 比 GTest 适合？**

- **零安装**：单文件 `tests/catch_amalgamated.{hpp,cpp}`，不需要 `apt install libgtest-dev`
- **测试用例可拆分为独立二进制**（`build/bin/test_*_standalone`），CI 跑得快
- **TDD 风格**：`TEST_CASE` + `REQUIRE` 比 GTest 的 `TEST_F` 简洁

**重要纠正**：所有标"测试框架是 GTest"的旧文档（README、copilot-instructions、`docs/01-quickstart/installation.md`、`docs/04-building/testing_guide.md`）是**错的**，以代码为准。

---

## §8 与真实硬件的语义对齐

UsrLinuxEmu 不是"看起来像 Linux 内核的玩具"，而是"**假装是真 GPU 驱动的开发环境**"。这一目标在 Phase 2 后具体化为：

| 真实硬件行为 | UsrLinuxEmu 实现 | 对应 ADR |
|--------------|------------------|----------|
| 用户态写 mmap doorbell 触发 GPU | `*(volatile u32*)doorbell_ptr` 触发 `DoorbellEmu::write` | ADR-024 |
| GPU 从 VRAM ring buffer DMA 拉命令 | Puller FETCH 阶段通过 `hal_mem_read` | ADR-021 |
| GPU 写 fence 通知完成 | `sim/fence_sim.cpp` 写共享 fence 内存 | ADR-021 |
| GPU 触发 MSI-X 中断 | `hal_interrupt_raise` → 用户态 callback | ADR-023 |
| 用户态 mmap BAR | `vma->vm_pgoff` 路由 doorbell / ring buffer 区域 | ADR-024 |
| 多进程 GPU VA 隔离 | VA Space handle + 进程范围检查 | ADR-017 / Phase 2 |

**这套对齐**让 TaskRunner 在模拟器和真机之间的代码差异**趋近于零**——验证过的逻辑在真机上几乎不会重新失败（除了硬件 bug）。

---

## §9 已识别的设计张力与开放问题

虽然 §3-§6 的决策都经过评审，但仍有几个**已知张力**值得记录：

1. **HAL 的间接调用开销**（约 2 次指针解引用）—— 在性能敏感路径上是否需要 `static inline` 优化？
2. **libgpu_core 的零日志约束**—— 调试时缺乏上下文，需要在 `sim/` 包装层加日志
3. **mmap 的安全边界**—— 用户态 mmap doorbell 仅映射到创建队列的进程，多进程权限模型未完全设计
4. **VA Space 多 GPU 注册**（`GPU_IOCTL_REGISTER_GPU` 0x32 已定义但未深入使用）—— 未来多 GPU / P2P 场景的语义需要更多 ADR 补充
5. **GTest vs Catch2 文档历史遗留**—— 12+ 个文件仍声称 GTest，需要 P2 级修复

这些**不**是阻塞性问题，但都在 [post-refactor-architecture.md](post-refactor-architecture.md) §3 的修复清单里有追踪。

---

## 附录 A：ADR 索引（本文引用）

| ADR | 标题 | 关联章节 |
|-----|------|----------|
| [ADR-001](../00_adr/adr-001-user-mode-emulation.md) | 采用用户态模拟而非内核模块 | §1 |
| [ADR-002](../00_adr/adr-002-cpp17-language.md) | 采用 C++17 作为开发语言 | §7.1 |
| [ADR-003](../00_adr/adr-003-plugin-architecture.md) | 采用插件化架构 | §2.1 |
| [ADR-004](../00_adr/adr-004-buddy-allocator.md) | Buddy Allocator 管理 GPU 内存 | §4 |
| [ADR-005](../00_adr/adr-005-ring-buffer.md) | Ring Buffer 管理 GPU 命令队列 | §6 |
| [ADR-006](../00_adr/adr-006-layered-architecture.md) | 采用分层架构设计 | §2.2 |
| [ADR-008](../00_adr/adr-008-linux-api-compat.md) | Linux 内核 API 兼容层 | §2.3 |
| [ADR-009](../00_adr/adr-009-singleton-pattern.md) | 单例模式实现核心服务 | §2.3 |
| [ADR-010](../00_adr/adr-010-gtest-migration.md) | GTest 迁移（未实施，实际 Catch2） | §7.3 |
| [ADR-015](../00_adr/adr-015-gpu-ioctl-unification.md) | GPU IOCTL 接口统一（System C） | §6, §3 |
| [ADR-017](../00_adr/adr-017-gpfifo-queue-abstraction.md) | GPFIFO/Queue 抽象 | §6.3 |
| [ADR-018](../00_adr/adr-018-driver-sim-separation.md) | 驱动/仿真代码分离策略 | §3（核心） |
| [ADR-020](../00_adr/adr-020-libgpu-core-extraction.md) | libgpu_core 算法核心提取 | §4（核心） |
| [ADR-021](../00_adr/adr-021-hardware-puller.md) | Hardware Puller GPFIFO 状态机 | §5（核心） |
| [ADR-023](../00_adr/adr-023-hal-interface.md) | 仿真层接口契约（HAL） | §3（核心） |
| [ADR-024](../00_adr/adr-024-user-mode-queue-submission.md) | 用户态队列命令提交架构 | §6（核心） |

---

## 附录 B：变更记录

| 版本 | 日期 | 变更 | 作者 |
|------|------|------|------|
| v0.1-draft | 2026-04-07 | 初始草案（旧目录布局） | DevMate |
| v0.2-draft | 2026-04-07 | 完整架构设计（旧布局） | DevMate + OpenCode |
| **v1.0** | **2026-06-16** | **基于 Phase 2 重写**；聚焦"为什么"而非"是什么"；引用 ADR-001/003/006/008/009/015/017/018/020/021/023/024 共 12 个 ADR；移除所有过期路径与已弃用 API 引用；交叉链接 `architecture.md` + `post-refactor-architecture.md` | Sisyphus |

---

**维护者**: UsrLinuxEmu Architecture Team
**最后更新**: 2026-06-16
**对应代码 commit**: `374d463`
