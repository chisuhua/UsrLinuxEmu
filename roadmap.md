# UsrLinuxEmu 架构演进路线图

> **项目目标**: **开发一个易移植到 Linux 内核的 GPU 驱动**。
>
> 本路线图描述**从当前 MVP 到实现此目标的演进路径**（4 阶段 + 蓝图）。

> **性质**: 架构层叙事，描述从当前 MVP 到终态蓝图的演进路径
> **不绑定**: 本路线图不引用具体 OpenSpec change 编号。后续 OpenSpec change 根据本路线图派生
> **同步关系**: 与 `docs/sync-plan.md` 互补（sync-plan 负责跨仓同步点，本路线图负责架构演进阶段）
> **最后更新**: 2026-09-08（v0.2.1 状态同步：阶段关系图 5.5.1 / 5.5.2 升 ✅ 已归档、5.5.3 标 🔄 进行中；新增「Stage 5 与 5.5 并行」说明；详见 [pcie-bus-bridge-roadmap.md §修订记录 v0.2.1](docs/roadmap/pcie-bus-bridge-roadmap.md)）
> **维护者**: UsrLinuxEmu Architecture Team

---

## 架构原则：3 区分 + HAL 桥

UsrLinuxEmu 的所有工作围绕三个清晰分离的层面 + 一个桥接适配器：

| 编号 | 层次 | 路径 | 职责 |
|------|------|------|------|
| ① | Linux 内核环境模拟 | `src/kernel/`, `include/kernel/`, `include/linux_compat/` | 提供 Linux 内核 API（VFS, 调度, IOMMU, mmu_notifier, DRM, PCIe, 中断）|
| ② | 可移植的驱动代码实现 | `plugins/gpu_driver/drv/` | GPGPU 驱动逻辑（KFD 风格），用真实 Linux 内核 API 写，可编译进真实内核模块 |
| ③ | 硬件模拟 | `plugins/gpu_driver/sim/` | 模拟真实 GPU 硬件（pushbuffer, 调度器, 寄存器, fence, 中断）|
| HAL | **桥（bridge）** | `plugins/gpu_driver/hal/` | **71 个函数指针**（65 基础演进 + 3 ADR-090 PTX-EMU + 3 ADR-092 adapter），append-only per [ADR-023 §D4](docs/00_adr/adr-023-hal-interface.md)；② 与 ③ 之间的依赖反向注入点 |

**HAL 不是第 4 层**，HAL 是 ② 调 ③ 的桥接适配器。UsrLinuxEmu 通过 `hal_mock.cpp` 注入 sim，真机通过 `hal_user.cpp` 注入真实硬件。驱动代码本身零修改即可切换环境。

**完整原则**: 见 [ADR-036](docs/00_adr/adr-036-three-way-separation.md) ✅ Accepted
**当前 SSOT 实现**: 见 [core-architecture.md §1.10](docs/02_architecture/core-architecture.md)

---

## 阶段总览

