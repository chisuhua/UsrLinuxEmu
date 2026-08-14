# Multi-Process GPU Simulator Stack — UsrLinuxEmu Integration SSOT

> **状态**: 📋 Draft v0.1（2026-08-14，架构阶段独立调研产出）
> **角色**: 跨仓集成 SSOT——桥接 `superpowers/specs/2026-08-13-multi-process-gpu-simulator-vision.md`（4 仓 vision 顶层 spec）与 [`scale-up-fabric-architecture.md`](scale-up-fabric-architecture.md) v0.2（UsrLinuxEmu scale-up 局部 SSOT）
> **Owner**: UsrLinuxEmu Architecture Team
> **作者**: Sisyphus（基于 Oracle 任务 `bg_49b5caf0`（multi-process GPU 仿真器研究）+ `bg_33efdfc5`（vision 架构分析）+ vision spec 全文）
> **范围**: 多仓多进程 GPU 仿真栈与 UsrLinuxEmu scale-up 轨道的集成架构；不涉及执行计划
> **关系图**:
> ```
> post-refactor-architecture.md (项目级 SSOT, v0.1.7)
>      │
>      ├─ scale-up-fabric-architecture.md (Scale-up 局部 SSOT, v0.2)
>      │       │
>      │       └─ ADR-077 ~ ADR-086 (scale-up 决策)
>      │
>      └─ multi-process-gpu-simulator-integration.md (本文件, 跨仓集成 SSOT)
>              │
>              ├─ ADR-087 (跨仓设备与 fabric seam ADR, 待建)
>              │
>              └─ superpowers/specs/2026-08-13-multi-process-gpu-simulator-vision.md
>                  (4 仓 vision 顶层 spec, 由 vision 起草人 Sisyphus 维护)
> ```
> **关联 ADR**:
> - [ADR-023](../00_adr/adr-023-hal-interface.md) ✅ HAL 接口契约
> - [ADR-035](../00_adr/adr-035-governance-policy.md) ✅ 治理规则（§R5.1 cross-repo 协议）
> - [ADR-036](../00_adr/adr-036-three-way-separation.md) ✅ 3 区分原则
> - [ADR-038](../00_adr/adr-038-network-stack-three-way-separation.md) ✅ net_driver 模板
> - [ADR-076](../00_adr/adr-076-gpgpu-kernel-module-ioctl.md) ✅ PTX-EMU 集成（Audit Phase 1 教训）
> - [ADR-077](../00_adr/adr-077-node-fabric-address-model.md) 📋 Scale-up NFA charter
> - [ADR-085](../00_adr/adr-085-consumer-contract.md) 📋 消费者契约（taskrunner IFabricDriver）
> - **ADR-087**（待建）跨仓设备与 fabric seam ADR
> **目标读者**:
> - 4 仓 owner（UsrLinuxEmu + CppTLM + PTX-EMU + TaskRunner）
> - Scale-up 轨道架构师
> - Phase 3 IPC seam 实施者
> **最后更新**: 2026-08-14（v0.1 初版）

---

## 目录

