# 架构决策记录 (Architecture Decision Records)

本文档目录包含 UsrLinuxEmu 项目中所有已通过和提议中的架构决策记录。

> **最后更新**: 2026-08-20（TaskRunner TADR mirror 表更新：tadr-307 STALE + slimmed 标注 + 新增 tadr-308 行反映 Oracle A' 修订 2026-08-20；submodule pointer 待 bump）
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
| [adr-011](adr-011-multiprocess-support.md) | 多进程支持方案 | 🚫 Superseded by ADR-087 | 2026-03 |
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
| [adr-038](adr-038-network-stack-three-way-separation.md) | **网络栈 3 区分架构边界** | ✅ 已接受 (Stage 2 已交付，前置条件全部满足) | 2026-07-05 |
| [adr-039](adr-039-mem-pool-export-ioctl.md) | **MEM_POOL_EXPORT IOCTL (0x68) for cuMemPoolExportToShareableHandle** | ✅ 已接受 | 2026-07-07 |
| [adr-040](adr-040-puller-fence-completion.md) | **HardwarePullerEmu Fence Completion 回调机制**（Phase 4 前置 ADR）| ✅ Accepted | 2026-07-09 |
| [adr-041](adr-041-graph-node-to-gpfifo-serialization.md) | **Graph Node → GPFIFO Entry 序列化**（Phase 4 前置 ADR）| ✅ Accepted | 2026-07-09 |
| [adr-042](adr-042-pushbuffer-method-encoding.md) | **Pushbuffer Method 编解码格式**（Phase 5）| ✅ 已采纳 (Accepted) | 2026-07-27 |
| [adr-043](adr-043-cp-portability-boundary.md) | **命令处理器可移植性边界**（Phase 4 前置 ADR）| ✅ Accepted | 2026-07-09 |
| [adr-044](adr-044-multi-channel-hyperqueue-scheduling.md) | **多通道调度与 HyperQueue 语义**（Phase 5）| ✅ 已采纳 (Accepted) | 2026-07-27 |
| [adr-045](adr-045-priority-scheduling.md) | **优先级调度**（Phase 5.5）| ✅ Accepted | 2026-07-09 (2026-07-30 backfill) |
| [adr-046](adr-046-preemption-context-switch.md) | **抢占与上下文切换**（Phase 6）| ✅ Accepted | 2026-07-09 (2026-07-30 Accepted) |
| [adr-047](adr-047-hardware-semaphore-barrier.md) | **Hardware Semaphore & Barrier Model**（Phase 5.5）| ✅ Accepted | 2026-07-09 (2026-07-30 backfill) |
| [adr-048](adr-048-interrupt-event-model.md) | **中断与事件模型**（Phase 5）| ✅ 已采纳 (Accepted) | 2026-07-27 |
| [adr-049](adr-049-cross-engine-synchronization.md) | **跨引擎同步**（Phase 6 — Stage 4.5 实施修订 D1）| ✅ 已接受 | 2026-07-09 (rev. 2026-07-29) |
| [adr-050](adr-050-indirect-buffer-command-chaining.md) | **Indirect Buffer 命令链**（Phase 5+）| ✅ Accepted | 2026-07-09 (2026-07-30 backfill) |
| [adr-051](adr-051-predication-conditional-execution.md) | **Predication 条件执行**（Phase 6）| ✅ Accepted | 2026-07-09 (2026-07-31 Accepted) |
| [adr-052](adr-052-aql-pm4-native-support.md) | **AQL/PM4 Native 支持**（Phase 6）| ✅ Accepted (PM4 deferred to Phase 6.5) | 2026-07-09 (2026-07-31 Accepted) |
| [adr-053](adr-053-doorbell-aggregation-oversubscription.md) | **Doorbell 聚合与过订阅** | ⏸️ Deferred (Never) | 2026-07-09 |
| [adr-054](adr-054-mqd-hqd-state-management.md) | **MQD/HQD 状态管理**（Phase 5）| ✅ 已采纳 (Accepted) | 2026-07-27 |
| [adr-055](adr-055-cp-error-handling-engine-recovery.md) | **CP 错误处理与引擎恢复** | ⏸️ Deferred (Never) | 2026-07-09 |
| [adr-056](adr-056-green-context-pdl.md) | **Green Context / PDL**（Phase 7）| ✅ Accepted | 2026-07-09 (2026-08-01 Accepted) |
| [adr-057](adr-057-cp-profiling-hooks-timestamp.md) | **CP Profiling Hooks / Timestamp**（Phase 5）| ✅ 已采纳 (Accepted) | 2026-07-27 |
| [adr-058](adr-058-sim-mem-pool-real-va.md) | **sim_mem_pool Real VA Allocation via gpu_buddy + mmap Backing**（Phase 4）| ✅ Accepted | 2026-07-11 |
| [adr-059](adr-059-kfd-multi-file-integration.md) | **KFD Multi-File Integration Architecture Boundary**（C-12 sub-project, Stage 1.4 后续子项目）| ✅ Accepted | 2026-07-14 |
| [adr-060](adr-060-message-notification-threading.md) | **Linux Kernel Message Notification Threading for KFD Simulation**（C-12 前置 gate，kernel_thread_base + kernel_workqueue）| ✅ Accepted | 2026-07-14 |
| [adr-061](adr-061-hal-iommu-extension.md) | **HAL IOMMU ops 扩展**（C-12, B.3.4, hal_iommu_map/unmap）| ✅ 已接受 | 2026-07-15 |
| [adr-062](adr-062-hal-event-signal-extension.md) | **HAL Event Signal ops 扩展**（C-12, B.4.4, hal_event_signal）| ✅ 已接受 | 2026-07-15 |
| [adr-063](adr-063-sim-pfh-pm-realification.md) | **sim_pfh / sim_pm 真实化状态机边界**（C-12 Phase C.1, sim 层 3 态 + sim_proxy.h + IOTLB 桥接 + mm_shim wire-up）| ✅ 已接受 | 2026-07-15 |
| [adr-064](adr-064-memory-model-staging.md) | **GPU 内存模型保真度分阶段策略**（BO 简化堆 → Stage 4 真实 BAR + ioremap；HAL 边界强制执行规则）| ✅ 已接受 | 2026-07-20 |
| [adr-065](adr-065-version-policy.md) | **项目版本号 SSOT 与 Git Tag 命名规范**（CMake VERSION 唯一权威来源、严格 semver tag、v1.5 tag 解决）| ✅ 已接受 | 2026-07-22 |
| [adr-069](adr-069-bar-ioremap-emulation.md) | **真实 PCIe BAR + ioremap 仿真架构**（I/O 语义 vs 内存语义共存；Stage 4 前置 ADR）| ✅ Accepted | 2026-07-25 |
| [adr-072](adr-072-portability-validation.md) | **驱动代码可移植性验证框架**（L1 静态分析 + L2 内核编译测试 + L3 docs-audit）| ✅ Accepted | 2026-07-25 |
| [adr-073](adr-073-dma-coherent-emulation.md) | **DMA 一致性内存仿真架构**（独立 DMA 地址空间 + coherent/streaming 语义分离；依赖 ADR-069, ADR-064 条件 2/3）| ✅ Accepted | 2026-07-25 |
| [adr-074](adr-074-archive-tasks-md-checkbox-hygiene.md) | **Archive Tasks.md Checkbox Hygiene Policy**（归档 checkbox 可同步以反映实施状态，与"archive spec 不修改"正交）| ✅ Accepted | 2026-07-31 |
| [adr-075](adr-075-stage4-7-bclass-l2-foundation-removal.md) | **Stage 4.7 B-class L2 Foundation Removal 回顾记录**（1+N 模式：5 项移除 proposal 全部 ship + 归档；HAL 11→33→65 append-only；回顾性 ADR，不替代 ADR-023/072）| ✅ Accepted | 2026-08-07 |
| [adr-076](adr-076-gpgpu-kernel-module-ioctl.md) | **GPGPU Kernel Module IOCTL（PTX-EMU Image Executor HAL Backend）**（HAL 65→68 fn-ptrs append-only + GPU_IOCTL_LOAD/LAUNCH/UNLOAD_KERNEL_MODULE 0x27/0x28/0x29 + `hal_user.cpp` dlsym `libptxemu_device.so`；canonical source for PTX-EMU ADR-0029 §D8 跨仓协作；TaskRunner tadr-307 consumer-side 对偶；**2026-08-15 v2/v3 修订**：v2 后续演进讨论推迟，v3 退出推迟 + 创建演进路线图章节（4 项评估 (a)(b) 已确定，(c)(d) 待启动）；**2026-08-17 v3 退役声明**：Oracle session `ses_ff2106f84ffeM2oItBEa9iu4hL` 识别层次违规（HAL 承担硬件行为提供者职责），由 [ADR-090](adr-090-ptxir-via-h2d-dma.md) Supersede v2）| 🚫 **Superseded v2 by ADR-090**（**2026-08-17** — 已 ship 实施产物保留作为历史记录，退役路径见 ADR-090 §Migration）| 2026-08-13 |
| [adr-087](adr-087-multi-process-device-fabric-seam.md) | **Multi-Process Device & Fabric Seam ADR**（节点内统一 PA + L1 Switch 独立 CPU 进程 + CppLink 协议 + 4-owner 评审）| 🔄 Proposed v0.2（2026-08-14，Oracle 评审修复 F1-F11；pre-circulation）| 2026-08-14 |
| [adr-088](adr-088-dgpu-complete-simulation.md) | **dGPU 参考设计 — 完整硬件子系统仿真**（**不是仿真 NVIDIA/AMD 真实硬件**，而是作为参考设计实现 dGPU；**CppTLM 仅仿真 dGPU 板卡**（BAR MMIO + PCIe Config Space + MSI-X + 多板卡枚举 + backdoor + DMA translate cb，**23 ABI**）；**系统级硬件**（系统 IOMMU + CXL.mem）由 UsrLinuxEmu `src/system_hw/` 功能级仿真（不需时钟精确；复用 `src/kernel/iommu/` 框架）；**仿真拓扑与真硬件一致**；关键承诺：driver 代码移植到真硬件**无需任何代码改动**；linux_compat/ 为桥接层（仅 Linux 内核 API 表面 ~2500-3000 行）；实施约 **24-32 周**（CppTLM 5-7 周 ∥ UsrLinuxEmu 13-17 周可并行）；为 SVA/SVM / page-migration / CXL.mem 等高级特性打基础；**配套实施 spec 已 v4.0 重写**（2026-08-16））| ✅ Accepted（2026-08-15 Oracle 二次评审通过；2026-08-16 范围收窄修订；**配套实施 spec 已 v4.0 重写**）| 2026-08-16 |
| [adr-089](adr-089-v55-system-hw-simulation.md) | **v5.5+ src/system_hw/ 仿真范围扩展（VFIO / IOMMUFD / Live Migration / vDPA）**（基于 ADR-088 §Open Questions v5.5+ 评估；4 阶段实施：v5.5.1 VFIO 核心 6-8 周 + v5.5.2 IOMMUFD 6-8 周 + v5.5.3 vDPA 4-6 周 + v5.5.4 Live Migration 4-6 周 = **总计 20-28 周**；充分复用 `src/kernel/iommu/` 1262 行现有 IOMMU 仿真；vDPA 直接移植 `drivers/vdpa/vdpa_sim/vdpa_sim_net.c` 源码；vendor-specific 数据由真实驱动提供；HAL 68 fn-ptrs 保持不变（per ADR-023 append-only）；调研基础：[系统级硬件仿真调研报告](../05-advanced/system-hw-survey-2026-08-16.md) v0.2 + [Live Migration 独立深度报告](../05-advanced/vfio-live-migration-research.md)；**§D7 Consumer Drivers**：consumer drivers **不与 `gpu_driver` 耦合**，采用 **方案 C（Catch2 测试即 consumer）为主 + 聚合插件兜底** 路径（**三层阈值**：200-400 目标 / 500 坏味道 / 1000+ 升格；Linux 命名 + `_consumer` 后缀；KFD 是 IOMMUFD 真实用户采用方案 B — KFD 走 HAL IOMMU 路径，下层由 IOMMUFD 仿真承接）| ✅ Accepted v0.5（2026-08-16；v0.3 修复 Oracle 4 项 Blocking + 7 项 Minor；v0.4 追加 §D7 Consumer Drivers；v0.5 Oracle 复审 PASS 合入 4 Minor + 2 OQ 裁决）| 2026-08-16 |
| [adr-090](adr-090-ptxir-via-h2d-dma-v2.md) | **PTXIR Image Loading via CppTLM dGPU Board Submodule**（🚫 Supersedes ADR-090 v1 + ADR-076 v2；**§C0 Canonical 仲裁** 推翻 v1 "Supersedes ADR-076 v2" 假设，HSK-1 真相源 = PTX-EMU 仓 8 函数 ABI (CPPTLM_MODULE_VERSION 2)；**§D1 8 函数 ABI 全量采纳** — 逐字引用 PTX-EMU `cpptlm_module.h:12-52`，HAL #66 仅承载 `ptxemu_image_load` 语义，#67/#68 deprecated stub 保留；**§D3 Mode B** = submodule + CppTLM dGPU Board（PCIe 设备语义，gem5 惯例），最小完备集 `DGpuBar + Doorbell + SQ/CQ`；**§D4 废除 Mode A layered fallback**（采用冻结基线）；**§D5 HSK-6 联发协议**（PTX-EMU 发起 + CppTLM ack + UsrLinuxEmu 利益相关方 ack）；**§D6 两阶段删除流程**（freeze → Mode B E2E → physical delete）；**§E 9 周双轨 P0-P4 时间线**；ADR-088 §C2 + §D6.2 修订注记；Oracle session `ses_fef78854dffeLfDJh7p8ELuMLy` v2 决策 + 本地 file:line 验证）| ✅ **Accepted**（2026-08-18 — Gate #1/#2/#5/#6 ✅；Gate #3 / #4 / #7 由跨仓 work items 跟进：[PTX-EMU #12](https://github.com/chisuhua/PTX-EMU/issues/12) closed 等 HSK-6；[TaskRunner #10](https://github.com/chisuhua/TaskRunner/issues/10) closed 等 tadr-308；[CppTLM #19](https://github.com/chisuhua/CppTLM/issues/19) ack comment 2026-08-18 00:09:10 UTC；v1 ship 实施产物保留作为历史）| 2026-08-17 (v1) / 2026-08-18 (v2) |

