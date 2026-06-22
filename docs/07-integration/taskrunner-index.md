# TaskRunner 协同工作文档索引

**版本**: 1.1
**日期**: 2026-05-06
**维护者**: UsrLinuxEmu Architecture Team + TaskRunner Team

---

## 一、概述

本文档索引了 UsrLinuxEmu 项目中所有与 TaskRunner 协同开发相关的文档。TaskRunner 是独立子模块（`external/TaskRunner/`），通过符号链接与 UsrLinuxEmu 交互，采用零耦合架构。

### 核心交互模式

```
TaskRunner (消费者)                    UsrLinuxEmu (驱动实现者)
      │                                       │
      │  GPU_IOCTL_* (ioctl)                  │
      │  ←─────────────────────────────────   │
      │         /dev/gpgpu0                    │
      │                                       │
      │  shared/ (符号链接)                    │
      │  ←─────────────────────────────────   │
      │  Canonical 接口定义                     │
```

### 双路径集成架构

| 路径 | 描述 | 状态 | 文档 |
|------|------|------|------|
| **System C** (零耦合) | GPU_IOCTL_* + shared/ 头文件 | 当前主路径 | ADR-015 |
| **Phase 1 兼容** | CUDA_IOCTL_* → CudaStub 转译 | 废弃中 | cuda_compat_ioctl.cpp |

---

## 二、文档分类索引

### 2.1 集成指南 (07-integration/)

| 文档 | 说明 | 维护方 |
|------|------|--------|
| [README.md](../07-integration/README.md) | **入口索引** — 面向 TaskRunner 团队的文档目录，3 篇核心文档，核心 ioctl 表，引用 Issue #5 | UsrLinuxEmu |
| [gpu-integration-guide.md](gpu-integration-guide.md) | **联调教程** — 6 步完整示例（设备验证→GET_DEVICE_INFO→ALLOC_BO→PUSHBUFFER→WAIT_FENCE→FREE_BO），131 行 C++ 示例 | UsrLinuxEmu |
| [gpu-api-reference.md](gpu-api-reference.md) | **API 手册** — 6 个 ioctl 完整规格（签名、参数、结构体、返回值），引用 Issue #3/#4/#9 | UsrLinuxEmu |
| [gpu-debug-faq.md](gpu-debug-faq.md) | **问题排查** — 7 个常见故障（设备打开失败、EFAULT、ENOMEM、EINVAL、SUBMIT 无响应、WAIT_FENCE 超时、插件未加载），引用 Issue #11 | UsrLinuxEmu |
| [h3-plan-review-feedback.md](h3-plan-review-feedback.md) | **H-3 计划审查反馈**（2026-06-22）— 对 `external/TaskRunner/plans/2026-06-19-h3-phase2-openspec-skeleton/` 的结构化审查，4 项必改 (B1-B4) + 7 项建议改 (N1-N7)，含完整修复流程 | UsrLinuxEmu |
| [h3-activation-followup.md](h3-activation-followup.md) | **H-3 激活后 Follow-up**（2026-06-22）— 11 项 review 反馈判定（10 ✅ + 1 ⚠️）+ 4 项 minor fix request（F1-F4），清理激活引入的 3 项 regression | UsrLinuxEmu |
| [taskrunner-index.md](taskrunner-index.md) | **本文档** — 全量索引，跨所有目录的 TaskRunner 相关文档 | UsrLinuxEmu |

**推荐阅读顺序**: README → gpu-integration-guide → gpu-api-reference → gpu-debug-faq

### 2.2 同步计划 (Sync Plan)

| 文档 | 说明 | 状态 |
|------|------|------|
| [sync-plan.md](../../archive/historical-plans-2026-06-15/sync-plan.md) | **主要协同契约**（已归档于 2026-06-15）— 320 行，定义同步门限法工作流（S0-S5 同步点）、Phase 0-3 实施计划（35 项任务）、沟通机制（3 天超时预警、headless 测试、Issue 跟踪）、风险缓解 | 🔄 已归档 |

**同步点定义**:

| 同步点 | 阶段 | 等待方 | 关键问题 |
|--------|------|--------|----------|
| **S0** | Phase 0 | UsrLinuxEmu | 符号链接方向确认 |
| **S1** | Phase 1 | UsrLinuxEmu | GET_DEVICE_INFO 返回哪些字段 |
| **S2** | Phase 1 | UsrLinuxEmu | ALLOC_BO domain 参数取值、handle 格式 |
| **S3** | Phase 1 | UsrLinuxEmu | PUSHBUFFER_SUBMIT_BATCH entries 格式、fence 返回位置 |
| **S4** | Phase 1 末 | 双方 | 端到端集成测试 |
| **S5** | Phase 2 | UsrLinuxEmu | TaskRunner 是否需要 VA Space/Queue 抽象 |

### 2.3 架构文档 (Architecture)

| 文档 | 说明 |
|------|------|
| [05-advanced/gpu_driver_architecture.md](../05-advanced/gpu_driver_architecture.md) | **核心架构文档** — 413 行，定义零耦合原则、shared/ 符号链接规范、ioctl 接口契约、事件回调规范、协同开发工作流、ADR-GPU-001~003 |
| [02_architecture/architecture_design.md](../02_architecture/architecture_design.md) | **架构设计** — 四层架构图，TaskRunner 作为运行时层（CmdStream/CmdProcessor/Barrier/Fence），规划 taskrunner_compat/ 目录和 test_taskrunner_integration.cpp |

**关键架构原则**:
- 零二进制耦合：仅通过 `shared/` 头文件交互，无二进制依赖
- 事件驱动：页迁移通过 PAGE_INVALIDATE/PAGE_REMAP 事件回调而非函数调用
- DRM 标准对齐：遵循 Linux 内核 DRM/GEM/TTM 接口规范
- 接口定义原则：UsrLinuxEmu（驱动实现者）定义接口，是 canonical 源

### 2.4 架构决策记录 (00_adr/)

| ADR | 标题 | 状态 | 与 TaskRunner 的关系 |
|-----|------|------|---------------------|
| **ADR-015** | [gpu-ioctl-unification](../00_adr/adr-015-gpu-ioctl-unification.md) | ✅ 已接受 | 废弃 System A/B，确立 System C (GPU_IOCTL_*) 为 canonical 接口，跨项目联合评审 |
| **ADR-016** | [gpu-memory-domain](../00_adr/adr-016-gpu-memory-domain.md) | ✅ 已接受 | 三层 Domain 模型 (VRAM/GTT/CPU)，ALLOC_BO 增加 domain 参数，TaskRunner 为评审者 |
| **ADR-017** | [gpfifo-queue-abstraction](../00_adr/adr-017-gpfifo-queue-abstraction.md) | ✅ 已接受 | GpuVaSpace 和 GpuQueue 抽象，支持多队列/优先级，TaskRunner 为评审者 |

**ADR 关联图**:
```
ADR-015 (IOCTL 统一)
    ├── ADR-016 (Memory Domain)
    └── ADR-017 (GPFIFO/Queue)
```

### 2.5 归档文档 (archive/planning/)

| 文档 | 说明 |
|------|------|
| [development_implementation_plan.md](../archive/planning/development_implementation_plan.md) | 第 7.5 子任务：TaskRunner 集成验证（verify_symlinks.sh CI 预检、端到端测试、无二进制依赖验证） |
| [ROADMAP.md](../archive/planning/ROADMAP.md) | 里程碑 3.3e：TaskRunner 集成验证（符号链接配置、CI symlink 预检、端到端测试） |

### 2.6 TaskRunner 子模块内部文档