| 阶段 | 状态 | 目标 | 文件 |
|------|------|------|------|
| **阶段 0** | ✅ 已达成 | MVP，单一 GPGPU 设备可验证 | [docs/roadmap/stage-0-mvp.md](docs/roadmap/stage-0-mvp.md) |
| **阶段 1** | ✅ 已达成 (2026-07-16) | Linux 内核环境模拟（DRM + UVM + IOMMU + ATS + PCIe BAR/中断）；C-12 KFD 多文件集成 81% 完成 + L1↔L2 bridge skeleton | [docs/roadmap/stage-1-kernel-emu.md](docs/roadmap/stage-1-kernel-emu.md) |
| **阶段 2** | ✅ 已达成 (2026-07-05) | 多设备插件化（网络 + 存储）| [docs/roadmap/stage-2-multi-device.md](docs/roadmap/stage-2-multi-device.md) |
| **阶段 3** | ✅ 已达成 (2026-07-23) | v1.0 稳定（CUDA E2E ✅、sanitizer ✅、bridge ✅、perf ✅、errno 审计 ✅、文档 ✅、CI ubuntu ✅、Release ✅）| [docs/roadmap/stage-3-v1.0.md](docs/roadmap/stage-3-v1.0.md) |
| **阶段 4** | ✅ 已完成（4.1-4.7.2 全部 ship + 归档，2026-07-26 ~ 2026-08-05）| 真实 BAR + ioremap 模拟 + GPU CP Phase 4-7 完整化 + B-class L2 违规清理；HAL 11 → 33 → 65 fn-ptrs (append-only per ADR-023 §D4)；5 个 removal 已 ship（drv/ 不再 #include sim/* headers）| [docs/roadmap/stage-4-bar-ioremap.md](docs/roadmap/stage-4-bar-ioremap.md) |
| **阶段 5** | 📋 规划中（trigger-gated） | 真实多引擎 Puller + PM4 microcode 解析 + 4.6 closeout follow-up；triggered by ADR-049 Phase 6+ / ADR-052 Phase 6.5 条件（详见各 ADR）| [docs/roadmap/stage-5-multi-engine-pm4.md](docs/roadmap/stage-5-multi-engine-pm4.md)（占位，待 trigger 启动）|
| **阶段 5.5** | ✅ Accepted (2026-08-15) | **CppTLM dGPU 参考设计集成 + PCIe 子系统仿真** — 通过 dlopen `libcpptlm_emulator.so` 把 dGPU 板卡仿真（**23 个 C ABI**：BAR MMIO + PCIe Config Space + MSI-X + 多板卡枚举 + backdoor + DMA translate cb）委托给 CppTLM；系统 IOMMU + CXL.mem 由 UsrLinuxEmu `sim_hardware/` 功能级仿真；dGPU-first；drv/ 零修改；**细化为 5 个子阶段 5.5.1-5.5.5**（4 象限重构 → sim_hardware 基础 → SR-IOV/Link/Completion → PHY/AXI → VFIO + 真机一致性；36-54 周）| [docs/roadmap/pcie-bus-bridge-roadmap.md](docs/roadmap/pcie-bus-bridge-roadmap.md)（路径图）+ [docs/00_adr/adr-088-dgpu-complete-simulation.md](docs/00_adr/adr-088-dgpu-complete-simulation.md) + [ADR-091](docs/00_adr/adr-091-pci-driver-architecture-and-four-quadrant.md) |
| **阶段 5.5.6+** | 📋 规划中（延续编号） | **GPU PF 驱动虚拟化扩展轨道** — PF 6 大责任（基础 PCI / SR-IOV Core / 硬件资源调度 / vGPU 扩展 / Live Migration / 宿主机 I/O + GSP 协同）+ 4 阶段开发路径（46-72 周）；编号延续 pcie-bus-bridge-roadmap（5.5.5 之后）| [docs/02_architecture/gpu-pf-driver-virtualization.md](docs/02_architecture/gpu-pf-driver-virtualization.md) |
| **阶段 6+** | 📋 蓝图后扩展轨道（未编号） | **蓝图后扩展** — mdev + Trap-and-Emulate + GPU 专属状态快照（Live Migration 完整化）；根 roadmap 仅定义到 5.5.6+，Stage 6+ 属蓝图后未编号轨道 | [docs/02_architecture/gpu-pf-driver-virtualization.md](docs/02_architecture/gpu-pf-driver-virtualization.md) §3.1 |
| **终态蓝图** | 📋 愿景 | 3 区分成熟形态，可移植驱动可在真实 Linux 内核中编译运行 | [docs/roadmap/blueprint.md](docs/roadmap/blueprint.md) |

---

## 阶段关系图

```
阶段 0 (MVP, 已达成)
   ↓
阶段 1 (Linux 内核环境模拟，5 子阶段)
   ├── 1.0 PCIe 设备模拟
   ├── 1.1 IOMMU + ATS
   ├── 1.2 DRM 子集（drm_device 风格重构）
   ├── 1.3 UVM/HMM
   └── 1.4 集成验证（KFD 编译跑通）
   ↓
阶段 2 (多设备插件化)
   ↓
阶段 3 (v1.0 稳定)
   ↓
阶段 4 (真实 BAR + ioremap + GPU CP 完整化 + B-class L2 Phase 2)
   ├── 4.1 BAR + ioremap (✅ 2026-07-26)
   ├── 4.2-4.6 GPU CP Phase 4-7 (✅ 2026-07-27 ~ 08-01)
   └── 4.7 B-class L2 Phase 2:
       ├── 4.7.1 Phase 1+2 foundation (✅ 2026-08-03~04, HAL 11→33→65)
       └── 4.7.2 5 个 removal changes (✅ 2026-08-04~05 全部 ship + 归档)
   ↓
阶段 5 (multi-engine Puller + PM4 microcode + 4.6 closeout follow-up；trigger-gated)
   ↘
阶段 5.5 (CppTLM dGPU 参考设计集成 + PCIe 子系统仿真)
   ├── 5.5.1 4 象限重构 (✅ 已归档, 4-6 周)
   ├── 5.5.2 sim_hardware 基础 + Tier 1+2 (✅ 已归档, 6-8 周)
   ├── 5.5.3 Tier 3+5+6 (SR-IOV / Link / Completion, 10-16 周, 🔄 进行中)
   ├── 5.5.4 Tier 4+7 (PHY / AXI, 8-12 周, 📋 待启动)
   └── 5.5.5 Tier 8 + VFIO + 真机一致性 (8-12 周, 📋 待启动)
       ↓
   5.5.6+ (GPU PF 驱动虚拟化扩展轨道 — 延续编号)
   ├── 阶段 1: 基础 PCI 设备管理
   ├── 阶段 2: SR-IOV Core 管理
   ├── 阶段 3: 虚拟化扩展（vGPU 暴露）──────────→ 6+ (蓝图后扩展轨道)
   └── 阶段 4: 状态保存/恢复（Live Migration）──→ mdev + Trap-and-Emulate + GPU 快照
       ↓
终态蓝图（3 区分成熟形态）
```