> **2026-08-16 变更（ADR-089 ✅ Accepted 升档）**：ADR-089（v5.5+ src/system_hw/ 仿真范围扩展 — VFIO / IOMMUFD / Live Migration / vDPA）从 🔄 Proposed 升 ✅ Accepted。Oracle 复审 PASS（v0.4 + 4 Minor + 2 OQ 合入 v0.5）。**核心决策**：① 4 阶段实施 v5.5.1-v5.5.4 = 总计 20-28 周（UsrLinuxEmu 团队主导 14-20 周，可与 v5.5.1 部分并行）；② 23 ABI = 基础 6 + callback typedef 4 + register 1 + 板卡扩展 8 + MSI-X 3 + DMA translate 1；③ §D7 Consumer Drivers = 方案 C（Catch2 测试即 consumer）为主 + 聚合插件兜底，三层阈值（200-400 目标 / 500 坏味道 / 1000+ 升格）。**Open Questions 裁决**：OQ1 KFD↔IOMMUFD 拓扑采用方案 B（KFD 走 HAL IOMMU 路径，下层由 IOMMUFD 仿真承接，保持"零修改移植"承诺）；OQ2 **不拆分**为多个子 ADR（4 大子系统紧密耦合，单 ADR 总纲更合适）。**调研基础**：[系统级硬件仿真调研报告](../05-advanced/system-hw-survey-2026-08-16.md) v0.2 + [Live Migration 独立深度报告](../05-advanced/vfio-live-migration-research.md) 18KB。状态分布：Accepted 60→**61**，PROPOSED 6→**5**，总计 72 维持。HAL append-only 治理（ADR-023 §D4）继续生效，HAL 68 fn-ptrs 不变。**后续阶段 0 行动**：v5.5.1 VFIO kickoff + `tests/test_vfio_consumer_standalone.cpp` 作为首个 consumer 模板（per §D7 规则 1）。

