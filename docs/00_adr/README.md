# 架构决策记录 (Architecture Decision Records)

本文档目录包含 UsrLinuxEmu 项目中所有已通过和提议中的架构决策记录。

> **最后更新**: 2026-07-09（GPU CP Blueprint ADRs 040-057 新增，Phase 4 sim-graph-launch-real-impl 架构基础）
> **维护者**: UsrLinuxEmu Architecture Team + TaskRunner owner
> **治理规则**: 见 [ADR-035](adr-035-governance-policy.md)

## ADR 索引

| 编号 | 标题 | 状态 | 日期 |
|------|------|------|------|
| [adr-001](adr-001-user-mode-emulation.md) | 采用用户态模拟而非内核模块 | ✅ 已接受 | 2025-12 |
| [adr-002](adr-002-cpp17-language.md) | 采用 C++17 作为开发语言 | ✅ 已接受 | 2025-12 |
| [adr-003](adr-003-plugin-architecture.md) | 采用插件化架构 | ✅ 已接受 | 2025-12 |
| [adr-004](adr-004-buddy-allocator.md) | 使用 Buddy Allocator 管理 GPU 内存 | ✅ 已接受 | 2025-12 |
| [adr-005](adr-005-ring-buffer.md) | 使用 Ring Buffer 管理 GPU 命令队列 | ✅ 已接受 | 2025-12 |
| [adr-006](adr-006-layered-architecture.md) | 采用分层架构设计 | ✅ 已接受 | 2025-12 |
| [adr-007](adr-007-cmake-build-system.md) | 使用 CMake 作为构建系统 | ✅ 已接受 | 2025-12 |
| [adr-008](adr-008-linux-api-compat.md) | 提供 Linux 内核 API 兼容层 | ✅ 已接受 | 2026-01 |
| [adr-009](adr-009-singleton-pattern.md) | 采用单例模式实现核心服务 | ✅ 已接受 | 2026-01 |
| [adr-010](adr-010-gtest-migration.md) | 测试框架选型 — Catch2（最终采用）vs GTest | ✅ 已接受 | 2026-02 |
| [adr-011](adr-011-multiprocess-support.md) | 多进程支持方案 | 🔄 提议中 | 2026-03 |
| [adr-012](adr-012-performance-optimization.md) | 性能优化策略 | 🔄 提议中 | 2026-03 |
| [adr-013](adr-013-error-handling-strategy.md) | 错误处理策略 | 🔄 提议中 | 2026-03 |
| [adr-014](adr-014-logging-enhancement.md) | 日志系统增强 | 🔄 提议中 | 2026-03 |
| [adr-015](adr-015-gpu-ioctl-unification.md) | GPU IOCTL 接口统一 | ✅ 已接受 | 2026-04 |
| [adr-016](adr-016-gpu-memory-domain.md) | GPU Memory Domain 模型 | ✅ 已接受 | 2026-04 |
| [adr-017](adr-017-gpfifo-queue-abstraction.md) | GPFIFO/Queue 抽象 | ✅ 已接受 | 2026-04 |
| [adr-018](adr-018-driver-sim-separation.md) | 驱动/仿真代码分离策略 | ✅ 已接受 | 2026-05 |
| [adr-019](adr-019-drm-gem-ttm-alignment.md) | DRM/GEM/TTM 标准接口对齐路径 | ✅ 已接受 | 2026-05 |
| [adr-020](adr-020-libgpu-core-extraction.md) | libgpu_core 算法核心提取 | ✅ 已接受 | 2026-05 |
| [adr-021](adr-021-hardware-puller.md) | Hardware Puller GPFIFO 状态机构架 | ✅ 已接受 | 2026-05 |
| [adr-023](adr-023-hal-interface.md) | 仿真层接口契约 (HAL) | ✅ 已接受 | 2026-05 |
| [adr-024](adr-024-user-mode-queue-submission.md) | 用户态队列命令提交架构 | ✅ 已接受 (Accepted) | 2026-06 |
| [adr-022](adr-022-gpu-compute-unit-emulation.md) | GPU 计算单元仿真 | ✅ 已接受 (Accepted) | 2026-06 |
| [adr-025](adr-025-phase3-placeholder.md) | Phase 3+ 议题占位 | ⏸️ 显式 Deferred (Phase 3+) | 2026-06 |
| [adr-026](adr-026-phase3-placeholder.md) | Phase 3+ 议题占位 | ⏸️ 显式 Deferred (Phase 3+) | 2026-06 |
| [adr-027](adr-027-linux-compat-strategy.md) | Linux 内核兼容层扩展策略 | ✅ 已接受 (Phase 3 触发后细化) | 2026-06 |
| [adr-028](adr-028-phase3-placeholder.md) | Phase 3+ 议题占位 | ⏸️ 显式 Deferred (Phase 3+) | 2026-06 |
| [adr-029](adr-029-phase3-placeholder.md) | Phase 3+ 议题占位 | ⏸️ 显式 Deferred (Phase 3+) | 2026-06 |
| [adr-030](adr-030-phase3-placeholder.md) | Phase 3+ 议题占位 | ⏸️ 显式 Deferred (Phase 3+) | 2026-06 |
| [adr-031](adr-031-ttm-migration-priority.md) | TTM 迁移实施优先级 | ✅ 已接受 (Accepted) | 2026-06 |
| [adr-032](adr-032-h2-5-igpu-driver-abstraction.md) | **H-2.5 IGpuDriver 抽象层** | ✅ 已接受 | 2026-06-23 |
| [adr-033](adr-033-h3-phase2-lifecycle.md) | **H-3 Phase 2 Lifecycle** | ✅ 已接受 | 2026-06-23 |
| [adr-034](adr-034-h7-deferred-registry.md) | **H-7 Deferred Registry**（3 owner-flagged upstream issues）| ✅ 已接受（先前 Deferred，H-3.6/3.7/3.8 全部修复）| 2026-06-23 |
| [adr-035](adr-035-governance-policy.md) | **Architecture Governance Policy** | ✅ 已接受 | 2026-06-23 |
| [adr-036](adr-036-three-way-separation.md) | **3 区分架构原则 (3-Way Architectural Separation)** | ✅ 已接受 | 2026-06-23 |
| [adr-037](adr-037-render-node-permissions.md) | **VFS Device Permission Model (Render Node 权限分离)** | ✅ 已接受 | 2026-07-03 |
| [adr-038](adr-038-network-stack-three-way-separation.md) | **网络栈 3 区分架构边界** | 🔄 Proposed (Stage 2 前置 ADR) | 2026-07-05 |
| [adr-039](adr-039-mem-pool-export-ioctl.md) | **MEM_POOL_EXPORT IOCTL (0x68) for cuMemPoolExportToShareableHandle** | ✅ 已接受 | 2026-07-07 |
| [adr-040](adr-040-puller-fence-completion.md) | **HardwarePullerEmu Fence Completion 回调机制**（Phase 4 前置 ADR）| ✅ Accepted | 2026-07-09 |
| [adr-041](adr-041-graph-node-to-gpfifo-serialization.md) | **Graph Node → GPFIFO Entry 序列化**（Phase 4 前置 ADR）| ✅ Accepted | 2026-07-09 |
| [adr-042](adr-042-pushbuffer-method-encoding.md) | **Pushbuffer Method 编解码格式**（Phase 5）| 📋 PROPOSED | 2026-07-09 |
| [adr-043](adr-043-cp-portability-boundary.md) | **命令处理器可移植性边界**（Phase 4 前置 ADR）| ✅ Accepted | 2026-07-09 |
| [adr-044](adr-044-multi-channel-hyperqueue-scheduling.md) | **多通道调度与 HyperQueue 语义**（Phase 5）| 📋 PROPOSED | 2026-07-09 |
| [adr-045](adr-045-priority-scheduling.md) | **优先级调度**（Phase 5.5）| 📋 PROPOSED | 2026-07-09 |
| [adr-046](adr-046-preemption-context-switch.md) | **抢占与上下文切换**（Phase 6）| 📋 PROPOSED | 2026-07-09 |
| [adr-047](adr-047-hardware-semaphore-barrier.md) | **Hardware Semaphore & Barrier Model**（Phase 5.5）| 📋 PROPOSED | 2026-07-09 |
| [adr-048](adr-048-interrupt-event-model.md) | **中断与事件模型**（Phase 5）| 📋 PROPOSED | 2026-07-09 |
| [adr-049](adr-049-cross-engine-synchronization.md) | **跨引擎同步**（Phase 6）| 📋 PROPOSED | 2026-07-09 |
| [adr-050](adr-050-indirect-buffer-command-chaining.md) | **Indirect Buffer 命令链**（Phase 5+）| 📋 PROPOSED | 2026-07-09 |
| [adr-051](adr-051-predication-conditional-execution.md) | **Predication 条件执行**（Phase 6）| 📋 PROPOSED | 2026-07-09 |
| [adr-052](adr-052-aql-pm4-native-support.md) | **AQL/PM4 Native 支持**（Phase 6）| 📋 PROPOSED | 2026-07-09 |
| [adr-053](adr-053-doorbell-aggregation-oversubscription.md) | **Doorbell 聚合与过订阅** | ⏸️ Deferred (Never) | 2026-07-09 |
| [adr-054](adr-054-mqd-hqd-state-management.md) | **MQD/HQD 状态管理**（Phase 5）| 📋 PROPOSED | 2026-07-09 |
| [adr-055](adr-055-cp-error-handling-engine-recovery.md) | **CP 错误处理与引擎恢复** | ⏸️ Deferred (Never) | 2026-07-09 |
| [adr-056](adr-056-green-context-pdl.md) | **Green Context / PDL**（Phase 7）| 📋 PROPOSED | 2026-07-09 |
| [adr-057](adr-057-cp-profiling-hooks-timestamp.md) | **CP Profiling Hooks / Timestamp**（Phase 5）| 📋 PROPOSED | 2026-07-09 |
| [adr-058](adr-058-sim-mem-pool-real-va.md) | **sim_mem_pool Real VA Allocation via gpu_buddy + mmap Backing**（Phase 4）| 📋 PROPOSED | 2026-07-11 |
| [adr-059](adr-059-kfd-multi-file-integration.md) | **KFD Multi-File Integration Architecture Boundary**（C-12 sub-project, Stage 1.4 后续子项目）| ✅ Accepted | 2026-07-14 |
| [adr-060](adr-060-message-notification-threading.md) | **Linux Kernel Message Notification Threading for KFD Simulation**（C-12 前置 gate，kernel_thread_base + kernel_workqueue）| ✅ Accepted | 2026-07-14 |
| [adr-061](adr-061-hal-iommu-extension.md) | **HAL IOMMU ops 扩展**（C-12, B.3.4, hal_iommu_map/unmap）| ✅ 已接受 | 2026-07-15 |
| [adr-062](adr-062-hal-event-signal-extension.md) | **HAL Event Signal ops 扩展**（C-12, B.4.4, hal_event_signal）| ✅ 已接受 | 2026-07-15 |