- [§0 文档定位与状态](#0-文档定位与状态)
- [§1 范围与边界](#1-范围与边界)
- [§2 Multi-Process Vision 摘要（4 仓 vision spec 提炼）](#2-multi-process-vision-摘要4-仓-vision-spec-提炼)
- [§3 集成映射：Scale-up Fabric 与 Multi-Process Vision](#3-集成映射scale-up-fabric-与-multi-process-vision)
- [§4 Scale-up Fabric 必须修订的 3 项核心假设](#4-scale-up-fabric-必须修订的-3-项核心假设)
- [§5 Vision 遗漏的 6 个跨仓架构决策](#5-vision-遗漏的-6-个跨仓架构决策)
- [§6 8 项架构性影响（IMP-1 ~ IMP-8）](#6-8-项架构性影响imp-1--imp-8)
- [§7 跨仓 ADR 候选（ADR-087 等）](#7-跨仓-adr-候选adr-087-等)
- [§8 4-Owner 评审 Checklist](#8-4-owner-评审-checklist)
- [§9 阶段路径对齐：Vision Phases vs Scale-up Waves](#9-阶段路径对齐vision-phases-vs-scale-up-waves)
- [§10 Non-Goals](#10-non-goals)
- [§11 风险登记](#11-风险登记)
- [§12 待澄清问题](#12-待澄清问题)
- [附录 A 术语表](#附录-a-术语表)

---

## §0 文档定位与状态

### 0.1 角色

本文件是 **跨仓集成 SSOT**，聚焦于 **UsrLinuxEmu ↔ Multi-Process Vision** 之间的架构衔接：

- **不重复** vision spec 的 4 仓顶层设计（已在 vision.md 详述）
- **不重复** scale-up-fabric-architecture.md 的内部架构（已在 v0.2 详述）
- **聚焦** 两者间的集成约束、修订、缺失决策、IMP

### 0.2 与 vision spec 的关系

| 维度 | vision spec | 本文件 |
|------|------------|--------|
| 角色 | 4 仓顶层 spec | UsrLinuxEmu 集成 SSOT |
| 范围 | 4 仓 vision 一致 | UsrLinuxEmu ↔ vision 集成 |
| 决策 | D1-D4 + Phases | 6 项遗漏决策 + 8 项 IMP + 3 项 Scale-up 修订 |
| Owner | Vision 起草人（4 仓 owner 评审） | UsrLinuxEmu Architecture Team |
| 阶段 | ship 阻塞（Phase R 之前） | 架构阶段独立调研 |

### 0.3 状态路径

```
Draft（本文件当前）
  ↓ 4 仓 owner 评审通过
Proposed
  ↓ ADR-087 Accepted + Scale-up 修订完成
Effective（成为 UsrLinuxEmu 跨仓治理 SSOT）
```

### 0.4 不在本文件范围

- ❌ 执行计划（commit 顺序、time line、worktree 划分）
- ❌ 实施 PR/change 创建
- ❌ 4 仓 owner 评审流程（由 vision spec + UsrLinuxEmu governance 协调）
- ❌ 各仓内部架构（各仓自有 SSOT）

---

## §1 范围与边界

### 1.1 集成范围

**纳入**：
- 4 仓 vision 的 D1-D4 决策对 UsrLinuxEmu 现有架构的影响
- Scale-up Fabric 必须在 multi-process 上下文中修订的假设
- Vision spec 未明确的 6 个跨仓架构决策
- 8 项 IMP（架构性影响，需 4 仓 owner 决议）
- 跨仓 ADR 候选（特别是 ADR-087）
- 阶段路径对齐（Vision Phase R-5 vs Scale-up Wave 0-4）

**不纳入**：
- 4 仓各自的内部架构（各自 SSOT）
- 实施层面的 PR/change 流程
- 性能基准（除非纳入 ADR）
- 第三方工具选型（如 build system、CI）

### 1.2 4 仓责任矩阵

| 仓 | 核心职责 | Scale-up 集成相关 |
|-----|---------|-------------------|
| **UsrLinuxEmu** | Linux kernel sim + gpu_driver + HAL + ①层 + ②层 driver | ADR-077~086 起草 + PTX-EMU HAL 集成（ADR-076 v2）+ Scale-up 修订 |
| **CppTLM** | TLM 2.0 NoC + GPU cluster sim + ③层 GPU device model | NVLink sim + Memory bridge + ADR-SOC-04 v2 Host Adapter |
| **PTX-EMU** | PTX kernel 解释 + 最小化 subset | `libptxemu_minimal.so` 拆分 + ABI 契约 |
| **TaskRunner** | CUDA runtime shim + ②层 consumer | `IGpuDriver` 扩展 + `IFabricDriver` 引入 |

### 1.3 与现有 ADR 的关系

| 现有 ADR | 集成相关性 |
|---------|-----------|
| ADR-023 HAL append-only | 边界扩展约束（ADR-087 须遵循） |
| ADR-035 治理 §R5.1 cross-repo | 4 仓评审 + commit 顺序协议 |
| ADR-036 3 区分 | 跨进程仍维持（每仓 ①②③ 边界不变）|
| ADR-038 net_driver 3 区分 | 跨仓 plugin 模板 |
| ADR-076 PTX-EMU HAL | Phase R 教训 → 多进程必须重设计 |
| ADR-077~086 Scale-up 局部分析 | 需在 multi-process 上下文下重新映射 |

---

## §2 Multi-Process Vision 摘要（4 仓 vision spec 提炼）

> **来源**: [`docs/superpowers/specs/2026-08-13-multi-process-gpu-simulator-vision.md`](../../superpowers/specs/2026-08-13-multi-process-gpu-simulator-vision.md) (294 lines, Author: Sisyphus, 2026-08-13)

### 2.1 4 仓协作架构

```
┌─────────────────────────────────────────────────────────────────┐
│ Vision: 端到端 GPU 仿真栈（产品形态）                              │
└─────────────────────────────────────────────────────────────────┘

[Process 1] Software Stack Sim (host CPU 侧)
───────────────────────────────────────────── TaskRunner
    - cuLaunchKernel / cuMemcpy / cuModuleLoad
    - libcuda_taskrunner.so shim (LD_PRELOAD)
    ↓ IPC: MMIO + doorbell + fence + interrupt
  UsrLinuxEmu
    - gpu_driver plugin (GpgpuDevice, HAL)
    - HardwarePullerEmu (PM4 microcode / CP firmware)
    - Linux kernel sim (VFS, ModuleLoader, scheduler)

═══════════════ IPC / PCIe sim 边界 ═══════════════

[Process 2..N] GPU Device Sim (per-GPU)
─────────────────────────────────────────────
  CppTLM (SoC framework)
    - CppTLMBridge 端点 (driver→GPU MMIO 入口)
    - KernelLaunchTLM (PM4 dispatcher, AQL parser, doorbell handler)
    - ComputeUnitTLM × N (黑盒 SM)
    - TCC (GPU L2) + HBM mode
    - MemoryBridge (替代 PTX-EMU SimpleMemory)
  PTX-EMU libptxemu_device.so (minimal)
    - PtxContextAdapter (PTX-IR → StatementContext)
    - PtxInterpreter core (StatementContext 执行)
    - 不持有 SimpleMemory（由 CppTLM MemoryBridge 提供）
    - 不持有 GPUContext singleton（由 CppTLM KernelLaunchTLM 管）

═══════════════ Inter-GPU NVLink sim ═══════════════

[Process 2] GPU0  ←── NVLink IPC ──→ [Process 3] GPU 1
```

### 2.2 4 个关键设计决策

| 决策 | 选择 | 关键论证 |
|------|------|---------|
| **D1.b** | CPU sim 1 进程 + 每 GPU 1 进程 | 真实硬件 CPU/GPU die 分裂的自然延伸；IPC 开销 ~200cy/MMIO 对 cycle-accurate sim 可接受 |
| **D2.a** | 共享内存映射 v1 | 最简单；接近 APU 共享内存语义；可演进到 dGPU pass-through |
| **D3.b** | `libptxemu_minimal.so` 拆分 | 保留 PTX-EMU 仓所有权；防止 `SimpleMemory`/`GPUContext` 重复 |
| **D4.c** | ADR-SOC-04 拆 v1（黑盒）+ v2（Host Adapter opt-in）| 兼顾 TLM 黑盒验证与真实 host software 接入 |

### 2.3 6 阶段实施路径

| Phase | 范围 | 仓 | 难度 | ship 标准 |
|-------|------|---|------|----------|
| **R** | Audit 修复 + ADR-076 v2 | UsrLinuxEmu + PTX-EMU + TaskRunner | Quick-Medium | 真 .so e2e 通过 |
| **1** | CppTLM submodule 集成 | UsrLinuxEmu + CppTLM | Easy | 三仓 build 链接 |
| **2** | CppTLM GPU cluster API 化 | CppTLM | Medium | `gpu_simulator_init(config)` API |
| **3** | IPC seam（4 通道）| UsrLinuxEmu + CppTLM | Hard | CPU/GPU 二进程 e2e |
| **4** | Multi-GPU（同/跨进程）| All | Hard | 双 GPU + NVLink sim |
| **5** | Multi-node（跨机器）| All | Hard | 2 机 × 2 GPU 集群 |
| **6** | 真实 GPU ISA swap | PTX-EMU | Extreme | 替换 PTX-EMU 为 AMDGCN/Xe |

### 2.4 Vision 文档未明确的 6 项决策

> **关键发现**（来自调研任务 `bg_33efdfc5`）：vision spec 给出了 **D1-D4** 4 个决策，但**未明确以下 6 个跨仓架构决策**。本文件 §5 列出。

---

## §3 集成映射：Scale-up Fabric 与 Multi-Process Vision

### 3.1 Scale-up Fabric = Vision Phase 4

```
Multi-Process Vision 总栈
├─ Phase R (audit fix) → Phase 1 (CppTLM submodule) → Phase 2 (cluster API)
├─ Phase 3 (IPC seam) ← HARD GATE for Phase 4
│
└─ Phase 4 (Multi-GPU/NVLink) ← Scale-up-fabric 轨道主战场
   ├─ L1 Switch = NVLink sim + cross-process gateway
   ├─ Unified NFA = shared memory + descriptors
   ├─ Per-GPU MMU = cross-process page table
   ├─ Multicast = replicated across per-GPU processes
   ├─ PGAS = per-process VA over shared NFA
   └─ NIC (semantic RDMA) = cross-process IPC to external
```

### 3.2 决策映射表

| Scale-up 决策 | Vision 决策 | 集成关系 |
|--------------|-------------|---------|
| NFA charter (ADR-077) | D2.a shared memory | NFA 是 shared memory **之上的** 抽象；D2.a 提供 transport |
| Switch driver (ADR-078) | D1.b per-GPU process | **Switch 进程边界未明确**（IMP-1）|
| UVM/PGAS 翻译 (ADR-084) | D1.b + D2.a | 跨进程翻译表**必须**经过共享 NFA，不持 raw pointer |
| Multicast (ADR-083) | D1.b | 跨进程 replicated-object，**shared memory + 显式 txid** |
| Coherence Non-Goal (ADR-079) | D1.b | 单进程天然成立；**跨进程需 explicit contract**（R1）|
| Multi-instance spike (ADR-080) | Phase R 之前 | 集成多进程 IPC 准备（IMP-6）|
| Consumer contract (ADR-085) | D1.b + TaskRunner | TaskRunner `IFabricDriver` 跨进程 |

### 3.3 关键发现（来自调研）

> **gem5 + GPGPU-Sim** 是**最强直接先例**：双进程 + 共享内存 + 锁步 tick（CITATION 调研任务 `bg_33efdfc5` §1.2.3）
> 
> **NVIDIA MPS** 是**生产系统先例**：控制 daemon + MPS server + client runtimes 三进程架构
> 
> **公开 GPU 仿真器几乎全部单进程**（QEMU/gem5/GPGPU-Sim/Accel-Sim/MIAOW），multi-process vision 是**架构上新颖**的

---

## §4 Scale-up Fabric 必须修订的 3 项核心假设

> **关键修订（R1-R3）**：本节列出的 3 项假设在单进程仿真上下文中成立，但在 multi-process vision 上下文中**必须修订**。修订方向（不讨论实施）需 4 仓 owner 评审。

### 4.1 R1: "一致性天然成立" 必须拆分

**现状**（scale-up-fabric-architecture.md v0.2 §6.1）：
- 仿真器内一致性是天然的（单进程 CPU/GPU 共享地址空间）
- "Coherence 协议模拟" 是 Non-Goal（per ADR-073 D1）

**multi-process 下的挑战**：
- 进程边界打破了"共享地址空间"假设
- "天然一致"对**跨进程**不成立
- shared memory 仅提供 **transport-level sharing**，不提供 **host-cache coherence**

**修订方向**（**待 4 仓 owner 决议**）：
- 引入 **3 级 consistency contract**（参考 gem5 multi-GPU 经验）：
  - **Level 0**：functional consistency（payload bytes shared；不模拟 cache timing）
  - **Level 1**：simulated visibility consistency（write visible at simulated event + fence）
  - **Level 2**：timing/coherence model（cache line states + writeback + invalidation）
- Scale-up v1 = Level 0 + Level 1；Level 2 Non-Goal
- 同步原语（fence_id / counted write）必须**显式**区分 in-process vs cross-process

**影响 ADR**：
- ADR-077（NFA charter）：需新增 "NFA cross-process descriptor" 协议
- ADR-079（Coherence Non-Goal）：需重写引入 multi-process 边界
- ADR-084（PGAS 地址语义）：需明确"per-process VA + node-global NFA"在多进程下的映射

### 4.2 R2: Switch Driver 进程边界需重新审视

**现状**（scale-up-fabric-architecture.md v0.2 §9）：
- switch_driver = 单插件（在 1 个进程内）
- 双角色（fabric + NIC）；通过 ServiceRegistry 跨模块查找

**multi-process 下的挑战**：
- 真实硬件 Switch 可能本身就是独立器件（NVIDIA NVSwitch vs ConnectX-9 分离）
- 仿真架构中 Switch 是独立进程、CPU 进程内 coordinator、还是 GPU 进程内 replica？**未明确**

**修订方向**（**待 4 仓 owner 决议**）：
- 三选一：
  - **(a)** Switch 独立进程（架构最清晰；IPC 复杂度高）
  - **(b)** Switch 在 CPU 进程内 coordinator（最简单；与 in-process 仿真兼容）
  - **(c)** Switch 逻辑上归属于某 GPU 进程但提供全局 view（折中）
- **当前 scale-up 推荐 (b)**；**vision phase 4 可能需求 (a)**
- ADR-078 D2 需在 multi-process 上下文重审

**影响 ADR**：
- ADR-078（switch_driver 插件拆分）：需新增 "switch process ownership" 决策
- ADR-085（消费者契约）：可能需 `IFabricSwitch` 独立接口

### 4.3 R3: HAL fn-ptr 需 cross-process 语义考量

**现状**（scale-up-fabric-architecture.md v0.2 §8）：
- H3-H7 ops 设计为 in-process 调用（同步或 fence 异步）
- `hal_fabric_route(ctx, nfa, size, target_partition_id)` 假设 ctx 在同进程

**multi-process 下的挑战**：
- 部分 op 跨进程后**不能再传 raw pointer**（ctx, va ranges）
- 必须改用 **descriptor**（region ID + offset + generation + size + permissions）
- 跨进程同步需 IPC-friendly 原语

**修订方向**（**待 Scale-up + vision 评审**）：
- 重命名/重新设计 `hal_fabric_route`：参数从 `nfa + size` 改为 `nfa_descriptor{(region_id, offset, size, generation)}`
- 所有 H3-H7 ops 添加 **session_id + generation tag** 用于跨进程消息校验
- 同步语义（fence/doorbell）需明确跨进程 in-process tracking（cannot use plain C++ atomics）

**影响 ADR**：
- ADR-081（H3+H4 batch）：需在 D1-D2 修订时同步
- ADR-082（H6+H7 batch）：同上
- ADR-075（Layer 2 Foundation Removal 回顾）：hal_fabric_* 设计需回溯兼容

---

## §5 Vision 遗漏的 6 个跨仓架构决策

> **关键发现**（调研 `bg_33efdfc5` §1.6）：vision spec 列了 R1-R5 风险，但**未明确以下 6 个跨仓架构决策**。本节列出待 4 仓 owner 评审。

### 5.1 Decision Gap 1: Switch Process Ownership

**Vision 上下文**：D1.b 仅规定"per-GPU 1 进程"，Switch 进程未明确

**选项**：
- **(a)** Switch 独立进程（与 CPU + per-GPU 进程并列）：架构最清晰；IPC 复杂度高
- **(b)** Switch 在 CPU 进程内 coordinator：最简单；in-process 仿真兼容
- **(c)** Switch 逻辑上归属于某 GPU 进程但提供全局 view：折中但易引入 bias
- **(d)** Switch 在多个进程间 replicated（一致性问题复杂）

**调研推荐**：(b) 为 v1 起点；(a) 为 Phase 4 演进方向

**待 4-owner 决策**

### 5.2 Decision Gap 2: Cross-Process Pointer Safety

**Vision 上下文**：D2.a shared memory 隐含指针安全（但未明确）

**问题**：
- 不同进程 mmap 同一 fd 可获得**不同虚拟地址**
- 跨进程结构体内**不能**含 raw pointer
- `void*`, `char*`, `fn_ptr` 等必须全是 process-local

**必选**：
- 所有共享结构体使用 **opaque handle**（region ID + offset + generation）
- canonical ABI 字段集：`uint64_t`, `uint32_t`, `enum`, `flags`, `magic`
- 无 vtable、无指针、无 C++ 对象

**调研推荐**：参考 vhost-user 的 memory region descriptors 设计

**待 4-owner 决策**

### 5.3 Decision Gap 3: IPC Protocol Versioning

**Vision 上下文**：Phase 3 IPC seam 提到 4 通道但**未明确协议版本**

**需求**：
- 4 通道协议必须**有版本号**（避免 breaking change 升级灾难）
- 能力协商（capability flags）
- 兼容性矩阵（v1 / v2 / v3 互通？）
- 升级路径（rolling upgrade？）

**必选**：
- 协议头有 `protocol_version` field
- 启动时能力协商
- 文档化的 deprecation 政策

**调研推荐**：参考 NVLink 演进（v1-v5，每代不兼容但 SIMD 兼容）

**待 4-owner 决策**

### 5.4 Decision Gap 4: Process Crash Recovery

**Vision 上下文**：vision 提到 heartbeat（在 MIPS 风险条目中）但**未明确恢复协议**

**问题**：
- GPU 进程崩溃时，CPU 进程如何处理？
- 共享内存状态如何恢复？
- 进行中的 work item 如何处理？
- 重启后能否接续？

**必选**：
- Heartbeat 协议（双方周期 ping）
- Watchdog timeout（默认 1s？）
- 重连机制（CPU 侧检测到 GPU 死，自动重启）
- 事务 recovery（in-flight ops abandon 或 replay）
- 共享内存状态清理协议

**调研推荐**：参考 QEMU vhost-user reconnect 协议

**待 4-owner 决策**

### 5.5 Decision Gap 5: State Dump / Checkpoint

**Vision 上下文**：vision 提到 sim snapshots 但**未明确**跨进程

**gem5 警告**：
> "Checkpoints are not supported inside a GPU kernel and should be taken when no GPU kernels are running."

**需求**：
- 跨进程 quiesce handshake（所有 GPU 进程 ack 后才能 dump）
- 共享内存状态序列化
- 启动时状态恢复协议
- checkpoint 期间不允许新的 GPU kernel 启动

**必选**：
- Global checkpoint barrier
- Per-process quiesce handshake
- Versioned state sections
- 共享 checkpoint manifest + generation
- "No checkpoint inside kernel" rule

**待 4-owner 决策**

### 5.6 Decision Gap 6: Memory-Region Descriptors

**Vision 上下文**：D2.a shared memory 提到共享但**未明确 descriptor 协议**

**需求**：
- 共享 memory region 必须有 **immutable descriptor**
- descriptor 包含：region_id, generation, nfa_base, size, fd, permissions, owner_device, cache_policy
- descriptor 通过 control channel 传递（Unix socket）
- fd 通过 `SCM_RIGHTS` 传递
- **无 raw pointer** 出现在 descriptor 内

**示例**：
```cpp
struct memory_region_descriptor {
    uint32_t region_id;
    uint32_t generation;
    uint64_t nfa_base;
    uint64_t size;
    uint64_t file_offset;
    uint32_t permissions;
    uint32_t owner_device;
    uint32_t cache_policy;
    uint32_t flags;
};
```

**调研推荐**：参考 QEMU vhost-user memory region protocol（设计已被验证）

**待 4-owner 决策**

---

## §6 8 项架构性影响（IMP-1 ~ IMP-8）

> **架构阶段影响清单**：这些不是 bug，是 multi-process vision 强加的设计约束。每项 IMP 都需要在 Scale-up + Vision 评审中给出方向。

### 6.1 IMP-1: Switch Driver 进程边界

**现状**：scale-up AD下，switch_driver 在 1 个进程内（UsrLinuxEmu）
**约束**：Vision Phase 4 可能需要 Switch 独立进程
**影响**：ADR-078 D2 需增加 "Switch process ownership" 决策
**优先级**：🔴 P0（Wave 0 必决）

### 6.2 IMP-2: g_vram_store 跨进程重构

**现状**：ADR-080 D1 重构为节点级 backing store
**约束**：Multi-process 下每个 GPU 进程需独立 mmap 同一 fd
**影响**：backing store 必须用 `memfd_create` + `SCM_RIGHTS` 传递；非 raw mmap
**优先级**：🔴 P0（ADR-080 必决）

### 6.3 IMP-3: HAL ops cross-process semantics

**现状**：H3-H7 ops 设计为 in-process
**约束**：跨进程后部分 ops 需 IPC 包装
**影响**：ADR-081 / ADR-082 需在 multi-process 上下文重审
**优先级**：🟠 P1（Wave 2 必决）

### 6.4 IMP-4: CppTLM NVLink vs Scale-up L1 Switch

**现状**：Scale-up arch 把 L1 Switch 作为 UsrLinuxEmu 内 sim
**约束**：Vision Phase 4 NVLink sim 归属 CppTLM
**影响**：要么 (a)Scale-up 退出 NVLink 数据面，仅做 fabric control plane；(b)Scale-up 接管 NVLink data plane 但需与 CppTLM 协作
**优先级**：🔴 P0（Wave 0 必决）

### 6.5 IMP-5: ADR-085 vs tadr-307 时序

**现状**：Scale-up ADR-085 是 TaskRunner `IFabricDriver`；tadr-307 是 TaskRunner `IGpuDriver` 扩展
**约束**：两者都是 TaskRunner 扩展，需协调
**影响**：ADR-085 引用 tadr-307，互为姊妹文档
**优先级**：🟠 P1（Wave 4 必决）

### 6.6 IMP-6: ADR-080 Spike 扩展到 IPC 准备

**现状**：ADR-080 spike 仅 multi-instance GO/NO-GO
**约束**：Vision Phase 3 之前需 IPC 准备
**影响**：spike 增加 IPC readiness 检查（Unix socket 创建 / `memfd_create` / 跨进程 mmap 验证）
**优先级**：🟠 P1（Wave 0 必决）

### 6.7 IMP-7: Wave 0-4 ↔ Phase R-5 时序对齐

**现状**：Scale-up 路线 vs Vision 阶段路径独立
**约束**：需协调以避免重复工作
**影响**：建立统一协调表（§9）
**优先级**：🟡 P2（Wave 0 必决）

### 6.8 IMP-8: PTX-EMU minimal split 影响 multi-GPU

**现状**：D3.b 拆 `libptxemu_minimal.so`
**约束**：Multi-GPU 下每 GPU 进程需要独立 PTX-EMU instance
**影响**：minimal lib 必须支持多实例 + 跨进程内存/sync 回调
**优先级**：🟠 P1（Phase 4 必决）

---

## §7 跨仓 ADR 候选（ADR-087 等）

### 7.1 ADR-087 候选：Multi-Process Device and Fabric Seam ADR

**标题**：跨仓设备与 Fabric Seam 治理（4 仓 owner 共识）

**焦点**（待 4-owner 评审细化）：
- D1.b：CPU/GPU 二分（已定）+ Switch 进程边界（IMP-1）
- D2.a 修订：shared memory + descriptor 协议
- D3.b：minimal lib 跨进程准则
- D4.c：Host Adapter 跨进程接口
- 6 项 vision 遗漏决策（§5）
- IPC 协议版本化 + 描述符格式
- Process crash recovery
- State dump 协议

**关联 ADR**：
- ADR-077 (~086) scale-up 局部分析
- ADR-076 PTX-EMU HAL 集成（Phase R 教训）
- ADR-035 §R5.1 cross-repo 流程
- ADR-023 HAL append-only
- CppTLM ADR-SOC-04（v2 修订）
- PTX-EMU ADR-0029
- TaskRunner tadr-301/tadr-307

**焦点面 vs 广度**：建议**单一 ADR**（governance 集中决策），但**通过附录/引用**分解到 Scale-up ADR-077~086 与 CppTLM/PTX-EMU/TaskRunner 各自 ADR

**待 4-owner 评审**

### 7.2 Scale-up ADR-077~086 vs ADR-087 关系

| ADR | 范围 | 与 ADR-087 关系 |
|-----|------|----------------|
| ADR-077 | NFA charter（节点地址模型）| 引用 ADR-087 描述符协议 |
| ADR-078 | switch_driver 插件拆分 | 引用 ADR-087 Switch 进程边界 |
| ADR-079 | Coherence Non-Goal | 扩展到 multi-process contract |
| ADR-080 | 多实例前置 | 包含 IPC 准备（IMP-6）|
| ADR-081 | HAL Phase-1 | 引用 ADR-087 IPC 描述符 |
| ADR-082 | HAL Phase-2 | 同上 |
| ADR-083 | 多播语义 | 引用 ADR-087 跨进程 multicast txid |
| ADR-084 | PGAS 地址 | 引用 ADR-087 跨进程 translation |
| ADR-085 | 消费者契约 | 引用 ADR-087 跨仓协议 |
| ADR-086 | Trigger registry | 扩展 multi-process triggers |
| **ADR-087** | **跨仓 seam 治理** | **SSOT for cross-repo** |

**结论**：ADR-077~086 范围**不变**，但**每个 ADR 需在 multi-process 上下文中修订**；ADR-087 是它们的 cross-repo 引用源。

### 7.3 推荐 ADR 创建顺序（含 ADR-087）

```
阶段 1（4 仓共识 + 跨仓 seam）：
  1. ADR-087（跨仓 seam 治理）— 4 仓 owner 联合评审
  2. Scale-up ADR-077~086 修订（吸收 multi-process 修订）

阶段 2（Wave 0）：
  3. ADR-080（多实例 + IPC 准备）
  4. Scale-up ADR-079, ADR-086
  5. CppTLM ADR-SOC-04 v2
  6. PTX-EMU ADR-0029 §D7

阶段 3-5（Wave 1-4 + Phase R-5）：
  按 Phase R → 1 → 2 → 3 → 4 → 5 顺序
```

---

## §8 4-Owner 评审 Checklist

### 8.1 整体

- [ ] 4 仓 owner 对 Multi-Process Vision 大方向认可（特别是 D1.b + D2.a）
- [ ] 6 项 vision 遗漏决策（§5）有 owner 决策或明确 defer
- [ ] 8 项 IMP（§6）有 owner 决议或明确 defer
- [ ] 阶段路径对齐（§9）无 owner 冲突
- [ ] ADR-087 治理范围被 4 仓认可

### 8.2 Vision 决策

- [ ] D1.b 认可（含 D1.b 修订建议）
- [ ] D2.a 认可（含 shared memory + descriptor 协议修订）
- [ ] D3.b 认可（含 minimal lib 跨进程准则）
- [ ] D4.c 认可（含 Host Adapter 跨进程接口）
- [ ] Phase R ship 阻塞达成共识

### 8.3 Scale-up 修订

- [ ] R1（一致性拆分） 接受
- [ ] R2（Switch 进程边界）接受待决议
- [ ] R3（HAL cross-process）接受

### 8.4 ADR 治理

- [ ] ADR-087 范围认可
- [ ] ADR-077~086 修订接受
- [ ] tadr-307 扩展同步
- [ ] ADR-035 §R5.1 cross-repo 流程可承受 4 仓评审

### 8.5 风险

- [ ] R1-R5 vision 风险接受
- [ ] 任何 owner 额外风险登记

---

## §9 阶段路径对齐：Vision Phases vs Scale-up Waves

### 9.1 对齐表

| Vision Phase | Scale-up Wave | 同步性 | 冲突风险 |
|-------------|---------------|--------|---------|
| **Phase R（audit）** | Wave 0 之前 | 起点；4 仓共识 | 🟢 低 |
| **Phase 1（CppTLM submodule）** | Wave 0（并行）| 仓级别 build | 🟢 低 |
| **Phase 2（cluster API）** | Wave 0（并行）| API 收敛 | 🟡 中 |
| **Phase 3（IPC seam）** | Wave 0-1（关键）| **HARD GATE** for Phase 4 | 🔴 高 |
| **Phase 4（Multi-GPU）** | Wave 1-4（主战场）| Scale-up 主战场 | 🟠 中 |
| **Phase 5（Multi-node）** | Wave 4（trigger）| trigger-gated | 🟢 低 |

### 9.2 关键依赖

```
Phase 3 (IPC seam) ──→ 必须完成 ──→ Scale-up Wave 2 (Switch skeleton)
                                     │
                                     ↓
                              Scale-up Wave 3 (UVM + Multicast)
                                     │
                                     ↓
                              Scale-up Wave 4 (NIC + E2E)
                                     │
                                     ↓
                              Phase 5 (Multi-node) trigger
```

**关键**：Scale-up Wave 2 启动**前**必须 Phase 3 完成。

### 9.3 可并行部分

- **Phase R** + **Scale-up Wave 0**（并行）：audit 修复 + 多实例 spike
- **Phase 1-2** + **Scale-up ADR 起草**（并行）：CppTLM 集成 + Scale-up ADR 评审
- **CvT 内部 phase**（各仓细节）+ **跨仓 ADR-087**（并行）

### 9.4 阻塞关系

| 若... | 则... |
|------|------|
| Phase R 未完成 | Scale-up Wave 0 不能启动 |
| Phase 3 未完成 | Scale-up Wave 2 不能启动 |
| ADR-087 未 Accepted | Scale-up ADR-077~086 修订不能 Accepted |
| CppTLM ADR-SOC-04 v2 未 Accepted | Phase 3 不能启动 |

---

## §10 Non-Goals

以下**明确不在本集成 SSOT 范围**：

| Non-Goal | 理由 |
|----------|------|
| 4 仓内部架构决策 | 各仓自有 SSOT 与治理 |
| 执行层面（commit 顺序、time line、worktree）| 各仓 implementation 自决 |
| 性能基准 | 除非纳入 ADR（如 ATR-012）|
| 第三方工具选型 | build system / CI 等独立决策 |
| 真 PCIe / NVLink 物理层 | 不在仿真范围 |
| Linux kernel-side 全仿真 | UsrLinuxEmu 已有 `linux_compat` |
| 真实 GPU ISA swap (vision Phase 6) | 当前不议 |
| 跨机器 gRPC transport (vision Phase 5) | 单独评估 |
| ROCm/HSA runtime 100% 兼容 | ADR-SOC-04 v2 opt-in 覆盖 |

---

## §11 风险登记

| # | 风险 | 严重度 | 缓解 |
|---|------|--------|------|
| 1 | **4 仓 owner 评审不被召集** | 🔴 High | 文档明示评审清单 + ADR-035 §R5.1 流程 |
| 2 | **Process 边界工程复杂度高估** | 🟠 Med | 双轨实施：logical-process (in-process) + 真跨进程；先 logical |
| 3 | **shared memory descriptor 协议设计不当** | 🟠 Med | 参考 vhost-user memory region + ADR-087 评审 |
| 4 | **Switch 进程边界反复** | 🟠 Med | ADR-087 强约束 owner 决策 |
| 5 | **PTX-EMU minimal lib 跨进程 ABI 漂移** | 🟠 Med | 跨仓 ABI 委员会 + 真 .so 集成测试 |
| 6 | **Scale-up Vision 修订冲突** | 🟡 Low-Med | 同步评估 + 修订合并 |
| 7 | **IPC 协议版本不兼容** | 🟡 Low-Med | capability 协商 + versioning policy |
| 8 | **跨进程 performance 退化** | 🟡 Low-Med | logical-process 模式 + benchmark |
| 9 | **Vision Phase 3 延期** | 🟠 Med | Scale-up Wave 0-1 与 Phase 3 并行；Wave 2 推迟 |
| 10 | **跨仓文档漂移** | 🟡 Low-Med | 统一 SSOT 引用 + 评审 checklist |

---

## §12 待澄清问题

> **4-owner 评审待决问题清单**（独立于 vision spec 与 scale-up arch 的待决项）

### 12.1 必须在 4-owner 评审前澄清

| # | 问题 | 备选 |
|---|------|------|
| Q1 | Switch 进程边界 | (a) 独立进程 / (b) CPU 进程内 / (c) GPU 进程内 / (d) replicated |
| Q2 | Cross-process pointer safety | (a) 严格 descriptor 协议 / (b) C ABI only / (c) 跨 IPC 库 |
| Q3 | IPC 协议 versioning | (a) 主版本 + 能力 / (b) 仅能力 / (c) 无版本（仅 phase 治理）|
| Q4 | Process crash recovery | (a) Heartbeat + reconnect / (b) No recovery（崩溃即终止）/ (c) 用户指定 |

### 12.2 后续阶段澄清

| # | 问题 | 触发 |
|---|------|------|
| Q5 | Switch 在 Phase 4 是否独立化 | Phase 4 multi-GPU 启动 |
| Q6 | PTX-EMU minimal lib 跨进程实例策略 | Phase 3 实施 |
| Q7 | State dump 跨进程频率 | Phase 5 multi-node |
| Q8 | Cross-machine transport 选择（gRPC / RDMA / shared memory network）| Phase 5 multi-node |

---

## 附录 A 术语表

| 术语 | 含义 |
|------|------|
| **Multi-Process Vision** | 4 仓 vision spec（`docs/superpowers/specs/2026-08-13-multi-process-gpu-simulator-vision.md`）|
| **Scale-up Fabric** | UsrLinuxEmu 节点内 L1 Switch + NFA + UVM/PGAS 架构（`scale-up-fabric-architecture.md` v0.2）|
| **IPC Seam** | Vision Phase 3 的 4 通道（MMIO/Doorbell/Fence/Interrupt）跨进程实现 |
| **Process Boundary** | 操作系统进程边界（vs logical process in-process）|
| **CPU/GPU 二分** | D1.b 决策：CPU sim 1 进程 + 每 GPU 1 进程 |
| **Shared Memory Mapping** | D2.a 决策：v1 用 shared memory 作为 transport |
| **NFA** | Node Fabric Address（Scale-up 中的节点级 fabric 地址）|
| **PTX-EMU Minimal** | D3.b 拆分后的 `libptxemu_minimal.so`（指令语义 + 回调）|
| **Host Adapter** | D4.c 中 ADR-SOC-04 v2 的 opt-in 路径 |
| **ADR-087** | 跨仓设备与 Fabric Seam 治理 ADR（拟建）|
| **Logical Process** | in-process 模拟多进程分割（vs 真实 OS 进程）|
| **IPC Descriptor** | 跨进程共享 memory region 的不可变描述符（无 raw pointer）|
| **Process Crash Recovery** | GPU 进程崩溃后 CPU 进程的恢复协议 |
| **Quiesce Handshake** | checkpoint 前所有进程同步静止的协议 |
| **CIP** | Cross-IPC（Imagination 行业常用，区别 us 这里的"cross-process IPC"）|
| **mGPUSim** | Multi-GPU 仿真器（学术）|
| **vhost-user** | QEMU 的 multi-process device 协议成熟参考 |
| **MPS** | NVIDIA Multi-Process Service（生产 multi-process GPU 共享）|
| **MIG** | NVIDIA Multi-Instance GPU（硬件分区）|

---

**文档版本**: v0.1 Draft
**下次更新触发**: §12 待澄清问题决议 / 4 仓 owner 评审反馈 / Vision spec 更新
**维护者**: UsrLinuxEmu Architecture Team
**最后更新**: 2026-08-14（v0.1 初版，基于两个独立调研任务产出）