> **并行关系说明**：**Stage 5 与 Stage 5.5 并行推进** — Stage 5 由 ADR-049/052 trigger-gated，Stage 5.5 由 ADR-091（v0.2 Accepted 2026-09-03）独立驱动，**5.5 不依赖 Stage 5 trigger**。当前 5.5.1/5.5.2 已 ship + 归档，5.5.3 前置 backlog 已 ship；详见 [pcie-bus-bridge-roadmap.md](docs/roadmap/pcie-bus-bridge-roadmap.md) §修订记录 v0.2.1。

> **编号说明**（per [gpu-pf-driver-virtualization.md §3.1](docs/02_architecture/gpu-pf-driver-virtualization.md)）：
> - **Stage 5.5.1-5.5.5** = pcie-bus-bridge-roadmap.md 覆盖（PCIe 子系统仿真，5 个 Stage）
> - **Stage 5.5.6+** = GPU PF 驱动虚拟化扩展轨道延续编号（虚拟化扩展文档）
> - **Stage 6+** = 蓝图后扩展轨道（根 roadmap 仅到 5.5.6+，Stage 6+ 未定义编号）

---

## 派生建议

> 基于 ADR trigger conditions 列出可派发的 candidate improvements。读者读 roadmap 时可一眼看到"哪些 ADR 触发后可派发哪些 improvement"。具体派发由 `add-improve` skill 引导。

| ADR | Trigger 条件 | 候选 improvement | 状态 |
|-----|--------------|------------------|------|
| [ADR-049](docs/00_adr/adr-049-cross-engine-synchronization.md) | Phase 6+ multi-engine Puller 真实并行 | add-multi-engine-puller-real-parallel | ⏸️ trigger 未满足 |
| [ADR-052](docs/00_adr/adr-052-aql-pm4-native-support.md) | Phase 6.5 PM4 microcode 解析完整实现 | implement-pm4-microcode-full | ⏸️ trigger 未满足 |
| [ADR-023](docs/00_adr/adr-023-hal-interface.md) §D4 | 4.7.3 spec 同步：65 fn-ptrs 列入 ADR 表格（取代旧 46 数字） | sync-adr-023-hal-fnp-tr-table | ✅ 可派发（Stage 4 follow-up） |
| — | L2 残余 `sim/sim_event.h` 清理（独立 proposal，不在 Stage 4 范围） | cleanup-sim-event-h-l2-residual | ✅ 可派发（独立 proposal） |
| [ADR-076](docs/00_adr/adr-076-gpgpu-kernel-module-ioctl.md) | PTX-EMU HARD gate ✅ CLEARED（`libptxemu_device.so` + `cpptlm_module.h` shipped + tag v0.1.0 发布，2026-08-13 audit）；HAL 65 → 68 append-only + 3 ioctls (0x27/0x28/0x29) + `hal_user.cpp` dlsym PTX-EMU | add-ptxemu-kernel-module-hal-extension | ✅ **可派发**（PTX-EMU gate 满足；TaskRunner tadr-307 SOFT gate 独立推进） |
| [ADR-088](docs/00_adr/adr-088-dgpu-complete-simulation.md) | dGPU 参考设计 — 完整硬件子系统仿真（CppTLM 仅仿真 dGPU 板卡，**23 个 C ABI**；系统 IOMMU + CXL.mem 由 `sim_hardware/` 功能级仿真；仿真拓扑与真硬件一致）；HAL in-place 替换模式；drv/ 零修改；5 阶段（**约 24-32 周**）| add-cpptlm-emu-bridge-integration | ✅ **Accepted**（2026-08-15 Oracle 二次评审通过；2026-08-16 范围收窄：CppTLM 仅 dGPU 板卡） |