> **2026-08-13 变更（ADR-076 Accepted 升档）**：ADR-076（GPGPU Kernel Module IOCTL — PTX-EMU Image Executor HAL Backend）从 🔄 Proposed 升 ✅ Accepted。HAL extension 完整实施 + Oracle APPROVED-WITH-CONDITIONS + 144/145 ctest PASS + L1 portability check PASS。状态分布：Accepted 58→**59**，PROPOSED 5→**4**，总计 71 维持。HAL append-only 治理（ADR-023 §D4）继续生效，HAL 65→68 fn-ptrs 已 ship。TaskRunner [tadr-307](../external/TaskRunner/docs/shared/adr/tadr-307-igpu-driver-kernel-module-extension.md) consumer-side 集成保持 SOFT gate 独立 track。

> **2026-08-15 变更（ADR-076 v2 修订：后续演进讨论推迟）**：ADR-076 状态维持 ✅ Accepted 不变（已 ship 实施产物：3 个 ioctl 0x27/0x28/0x29 + 3 个 HAL fn-ptr #66/#67/#68 + 144/145 ctest 全部有效）。**v2 修订仅推迟后续演进讨论**：为避免与 [ADR-088（dGPU 参考设计）](adr-088-dgpu-complete-simulation.md) 实施时出现冲突（env var 优先级 / dlopen 顺序 / C-ABI 命名空间 / HAL 静态变量共享），本 ADR 后续演进讨论统一推迟到 ADR-088 升 Accepted 之后重启（注：ADR-088 已于 2026-08-15 升 Accepted，推迟已退出，详见 ADR-076 §演进路线图）。per [ADR-088 §C2 共存关系](adr-088-dgpu-complete-simulation.md)"ADR-076 (PTX-EMU): 不被取代 — Kernel Module 仍走 PTX-EMU path"——两者共存不冲突，**演进推迟不等于撤销**。状态分布：Accepted 维持 **59**，PROPOSED 维持 **6**，总计 维持 **73**（v2 修订是同一 ADR 内部版本演进，不创建新文件）。