> **2026-07-14 变更（C-12 命名修复 + HAL ops ADR 创建）**：ADR-061 + ADR-062 创建 — 原 tasks.md B.3.4.5 误用 `adr-060` 编号，与 `Linux 内核消息通知线程架构` 冲突。已修正：
> - **ADR-061**（HAL IOMMU ops 扩展，237 行）：覆盖 C-12 tasks B.3.4 — `hal_iommu_map()` / `hal_iommu_unmap()` 2 个新 fn-ptr，遵循 ADR-023 Decision 4 spec-driven "追加不改" 原则
> - **ADR-062**（HAL Event Signal ops 扩展，276 行）：覆盖 C-12 tasks B.4.4 — `hal_event_signal()` 1 个新 fn-ptr，**硬依赖 ADR-060 `kernel_workqueue`** 实现 events 异步分发
> - 姊妹 ADR：ADR-061 + ADR-062 建议在 C-12 实施时**同一 commit** 同步追加 fn-ptr 到 `struct gpu_hal_ops`，但**走两个独立 ADR**（per ADR-059 D3 + ADR-035 §R3）
> - 状态分布总览已同步更新（Accepted 37→39，PROPOSED 17→15；061/062 ✅ Accepted）
> 
> **2026-07-15 变更**：ADR-061（HAL IOMMU ops 扩展）+ ADR-062（HAL Event Signal ops 扩展）状态升 ✅ Accepted。fn-ptrs 已 commit 到 `struct gpu_hal_ops`（11→14），hal_user/hal_mock stub 实现已落地。C-12 Phase A.2 hard gate CLEARED；Phase B 可启动。