| 文档 | 说明 | 状态 |
|------|------|------|
| `external/TaskRunner/plans/sync-plan.md` | TaskRunner 侧同步计划 — 问题映射、字段规格、S0-S5 Issue 追踪（S5 ✅ 已完成 2026-06-22）| 🟢 Active |
| `external/TaskRunner/plans/2026-06-19-h2-phase2-openspec-skeleton/` | **DEPRECATED H-2** — Oracle review 揭示 GpuDriverClient 是 dead code 后被 H-2.5 + H-3 取代，保留为 Path D 决策证据 | ⚠️ DEPRECATED |
| ~~`external/TaskRunner/plans/2026-06-19-h3-phase2-openspec-skeleton/`~~ | **H-3 (DRAFT)** — 2026-06-22 16:20 已迁移到 `openspec/changes/h3-phase2-management/`，原目录不再使用 | ✅ MIGRATED |
| `openspec/changes/h3-phase2-management/` | **H-3 (✅ ACTIVE)** — Phase 2 VA Space + Queue lifecycle，5 个 ioctl wrapper + 4 cross-cutting requirements；commit `171c97b` 激活，11 项 review 反馈已应用 10/11 + 4 项 follow-up (F1-F4) 待清理 | ✅ ACTIVE |
| `external/TaskRunner/plans/interface-unification-plan.md` | 接口统一计划 — 关键发现 K1/K4/K8、步骤、检查点 | 🔵 历史 |
| `external/TaskRunner/plans/findings.md` | UsrLinuxEmu 接口分析 — ABI 体系 A/B/C、TaskRunner 引用位置 | 🔵 历史 |
| `external/TaskRunner/plans/progress.md` | 接口统一进度跟踪 | 🔵 历史 |
| `external/TaskRunner/docs/plan.md` | TaskRunner 架构计划 — 三层解耦、库构件 | 🔵 历史 |
| `external/TaskRunner/docs/phase1-week1-plan.md` | Phase 1 周计划 — ioctl 对齐、端到端测试 | 🔵 历史 |
| `external/TaskRunner/docs/cuda-vulkan-runtime-architecture.md` | 完整 Runtime 架构设计 (625 行) | 🔵 历史 |
| `external/TaskRunner/docs/DDS-CUDA-Vulkan-Runtime-v1.2-final.md` | DDS v1.2 最终架构规范 (688 行) | 🔵 历史 |

**H-3 审查与激活**:
- 原 review feedback: [h3-plan-review-feedback.md](h3-plan-review-feedback.md)（2026-06-22 16:07）
- 激活 commit: `171c97b feat(h3): H-3 phase2-management openspec change activation`（2026-06-22 16:20，13 分钟完成 11 项修复）
- Follow-up fix request: [h3-activation-followup.md](h3-activation-followup.md)（2026-06-22 16:25+, 4 项 minor fix F1-F4 预计 15-25 分钟）

---

## 三、关键 Issue 追踪

| Issue | 说明 | 文档位置 |
|-------|------|----------|
| **#3** | ALLOC_BO 参数确认 (domain 参数取值、handle 格式) | sync-plan.md (S2), gpu-api-reference.md |
| **#4** | PUSHBUFFER_SUBMIT_BATCH 格式确认 (entries 格式、fence 返回位置) | sync-plan.md (S3), gpu-api-reference.md |
| **#5** | Phase 1 实现清单，联调指南 | 07-integration/README.md, gpu-integration-guide.md, adr-015.md |
| **#9** | GET_DEVICE_INFO 参数确认 (需要哪些 device 属性) | sync-plan.md (S1), gpu-api-reference.md |
| **#11** | VFS 单例问题（已修复），符号链接配置 | AGENTS.md, gpu_driver_architecture.md, gpu-debug-faq.md |

---

## 四、接口契约

### 4.1 Canonical 接口定义 (System C)

```
UsrLinuxEmu/plugins/gpu_driver/shared/
├── gpu_ioctl.h      # GPU_IOCTL_* 命令定义 (System C)
├── gpu_types.h      # 跨平台数据类型 (u32/u64)
├── gpu_regs.h       # GPU 寄存器偏移定义
└── gpu_events.h     # MMU_EVENT_* 事件类型定义
```

**接口定义原则**: UsrLinuxEmu（驱动实现者）定义接口，是 canonical 源；TaskRunner（消费者）使用接口。

### 4.2 符号链接结构

```
Git 子模块: external/TaskRunner (../TaskRunner)

TaskRunner/
└── UsrLinuxEmu → ../UsrLinuxEmu/    # TaskRunner 访问 UsrLinuxEmu 的入口
    └── plugins/gpu_driver/shared/    # 指向 UsrLinuxEmu 的 canonical 接口

验证脚本: tools/verify_symlinks.sh
```

### 4.3 核心 ioctl 命令 (System C)

| 命令 | 说明 | 同步点 | 优先级 |
|------|------|--------|--------|
| `GPU_IOCTL_GET_DEVICE_INFO` | 版本/能力协商 | S1 | P0 |
| `GPU_IOCTL_ALLOC_BO` | 内存分配（支持 domain 参数） | S2 | P0 |
| `GPU_IOCTL_FREE_BO` | 内存释放（防泄漏） | - | P0 |
| `GPU_IOCTL_MAP_BO` | 获取 GPU 内存地址 | - | P0 |
| `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH` | 命令提交（数据面） | S3 | P0 |
| `GPU_IOCTL_WAIT_FENCE` | fence 等待（同步） | - | P1 |