> **2026-08-16 跨仓实施 Spec v4.0 全面重写**：[`docs/05-advanced/cpptlm-v4-implementation-handoff.md`](../05-advanced/cpptlm-v4-implementation-handoff.md) — 给 CppTLM team 的完整实施 spec（13 章节 + 附录 A 23 ABI 完整清单）。**v4.0 重写要点**：① 删除所有 IOMMU/CXL 范围外声明（v3.0 附录 A.3 + A.4 + A.6 中 `cpptlm_iommu_domain` + `cpptlm_cxl_memdev` 已删除）；② 新增 §4.3 DMA translate callback ABI（per ADR-088 §D3.8）；③ 附录 A 按 ADR-088 §D5 重排为 23 ABI（基础 6 + callback typedef 4 + register 1 + 板卡扩展 8 + MSI-X 3 + DMA translate 1）；④ 版本字符串修正为 `"v1.0-dgpu-v0"`；⑤ 时间估算修正为 CppTLM 5-7 周（per ADR-088 §D2）。**v3.0 历史**：2026-08-15 创建时声称 31 ABI（含 IOMMU 5 + CXL 4），与 ADR-088 范围收窄冲突；v4.0 已全面修正。
>
> **2026-08-16 配套 Gap Analysis 重新定位**：[`docs/architecture/cpptlm-emu-integration-gap-analysis.md`](../architecture/cpptlm-emu-integration-gap-analysis.md) — 🚨 **已重新定位为"过时"**（不再作为实施依据）。v3.0 内容（48-72 ABI、dGPU amdgpu/nouveau 双 mode、v2 扩展 SMU/PSP/HMM 等 15 项）与 ADR-088 范围收窄冲突。v4.0 重写后顶部加 🚨 横幅 + §4.3 重写（对齐 ADR-088 §D6.1）。保留价值：§2 现状快照 + §3.11-3.25 Linux 内核深度调研有事实参考价值。

> **2026-08-15 变更（ADR-076 v3 修订：退出后续演进推迟 + 创建演进路线图）**：ADR-088 升 ✅ Accepted 触发 ADR-076 §后续演进推迟声明全部 3 项退出条件（ADR-088 升 Accepted ✅ / UsrLinuxEmu owner 启动 ✅ / 创建演进路线图章节 ✅）。**ADR-076 v3 修订**新增 ## 演进路线图 章节，列出 (a)-(d) 4 项评估项的具体决策：(a) PTX-EMU 与 CppTLM backend 共存契约澄清 = **维持**（Oracle 评审通过 4 维无冲突验证，库名/env var/C-ABI 命名空间/HAL 静态变量无冲突）；(b) HAL fn-ptr 是否需要扩展支持 CppTLM path = **不新增**（复用现有 3 个 kernel_module_* fn-ptrs，per ADR-023 §D4 append-only）；(c) TaskRunner [tadr-307](../external/TaskRunner/docs/shared/adr/tadr-307-igpu-driver-kernel-module-extension.md) 集成是否同步修订 = **待 TaskRunner owner 启动**；(d) 跨仓契约澄清文档 = **待文档化**（路径：`docs/05-advanced/adr-076-vs-088-cross-backend-contract.md`）。ADR-076 状态维持 ✅ Accepted 不变，v3 修订仅推迟退出 + 创建路线图，不涉及已 ship 实施。
>
> **2026-08-09 变更（ADR-076 跨仓协作契约）**：ADR-076 创建 — PTX-EMU ADR-0029 §D8 跨仓评审修订触发的 UsrLinuxEmu canonical ADR：3 个新 System C ioctl（0x27/0x28/0x29）+ 3 个新 HAL fn-ptrs（#66/#67/#68 append-only）+ `hal_user.cpp` dlsym `libptxemu_device.so` + 跨仓 commit 顺序协议（canonical in §Migration）。TaskRunner [tadr-307](../external/TaskRunner/docs/shared/adr/tadr-307-igpu-driver-kernel-module-extension.md) 作为 consumer-side 对偶文档。状态分布：Accepted 58，PROPOSED 4→**5**，总计 70→**71**。
>
> **2026-08-07 变更（ADR-075 回顾记录）**：ADR-075 创建 — 回顾性记录 Stage 4.7 B-class L2 Foundation 5 项移除 proposal（gpu_queue_emu / graph / hardware_puller_emu / mem_pool / stream_capture）全部 ship + 归档的事实；明确此 ADR 不替代 ADR-023（HAL 契约）+ ADR-072（可移植性验证）的治理边界。状态分布：Accepted 57→**58**，总计 69→**70**。
>
> **2026-07-14 变更（C-12 命名修复 + HAL ops ADR 创建）**：ADR-061 + ADR-062 创建 — 原 tasks.md B.3.4.5 误用 `adr-060` 编号，与 `Linux 内核消息通知线程架构` 冲突。已修正：
> - **ADR-061**（HAL IOMMU ops 扩展，237 行）：覆盖 C-12 tasks B.3.4 — `hal_iommu_map()` / `hal_iommu_unmap()` 2 个新 fn-ptr，遵循 ADR-023 Decision 4 spec-driven "追加不改" 原则
> - **ADR-062**（HAL Event Signal ops 扩展，276 行）：覆盖 C-12 tasks B.4.4 — `hal_event_signal()` 1 个新 fn-ptr，**硬依赖 ADR-060 `kernel_workqueue`** 实现 events 异步分发
> - 姊妹 ADR：ADR-061 + ADR-062 建议在 C-12 实施时**同一 commit** 同步追加 fn-ptr 到 `struct gpu_hal_ops`，但**走两个独立 ADR**（per ADR-059 D3 + ADR-035 §R3）
> - 状态分布总览已同步更新（Accepted 37→39，PROPOSED 17→15；061/062 ✅ Accepted）
> 
> **2026-07-15 变更**：ADR-061（HAL IOMMU ops 扩展）+ ADR-062（HAL Event Signal ops 扩展）状态升 ✅ Accepted。fn-ptrs 已 commit 到 `struct gpu_hal_ops`（11→14），hal_user/hal_mock stub 实现已落地。C-12 Phase A.2 hard gate CLEARED；Phase B 可启动。