## 状态分布总览（截至 2026-07-15）

| 状态 | 数量 | ADR 列表 |
|------|----:|----------|
| ✅ 已接受 | 39 | 001-010, 015-024, 027, 031-037, 039-041, 043, 059-062 |
| 📋 PROPOSED | 15 | 011-014, 038, 042, 044-052, 054, 056-058 |
| ⏸️ Deferred | 7 | 025, 026, 028-030, 053, 055 |
| **总计** | **61** | ADR-001 ~ ADR-062 |

> **2026-07-11 变更**：ADR-058 新增 — sim_mem_pool Real VA Allocation（Phase 4 cu-mempool-alloc-real-va change 架构基础）。镜像 Nvidia UVM `uvm_range_allocator` per-pool + per-device gpu_buddy + mmap backing at pool create 模式。
>
> **2026-07-11 变更**：ADR-059 新增 — KFD Multi-File Integration Architecture Boundary（C-12 sub-project）。记录 6 个新 KFD 模块（kfd_module/process/pasid/dispatch/mmu/events）的架构边界，严格遵循 ADR-036（3 区分）+ ADR-018（dr/hal/sim 分离）+ ADR-027（spec-driven）。关联文档：`docs/05-advanced/kfd-multi-file.md`（C-12 Phase A.1 设计文档）。
>
> **2026-07-11 变更（修订）**：原 ADR-060 引入 `kfd_thread_base`/`kfd_workqueue`（raw pthread_* 包装，规避 GCC 13 bug，2026-07-11 Oracle session `ses_0a20c2cc1ffeuc3KgE6isVHGtz` 10 决策点全部采纳）。**2026-07-14 修订**：rename `kfd_thread_base` → `kernel_thread_base`、`kfd_workqueue` → `kernel_workqueue`（命名对齐 ① layer；去 `kfd_` 前缀避免误读为 KFD 内部，per Oracle §CRIT-4 评审）。明确 C-12 6 模块 sync/async 边界：events 异步 + 其它 sync（mmu async opt-in）。**HardwarePullerEmu 重构明确不在本 ADR 范围**（未来单独 ADR）。
>
> **2026-07-14 变更**：ADR-059 + ADR-060 状态升 ✅ Accepted（Oracle 评审 session `ses_0a1fabadfffeJRp6kcN6p6j02S` 10 critical/risk 项全部修复 + docs-audit 43/43 PASS）。C-12 启动 gate 解锁；进入 Phase A.2 ABI 对比分析（per tasks.md §A.2 硬性 gate）。
>
> **2026-07-09 变更**：ADR-040~057 新增 — GPU 命令处理器 Blueprint ADR 集（18 文档），覆盖 Phase 4–7 CP 子系统架构决策。**ADR-040/041/043 已升级为 Accepted**（Phase 4 sim-graph-launch-real-impl 架构基础），其余 Phase 5+ 暂保持 PROPOSED。

