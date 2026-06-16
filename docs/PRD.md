# UsrLinuxEmu 产品需求文档 (PRD)

> **最后验证**: 2026-06-16 (commit `374d463`)
>
> **架构 SSOT**: [docs/02_architecture/post-refactor-architecture.md](02_architecture/post-refactor-architecture.md)
>
> **当前版本**: v0.5+（post-Phase 2）
>
> 本文档讲清楚"为什么做"和"是什么"。架构、目录、IOCTL 体系等"怎么做"的细节由 SSOT 与 ADR 给出。当本文与 SSOT 冲突时，以 SSOT 为准。

---

## 1. 产品概述

### 1.1 一句话描述

UsrLinuxEmu 是一款**用户态 Linux 内核模拟环境**，让驱动开发者在**无需 root 权限、无需内核编译**的情况下开发、验证、调试设备驱动（特别是 GPGPU 驱动），最终可零修改移植到真实 Linux 内核。

### 1.2 核心理念

> **"开发在用户态，部署到内核。仿真即验证，验证即迁移。"**

- 内核态驱动代码在用户态开发、调试、回归测试
- 验证通过后代码可移植到真实 Linux 内核
- 仿真层作为长期测试 mock，支持 CI/CD 持续验证

### 1.3 产品定位

| 维度 | 定位 |
|------|------|
| **不是** | 完整的 GPU 模拟器（不等同于 QEMU 设备模拟）|
| **不是** | 性能对标平台（不追求周期级仿真精度）|
| **是** | 驱动逻辑正确性验证平台 |
| **是** | 可移植内核驱动代码的开发环境 |
| **是** | 驱动/仿真物理分离的迭代框架（Phase 1.5 起）|

---

## 2. 核心产品特性

UsrLinuxEmu 在 v0.5+ 阶段区别于同类项目的差异化能力，全部围绕"驱动可移植"和"仿真可复用"两条主线。

### 2.1 驱动/仿真代码物理分离（ADR-018）

GPU 插件按四层目录结构组织（`plugins/gpu_driver/{drv,hal,sim,shared}/`），驱动代码与仿真代码物理隔离：

- `drv/`: `GpgpuDevice` + DRM 骨架 + ioctl handler，未来可零修改移植到 `drivers/gpu/`
- `hal/`: `struct gpu_hal_ops` 接口契约（11 个函数指针），连接驱动与仿真
- `sim/`: 硬件仿真实现（`GlobalScheduler`、`HardwarePullerEmu`、Ring Buffer 消费者）
- `shared/`: 与 TaskRunner 共享的 canonical 头文件（`gpu_ioctl.h`、`gpu_queue.h`）

移植到真实内核时，复制 `drv/` + `hal/` + `shared/`，删除 `sim/` 即可。**这一分层是 UsrLinuxEmu 的核心产品特征**，所有后续功能（VA Space、Queue、多队列 fetch）都在此分层之上构建。

### 2.2 VA Space 抽象（ADR-024）

Phase 2 引入 GPU 虚拟地址空间，作为 BO 与 Queue 的承载容器：

- 每个 VA Space 有独立的 page size（4KB / 64KB）与 flags
- Queue 必须归属于某个 VA Space（`gpu_queue_args.va_space_handle`）
- 提交 pushbuffer 时校验 VA Space 与 Queue 的归属关系
- 完整 ioctl 链路：`GPU_IOCTL_CREATE_VA_SPACE` → `GPU_IOCTL_CREATE_QUEUE` → `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH`

这是 Phase 2 的标志性能力，让"多进程隔离 VA"和"GPU 上下文切换"成为可能。

### 2.3 Queue + Ring Buffer 体系（ADR-005, ADR-017, ADR-024）

Phase 2 完整支持用户态队列命令提交：

- `gpu_ring_header` 包含 `write_idx` / `read_idx` / `capacity` / `entries[]`
- `GpuQueueEmu`（`plugins/gpu_driver/sim/gpu_queue_emu.cpp`）作为 Ring Buffer 消费者
- 多 queue 独立 fetch + doorbell 触发
- `LAUNCH_CB` 已删除（commit `b78edc9`），命令流统一通过 pushbuffer 提交