### 4.4 Phase 2 扩展接口

| 命令 | 说明 | 同步点 |
|------|------|--------|
| `GPU_IOCTL_CREATE_VA_SPACE` | 创建 VA Space | S5 |
| `GPU_IOCTL_DESTROY_VA_SPACE` | 销毁 VA Space | - |
| `GPU_IOCTL_REGISTER_GPU` | 注册 GPU 到 VA Space | S5 |
| `GPU_IOCTL_CREATE_QUEUE` | 创建队列 | S5 |
| `GPU_IOCTL_DESTROY_QUEUE` | 销毁队列 | - |
| `GPU_IOCTL_REGISTER_MMU_EVENT_CB` | MMU 事件回调注册 | - |
| `GPU_IOCTL_REGISTER_FIRMWARE_CB` | 固件回调注册 | - |

---

## 五、代码集成点

### 5.1 零耦合路径 (System C — 推荐)

| 文件 | 作用 |
|------|------|
| `plugins/gpu_driver/shared/gpu_ioctl.h` | Canonical 接口定义（GPU_IOCTL_* 命令） |
| `plugins/gpu_driver/shared/gpu_types.h` | 共享类型定义（ABI 兼容性） |
| `external/TaskRunner/include/gpu_driver_client.h` | TaskRunner 侧客户端封装（GpuDriverClient 类） |
| `external/TaskRunner/src/gpu_driver_client.cpp` | GpuDriverClient 实现 + 全局客户端管理 |
| `external/TaskRunner/CMakeLists.txt` | CMake 构建配置（symlink 断裂检测） |

### 5.2 兼容路径 (Phase 1 旧版 — 废弃中)

| 文件 | 作用 |
|------|------|
| `src/kernel/device/cuda_compat_ioctl.cpp` | 主要转译点：CUDA_IOCTL_* → taskrunner::CudaStub |
| `include/usr_linux_emu/cuda_compat_ioctl.h` | 前向声明 taskrunner::CudaStub |
| `external/TaskRunner/include/cuda_stub.hpp` | CudaStub 类定义 (namespace taskrunner) |
| `external/TaskRunner/src/cuda_stub.cpp` | CudaStub 实现 (316 行) |

### 5.3 子模块集成

| 文件 | 作用 |
|------|------|
| `.gitmodules` | Git 子模块配置（external/TaskRunner，URL ../TaskRunner） |
| `tools/verify_symlinks.sh` | CI 符号链接验证脚本 |
| `AGENTS.md` | 根级 AGENTS.md 中的 TaskRunner 集成说明 |

---

## 六、快速导航

### 6.1 首次联调

1. 阅读 [README.md](../07-integration/README.md) 了解文档结构
2. 按照 [gpu-integration-guide.md](gpu-integration-guide.md) 的"快速开始"步骤操作
3. 参考 [gpu-api-reference.md](gpu-api-reference.md) 了解详细接口规格

### 6.2 问题排查

1. 查阅 [gpu-debug-faq.md](gpu-debug-faq.md) 常见问题
2. 检查 [sync-plan.md](../../archive/historical-plans-2026-06-15/sync-plan.md) 同步点状态
3. 如遇架构问题，参考 ADR-015/016/017

### 6.3 架构理解

1. 阅读 [gpu_driver_architecture.md](../05-advanced/gpu_driver_architecture.md) 了解设计原则（第 1、3、8 章）
2. 阅读 [sync-plan.md](../../archive/historical-plans-2026-06-15/sync-plan.md) 了解协调工作流
3. 阅读 ADR-015/016/017 了解关键决策

### 6.4 完整 TaskRunner 端文档

TaskRunner 子模块内部有自己的计划文档，路径为 `external/TaskRunner/plans/` 和 `external/TaskRunner/docs/`。

---

## 七、相关项目

| 项目 | 关系 | 交互方式 |
|------|------|----------|
| **TaskRunner** | 消费者 | 通过 `GpuDriverClient` 调用 ioctl `/dev/gpgpu0`，通过符号链接访问 `shared/` |
| **UsrLinuxEmu** | 驱动实现者 | 定义 canonical 接口，实现 GPU 仿真，处理 `GPU_IOCTL_*` 命令 |

---

**最后更新**: 2026-06-22 (H-3 激活 + follow-up request 发布)
**维护者**: UsrLinuxEmu Architecture Team + TaskRunner Team