## ADR 状态说明

| 状态 | 说明 |
|------|------|
| ✅ 已接受 | 已通过评审，正式采用的决策 |
| 🔄 提议中 | 正在评审或等待实现的决策 |
| ⏸️ 显式 Deferred | 暂不决策，等待明确触发条件后重新打开（2026-06-17 引入；详见下文"deferred policy"）|
| ⚠️ 已弃用 | 已被新决策替代的旧决策 |
| ❌ 已拒绝 | 评审后未采纳的决策 |

**详细治理规则**：见 [ADR-035](adr-035-governance-policy.md) §Rule 2 — ADR 状态标记规则（仅允许 ✅/⏸️/🔄/🚫 4 个状态）。

## ADR 格式

每个 ADR 包含以下部分：

- **状态**: 提议/已接受/已弃用/已替代
- **日期**: 决策日期
- **背景**: 决策背景和问题描述
- **决策**: 最终决定是什么
- **理由**: 为什么要这样决定
- **后果**: 决策的影响和权衡

**H-4 起标准模板**（ADR-032 ~ ADR-035 已采用）：

```
# ADR-NNN: <Title>
**状态**: ...
**日期**: YYYY-MM-DD
**提案人**: ...
**评审者**: ...
**关联 ADR**: ...
**关联 Change**: ...
## Context / Decision / Consequences / Migration
```

## ADR 关系图