### 2.4 用户态运行 + 插件化（ADR-001, ADR-003）

- 无需 root 权限、无需加载内核模块
- 通过 `dlopen` + `dlsym("mod")` 动态加载设备插件
- `kernel` 库必须为 SHARED（Issue #11），保证 VFS 单例在可执行文件与插件间一致

### 2.5 System C 统一 IOCTL（ADR-015）

以 `GPU_IOCTL_*`（magic='G'）为唯一规范接口。System A（`CUDA_*`）已删除，System B（`GPGPU_*`）已归档到 `archive/system_b_drivers/gpu/`。TaskRunner 与 UsrLinuxEmu 通过符号链接共享头文件，零耦合集成。

### 2.6 Linux 兼容层（ADR-008）

`include/linux_compat/` 提供内核 API 的用户态实现：`u8/u32/u64` 类型、`ERR_PTR` 宏、`_IOR/_IOW/_IOWR` ioctl 编码、`GFP_*` 分配标志、DRM 子集（`drm_ioctl.h` / `drm_gem.h` / `drm_driver.h`）。驱动代码使用兼容 API，移植到真实内核时把 `#include "linux_compat/..."` 改为 `<linux/...>` 即可。

### 2.7 libgpu_core 算法核心提取（ADR-020）

`libgpu_core/` 目录提供纯 C、零依赖的算法核心（buddy allocator、MMU events）。Phase 1.5 从驱动代码中提取，可独立编译、被 `drv/` 与 `sim/` 共同引用。算法层无 C++ 依赖，方便在裸内核环境复用。

---

## 3. 目标用户

| 角色 | 关注点 | 使用方式 |
|------|--------|---------|
| **GPU 驱动开发工程师** | 驱动逻辑是否正确、是否可零修改移植到内核 | 在 UsrLinuxEmu 中开发 `drv/` 代码 |
| **TaskRunner 团队** | IOCTL 接口是否一致、能否提交命令到 `/dev/gpgpu0` | 通过 `GPU_IOCTL_*` 调用仿真设备 |
| **硬件仿真工程师** | 硬件行为是否被正确仿真 | 在 `sim/` 层实现硬件状态机 |
| **CI/CD 系统** | 每次提交是否通过回归测试 | 运行 `ctest` + 单元测试 |

---

## 4. 产品目标

### 4.1 核心目标

| # | 目标 | 衡量标准 |
|---|------|---------|
| G1 | 驱动代码可在用户态完整运行 | TaskRunner 能通过 `/dev/gpgpu0` 提交并完成 kernel launch |
| G2 | 驱动代码可零修改移植到真实 Linux 内核 | `drv/` 代码在内核编译环境下不依赖任何 UsrLinuxEmu 内部类型 |
| G3 | 与 TaskRunner 零耦合集成 | 仅通过 `shared/` 头文件 + ioctl 交互；`ldd libgpu_driver_plugin.so` 无 TaskRunner 依赖 |
| G4 | 仿真可复用于内核驱动开发 | `sim/` 层作为内核驱动的测试 mock 长期维护 |

### 4.2 非目标

| # | 不做什么 | 理由 |
|---|---------|------|
| N1 | **不做完整 GPU 指令集仿真** | 周期级仿真收益/成本比不适合驱动验证场景 |
| N2 | **不做性能对标** | 不要求仿真达到真实硬件的 kernel launch 延迟。语义正确即可，fence 等待不能死锁 |
| N3 | **不做 Vulkan/OpenGL 图形管线** | 专注于 GPGPU compute 语义 |
| N4 | **不做完整 DRM/KMS 显示子系统** | 只实现 `DRM_RENDER_ALLOW` 的 ioctl 子集 |
| N5 | **不做多节点集群管理** | 单节点、单 GPU 场景 |

---

## 5. 阶段交付