> **2026-08-17 变更（ADR-090 创建 + ADR-076 退役）**：[`adr-090-ptxir-via-h2d-dma.md`](adr-090-ptxir-via-h2d-dma.md) — Oracle session `ses_ff2106f84ffeM2oItBEa9iu4hL` 识别 ADR-076 v1 层次违规（HAL 桥承担硬件行为提供者职责），提议 PTX-EMU 移入 CppTLM submodule。**核心决策**：① HAL fn-ptrs 3→1（#66 kernel_module_load 保留重定义为 VRAM write + icache invalidate，#67/#68 deprecated stub）；② ioctl 0x27 重定义（返回 vram_addr，移除 kernel_name[256] 字段），0x28 stub 化返回 -ENOSYS，0x29 保留；③ PTX-EMU 移出 UsrLinuxEmu 进程，挂 CppTLM submodule（Mode B 终态）或 sim/ translateLaunch（Mode A interim）；④ ADR-088 §C2 修订注记（取消"不被取代"条款）+ §D6.2 扩展 SM executor +2~3 ABI（走 BREAKING 流程）。**ADR-076** 状态由 ✅ Accepted v3 改为 🚫 Superseded v2 by ADR-090（已 ship 实施产物保留作为历史记录）。**状态分布**：Accepted 61→**60**（ADR-076 移出），Proposed 5→**6**（新增 ADR-090），Superseded 1→**2**（新增 ADR-076），总计 72→**73**。HAL append-only 治理（ADR-023 §D4）继续生效，HAL 68 fn-ptrs 不变（#67/#68 槽位保留作 deprecated stub）。**ADR-090 实施期间**：Mode A 解耦于 ADR-088 Phase 1，层次修复可独立推进（节省 4-6 周串行时间）。

## 状态分布总览（截至 2026-08-18）

| 状态 | 数量 | ADR 列表 |
|------|----:|----------|
| ✅ 已接受 | **61** | 001-010, 015-024, 027, 031-052, 054, 056-075, 058, **088**, **089 v0.5**, **090 v2** |
| 🔄 提议中 | **5** | 012-014, **087 v0.2**（**090** 升 Accepted）|
| 🚫 Superseded | **2** | **011** (Superseded by ADR-087), **076 v2** (Superseded by ADR-090 v2) |
| 🚫 Superseded (v1) | **1** | **090 v1** (Superseded by ADR-090 v2 — 新增) |
| **总计** | **74** | ADR-001 ~ ADR-090（含跳号 066/067/068/070/071/077~086） |