```
adr-001 (用户态模拟)
    │
    ├── adr-002 (C++17)
    ├── adr-003 (插件化)
    ├── adr-006 (分层架构)
    │       │
    │       ├── adr-004 (Buddy Allocator)
    │       ├── adr-005 (Ring Buffer)
    │       └── adr-008 (Linux 兼容层)
    │
    ├── adr-007 (CMake)
    ├── adr-009 (单例模式)
    │
    └── adr-010 (GTest 迁移)
            │
            ├── adr-011 (多进程支持)
            ├── adr-012 (性能优化)
            ├── adr-013 (错误处理)
            └── adr-014 (日志系统)

    └── GPU 相关 (adr-015/016/017/018/019/020/021)
            │
            ├── adr-004 (Buddy Allocator - 内存子分配)
            ├── adr-005 (Ring Buffer - 命令队列)
            │
            ├── adr-015 (IOCTL 统一)
            │       ├── adr-016 (Memory Domain)
            │       └── adr-017 (GPFIFO/Queue)
            │
            ├── adr-018 (驱动/仿真分离)
            │       ├── adr-019 (DRM/GEM/TTM 对齐)
            │       ├── adr-020 (libgpu_core 提取)
            │       ├── adr-021 (Hardware Puller)
            │       └── adr-023 (HAL 接口契约)
            │               └── adr-036 (3 区分架构原则) ✅ Accepted
            │                       └── 关联: adr-018, adr-023, adr-035
            │
            └── (Phase 3+ 规划 — 已分流：已接受 / 显式 Deferred)
                    ├── adr-022 (GPU 计算单元仿真) ✅
                    ├── adr-031 (TTM 迁移优先级) ✅
                    ├── adr-024 (用户态队列提交) ✅
                    ├── adr-027 (Linux 兼容层扩展) ✅
                    ├── adr-025 (Phase 3+ 议题) ⏸️ Deferred
                    ├── adr-026 (Phase 3+ 议题) ⏸️ Deferred
                    ├── adr-028 (Phase 3+ 议题) ⏸️ Deferred
                    ├── adr-029 (Phase 3+ 议题) ⏸️ Deferred
                    └── adr-030 (Phase 3+ 议题) ⏸️ Deferred

    └── H-2.5 + H-3 + H-4 跨仓架构 (2026-06-23)
            │
            ├── adr-032 (H-2.5 IGpuDriver 抽象层) ✅
            │       └── 关联: adr-015 (IOCTL), adr-024 (User Mode Queue), adr-017 (GPFIFO)
            │
            ├── adr-033 (H-3 Phase 2 Lifecycle) ✅
            │       └── 关联: adr-032 (H-2.5)
            │
            ├── adr-034 (H-7 Deferred Registry) ✅ Accepted (H-3.8 complete, 3 issues resolved)
            │       └── 关联: adr-033 (H-3), adr-024 (User Mode Queue)
            │
            └── adr-035 (Architecture Governance Policy) ✅
                    └── 元决策: 规范 ADR 治理规则本身

    └── Stage 1.x 内核环境模拟 (2026-07-03, ✅ Done 3/5)
            │
            └── adr-037 (VFS Device Permission Model / Render Node 权限分离) ✅
                    └── 关联: adr-019 (DRM/GEM/TTM 对齐), adr-035 (Governance)
                            → Stage 1.2 closeout 同步接受（VFS-1~VFS-4 全实施，52/52 tests pass）

    └── GPU CP Blueprint (2026-07-09, 📋 PROPOSED)
            │
            ├── Phase 4: adr-040 (Puller Fence Completion) + adr-041 (Graph→GPFIFO) + adr-043 (CP Boundary)
            │       └── 关联: adr-021 (Hardware Puller), adr-024 (User Mode Queue), adr-036 (3-Way Separation)
            │
            ├── Phase 5: adr-042 (Method Encoding), adr-044 (HyperQueue), adr-048 (Interrupt), adr-054 (MQD/HQD), adr-057 (Profiling)
            │       └── 关联: adr-040 (Fence), adr-041 (Graph→GPFIFO), adr-043 (CP Boundary)
            │
            ├── Phase 5.5: adr-045 (Priority), adr-047 (Semaphore/Barrier)
            │       └── 关联: adr-021 (Puller FSM), adr-040 (Completion Token)
            │
            ├── Phase 5+: adr-050 (Indirect Buffer)
            │
            ├── Phase 6: adr-046 (Preemption), adr-049 (Cross-Engine Sync), adr-051 (Predication), adr-052 (AQL/PM4)
            │
            ├── Phase 7: adr-056 (Green Context/PDL)
            │
            └── Deferred (Never): adr-053 (Over-subscription), adr-055 (Error Recovery)

    └── Linux 内核消息通知线程架构 (2026-07-11, 📋 PROPOSED)
            │
            └── adr-060 (Linux Kernel Message Notification Threading for KFD Simulation)
                    │       C-12 前置 gate
                    │       └── 引入: kernel_thread_base (raw pthread_*) + kernel_workqueue (workqueue 模拟)
                    │       └── 关联: adr-035 (Governance), adr-018 (dr/sim 分离), adr-023 (HAL), adr-036 (3-Way)
                    │       └── 关联: adr-059 (KFD 多文件集成, C-12 依赖本 ADR)
                    │       └── 规避: GCC 13 + glibc pthread/sched_yield weakref bug (kfd-portability-report.md §4.2)
                    │       └── 同步 vs 异步: events 异步 + process/pasid/dispatch sync + mmu sync (async opt-in)
                    │       └── 验证: ASan+UBSan 基线 + 新增 TSan (Clang) + stress tests
                    │       └── Non-Decisions: HardwarePullerEmu 重构 / kthread/completion/fasync 模拟 / per-CPU workqueue
                    │                       → 均不在本 ADR 范围，留作未来 ADR

    └── KFD HAL ops 扩展 (2026-07-14, 📋 PROPOSED)
            │
            ├── adr-061 (HAL IOMMU ops 扩展, B.3.4)
            │       └── 追加 hal_iommu_map / hal_iommu_unmap 2 个 fn-ptr
            │       └── 路由: hal_mock → sim_pm_migrate_to_device/system;hal_user 桩 (-ENOSYS)
            │       └── 关联: adr-023 (HAL Decision 4 spec-driven 扩展), adr-018, adr-059 (D3)
            │       └── 关联: adr-060 (mmu async opt-in via kfd_mmu_get_workqueue accessor)
            │
            └── adr-062 (HAL Event Signal ops 扩展, B.4.4)
                    └── 追加 hal_event_signal 1 个 fn-ptr
                    └── 路由: hal_mock → kfd_events_thread_ (kernel_workqueue) → sim_signal_event;hal_user 桩 (-ENOSYS)
                    └── 关联: adr-023 (HAL Decision 4 spec-driven 扩展), adr-018, adr-059 (D3)
                    └── 硬依赖: adr-060 (events 异步路径必须用 kernel_workqueue)
                    └── 建议与 adr-061 同一 commit 追加 fn-ptr 到 struct gpu_hal_ops
```