## 跨仓评审中 ADRs

> 待 owner + consumer 双评审的 cross-repo consumer-side ADR 临时占位段。ADR 升 Accepted 后：从本段移除 + 加入对应 stage-X doc 4 象限"对应 ADRs"表 + 创建 improvement + 加入 proposal-suggestions.md。

| ADR | 标题 | 状态 | 评审方 | 链接 |
|-----|------|------|--------|------|
| adr-076 | GPGPU Kernel Module IOCTL（HAL Extension for PTX-EMU Image Executor 集成） | 🔄 Proposed（**PTX-EMU HARD gate ✅ cleared 2026-08-13**；TaskRunner tadr-307 SOFT gate pending — 不阻塞 UsrLinuxEmu HAL extension） | UsrLinuxEmu owner ✅ PTX-EMU owner ✅ + TaskRunner owner ⏳ | [adr-076](docs/00_adr/adr-076-gpgpu-kernel-module-ioctl.md) |

> **2026-08-13 更新**：经 arch-side 审计（详见 [docs/architecture/adr-076-ptxemu-hal-backend-gap-analysis.md](docs/architecture/adr-076-ptxemu-hal-backend-gap-analysis.md)），PTX-EMU 仓 `libptxemu_device.so` + `cpptlm_module.h` 已 ship + tag `v0.1.0` 已发布 + Phase 0+1 全部 5+3 gates PASS。**PTX-EMU HARD gate CLEARED**。TaskRunner tadr-307 (PROPOSED) SOFT gate 不阻塞 UsrLinuxEmu HAL extension 启动。已加入"派生建议"表，标记 ✅ 可派发。

---

## Stage 5 触发条件（占位文档）

Stage 5 仅在 ADR-049 / ADR-052 的 Phase 6+ / Phase 6.5 触发条件满足时启动。当前各 deferred 触发详见：

- [ADR-049 §"Phase 6+ 触发条件"](docs/00_adr/adr-049-cross-engine-synchronization.md) — multi-engine Puller 真实并行（COMPUTE+COPY+GRAPHICS）
- [ADR-052 §"Phase 6.5 触发条件"](docs/00_adr/adr-052-aql-pm4-native-support.md) — PM4 microcode 解析完整实现
- [stage-4 §"Stage 4 整体验收"](docs/roadmap/stage-4-bar-ioremap.md) — ② 可移植性 (Linux 6.12 LTS L2) + BAR 性能 ≤20% 回归 CI gating
- ~~[stage-4-6 archive tasks 1.6/1.7/2.4/3.5/4.6/7.6/8.x/9.1](openspec/changes/archive/2026-08-01-stage4-6-cp-phase7-green-context-pdl/tasks.md)~~ — **已交付**：`2026-08-03-stage4-6-green-context-pdl-closeout`（commit `43973ce`）+ `2026-08-03-stage4-6-green-context-pdl-tests-standalone`（commit `2eb86f1`）+ 6 份 HAL user wiring（design `d875803` → plan `ed6c81a` → 归档 `78fbb8d`）

---

## 阅读顺序

1. **本文**，先了解整体路线
2. **[docs/roadmap/stage-0-mvp.md](docs/roadmap/stage-0-mvp.md)**，查看当前 MVP 状态（已完成）
3. **[docs/roadmap/stage-3-v1.0.md](docs/roadmap/stage-3-v1.0.md)**，下一步核心工作（v1.0 稳定进行中）
4. **[docs/roadmap/stage-2-multi-device.md](docs/roadmap/stage-2-multi-device.md)** + **[docs/roadmap/stage-3-v1.0.md](docs/roadmap/stage-3-v1.0.md)**，后续规划
5. **[docs/roadmap/stage-4-bar-ioremap.md](docs/roadmap/stage-4-bar-ioremap.md)**，长期演进（真实 BAR + GPU CP 完整化）
6. **[docs/roadmap/pcie-bus-bridge-roadmap.md](docs/roadmap/pcie-bus-bridge-roadmap.md)**，阶段 5.5.1-5.5.5 PCIe 子系统仿真路径
7. **[docs/02_architecture/gpu-pf-driver-virtualization.md](docs/02_architecture/gpu-pf-driver-virtualization.md)**，5.5.6+ GPU PF 虚拟化扩展轨道
8. **[docs/roadmap/driver-stack-flow-roadmap.md](docs/roadmap/driver-stack-flow-roadmap.md)**，Stage 5.5.2 驱动栈图谱修订路径（P0-P4）
9. **[docs/roadmap/blueprint.md](docs/roadmap/blueprint.md)**，终态愿景