### 5.1 已完成阶段

| 阶段 | 时间 | 关键交付 | 关联 ADR |
|------|------|---------|---------|
| **Phase 0** | 2025-12 ~ 2026-02 | 早期单仓库布局；`drivers/gpu/` + `simulator/gpu/` + `include/kernel/device/gpgpu_device.h` | ADR-001, 002, 003, 004, 005, 006, 007, 008, 009 |
| **Phase 1** | 2026-04 | System C 引入（`GPU_IOCTL_*` 替代 `GPGPU_*`）；`plugins/gpu_driver/shared/gpu_ioctl.h` 新建 | ADR-015 |
| **Phase 1.5** | 2026-05 上 | 驱动/仿真代码物理分离（`drv/hal/sim/shared/`）；HAL 接口契约（11 个函数指针）；`libgpu_core/` 提取；Hardware Puller 状态机 | ADR-016, 018, 019, 020, 021, 023 |
| **Phase 2** | 2026-05-13 | Ring Buffer + `GpuQueueEmu` 多队列 fetch + doorbell 修复 + `LAUNCH_CB` 删除 + 队列 ioctl 接线 + `fence_id` 异步跟踪 + **VA Space 抽象** + 用户态队列命令提交 | ADR-017, 021, 024 |

### 5.2 后续阶段

| 阶段 | 方向 | 关联 ADR |
|------|------|---------|
| **Phase 3** | 网络设备 / 存储设备插件 | （规划中，无具体 ADR）|
| **Phase 4** | 稳定的 v1.0 发布；多进程支持、性能优化、错误处理、日志增强 | ADR-011, 012, 013, 014（提议中）|

> ADR 索引与状态详见 [docs/00_adr/README.md](00_adr/README.md)。**编号 022 在 021 与 023 之间存在 gap**，无对应文件，引用时请跳过该编号。

---

## 6. 架构约束

### 6.1 可移植性约束（`drv/` 目录）

`drv/` 中所有代码必须满足以下规则，移植到真实 Linux 内核时无需重写：

1. 不使用 C++ STL 容器（`vector` / `map` / `string` 等）
2. 不使用 C++ 异常（`-fno-exceptions` 必须能编译）
3. 不使用 RTTI（`typeid` / `dynamic_cast`）
4. 不使用 `std::cout` / `iostream`
5. 不使用 `std::thread` / `std::mutex`
6. 所有硬件访问通过 HAL 接口（`struct gpu_hal_ops`）
7. 使用 `linux_compat` 类型（`u32` / `u64` 等）
8. 返回 Linux 错误码（`-EINVAL` / `-ENOMEM` 等）
9. 不使用全局/静态非平凡构造
10. 允许 C++ class、继承、虚函数、`constexpr`、namespace

### 6.2 零耦合约束

```
TaskRunner 与 UsrLinuxEmu 的交互边界：
┌──────────────────────────────────────────────────────┐
│ 1. 仅通过 GPU_IOCTL_* (magic='G') ioctl 交互          │
│ 2. 仅通过 shared/ 头文件共享类型定义                  │
│ 3. libgpu_driver_plugin.so 不链接任何 TaskRunner 库   │
│ 4. 符号链接断裂 = build fatal error                  │
│ 5. ldd plugin.so | grep -i taskrunner = 空           │
└──────────────────────────────────────────────────────┘
```

### 6.3 代码分离约束

| 目录 | 内容 | 移植到内核 | sim/ 依赖 |
|------|------|----------|----------|
| `shared/` | canonical ioctl/types 头文件 | 复制 | 否 |
| `drv/` | `GpgpuDevice`、DRM、ioctl handler | 移植 | 否（通过 HAL）|
| `hal/` | `gpu_hal.h` 接口定义 | 复制 | 否（接口）|
| `sim/` | 仿真实现（puller / buddy / emu） | 保留 | N/A |
| `libgpu_core/` | 纯 C 算法 | 复制 | 否 |

### 6.4 测试框架约定