## 维护指南

### 添加新 ADR

1. 在本目录创建新文件 `adr-XXX-title.md`（XXX = 当前最大编号 + 1）
2. 使用标准 ADR 格式（参见 "H-4 起标准模板"）
3. 更新本索引表 + 状态分布表
4. 在相关 ADR 中添加交叉引用
5. 关联 change 路径：`openspec/changes/<change-name>/` 或 `archive/YYYY-MM-DD-...`

### 更新现有 ADR

1. 修改对应文件
2. 更新"最后更新"日期
3. 如状态变更，在索引表与状态分布表中同步更新

### 废弃 ADR

1. 将状态改为"🚫 已拒绝"（仅在评审否决时使用，详见 ADR-035 §Rule 2）
2. 添加"被替代者"引用
3. 说明废弃原因

---

**最后更新**: 2026-07-11（ADR-058 sim_mem_pool Real VA + ADR-059 KFD 多文件集成架构边界 + ADR-060 Linux 内核消息通知线程架构；C-12 启动需 ADR-059 + ADR-060 均 Accepted）

## 编号 gap 治理（2026-06-16 → 2026-06-17）

2026-06-16 之前的 ADR 编号 022 + 025~031 一直缺失（被 `tools/docs-audit.sh` §3.1/3.2 标记为 "intentional placeholder"）。2026-06-16 本轮治理补齐了 8 份**占位骨架 ADR**，明确每份占位的"决策待定"状态和潜在候选方向。

- 022：GPU 计算单元仿真（具体 topic）
- 025/026/028/029/030：通用 Phase 3+ 占位（候选 A/B/C/D 列出）
- 027：Linux 兼容层扩展策略（具体 topic）
- 031：TTM 迁移优先级（具体 topic，承接 adr-019 §6）

### 2026-06-17 二次治理（change `cleanup-adr-placeholders`）

2026-06-17 由 OpenSpec change `cleanup-adr-placeholders` 完成第二轮治理：

- **ADR-022**：从占位升级为 ✅ v1（operator-level emulation，4 个 kernel template）
- **ADR-031**：从占位升级为 ✅ v1（TTM thin wrapper over `libgpu_core/gpu_buddy`）
- **ADR-025/026/028/029/030**：从占位转为 **⏸️ 显式 Deferred**，每份附加明确 Phase 3 触发条件
- **ADR-027**：保持 `🔄 提议中`（承接 linux_compat 规划，已迁移至 ADR-027 v1；未在本次清理范围）

### Deferred Policy（2026-06-17 引入）

**025/026/028/029/030** 标注为 `⏸️ 显式 Deferred` 而非 `🔄 提议中`：

- **区别于"永久拒绝"**：`⏸️ 显式 Deferred` 表示"暂不决策，触发条件满足后重新打开"
- **每份 ADR 必须有明确触发条件**：写在 ADR 文件的 `## Phase 3 触发条件` 段（commit 事件 / issue 编号 / 第一个用例）
- **重新打开工作流**：owner 认领后：
  1. 更新 `## 决策` 章节并把 status 改为 `✅ 已接受`
  2. 保留 `## 讨论历史 (v0 占位)` 附录（v0 候选项不删除）
  3. 同步更新本 README 的索引表与关系图
- **可自动检测**：每份 deferred ADR 的 `## Phase 3 触发条件` 段给出 `git log` / `gh issue list` 等具体检测命令

后续 Phase 3+ 启动时，**owner 认领后应直接更新对应 ADR 的"决策"章节并将 status 改为 ✅ 已接受**，而不是新建一个文件。详细的占位 → 已接受工作流见各占位 ADR 的"## 后续"段。