---

## 跨引用

- [ADR-036](docs/00_adr/adr-036-three-way-separation.md), 3 区分架构原则
- [SSOT §1.10](docs/02_architecture/core-architecture.md), 3 区分的当前实现
- [ADR-035](docs/00_adr/adr-035-governance-policy.md), 治理规则（ADR/变更/SSOT 维护）
- [sync-plan.md](docs/sync-plan.md), 跨仓同步点（互补关系）
- [pcie-bus-bridge-roadmap.md](docs/roadmap/pcie-bus-bridge-roadmap.md), Stage 5.5.1-5.5.5 路径图（PCIe 子系统仿真）
- [driver-stack-flow-roadmap.md](docs/roadmap/driver-stack-flow-roadmap.md), Stage 5.5.2 驱动栈图谱修订路径（P0-P4）
- [gpu-pf-driver-virtualization.md](docs/02_architecture/gpu-pf-driver-virtualization.md), 5.5.6+ GPU PF 虚拟化扩展轨道

---

## 当前活跃 Changes

> **来源**: [openspec/changes/INDEX.md](openspec/changes/INDEX.md)
> **状态**: 截至 2026-08-10 — **1 个活跃 change**（self-referential roadmap 改造）+ 92+ 个已完成/已归档

| Change | 优先级 | 规模 | 当前进度 | 描述 |
|--------|--------|------|---------|------|
| [2026-08-10-roadmap-unified-index](openspec/changes/2026-08-10-roadmap-unified-index/) | P1 | 中 | 实施中 | Roadmap Unified Index 升级 + 4 inconsistencies 修复（self-referential rdd-workflow 示范） |

### 已归档 OpenSpec changes（最近 5 个）

| Change | 归档日期 | 主题 |
|--------|----------|------|
| [2026-08-08-implement-pm4-microcode-parsing](openspec/changes/archive/2026-08-08-implement-pm4-microcode-parsing/) | 2026-08-08 | PM4 microcode 解析（替换 FORMAT_PM4 stub） |
| [2026-08-08-implement-multiprocess-phase1-isolation](openspec/changes/archive/2026-08-08-implement-multiprocess-phase1-isolation/) | 2026-08-08 | 多进程隔离 Phase 1（registry + context skeleton） |
| [2026-08-08-complete-msi-x-vector-routing](openspec/changes/archive/2026-08-08-complete-msi-x-vector-routing/) | 2026-08-08 | MSI-X vector routing（per-vector handler dispatch） |
| [2026-08-08-complete-mmu-notifier-callback](openspec/changes/archive/2026-08-08-complete-mmu-notifier-callback/) | 2026-08-08 | mmu_notifier callback 集成（per-domain notifier list） |
| [2026-08-08-complete-event-page-writeback](openspec/changes/archive/2026-08-08-complete-event-page-writeback/) | 2026-08-08 | event page writeback（per-process 4KB event pages） |

> 完整归档列表：见 [openspec/changes/archive/](openspec/changes/archive/)。重新生成 top-5：`for d in $(ls openspec/changes/archive/); do echo "$(git log -1 --format=%cs -- openspec/changes/archive/$d) $d"; done | sort -r | head -5`

### 近期里程碑（2026-07）