> **2026-08-18 变更（ADR-090 v2 ✅ Accepted 升档）**：[`adr-090-ptxir-via-h2d-dma-v2.md`](adr-090-ptxir-via-h2d-dma-v2.md) — ADR-090 v2 从 🔄 Proposed 升 ✅ Accepted。**v1 → v2 关键变化**（基于 Oracle session `ses_fef78854dffeLfDJh7p8ELuMLy` 本地 file:line 验证）：
> - **§C0 NEW**: Canonical 归属仲裁 — 推翻 v1 "Supersedes ADR-076 v2" 假设，HSK-1 真相源 = PTX-EMU 仓 8 函数 ABI (CPPTLM_MODULE_VERSION 2)，adr-076 改 Historical，tadr-307 标 STALE
> - **§C4 NEW**: v1 三 RFC 失败复盘 — 12 事实错误 + 3 架构否决 + 9 缺失项
> - **§D1 REWRITE**: 8 函数 ABI 全量采纳（逐字引用 `cpptlm_module.h:12-52`），HAL #66 仅承载 `ptxemu_image_load` 语义
> - **§D3 REWRITE 强化**: Mode B = submodule + CppTLM dGPU Board（PCIe 设备语义），最小完整集 `DGpuBar + Doorbell + SQ/CQ`
> - **§D4 REWRITE**: 废除 `sim/translateLaunch` layered fallback，Mode A 重定义为冻结基线
> - **§D5 NEW**: HSK-6 联发协议（**PTX-EMU 发起**，C++TLM ack，UsrLinuxEmu 利益相关方）+ G-D4 static_assert 迁移门禁
> - **§D6 NEW**: 两阶段删除流程 + 4 测试文件迁移路径
> - **§E REWRITE**: 9 周双轨 P0-P4 时间线（取代 v1 M1-M9 串行假设）
> - **§F REWRITE**: 10 项风险矩阵（新增 R1 canonical 冲突复发 + R6 submodule 滞后）
>
> **Acceptance Gate 进度**（v2 升 Accepted 治理规则 — 2026-08-18 §F.1 升级流程优化版）：
> - 必须 ✅ (UsrLinuxEmu 内部 commit gates): **#1 HAL append-only, #5 Architecture Team review, #6 Oracle 复审** → 全部 ✅
> - 必须 ✅ (主要跨仓 anchor gate): **#2 CppTLM maintainer ack** → ✅ 2026-08-18 00:09:10 UTC ([CppTLM #19](https://github.com/chisuhua/CppTLM/issues/19))
> - 可延后 (次要跨仓协作 gates, 由外部 work items 跟踪):
>   - **#3 PTX-EMU owner ack** → 🚫 RFC #12 closed, 跟踪载体 = PTX-EMU HSK-6 公告
>   - **#4 TaskRunner owner ack** → 🚫 RFC #10 closed, 跟踪载体 = tadr-308 创建
>   - **#7 Cross-repo canonical** → ⏳ 由 #3 + #4 都完成时自动触发
>
> **跨仓 commit 顺序**（per ADR-035 §R5.1）：UsrLinuxEmu ADR-090 v2 ✅ → PTX-EMU HSK-6 → CppTLM v3.0.0 → TaskRunner tadr-308。**实施依赖**：Mode A 解耦于 ADR-088 Phase 1，层次修复可独立推进（节省 4-6 周串行时间）。
>
> **状态分布**：Accepted 60→**61**（ADR-090 v2 升档），Proposed 6→**5**（ADR-090 移出），Superseded 2→**3**（新增 ADR-090 v1），总计 73→**74**。HAL append-only 治理（ADR-023 §D4）继续生效，HAL 68 fn-ptrs 不变（#66 `#66 kernel_module_load` 保留为 H2D DMA write，`#67/#68` 槽位保留作 deprecated stub 永久锁定 `-ENOSYS`）。

> **2026-08-16 变更（ADR-089 创建 — v5.5+ src/system_hw/ 仿真范围）**：[`adr-089-v55-system-hw-simulation.md`](adr-089-v55-system-hw-simulation.md) — 基于 ADR-088 §Open Questions v5.5+ 评估 + 4 个 librarian 调研综合 + 项目实地探索（`src/kernel/iommu/` 1262 行基础）。**范围**：4 阶段实施（v5.5.1 VFIO 核心 6-8 周 + v5.5.2 IOMMUFD 6-8 周 + v5.5.3 vDPA 4-6 周 + v5.5.4 Live Migration 4-6 周 = **总计 20-28 周**）。**关键设计**：① 充分复用 `src/kernel/iommu/` 现有代码（双栈并存）；② vDPA 移植 `drivers/vdpa/vdpa_sim/vdpa_sim_net.c` 完整源码；③ Live Migration vendor-specific 数据由真实驱动提供（不在 usremu 范围）；④ HAL 68 fn-ptrs 保持不变（per ADR-023 append-only）。**调研基础**：[系统级硬件仿真调研报告 v0.2](../05-advanced/system-hw-survey-2026-08-16.md) + [Live Migration 独立深度报告](../05-advanced/vfio-live-migration-research.md) 425 行 18KB。**6 个关键事实 V1-V6 已验证完成**（可信度 ⭐⭐⭐⭐ 极高）。状态分布：PROPOSED 5→**6**，总计 71→**72**。

> **2026-08-16 变更（ADR-088 单文件整合）**：用户决策——ADR-088 只保留最新版本的单一文件。早期版本文件（v3 `adr-088-cpptlm-emu-bridge.md` / v4 `adr-088-v4-dgpu-reference-design.md` / v5 `adr-088-v5-dgpu-complete-simulation.md`）**已删除**，当前权威内容为 [`adr-088-dgpu-complete-simulation.md`](adr-088-dgpu-complete-simulation.md)：**dGPU 参考设计——完整硬件子系统仿真**（CppTLM 仅仿真 dGPU 板卡，**23 ABI**；系统 IOMMU + CXL.mem 由 UsrLinuxEmu `src/system_hw/` 功能级仿真；仿真拓扑与真硬件一致；driver 移植真硬件零修改；约 24-32 周，跨团队并行）。ADR-088 于 2026-08-15 经 Oracle 二次评审（APPROVED-WITH-CONDITIONS，条件全部完成）升 ✅ Accepted；2026-08-16 经 Oracle 独立复审修订（ABI 计数/桥接层措辞/ADR-061 协调/Gate 4.5-4.7）+ 用户范围收窄修订（CppTLM 仅 dGPU 板卡）。状态分布：Superseded 3 → **1**（088 v3/v4 文件删除），总计 73 → **71**（ADR 编号不变，ADR-088 仍为单个 Accepted 条目）。