## H-4 governance 增量（2026-06-23）

本轮 `h4-architecture-governance-cleanup` 新增 4 个 ADR + 本 INDEX 升级：

### 新增 ADR-032 ~ ADR-035 概要

| 编号 | 标题 | 状态 | 决策来源 |
|------|------|------|---------|
| [adr-032](adr-032-h2-5-igpu-driver-abstraction.md) | H-2.5 IGpuDriver 抽象层 | ✅ Accepted | `openspec/changes/archive/2026-06-19-h2-5-architecture-foundation/design.md` §D6-D11 |
| [adr-033](adr-033-h3-phase2-lifecycle.md) | H-3 Phase 2 Lifecycle | ✅ Accepted | `openspec/changes/archive/2026-06-22-h3-phase2-management/design.md` §D1-D5 + R2 |
| [adr-034](adr-034-h7-deferred-registry.md) | H-7 Deferred Registry | ✅ Accepted（先前 Deferred，H-3.6/3.7/3.8 全部修复）| H-3 design.md §R4 + §R5（3 owner-flagged upstream issues）|
| [adr-035](adr-035-governance-policy.md) | Architecture Governance Policy | ✅ Accepted | 元决策：本 change 自身 |

### 按 Capability 分组（新增）

- **gpu-driver-architecture** capability: ADR-032 (H-2.5), ADR-033 (H-3)
- **gpu-phase2-management** capability: ADR-033 (H-3), ADR-034 (H-7 deferred)
- **architecture-governance** capability: ADR-035 (governance)

### H-4 期间的 INDEX 升级

- 状态分布表（截至 2026-06-23）：29 Accepted + 6 Deferred + 1 Proposed = 36 total
- 关系图：新增 "H-2.5 + H-3 + H-4 跨仓架构" 子树
- 维护指南：补充 "H-4 起标准模板" 段落
- 末尾新增 "H-4 governance 增量" 段

### 跨仓镜像 (submodule) — TaskRunner TADR (H-5 scope clarification)

TaskRunner 独立 ADR 体系（`TADR-NNN` 编号），与本仓 ADR-NNN 区分。H-5 起按 3 scope 分类（test-fixture 1xx / umd-evolution 2xx / shared 3xx + tadr-107）。完整索引见 [external/TaskRunner/docs/shared/adr/README.md](../external/TaskRunner/docs/shared/adr/README.md)。

#### test-fixture scope（默认主线，已接受）

| TADR | 主题 | 关联 UsrLinuxEmu ADR |
|------|------|---------------------|
| [tadr-101](../external/TaskRunner/docs/test-fixture/adr/tadr-101-stub-tracker.md) | Stub Tracker (原 tadr-004) | — |
| [tadr-102](../external/TaskRunner/docs/test-fixture/adr/tadr-102-igpu-driver.md) | IGpuDriver 抽象层 consumer-lens (H-2.5, 原 tadr-005) | [ADR-032](adr-032-h2-5-igpu-driver-abstraction.md) |
| [tadr-103](../external/TaskRunner/docs/test-fixture/adr/tadr-103-h3-phase2.md) | Phase 2 5 方法 consumer-lens (H-3, 原 tadr-006) | [ADR-033](adr-033-h3-phase2-lifecycle.md) |
| [tadr-104](../external/TaskRunner/docs/test-fixture/adr/tadr-104-r2-mapping.md) | R2 mapping contract (原 tadr-007) | [ADR-033 §R2](adr-033-h3-phase2-lifecycle.md) |
| [tadr-105](../external/TaskRunner/docs/test-fixture/adr/tadr-105-h7-deferred.md) | H-7 上游 issue TaskRunner 侧注册点 (原 tadr-008) | [ADR-034](adr-034-h7-deferred-registry.md) |
| [tadr-106](../external/TaskRunner/docs/test-fixture/adr/tadr-106-test-fixture-scope-clarification.md) | test-fixture scope 明确化 (H-5 新增) | [ADR-036](adr-036-three-way-separation.md) |
| [tadr-109](../external/TaskRunner/docs/test-fixture/adr/tadr-109-igpu-driver-uniform-scheduling.md) | IGpuDriver 31 方法扩展 + CudaScheduler 抽象泄漏修复 (H-3.5 新增) | [ADR-033](adr-033-h3-phase2-lifecycle.md) |

#### umd-evolution scope（实验性愿景，提议中）