项目实际使用 **Catch2**（vendored 单文件，commit `e9eff35` 起），与 `tests/catch_amalgamated.{hpp,cpp}` 一致。ADR-010 "迁移到 GTest" 提议未实施，新代码应遵循 Catch2 的 `TEST_CASE` / `REQUIRE` 风格。

---

## 7. 验收标准

### 7.1 阶段验收

| 阶段 | 完成条件 |
|------|---------|
| **Phase 1（已完成）** | 6 个 P0 ioctl 实现并通过测试；TaskRunner 可完成 cuda_alloc → cuda_launch → cuda_wait 全链路 |
| **Phase 1.5（已完成）** | 驱动/仿真物理分离；HAL 接口 11 个全部实现；`libgpu_core/` 提取并独立编译 |
| **Phase 2（已完成）** | VA Space 抽象落地；多队列（≥ 2 queues）支持；`fence_id` 异步跟踪不丢失；Ring Buffer 消费者正确处理 write/read index |
| **Phase 3（规划中）** | 网络设备、存储设备插件至少 1 个能跑通基础 ioctl |
| **Phase 4（规划中）** | v1.0 发布；ADR-011/012/013/014 至少各 1 项落地 |

### 7.2 质量门禁

| 门禁 | 通过条件 |
|------|---------|
| 编译 | `cmake --build build/` exit code = 0 |
| 单元测试 | `ctest --output-on-failure` 100% pass |
| LSP 诊断 | 所有 `drv/` 文件零 error |
| 零耦合 | `ldd build/plugins/gpu_driver/libgpu_driver_plugin.so` 无 TaskRunner 依赖 |
| 可移植性 | `drv/` 文件不违反 §6.1 规则 |
| HAL 切换 | 至少 1 个 `hal_user` + 1 个 `hal_mock` 通过测试 |

---

## 8. 术语表

| 术语 | 定义 |
|------|------|
| `drv/` | 可移植到真实 Linux 内核的驱动代码 |
| `sim/` | 仅在用户态仿真环境使用的硬件行为仿真 |
| `hal/` | 驱动访问硬件的抽象接口层 |
| `shared/` | 与 TaskRunner 共享的 canonical 接口头文件 |
| `libgpu_core/` | 纯 C、零依赖算法核心（buddy allocator、MMU events）|
| VA Space | GPU 虚拟地址空间，BO 与 Queue 的承载容器（Phase 2 引入）|
| Ring Buffer | GPU 命令队列的环形缓冲（Phase 2 完整支持）|
| Hardware Puller | 仿真 GPFIFO 命令解码的硬件状态机 |
| Global Scheduler | Puller 和计算引擎之间的任务调度层 |
| System C | 以 `GPU_IOCTL_*`（magic='G'）为标识的规范 ioctl 体系 |
| Catch2 | 项目实际使用的测试框架（vendored 单文件）|

---

## 9. 风险评估

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|---------|
| `drv/` 代码违反可移植性约束 | 中 | 移植时需重写 | CI 门禁扫描，每次 PR 自动检查 §6.1 规则 |
| HAL 接口抽象不足 | 中 | 移植时需要新增接口 | Phase 1.5 已扩到 11 个；Phase 3 视需求扩展 |
| 仿真行为与内核不一致 | 中 | 移植后驱动行为异常 | 同一份 `drv/` 代码在用户态与内核态并行跑回归 |
| TaskRunner 接口变更 | 中 | 需同步更新 `shared/` | 符号链接 + 编译期检查；canonical 头版本化 |
| 团队对"可移植"标准理解不一致 | 低 | 代码风格混乱 | §6.1 明确定义 10 条规则；AGENTS.md 编码风格对齐 |

---

**维护者**: UsrLinuxEmu Architecture Team
**最后验证**: 2026-06-16 (commit `374d463`)
**关联 SSOT**: [docs/02_architecture/post-refactor-architecture.md](02_architecture/post-refactor-architecture.md)
**对应版本**: v0.5+（post-Phase 2）