> **2026-08-09 修订（ADR-076）**：ADR-076（GPGPU Kernel Module IOCTL — PTX-EMU Image Executor HAL Backend）状态 🔄 Proposed。canonical source for PTX-EMU ADR-0029 §D8 跨仓协作；TaskRunner [tadr-307](../external/TaskRunner/docs/shared/adr/tadr-307-igpu-driver-kernel-module-extension.md) consumer-side 对偶。状态分布：PROPOSED 4→**5**，总计 70→**71**。HAL append-only 治理（ADR-023 §D4）继续生效，HAL 65→68 fn-ptrs（追加 kernel_module_load/execute/unload）。
>
> **2026-08-14 修订（ADR-088 创建）**：ADR-088（CppTLM dGPU 仿真集成）创建。Oracle session `ses_fffbcb1d5ffeZzJVDbbys5UXn2` 2026-08-14 完成架构评审：采纳 in-place `hal_user.cpp` 替换模式 + dlopen `libcpptlm_emulator.so` 集成（per ADR-076 PTX-EMU reference pattern）。roadmap 新增 **Stage 5.5**。HAL 68 fn-ptrs 保持不变（in-place 替换不修改 HAL 接口契约）。Companion gap analysis：`docs/architecture/cpptlm-emu-integration-gap-analysis.md`。Roadmap 派生 improvement：`add-cpptlm-emu-bridge-integration`。Stage 5.5 与 Stage 5（multi-engine Puller + PM4 microcode，trigger-gated by ADR-049/052）独立推进——Stage 5 关注内部 sim/* 完善，Stage 5.5 关注外部 CppTLM 接管。（早期多版本演进已整合，见上方 2026-08-16 单文件整合注记）

> **2026-08-07 修订（ADR-075）**：ADR-075（Stage 4.7 B-class L2 Foundation Removal 回顾记录）状态升 ✅ Accepted。1+N 模式：1 个回顾性 ADR + 5 个已归档移除 proposal（gpu_queue_emu / graph / hardware_puller_emu / mem_pool / stream_capture）。状态分布：Accepted 57→**58**，PROPOSED 保持 4。HAL append-only 治理（ADR-023 §D4）继续生效，HAL 64 fn-ptrs + 1 helper = 65 total callable entries 维持。
>
> **2026-08-07 修订**：ADR-038（网络栈 3 区分）从 🔄 Proposed 升 ✅ 已接受（Stage 2 已交付）；ADR-058（sim_mem_pool Real VA）从 📋 PROPOSED 升 ✅ Accepted；ADR-069（BAR/ioremap 仿真）、ADR-072（可移植性验证）、ADR-073（DMA 一致性）从 📋 PROPOSED 升 ✅ Accepted。状态分布：Accepted 55→**57**，PROPOSED 6→**4**。Stage 4 主线 ADR 全部 Accepted。
>
> **2026-08-06 修订**：6 份 HAL user wiring P0/P1/P2 改进设计 (`d875803`) + 计划 (`ed6c81a`) ship，HAL fn-ptrs 增长到 65 个（Stage 4.7 B-class L2 Phase 1+2 已 ship）。
>
> **2026-08-03 修订**：4.7 B-class L2 Foundation Phase 1 ship（fence_id + method_codec + heap inline wrappers）；4.6 closeout + 4.6 standalone test ship。
>
> **2026-08-01 修订**：ADR-056（Green Context / PDL）状态升 ✅ Accepted（Stage 4.6 实施完成）。
>
> **2026-07-31 变更**：ADR-051（Predication 条件执行）+ ADR-052（AQL/PM4 Native 支持）+ ADR-074（Archive Tasks.md Checkbox Hygiene）状态升 ✅ Accepted。由 `stage4-5-cp-phase6-predication-aql` 实施完成。状态分布：Accepted 52→54。ADR-052 PM4 parsing deferred to Phase 6.5 per ADR-052 D3。

> **2026-07-30 变更**：ADR-045（优先级调度）+ ADR-046（抢占与上下文切换）+ ADR-047（Hardware Semaphore & Barrier）+ ADR-050（Indirect Buffer 命令链）状态升 ✅ Accepted。ADR-045/047/050 由 `stage4-4-gpu-cp-phase55` 实施完成，ADR-046 由本 change `stage4-5-cp-phase6-preemption-engine-finish` 实施完成。状态分布：Accepted 48→52，PROPOSED 12→8。

> **2026-07-27 变更**：ADR-057（CP Profiling Hooks / Timestamp）状态升 ✅ 已采纳 (Accepted)。Oracle 评审后修订 4 项：D5 暴露路径决策新增（Phase 5 test backdoor via sim C-ABI，Phase 5.5 延后 ioctl 暴露）、D3 ABI 影响分析新增（gpu_gpfifo_entry 76→84 字节，方案 1 复用 _reserved/semaphore_va vs 方案 2 新增字段，选方案 2 理由）、D2 resolve 语义约束新增（单线程同步 sim，resolve 必须在 submit 返回后调用，timeout_ms 死锁逃逸）、D4 条件标注（依赖 ADR-048，同批 Accepted）。

> **2026-07-27 变更**：ADR-048（中断与事件模型）状态升 ✅ 已采纳 (Accepted)。Oracle 评审后修订 7 项：ADR-060 workqueue 集成（异步 dispatch 防死锁）、ADR-062 协调（FENCE_SIGNALED/NOTIFY_INTR 共享 hal_event_signal + workqueue 通道，无独立管线）、唤醒路径补全（interrupt_raise -> workqueue -> handler -> WaitQueue -> WAIT_FENCE 返回）、interrupt_raise 签名按 ADR-023 "追加不改" 追加 interrupt_raise_ex（旧 op deprecated）、交叉引用补全（NOTIFY_INTR->ADR-042, dispatch->ADR-060, events->ADR-062）、ENGINE_HANG 标记 reserved（依赖 ADR-055 Deferred-Never）、Out-of-scope 明确（per-vector masking / MSI-X / coalescing）。

> **2026-07-27 变更**：ADR-044（多通道调度与 HyperQueue 语义）状态升 ✅ 已采纳 (Accepted)。Oracle 评审修订：MAX_QUEUES 修正（实际 1024，非 32）、scanQueues 关系澄清（CHANNEL_SWITCH 吸收 scanQueues 用于 ioctl submitBatch 路径）、线程安全声明（ChannelManager mutex + Issue #21 snapshot 模式）、FSM 图补充 SEMAPHORE 状态、GlobalScheduler 分层关系明确。

> **2026-07-27 变更**：ADR-054（MQD/HQD 状态管理）状态升 ✅ 已采纳 (Accepted)。Oracle 评审后修订 6 项：D0 Memory Placement 新增（MQD 在 BAR2/DMA coherent pool，HQD 控制位为 BAR0 MMIO 寄存器，引用 ADR-069/073）、mqd.h 从 sim/hardware/ 迁移到 shared/（②③ 共享契约，② 已有 mqd 指针）、D3 HQD 语义修订（BAR0 writel/readl 访问，移除"指针就是 HQD"）、D4 状态转移表新增（IDLE/ACTIVE/PREEMPTED × activate/deactivate/preempt/destroy）、D5 wptr/rptr 所有权新增（② 写 wptr、③ 写 rptr，对齐 ADR-024 ring buffer 语义）、交叉引用补全（ADR-069 BAR、ADR-073 DMA coherent、ADR-044 ChannelManager 关系）。

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

    └── 内存模型保真度 (2026-07-20, ✅ Accepted)
            │
            └── adr-064 (GPU 内存模型分阶段策略)
                    └── 关联: adr-023 (HAL 边界规则修订 §Decision 5), adr-036 (3 区分)
                    └── 关联: adr-020 (libgpu_core), blueprint.md (Stage 4 BAR/ioremap)
                    └── 触发: Oracle 评审 ses_081492340ffeMYwkS4y0D6eyMt
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

**最后更新**: 2026-08-20（TaskRunner TADR mirror 表：tadr-307 STALE + slimmed 2026-08-20 + 新增 tadr-308 行 per Oracle `ses_fe0443831ffenUxpEQxZqWE8Cp` A' 修订；ADR-090 v2 §C0 验证 tadr-307 STALE 缘由；tadr-308 canonical consumer-side 对偶）

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
| [tadr-307](../external/TaskRunner/docs/shared/adr/tadr-307-igpu-driver-kernel-module-extension.md) | **IGpuDriver Kernel Module Extension**（PTX-EMU Image Executor HAL Backend 集成；**3 方法方案已 STALE**, per Oracle session `ses_ff2106f84ffeM2oItBEa9iu4hL` 识别违反 ADR-036（HAL 桥承担硬件行为提供者职责）；2026-08-20 精简为 67 行历史决策摘要 + redirect 到 [tadr-308](../external/TaskRunner/docs/shared/adr/tadr-308-igpu-driver-vram-load.md) per ADR-035 §R2.4；不得作为实施依据） | tadr-301, [adr-076](adr-076-gpgpu-kernel-module-ioctl.md) (since 🚫 Superseded by ADR-090 v2), PTX-EMU ADR-0029 §D8 (since pending amendment per ADR-090 v2 §C4) |
| [tadr-308](../external/TaskRunner/docs/shared/adr/tadr-308-igpu-driver-vram-load.md) | **IGpuDriver VRAM-Load Extension**（H2D DMA 路径；append-only 新增 1 个 IGpuDriver 方法 `load_kernel_module(image, image_size, *out_vram_addr)` 默认 `-ENOSYS`；CUmodule 重定义为 `uint64_t GPU VA`（per §Decision 1.2）；image_size 从 PTXIR 24B header (`string_table_offset + string_table_size`) 推断（per Oracle session `ses_fe0443831ffenUxpEQxZqWE8Cp` A′ 修订 2026-08-20）；kernel_name **由应用在 `cuModuleGetFunction` 传入**（不调 PTX-EMU ABI，零 UMD PTXIR 解析强制）；`cuModuleLoad` 同步改造走 VRAM-load 路径 + 独立 `next_func_id` 计数器（D6 owner 决策）；`cuModuleUnload` 走 `get_bo_gpu_va` 反查 + `free_bo` (G2 VA 翻译)；DISPATCH_KERNEL packet (T-013 G1) 携带 `vram_addr + kernel_name`；consumer-side 对偶 UsrLinuxEmu [ADR-090 v2](adr-090-ptxir-via-h2d-dma-v2.md) (canonical ✅ Accepted)；[tadr-307 STALE](../external/TaskRunner/docs/shared/adr/tadr-307-igpu-driver-kernel-module-extension.md) 替代） | tadr-301 (exception), tadr-307 (STALE), [ADR-090 v2](adr-090-ptxir-via-h2d-dma-v2.md) (canonical), [PTX-EMU ADR-0023](https://github.com/chisuhua/PTX-EMU/blob/main/docs/adr/ADR-0023-ptxir-binary-format.md) (24B header), [PTX-EMU ADR-0028](https://github.com/chisuhua/PTX-EMU/blob/main/docs/adr/ADR-0028-multi-kernel-manifest.md) (kernels[]), [PTX-EMU ADR-0029 §D1](https://github.com/chisuhua/PTX-EMU/blob/main/docs/adr/ADR-0029-ptxemu-image-executor.md#d1-新-abi-header--cpptlm_moduleh) (cpptlm_module.h 8 ABI, **§D8 待 amendment per ADR-090 v2 §C4**), CppTLM #19 |

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