| TADR | 主题 | 关联 UsrLinuxEmu ADR |
|------|------|---------------------|
| [tadr-201](../external/TaskRunner/docs/umd-evolution/adr/tadr-201-unified-scheduler.md) | CUDA/Vulkan 统一调度器 (原 tadr-001, 🔄 Proposed) | — |
| [tadr-202](../external/TaskRunner/docs/umd-evolution/adr/tadr-202-layered-design.md) | CUDA/Vulkan 分层设计 (原 tadr-002, 🔄 Proposed) | — |
| [tadr-203](../external/TaskRunner/docs/umd-evolution/adr/tadr-203-sync-unified.md) | CUDA/Vulkan 同步统一 (原 tadr-003, 🔄 Proposed) | — |
| [tadr-204](../external/TaskRunner/docs/umd-evolution/adr/tadr-204-umd-evolution-scope-clarification.md) | umd-evolution scope 明确化 (H-5 新增) | [ADR-036](adr-036-three-way-separation.md) |
| [tadr-205](../external/TaskRunner/docs/umd-evolution/adr/tadr-205-umd-evolution-poc-roadmap.md) | UMD PoC 路线图 (H-5 新增, deferred Phase D) | — |

#### shared scope（跨切面契约，已接受）

| TADR | 主题 | 关联 UsrLinuxEmu ADR |
|------|------|---------------------|
| [tadr-107](../external/TaskRunner/docs/shared/adr/tadr-107-shared-infrastructure-boundary.md) | shared 边界 (H-5 新增) | [ADR-036](adr-036-three-way-separation.md) |
| [tadr-108](../external/TaskRunner/docs/shared/adr/tadr-108-build-mode-selection.md) | build mode selection (H-5.1 新增, `TASKRUNNER_BUILD_MODE` option) — **SUPERSEDED 2026-07-09** by [build-default-on](../external/TaskRunner/openspec/changes/umd-evolution-build-default-on/) (default flipped: test-fixture → umd-evolution) | [ADR-035](adr-035-governance-policy.md), [ADR-036](adr-036-three-way-separation.md) |
| [tadr-301](../external/TaskRunner/docs/shared/adr/tadr-301-igpu-driver-contract.md) | IGpuDriver 28→47 方法契约 (H-5 新增, H-3.5 + Phase 3 + Phase 4 扩展, 2026-07-07 PR #7, Phase 4 tadr-305) | [ADR-032](adr-032-h2-5-igpu-driver-abstraction.md), tadr-305 |
| [tadr-302](../external/TaskRunner/docs/shared/adr/tadr-302-sync-primitives.md) | Sync Primitives 抽象 (H-5 新增) | — |
| [tadr-303](../external/TaskRunner/docs/shared/adr/tadr-303-error-handling.md) | Error Handling 基础 (H-5 新增, Result\<T\> + ErrorCode) | — |
| [tadr-304](../external/TaskRunner/docs/shared/adr/tadr-304-error-handling-strategy.md) | Error Handling 策略层 (H-5.1 新增, Linux errno 语义, 扩展自 tadr-303) | tadr-303 |
| [tadr-305](../external/TaskRunner/docs/shared/adr/tadr-305-mempool-export-shareable.md) | IGpuDriver::memPoolExportShareable 契约 (Phase 4 新增 47 方法) | tadr-301, [ADR-039](adr-039-mem-pool-export-ioctl.md) |

#### 向后兼容 redirect 文件（DEPRECATED）

8 个原 TADR 编号保留为 redirect 文件（指向新路径）：`tadr-001` ~ `tadr-008` → `tadr-201/202/203` + `tadr-101/102/103/104/105`。位于各 scope 的 `adr/tadr-NNN-redirect.md`。

**维护政策**：本表是 canonical，TaskRunner `docs/shared/adr/README.md` §索引 是 mirror。改动时先改本表，TaskRunner 端同步更新。同步协议遵循 ADR-035 §Rule 5.1 4 步流程。

**Dual-track 分类原则**（H-5）：
- **test-fixture** (1xx): 当前已接受，默认主线
- **umd-evolution** (2xx): 实验性愿景，提议中，延后至 Phase D
- **shared** (3xx + tadr-107): 跨切面抽象，dual review 必需

变更 shared scope 任何文件须至少 1 名 test-fixture scope 维护者 + 1 名 umd-evolution scope 维护者（或其指定人）共同 review。涉及 ABI 契约变更（如 `igpu_driver.hpp`）须同步通知 UsrLinuxEmu 维护者（ADR-036 跨仓策略）。

### 跨引用规范

在 `docs/` 其他文档或 openspec change 中引用 ADR：

```markdown
详见 [ADR-032](../00_adr/adr-032-h2-5-igpu-driver-abstraction.md) §Decision。
```

```markdown
**关联 Source**: openspec/changes/archive/<source-change>/design.md §X
```