| 日期 | 事件 |
|------|------|
| 07-05 | Stage 2 达成 — 多设备插件化 76/76 ctest |
| 07-16 | C-12 KFD 多文件集成归档 — 81% 原子任务, 104/104 ctest |
| 07-16 | three-sanitizer-infra + kfd-l1-l2-bridge-e2e 创建 (C-12 follow-up) |
| 07-17 | ASan/UBSan/TSan 三 sanitizer 落地, CI matrix unified |
| **07-20** | **CUDA E2E real-path 全部 6 Phase ✅** — BO 真实内存 + Puller MEMCPY HAL + fence 异步 + E2E 测试 |
| 07-20 | ADR-064 内存模型分阶段策略 + ADR-023 HAL 边界强制执行规则 |
| 07-20 | 回归: docs-audit 43/43 + 104/104 + 14/14 ctest PASS |
| **07-21** | **three-sanitizer-infra ✅** — 34/34 tasks 归档确认；**kfd-l1-l2-bridge-e2e ✅** — 双仓归档确认 |
| 07-21 | Arch-handoff + roadmap 数据同步：确认 stage3-2 ✅ (PR #30, 2026-07-11) |
| **07-21** | **stage3-3-errno-coverage-audit ✅** — 12 处 Linux errno 修复 + 5 测试文件 + 105 ctest PASS
| **07-25** | **ADR-069 BAR/ioremap + ADR-072 可移植性验证 + ADR-073 DMA 一致性提案** — Stage 4 前置架构决策集 |
| **07-30** | **stage4-5-cp-phase6-preemption-engine-finish ✅** — Preemption 引擎核心收尾：MQD state save/restore wiring、pending fence 表、Puller FSM preempt checkpoint、SEM_WAIT 挂起态保存/恢复、17 个 standalone 测试、ADR-045/046/047/050 状态升 Accepted |
| **07-31** | **stage4-5-cp-phase6-predication-aql ✅** - Predication + AQL/PM4 (ADR-051 + ADR-052 Accepted). Predicate register + SET_PREDICATE entry + DECODE skip + preempt persistence (ChannelState snapshot). AQL packet parsing + completion_signal -> Timeline Semaphore bridge. PM4 parsing deferred to Phase 6.5. |

**Preemption spec addendum**: [`openspec/changes/stage4-5-cp-phase6-preemption-timeline-sem-gaps/specs/preemption-spec-correction/spec.md`](openspec/changes/stage4-5-cp-phase6-preemption-timeline-sem-gaps/specs/preemption-spec-correction/spec.md) — IB jump_stack defer behavior（NOT save/restore；clarifies preemption-engine-finish canonical）。

### 后续任务建议

> **当前活跃方向（2026-08-07）**:
>
> Stage 4 ✅ **全部完成**（4.1-4.7.2 全部 ship + 归档，2026-07-26 ~ 2026-08-05）：
> - 4.7.1 Phase 1+2 foundation ✅（merge `489655a`）
> - 4.7.2 5 个 removal ✅（graph `22c41af`→`e1ede1b`、mem_pool `dfe97e7`→`8e0eb21`、stream_capture `0ab7133`→`6749800`、gpu_queue_emu `f1070ec`→`b819b9f`、hardware_puller_emu `5929f50`→`e07a409`）
> - L2: 8 → 1（仅 `sim/sim_event.h` 残留，kfd_events.c 范围外，需独立 proposal）
>
> **待启动 follow-up 项**:
> - 4.7.3 ADR-023 §D4 spec 同步: 把 65 个 fn-ptrs 列入 ADR 表格（取代旧 46 数字）
> - L2 残余 sim_event.h: 独立 proposal（不在 Stage 4 范围）
>
> **Trigger-gated**: Stage 5 (multi-engine + PM4 microcode) 等待 ADR-049/052 Phase 6+ 触发。
>
> **改进提案入口**: 见 [proposal-suggestions.md](proposal-suggestions.md)（待审批）/ [proposal-approved.md](proposal-approved.md)（已批准）/ [openspec/changes/INDEX.md](openspec/changes/INDEX.md)（在途 changes）

```
1. Stage 3.4 文档完善 (Doxygen API 参考 + docs-audit 持续 PASS)
2. Stage 3.1 CI/CD 全平台验证 (macOS/aarch64 deferred 补齐)
3. Stage 3.3 回归验证 (errno audit 后的 105 ctest 持续 PASS)
4. v1.0 发布清单 (Release notes + Migration guide + Binary release)
```

---

## 维护说明

本路线图文件由 UsrLinuxEmu Architecture Team 维护。任何阶段状态变更、原则修正、目标调整需走 ADR 流程（见 ADR-035）。

阶段文件 (`stage-*.md` / `blueprint.md`) 与 OpenSpec change 解耦：阶段描述"做什么"与"为什么"，OpenSpec change 描述"如何做"与"何时做"。两者通过 ADR 关联，不通过编号绑定。

---

**对应 ADR**: ADR-035 (governance) + ADR-036 (3-way principle)
**对应 SSOT 章节**: §1.10
**OpenSpec 状态**: 不绑定（无 change 编号）
