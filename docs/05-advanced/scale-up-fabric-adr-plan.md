# Scale-up Fabric ADR 群创建计划草案

> **状态**: 📋 Plan Draft v0.3（2026-08-14，待用户审阅）
> **目的**: 为 `docs/02_architecture/scale-up-fabric-architecture.md` v0.5 §12 列出的 **12 个聚焦 ADR**（ADR-077 ~ ADR-089 + ADR-087）做创建计划草案，统一审阅后逐个创建。
> **Owner**: UsrLinuxEmu Architecture Team
> **最后更新**: 2026-08-14（v0.3 ADR-087 D6 决策后修订：L1 Switch 跨 3 仓实现 = 与 GPU 软件栈同构 + 新增 ADR-089（UsrLinuxEmu switch driver）+ ADR-088 合并到 ADR-078 + ADR-090 合并到 ADR-085 + 评审顺序调整 + v0.5 拓扑对齐）
> **对应 SSOT**: [`docs/02_architecture/scale-up-fabric-architecture.md`](../02_architecture/scale-up-fabric-architecture.md) v0.5
> **关联证据**:
> - [`docs/05-advanced/scale-up-fabric-research.md`](scale-up-fabric-research.md) v0.2（用户愿景）
> - [`docs/05-advanced/ats-cxl-30-implementation-research.md`](ats-cxl-30-implementation-research.md) v0.3（ATS/CXL 协议基础）
> - NVIDIA Rubin 深度调研报告（2026-08-14，Oracle 任务 `ses_0037996c5`）— v0.2 修订事实基础
> - Oracle 综合评审报告（2026-08-14，Oracle 任务 `ses_003b3fa7` + `bg_33efdfc5` + `bg_49b5caf0` + `ses_00370deb6` + `ses_000f92e51`）
> - **Multi-Process Vision Spec**（2026-08-13，`docs/superpowers/specs/2026-08-13-multi-process-gpu-simulator-vision.md`）— v0.5 §2 拓扑基础
> - **ADR-087 D6 决策**（2026-08-14，`docs/00_adr/adr-087-multi-process-device-fabric-seam.md`）— 本计划 v0.3 的核心输入

---

## 目录

- [§0 计划元信息](#0-计划元信息)
- [§1 总览：12 个 ADR 一览](#1-总览12-个-adr-一览)
- [§2 创建顺序与 Gate 依赖](#2-创建顺序与-gate-依赖)
- [§3 ADR-077 草案：节点 Fabric 地址模型](#3-adr-077-草案节点-fabric-地址模型)
- [§4 ADR-078 草案：L1 Switch 进程拆分 + CppLink 协议（v0.3 扩展：CppTLM SoC spec 合并）](#4-adr-078-草案l1-switch-进程拆分--cpplink-协议v03-扩展cpptlm-soc-spec-合并)
- [§5 ADR-079 草案：Coherence 协议模拟 Non-Goal](#5-adr-079-草案coherence-协议模拟-non-goal)
- [§6 ADR-080 草案：多实例 GPU 重构前置 + IPC Readiness Spike](#6-adr-080-草案多实例-gpu-重构前置--ipc-readiness-spike)
- [§7 ADR-081 草案：Fabric HAL Phase-1 batch](#7-adr-081-草案fabric-hal-phase-1-batch)
- [§8 ADR-082 草案：Fabric HAL Phase-2 batch](#8-adr-082-草案fabric-hal-phase-2-batch)
- [§9 ADR-083 草案：PGAS 多播语义](#9-adr-083-草案pgas-多播语义)
- [§10 ADR-084 草案：PGAS 地址语义](#10-adr-084-草案pgas-地址语义)
- [§11 ADR-085 草案：消费者契约 + TaskRunner ISwitchDriver（v0.3 扩展：合并原 ADR-090）](#11-adr-085-草案消费者契约--taskrunner-iswitchdriverv03-扩展合并原-adr-090)
- [§12 ADR-086 草案：Trigger Registry 整合](#12-adr-086-草案trigger-registry-整合)
- [§12a ADR-087 草案：Multi-Process Device & Fabric Seam](#12a-adr-087-草案multi-process-device--fabric-seam)
- [§12b ADR-089 草案：L1 Switch Device Driver（v0.3 新增，UsrLinuxEmu）](#12b-adr-089-草案l1-switch-device-driverv03-新增usrlinuxemu)
- [§13 跨 ADR 主题（cross-cutting themes）](#13-跨-adr-主题cross-cutting-themes)
- [§14 审阅 Checklist（建议您重点关注）](#14-审阅-checklist建议您重点关注)
- [§15 待澄清项（创建前需要您先确认的）](#15-待澄清项创建前需要您先确认的)
- [§16 v0.3 变更记录](#16-v03-变更记录)
- [附录 A 后续流程](#附录-a-后续流程)
- [附录 B 版本控制](#附录-b-版本控制)

---

## §0 计划元信息

### 0.1 范围

本计划覆盖 **`scale-up-fabric-architecture.md` v0.5 §12 列出的 12 个 ADR**：

| ADR# | 类型 | 主题 |
|------|------|------|
| ADR-077 | 基础设施（地址）| 节点 Fabric 地址模型（NFA charter + region descriptor 政策）|
| ADR-078 | 基础设施（进程）| L1 Switch 进程拆分 + CppLink 协议 + CppTLM SoC spec（v0.3 扩展）|
| ADR-079 | 基础设施（治理）| Coherence 协议模拟 Non-Goal + 跨进程一致性契约 |
| ADR-080 | 基础设施（前置）| 多实例 GPU 重构前置 + IPC readiness spike |
| ADR-081 | HAL 扩展（Wave 2）| Fabric HAL Phase-1 batch（H3 + H4，descriptor-based 签名）|
| ADR-082 | HAL 扩展（Wave 3）| Fabric HAL Phase-2 batch（H6 + H7，switch_sim fan-out 委托 per D6）|
| ADR-083 | 语义（Wave 3）| PGAS 多播语义（replicated-object + switch_sim 进程 fan-out per D6）|
| ADR-084 | 语义（Wave 3）| PGAS 地址语义（zone 划分 + 翻译规则）|
| ADR-085 | 治理（Wave 4 gate）| 消费者契约 + **ISwitchDriver**（v0.3 合并原 ADR-090）|
| ADR-086 | 治理（Wave 0）| Trigger Registry 整合 |
| ADR-087 | 治理（Wave 0a 关键路径）| **Multi-Process Device & Fabric Seam**（4-owner 评审）|
| **ADR-089** | 基础设施（② driver，v0.3 新增）| **L1 Switch Device Driver**（UsrLinuxEmu，借鉴 gpu_driver 模板 per user 2026-08-14 决策）|

**ADR 编号策略（v0.3 修订）**：
- **088**：**不创建**（已合并到 ADR-078 §D1 作为 CppTLM SoC spec）
- **090**：**不创建**（已合并到 ADR-085 §D8 作为 ISwitchDriver 契约）
- **091+**：v2 扩展按需
| ADR-086 | 治理（Wave 0）| Trigger Registry 整合 |
| ADR-087 | 治理（跨仓 seam）| Multi-Process Device & Fabric Seam ADR（4-owner 评审关键路径）|

### 0.2 不在本计划范围

- **架构 SSOT 本身**（已 v0.2 在 `docs/02_architecture/`）—— 不创建为 ADR
- **Research 文档**（已在 `docs/05-advanced/`）—— 不创建为 ADR
- **实施 change 计划**（per ADR 实施时另开 `openspec/changes/` 目录）

### 0.3 创建流程（建议）

```
Wave 0a: ADR-087 起草 + 4-owner 评审启动（关键路径，per arch v0.3 §12.6）
Wave 0b: ADR-077/078/080/086 起草并行；Accept 以 ADR-087 Accepted 为门
↓
Wave 1: ADR-079 + CXL Scope A
↓
Wave 2: ADR-081 (HAL Phase-1 batch, descriptor-based)
↓
Wave 3: ADR-083 (多播语义, switch 进程 fan-out) + ADR-084 (PGAS 地址)
↓
Wave 4: ADR-085 (消费者契约) + ADR-082 (HAL Phase-2 batch) 触发
```

**每个 ADR 的标准创建流程**（per ADR-035 §R2）：

1. 在 `docs/00_adr/` 创建 `adr-NNN-slug.md`（🔄 Proposed 状态）
2. 使用 H-4 标准 frontmatter（per ARCH §0.1）+ 标准 §结构（Context / Decision / Consequences / Migration）
3. 同步更新 `docs/00_adr/README.md` 索引表 + 状态分布总览
4. 关联文档交叉引用双向检查
5. `tools/docs-audit.sh` 通过后才视为正式落地

---

## §1 总览：12 个 ADR 一览（v0.3 修订）

| ADR# | 标题 | 焦点决策 | 类型 | 阻塞 | LOC 影响 | Gate |
|------|------|---------|------|------|---------|------|
| **077** | 节点 Fabric 地址模型 | NFA 命名空间 charter + base 值 + VA→NFA→device 翻译栈 + 内存池 backing 归属 + **region descriptor policy（per ADR-087 D2）**| 基础设施 | Wave 2 | 0（治理）| ADR-087 D2 + 用户审阅 |
| **078** | **L1 Switch 进程拆分 + CppLink 协议 + CppTLM SoC spec（合并原 ADR-088）** | 独立 CPU 进程（per v0.5 §9.1 + ADR-087 D6）+ CppLink v1.0 4 通道 + **CppTLM SoC spec（`soc_arch/switch/`）**+ 双角色保留 + 平面语义边界 | 基础设施（**3-owner critical path**）| Wave 2 | 0（治理）| 用户审阅 + ADR-087 D3 + CppTLM owner |
| **079** | Coherence 协议模拟 Non-Goal + 跨进程一致性契约 | 不模拟目录/MESI/snoop 协议 + **3-level consistency contract（Level 0/1/2 per Oracle §2.5）**+ 行业证据 | 基础设施 | Wave 2 | 0（治理）| ADR-087 D2 |
| **080** | 多实例 GPU 重构前置 + IPC readiness | `g_vram_store` → 节点分区 + spike GO/NO-GO + **IPC readiness 验证**（Unix socket + memfd + SCM_RIGHTS per ADR-087 D2）| 基础设施 | Wave 2 | ~200（spike）+ 后续重构 | Spike GO + ADR-087 D2 |
| **081** | Fabric HAL Phase-1 batch | H3（switch route）+ H4（mem pool attach），2-4 个 fn-ptr（**descriptor-based signatures per ADR-087 D2**）| HAL 扩展 | Wave 2-3 | 1500-2500（实施）| W0 + W1 + ADR-087 D2 |
| **082** | Fabric HAL Phase-2 batch | H6（mcast，**fan-out 委托 switch_sim 进程 per D6**）+ H7（NIC），2-4 个 fn-ptr | HAL 扩展 | Wave 3-4 | 2200-3300（实施）| W2 测试绿 |
| **083** | PGAS 多播语义 | replicated-object + **switch_sim 进程（CppTLM）fan-out per D6**+ 完成机制 + reduction Non-Goal 确认 | 语义 | Wave 3 | 1500-2500（实施，CppTLM 仓实施）| ADR-077 + ADR-078 |
| **084** | PGAS 地址语义 | zone 划分 + 翻译规则（NVSHMEM symmetric heap 参照）| 语义 | Wave 3 | 0（治理，部分并入 ADR-077）| ADR-077 Accepted |
| **085** | **消费者契约 + TaskRunner ISwitchDriver（合并原 ADR-090）** | TaskRunner `IFabricDriver` + `ISwitchDriver`（v0.3 新增）+ CppLink layer 正交 + IMEX/NVLink Fusion 对照 + 3 仓 switch 架构契约 | 治理 | Wave 4 | 0（治理）| Wave 4 gate + CppTLM/PTX-EMU/TaskRunner owner |
| **086** | Trigger Registry 整合 | 单 trigger 入口（8 类 trigger 合并）+ **Multi-Process / switch_sim repo / CppLink ABI / ISwitchDriver triggers（v0.3 新增 4 项）** | 治理 | Wave 0 | 0（治理）| 无 |
| **087** | **Multi-Process Device & Fabric Seam** | **跨仓跨进程边界治理**（D2 policy / D6 IMP-4 决策 / D8 ADR-011 边界 / D9 4-owner 评审）| 治理（**4-owner critical path**）| Wave 0a | 0（治理）| **4-owner Accepted** |
| **089** 🆕 | **L1 Switch Device Driver** | UsrLinuxEmu `plugins/switch_driver/` 模块结构 + IOCTL 契约（`SWITCH_IOCTL_*`）+ HAL bridge + **借鉴 `gpu_driver` 模板（per ADR-087 D6.2 + user 2026-08-14 决策）** | 基础设施（**2-owner 评审：UsrLinuxEmu + CppTLM**）| Wave 2 | 1500-2500（实施）| ADR-077 + ADR-078 |

**ADR 编号策略**：
- **088**：**不创建**（合并到 ADR-078 §4.3 扩展 — CppTLM SoC spec）
- **090**：**不创建**（合并到 ADR-085 §11 — ISwitchDriver 契约）
- **091+**：v2 扩展按需
- **总 ADR 数量**：**12**（077-087 + 089）

**LOC 合计**：治理 ADR 全部 0 LOC；实施 ADR（**081/082/083/089**）合计 **6700-10800 LOC**（per arch v0.5 §13.2 估算 + ADR-089 新增）。

---

## §2 创建顺序与 Gate 依赖

### 2.1 推荐创建顺序（v0.3 修订）

```
Wave 0a（4-owner 关键路径）：
  1. ADR-087（Multi-Process Device & Fabric Seam）—— 治理（4-owner 评审启动；D6 IMP-4 已决）

Wave 0b（并行起草；Accept 以 ADR-087 Accepted 为门）：
  2. ADR-077（节点 Fabric 地址模型）—— 基础设施，决定 §3.1/§3.3 charter + 内存池归属 + region descriptor 引用 D2
  3. ADR-078（L1 Switch 进程拆分 + CppLink 协议 + CppTLM SoC spec）—— 基础设施，3-owner 评审（UsrLinuxEmu + CppTLM + TaskRunner）
  4. ADR-080（多实例前置 + IPC readiness）—— 基础设施，决定 §7 实施路径
  5. ADR-086（Trigger Registry 整合）—— 治理整合（汇总 §11.3 trigger 表 + 4 项 v0.3 新增）

Wave 1（基础设施补全）：
  6. ADR-079（Coherence Non-Goal + 跨进程一致性契约）—— 基础设施补全

Wave 2（设备 driver + HAL 扩展，descriptor-based）：
  7. ADR-089（L1 Switch Device Driver，UsrLinuxEmu）—— 借鉴 gpu_driver 模板
  8. ADR-081（HAL Phase-1 batch）—— descriptor-based signatures（per ADR-087 D2）

Wave 3（语义 + 进程内 fan-out）：
  9. ADR-083（多播语义，**switch_sim 进程 fan-out per D6**）—— 由 CppTLM 实施
  10. ADR-084（PGAS 地址语义）—— 与 ADR-083 评审并行

Wave 4（消费者契约 + HAL Phase-2 触发）：
  11. ADR-085（消费者契约 + IFabricDriver + **ISwitchDriver 合并 ADR-090**）—— 硬门，3-owner
  12. ADR-082（HAL Phase-2 batch）—— 由 Wave 4 trigger 触发
```

**关键路径**：
- **ADR-087**（4-owner critical path）必须 Wave 0a 启动
- **ADR-078**（3-owner：UsrLinuxEmu + CppTLM + TaskRunner）也是关键路径
- **ADR-089**（2-owner：UsrLinuxEmu + CppTLM）Wave 2 启动；需 ADR-077/078 Accepted
- 启动 ADR-078 或 ADR-089 晚于 ADR-087 都增加整体风险

### 2.2 Gate 依赖图

```
ADR-087 (seam governance, 4-owner critical path) ──→ 所有 Wave 0b/1/2+ ADR Accept 门
       │
ADR-080 (spike GO + IPC readiness) ──→ 所有 Wave 2+ ADR 实施
       │
ADR-077 (NFA charter, descriptor 引用 ADR-087 D2) ──→ ADR-083, ADR-084, ADR-081, ADR-082
       │
ADR-078 (process split + CppLink v1.0) ──→ ADR-081, ADR-082, ADR-083 (fan-out 位置)
       │
ADR-079 (Non-Goal) ──→ 无直接依赖（独立治理）
       │
ADR-086 (Trigger registry, 11+ 类) ──→ 无直接依赖（独立治理）
       │
ADR-083 (多播语义, switch 进程 fan-out) ──→ ADR-082 (H6 multicast op)
       │
ADR-084 (PGAS 地址) ──→ ADR-083 (PGAS zone 多播映射)
       │
ADR-085 (消费者契约 + CppLink layer 正交) ──→ 所有 Wave 4+ 实施 gate
```

**关键观察**：**ADR-087 是 4-owner 关键路径**，必须在 Wave 0b ADR Accept 之前 Accepted。**所有基础设施 ADR（077-080）必须在 Wave 2 之前 Accepted**，否则后续实施 ADR 无基础。**ADR-086 是治理整合，独立可早期完成**。

### 2.3 创建期间的并行/串行约束

- **可并行**：077 + 078 + 080 + 086（Wave 0b，per ADR-087 Accepted 门控）
- **需串行**：079 → 081（079 是 081 设计依据之一）
- **可并行**：086 + 任何基础设施 ADR（086 是 trigger 整合，无内容依赖）
- **需串行**：083 + 082（083 定义多播语义 → 082 H6 实现）
- **需串行**：084 + 083（084 定义 PGAS zone → 083 多播映射规则）
- **需串行**：087 → 077/078/080/086 Accept（ADR-087 是 Wave 0b 全部 ADR 的 Accept 门）

---

## §3 ADR-077 草案：节点 Fabric 地址模型

### 3.1 文件名

`docs/00_adr/adr-077-node-fabric-address-model.md`

### 3.2 Frontmatter（建议）

```markdown
# ADR-077: 节点 Fabric 地址模型（NFA Charter）

**状态**: 🔄 Proposed
**日期**: 2026-08-14
**提案人**: UsrLinuxEmu Architecture Team（基于 scale-up-fabric-architecture.md v0.2 §3 + Oracle 综合评审 `ses_00370deb6` U-03）
**评审者**: UsrLinuxEmu Architecture Team + Architecture Owner 签字

**关联 ADR**:
- [ADR-023](adr-023-hal-interface.md) ✅ HAL 接口契约（incremental base）
- [ADR-036](adr-036-three-way-separation.md) ✅ 3 区分原则（NFA 由 ③ switch sim 拥有）
- [ADR-035](adr-035-governance-policy.md) ✅ 治理规则（本 ADR 走 §R3）
- [ADR-058](adr-058-sim-mem-pool-real-va.md) ✅ sim_mem_pool（backing 复用）
- [ADR-069](adr-069-bar-ioremap-emulation.md) ✅ BAR 定基址（per-device PA 命名空间）
- [ADR-073](adr-073-dma-coherent-emulation.md) ✅ DMA 独立命名空间
- [ADR-078](adr-078-switch-driver-plugin.md) 📋 插件拆分（协作 ADR）
- [ADR-080](adr-080-multi-instance-gpu-prerequisite.md) 📋 多实例前置（g_vram_store 重构）

**关联文档**:
- [scale-up-fabric-architecture.md](../02_architecture/scale-up-fabric-architecture.md) v0.2 §3（本 ADR 是该 SSOT 的基础设施 ADR 之一）
- [scale-up-fabric-research.md](../05-advanced/scale-up-fabric-research.md) v0.2 §1.4（用户愿景）
- NVIDIA Rubin 深度调研报告 [R §5.4]（VA→PA→FA 三地址模型来源）
- Oracle 综合评审 `ses_003b3fa7` §3.1（SYN-3 + 命名错位识别）
```

### 3.3 §结构大纲

#### Context
- UsrLinuxEmu scale-up 轨道的核心架构新元素：节点级 fabric 地址空间
- 既有命名空间（ADR-058 device VA / ADR-069 BAR physical / ADR-073 DMA addr）的角色与重叠风险
- 行业先例：NVIDIA IMEX 的 VA→PA→FA 三地址模型（v0.2 §3.1 已整合）
- 用户原方案使用 "Node Unified PA" 命名 → Oracle 评审指出与 per-device PA 概念冲突，建议改 NFA

#### Decision（核心）
- **D1**: 新增第五命名空间 `Node Fabric Address (NFA)`，base `0x100_0000_0000`（1 TiB），与既存 4 个严格互斥
- **D2**: NFA 由 ③ switch sim 拥有，backing 是 node-wide mmap（refactored `g_vram_store`）
- **D3**: 翻译栈为 2 层（v1 简化）：`UVM VA → NFA → device-local partition`
  - **层省略声明**：Rubin 三地址模型中的 device-local PA 在仿真器中省略（backing store partition offset 即之），属实现简化而非架构决策
- **D4**: 静态分区（v1）：节点启动时分配固定 NFA 区间；无动态路由表（v2 升级选项）
- **D5**: Memory pool backing 归属：复用 ADR-058 `plugins/gpu_driver/sim/mem_pool`；switch sim 持有节点分区视图（经 ServiceRegistry 引用）
- **D6**: **IPC region descriptor 引用**（per ADR-087 D2 policy）：NFA region 在跨进程场景下以 descriptor 形式发布；descriptor 字段集与魔数校验由 ADR-087 锁定，本 ADR 仅引用 NFA↔descriptor 的映射关系（policy/artifact 分离）

#### Consequences
**正面**：
- ✅ 与 NVIDIA FA 模型同构，便于评审者类比理解
- ✅ 节点内统一编址使 fabric load/store 语义可表达
- ✅ 5 个命名空间 charter 清晰，避免 base 值混淆

**负面**：
- ⚠️ v1 无动态 NFA 重映射（FM 角色 v2 化）
- ⚠️ device-local PA 层省略可能让熟悉 CUDA 的评审者疑惑（已在 D3 显式声明）
- ⚠️ Memory pool backing 在 gpu_driver/sim 而非 switch_driver/sim——轻微违反"节点级资源归节点级插件"的直觉（D5 给出复用 ADR-058 的 LOC 节省理由）

**风险**：
- 🟡 Rubin's documented fabric address layer 行为（可重映射）可能在 v1 实现中简化掉——ADR-079 + ADR-083 应明确"v1 静态、不重映射"
- 🟡 内存池 backing 归属争议——ADR-078 评审可能反对（届时 ADR-077 需调整）

#### Migration
- **零代码**：本 ADR 是治理决策，不直接产生代码
- **D2 的 `g_vram_store` 重构**：实际重构走 ADR-080（多实例前置），本 ADR 仅声明 backing 形式
- **测试影响**：现有 98 个 test binary 应保持通过（per ADR-080 spike 检查表）

### 3.4 审阅 Checklist
- [ ] D1 base 值 `0x100_0000_0000` 是否与既存 4 个命名空间互斥？
- [ ] D3 翻译栈 2 层简化是否需要 3 层（device-local PA 显式）？
- [ ] D5 内存池 backing 归属是否应改为 switch_driver/sim？
- [ ] 关联 ADR 列表是否完整（是否需要 ADR-023 HAL 接口作为约束）？

**Wave 依赖**：ADR-077 NFA charter 引用 ADR-087 D2 descriptor policy（policy/artifact 分离）；本 ADR Accept 需 ADR-087 Accepted。

---

## §4 ADR-078 草案：L1 Switch 进程拆分 + CppLink 协议（v0.3 扩展：CppTLM SoC spec 合并）

### 4.1 文件名

`docs/00_adr/adr-078-l1-switch-process-and-cpplink.md`

### 4.2 Frontmatter（v0.3 修订：含 CppTLM SoC spec）

```markdown
# ADR-078: L1 Switch 进程拆分 + CppLink 协议 + CppTLM SoC spec

**状态**: 🔄 Proposed
**日期**: 2026-08-14
**提案人**: UsrLinuxEmu Architecture Team（基于 scale-up-fabric-architecture.md v0.5 §4.6 + §9 + **ADR-087 D6 IMP-4 决策**）
**评审者**: **3-owner critical path**
- UsrLinuxEmu Architecture Team（② driver 端）
- CppTLM owner（③ SoC sim 端）
- TaskRunner owner（① consumer 端）

**关联 ADR**:
- [ADR-023](adr-023-hal-interface.md) ✅ HAL 接口契约（CppLink-D/E/T HAL bridge 约束）
- [ADR-035](adr-035-governance-policy.md) ✅ 治理规则
- [ADR-036](adr-036-three-way-separation.md) ✅ 3 区分原则（**v0.3：每仓独立维护 ①②③**）
- [ADR-038](adr-038-network-stack-three-way-separation.md) ✅ net_driver 模板
- [ADR-076](adr-076-076-gpgpu-kernel-module-ioctl.md) ✅ 跨仓 HAL 扩展流程模板
- [ADR-077](adr-077-077-node-fabric-address-model.md) 📋 NFA charter（协作 ADR）
- [ADR-080](adr-080-080-multi-instance-gpu-prerequisite.md) 📋 多实例前置
- [ADR-083](adr-083-083-multicast-semantics.md) 📋 多播语义（fan-out 在 switch_sim 进程内 per D6）
- [ADR-085](adr-085-085-consumer-contract.md) 📋 消费者契约（含 ISwitchDriver）
- [ADR-087](adr-087-multi-process-device-fabric-seam.md) 🔄 **D2/D3/D6 政策来源**（4-owner 关键路径）
- [ADR-089](adr-089-l1-switch-device-driver.md) 📋 L1 Switch device driver（v0.3 新增，② 端 UsrLinuxEmu 实施）

**关联外部 ADR**:
- CppTLM ADR-SOC-04 (v1) + ADR-SOC-06 (v2, 规划中) + ADR-SOC-07 (IPC seam, 规划中)
- PTX-EMU ADR-0029 §D7（minimal split — 同模式参考）
- TaskRunner tadr-307（IGpuDriver 姊妹文档）

**关联文档**:
- [scale-up-fabric-architecture.md](../02_architecture/scale-up-fabric-architecture.md) **v0.5** §4.6 CppLink 协议 + §9 进程架构 + §13.4 Phase 对齐
- [multi-process-gpu-simulator-integration.md](../02_architecture/multi-process-gpu-simulator-integration.md) v0.1（Oracle 调研综合）
- NVIDIA Rubin 深度调研报告 [R §3.8]（NVSwitch ≠ NIC 实证）
- Oracle 评审 `ses_00370deb6` §3.3, §4.5 + `ses_000f92e51` §3 Q2
- Multi-Process Vision Spec §3.1 D1.b（per-GPU process split 依据）
```

### 4.3 §结构大纲

#### Context
- scale-up 轨道需要节点级 fabric 仿真（节点内 GPU 互联 + 内存池接入 + scale-out NIC）
- Multi-Process Vision D1.b：per-GPU process split（已采纳）
- 用户愿景：L1 Switch 双角色（fabric switch + NIC）
- **v0.3 新增：ADR-087 D6 IMP-4 决策**——L1 Switch **跨 3 仓实现**（与 GPU 软件栈同构）：
  - ③ 硬件 sim → **CppTLM** `soc_arch/switch/`
  - ② 设备 driver → **UsrLinuxEmu** `plugins/switch_driver/`（per ADR-089）
  - ① 用户态 API → **TaskRunner** `src/switch/`（per ADR-085）
- Rubin 实证：NVSwitch 与 NIC 是分离器件（"combined NIC and switch — Not supported" [R §9.4]）—— v0.3 偏离需有意识论证
- 现有 net_driver 资产可承担 scale-out RDMA 角色（备选 v2）
- Oracle CppLink 协议初版（per arch v0.3 §4.6）—— 4 通道 + region descriptor

#### Decision（核心，v0.3 修订）
- **D1**: **L1 Switch 跨 3 仓实现**（per ADR-087 D6 + user 2026-08-14 决策）：
  - ③ 硬件 sim：CppTLM `soc_arch/switch/`（与 `soc_arch/gpu/` 同构；TLM 模型 + NFA routing + multicast + NVLink timing）
  - ② 设备 driver：UsrLinuxEmu `plugins/switch_driver/`（与 `plugins/gpu_driver/` 同构；IOCTL + HAL；详见 ADR-089）
  - ① 用户态 API：TaskRunner `src/switch/`（与 `src/cuda/` 同构；ISwitchDriver 契约；详见 ADR-085）
  - **v1 借鉴 GPU 软件栈**（per user 决策）；后续 ADR 可评估独立软件栈
- **D2**: **进程拓扑**（per arch v0.5 §2.2）：UsrLinuxEmu CPU sim + CppTLM switch_sim + TaskRunner switch_userspace + N 个 GPU sim 进程
- **D3**: **CppLink v1.0 协议**（per arch v0.3 §4.6）：
  - 4 通道（C/D/E/T）+ region descriptor（含 magic/kind/abi_version 字段 per ADR-087 D2）
  - 启动序列：Switch 监听 + CPU/GPU 接入 + readiness 屏障
  - v1 = fail-fast + 心跳参数 + 进程崩溃 fail-fast（per ADR-087 D4）
- **D4**: **双角色保留**（fabric + NIC 合并在 Switch sim 进程内），含 net_driver 备选裁决（v2 评估）
- **D5**: **平面语义边界**：fabric 平面 ops（route/multicast）与 scale-out 平面 ops（rdma_send/recv）分开命名（HAL H3/H6 vs H7）
- **D6**: **跨仓接口契约**（per ADR-087 D6.4）：
  - CppLink-D 数据：routing / MMIO proxy / doorbell 下发
  - CppLink-C 控制：endpoint 注册 / capability negotiate
  - CppLink-E 事件：fence / 中断 / 多播完成
  - CppLink-T tick：模拟时钟同步

#### Consequences
**正面**：
- ✅ 与 Vision D1.b + ADR-087 D6 决策一致；3 仓同构于 GPU 软件栈
- ✅ **v0.3 IMP-4 已决**——§13.4 Phase 4 条件性 gate 解除
- ✅ GPU driver 进程不嵌入 switch 逻辑（per-process 边界，符合 ADR-036 clean）
- ✅ CppLink v1.0 协议可被 ADR-087 4-owner 评审独立审计
- ✅ 借鉴 GPU 软件栈复用大量模板（per user 决策）

**负面**：
- ⚠️ 偏离 Rubin 平面分离（业界首例）—— 需在评审中解释
- ⚠️ §15 风险 #1 🔴 High 持续存在：dual-role switch 无真 Linux 内核驱动 archetype
- ⚠️ 3 仓治理需要 3-owner 评审，关键路径
- ⚠️ CppLink ABI 在 CppTLM / UsrLinuxEmu / TaskRunner 三仓同步，文档维护成本

**风险**：
- 🔴 真硬件 archetype 缺失——已在 §15 风险 #1 登记，缓解 = 备选记录
- 🟡 评审者（TaskRunner owner）可能要求"先实现 net_driver 路径"——需说明 v1 简化优先
- 🟡 CppTLM 与 UsrLinuxEmu 实施时序可能错位（W2 + W3 gates）

#### Migration
- **零代码**：本 ADR 是治理决策
- **v1 实施**（per ADR-087 D6 + user 决策）：
  - CppTLM 仓 `soc_arch/switch/` 新建子目录（per ADR-SOC-04 v1 黑盒 + ADR-SOC-06 v2 选型）
  - UsrLinuxEmu 仓 `plugins/switch_driver/` 新建插件（**per ADR-089 草案**，详见 §12b）
  - TaskRunner 仓 `src/switch/` 新建库（**per ADR-085 扩展**，详见 §11）
- **实施触发**：Wave 2（per `scale-up-fabric-architecture.md` §13.1）

### 4.4 审阅 Checklist
- [ ] D1 3 仓归属是否清晰？ADR-089 + ADR-085 范围是否避免重复？
- [ ] D3 CppLink region descriptor 字段集是否完整（magic/kind/abi_version per ADR-087 D2）？
- [ ] D5 平面语义边界是否清晰？fabric vs scale-out ops 在 HAL 层如何命名？
- [ ] D6 跨仓接口契约是否可被 3-owner 联合评审通过？
- [ ] CppLink v1.0 4 通道（C/D/E/T）是否覆盖所有 Phase 3 IPC seam（vision L175-180）？
- [ ] CppTLM SoC spec（合并自原 ADR-088）是否需要独立子节？

### 4.4a 命名与归属冲突（v0.3 解决）

- ✅ **switch_sim 代码归属**：**已决**——归 CppTLM 仓（per ADR-087 D6 IMP-4 决策）
- ✅ **"插件" 措辞移除**：v0.2 不再使用 "switch_driver 插件"，改为 "switch_sim 进程"
- ✅ **ServiceRegistry 移除**：v0.2 不假设单进程内 ServiceRegistry 查找；改用 CppLink-C endpoint 直接连接
- ✅ **net_driver 备选 reword**：v0.2 标注 net_driver 备选承担 scale-out 角色意味着 endpoint 在 CPU 进程内（不在 Switch 进程）；v1 保留双角色
- ✅ **原 ADR-088（独立 CppTLM SoC ADR）**——v0.3 决策：**合并**到本 ADR §D1 (③ sim 部分)

---

## §5 ADR-079 草案：Coherence 协议模拟 Non-Goal

### 5.1 文件名

`docs/00_adr/adr-079-coherence-protocol-non-goal.md`

### 5.2 Frontmatter

```markdown
# ADR-079: Coherence 协议模拟 Non-Goal（Rubin 行业实证背书）

**状态**: 🔄 Proposed
**日期**: 2026-08-14
**提案人**: UsrLinuxEmu Architecture Team（基于 scale-up-fabric-architecture.md v0.2 §6 + Oracle 评审 U-02）
**评审者**: UsrLinuxEmu Architecture Team

**关联 ADR**:
- [ADR-023](adr-023-hal-interface.md) ✅ HAL 接口契约
- [ADR-036](adr-036-three-way-separation.md) ✅ 3 区分原则
- [ADR-073](adr-073-dma-coherent-emulation.md) ✅ DMA 一致性仿真（**核心依据**：D1 简化原则）
- [ADR-064](adr-064-memory-model-staging.md) ✅ 内存模型保真度分阶段（Non-Goal 与其哲学一致）

**关联文档**:
- [scale-up-fabric-architecture.md](../02_architecture/scale-up-fabric-architecture.md) v0.2 §6.1
- NVIDIA Rubin 深度调研报告 [R §2.5, §9.4]（GPU↔GPU 软件管理 + "72 GPU 全局硬件相干 Not supported"）
- Oracle 评审 `ses_00370deb6` §3.2（行业证据强化）
```

### 5.3 §结构大纲

#### Context
- scale-up 轨道涉及 cache 一致性问题
- 原始用户愿景（`scale-up-fabric-research.md` v0.1）主张"硬件 cache coherent"
- 仿真器一致性建模选项：directory protocol / MESI / snoop / 完全不模拟
- ADR-073 D1 简化原则已为类似决策提供先例

#### Decision（核心）
- **D1**: 不模拟 cache coherence 协议（directory / MESI / snoop / snooping）
- **D2**: 不模拟 directory-based protocol 决策（`scale-up-fabric-research.md` §4.4.2 原推荐被本 ADR 覆盖）
- **D3**: 实施 CXL Scope A 范围内的 cache line flush event（`GPU_MMU_EVENT_CACHE_FLUSH`）—— 让该事件有真实语义（per CXL.cache 协议）
- **D4**: 在仿真器内：单进程 CPU/GPU 共享地址空间，cache coherence 天然 free（per ADR-073 D1 简化原则）
- **D5**: 愿景硬件的一致性语义：软件管理 / PGAS release —— 与 Rubin GPU↔GPU 模型一致 [R §2.5]

#### Consequences
**正面**：
- ✅ 节省 5-10K LOC（per `scale-up-fabric-architecture.md` §6.1）
- ✅ ADR-073 简化原则的逻辑延伸，治理一致
- ✅ NVIDIA Rubin 行业实证背书（72 GPU 全局硬件相干"Not supported"）
- ✅ 仿真器提供的一致性 = 任何真实硬件语义的超集近似——驱动在其上开发的代码不会因仿真器过强而学到错误习惯

**负面**：
- ⚠️ 期望"硬件 cache coherent"的驱动可能在仿真器内通过、迁移到真硬件时遇到 sync 问题（仿真器过强风险）
- ⚠️ §6.1 一致性分级表需清晰说明 5 个级别（v0.2 已就绪）

**风险**：
- 🟢 低：Rubin 行业背书 + ADR-073 先例 = 评审阻力小

#### Migration
- **零代码**：本 ADR 是治理决策
- **deletion from future scope**：ADR-081/082 不应包含任何 coherence 协议相关 fn-ptr
- **测试**：所有现有测试应保持通过（仿真器一致性增强不影响测试）

### 5.4 审阅 Checklist
- [ ] D1 范围是否清晰（directory / MESI / snoop / snooping 全包）？
- [ ] D3 与 CXL Scope A 边界是否清晰？
- [ ] D5 "软件管理 / PGAS release" 是否需要更精确的语义描述？

---

## §6 ADR-080 草案：多实例 GPU 重构前置 + IPC Readiness Spike

### 6.1 文件名

`docs/00_adr/adr-080-multi-instance-gpu-prerequisite.md`

### 6.2 Frontmatter

```markdown
# ADR-080: 多实例 GPU 重构前置（g_vram_store 节点分区 + Spike GO/NO-GO）

**状态**: 🔄 Proposed
**日期**: 2026-08-14
**提案人**: UsrLinuxEmu Architecture Team（基于 scale-up-fabric-architecture.md v0.2 §7 + ADR-023 2026-08-02 修订）
**评审者**: UsrLinuxEmu Architecture Team

**关联 ADR**:
- [ADR-023](adr-023-hal-interface.md) ✅ HAL 接口契约（kfd_sim_bridge 修订提到 g_vram_store 进程全局）
- [ADR-036](adr-036-three-way-separation.md) ✅ 3 区分原则（重构后归属 ③ sim）
- [ADR-058](adr-058-sim-mem-pool-real-va.md) ✅ sim_mem_pool（节点分区视图共享）
- [ADR-069](adr-069-bar-ioremap-emulation.md) ✅ BAR 定基址（per-device BAR 窗口）
- [ADR-077](adr-077-node-fabric-address-model.md) 📋 NFA charter（backing store 形式）

**关联 Change**: 
- `openspec/changes/2026-08-15-multi-instance-gpu-spike/`（Wave 0 第一个 change —— GO/NO-GO spike）

**关联文档**:
- [scale-up-fabric-architecture.md](../02_architecture/scale-up-fabric-architecture.md) v0.2 §7
- Oracle 综合评审 `ses_003b3fa7` §3.3（CONF-3 识别）
```

### 6.3 §结构大纲

#### Context
- scale-up 轨道需要 N 个 GPU 实例共存（N=8 v1）
- 既有架构：process-global sim 状态（`g_vram_store.init(256 MB)` per ADR-023 2026-08-02 kfd_sim_bridge 修订）
- 单 GPU 测试（98 个 test binary）+ 单 GPU 仿真器全局状态 = 多实例直接冲突
- ADR-069 单 BAR 布局 `0x10000000` 不支持 per-device BAR 窗口

#### Decision（核心）
- **D1**: 重构 `g_vram_store` 为节点级 backing store，按 device 分区（per `scale-up-fabric-architecture.md` §7.2）
- **D2**: Wave 0 必做 **Multi-Instance + IPC-Readiness Spike**（仿 `stage-2-spike-report.md` GO/NO-GO 格式），500 行 prototype 评估以下：
  - 2 个 `gpu_driver` 实例同时加载 → VFS 注册都成功
  - per-device BAR 窗口区分 → 调试接口可枚举 N 个 BAR 窗口
  - 重构后 per-device 内存隔离 → device0 写入不影响 device1
  - 现有 98 个测试 binary 仍绿 → 零 regression
  - **Unix socket 创建**（CppLink-C control channel）→ connector/listener 双方连通
  - **`memfd_create` + `SCM_RIGHTS` FD 传递**验证 → 跨进程 file descriptor 安全传递
  - **跨进程 mmap**验证（host same backing store 不同 process-local address）→ region descriptor 语义前置
  - **CppLink ABI version 协商**（不兼容拒绝 + 日志）→ ADR-087 D3 政策前置验证
- **D3**: Spike GO → 本 ADR Accepted；Spike NO-GO → 阻塞 Wave 2+ 全部 ADR 实施
- **D4**: 重构方向（负债变资产）：`g_node_pa_store.init(N * per_device_size + mem_pool_size + nic_size)` → 同时解决 CONF-3 + scale-up 需求

#### Consequences
**正面**：
- ✅ 一次性解决多实例 + scale-up 节点级 backing
- ✅ 与 ADR-058 sim_mem_pool（per-pool buddy）天然衔接
- ✅ spike GO/NO-GO 提供清晰决策门

**负面**：
- ⚠️ 重构触及 process-global 状态，影响所有 sim 路径 → spike 必须零 regression
- ⚠️ ADR-069 BAR 单窗口需扩展为 per-device（N 个 BAR 区段）
- ⚠️ spike 本身 ~200 LOC + 后续重构 ~1000-1500 LOC

**风险**：
- 🔴 Spike NO-GO → 整个 scale-up 轨道阻塞
- 🟡 per-device BAR 布局在仿真器中需新增枚举接口（如 VFS `/proc/iomem` 等价物）

#### Migration
- **Phase 1（Wave 0）**：Spike 创建 `openspec/changes/2026-08-15-multi-instance-gpu-spike/`，GO/NO-GO 判定
- **Phase 2（Wave 1-2 间）**：GO 后启动重构：
  - 拆分 `g_vram_store` → `g_node_pa_store` + per-device partitions
  - 修改 VFS BAR 注册路径支持 N 个窗口
  - 同步更新所有引用 `g_vram_store` 的 sim 路径
- **测试**：98 个现有测试必须绿；新增 multi-instance test（spike 必交付）

### 6.4 审阅 Checklist
- [ ] D2 spike 检查表是否完整？是否需加 `getenv("USR_LINUX_EMU_N_GPU")` 模拟参数化？
- [ ] D4 重构方向是否需要更具体的命名（`g_node_pa_store` vs `g_vram_store_node`）？
- [ ] BAR 枚举接口应使用什么 VFS 路径？
- [ ] spike 失败时是否需要 fallback 路径（如仅支持 N=2 而非 N=8）？

---

## §7 ADR-081 草案：Fabric HAL Phase-1 batch

### 7.1 文件名

`docs/00_adr/adr-081-fabric-hal-phase-1.md`

### 7.2 Frontmatter

```markdown
# ADR-081: Fabric HAL Phase-1 Batch（switch route + mem pool attach）

**状态**: 🔄 Proposed（Wave 2 启动前升至 Accepted）
**日期**: 2026-08-14
**提案人**: UsrLinuxEmu Architecture Team（基于 scale-up-fabric-architecture.md v0.2 §8 + ADR-023 append-only 模式）
**评审者**: UsrLinuxEmu Architecture Team + Architecture Owner（HAL append-only 一致性）

**关联 ADR**:
- [ADR-023](adr-023-hal-interface.md) ✅ HAL 接口契约（**本 ADR 是其扩展**，per Decision 4 spec-driven "追加不改"）
- [ADR-061](adr-061-hal-iommu-extension.md) ✅ HAL IOMMU ops 扩展（**模式借鉴**：本批次同样 append-only）
- [ADR-076](adr-076-gpgpu-kernel-module-ioctl.md) ✅ 跨仓 HAL 扩展流程模板
- [ADR-077](adr-077-node-fabric-address-model.md) 📋 NFA charter（fn-ptr 参数依据）
- [ADR-078](adr-078-switch-driver-plugin.md) 📋 插件结构（sim 实现位置）

**关联文档**:
- [scale-up-fabric-architecture.md](../02_architecture/scale-up-fabric-architecture.md) v0.2 §8 H3-H4
- NVIDIA Rubin 深度调研报告 [R §10.3]（switch 能力候选池）
```

### 7.3 §结构大纲

#### Context
- Wave 2 实施需要 HAL 层 ops 支持 fabric 核心
- ADR-023 append-only 规则：每新增 fn-ptr 走 ADR-035 R3
- ADR-059 D3 条件 4：每个新 op 需"≤5 行组合不可行性证据"
- 现有 HAL 68 fn-ptr（per `gpu_hal.h:4`）—— Phase-1 batch 预计新增 2-4 个

#### Decision（核心）
- **D1**: 新增 `hal_fabric_attach(ctx, fabric_endpoint_descriptor_t *ep)` —— GPU 注册到 fabric（descriptor-based，per ADR-087 D2）
- **D2**: 新增 `hal_fabric_detach(ctx, fabric_endpoint_descriptor_t *ep)` —— 反注册
- **D3**: 新增 `hal_fabric_route(ctx, region_id, offset, size, generation)` —— fabric 路由查询/编程（v1 = O(1) range check，无表；descriptor-based 参数，无 raw pointer）
- **D4**: **预设拒绝**：`hal_mem_pool_attach_fabric` —— 复用 ADR-058 既有 `mem_pool_*` ops（per CONF-4 验证 + ADR-023 > 50 阈值指引）
- **D5**: 严格遵循 ADR-023 append-only，**不动**现有 68 个 fn-ptr 签名
- **D6**: 跨仓契约（per ADR-076 + ADR-035 R5）：本批次仅内部 HAL 扩展，**不**新增 System C ioctl；如未来需要 ioctl 走独立 ADR

#### Consequences
**正面**：
- ✅ 与 ADR-023 治理严格一致
- ✅ 与 ADR-061 / ADR-076 同模式，评审者熟悉
- ✅ 2-3 个新 op 落在 HAL < 80 阈值内，不触发 SSOT §1.10.2 "复用优先"警告

**负面**：
- ⚠️ HAL 68 → 70-71 fn-ptrs（接近阈值）
- ⚠️ ADR-059 D3 条件 4 证据需逐 op 提供
- ⚠️ 后续 Wave 3/4 还需要追加 fn-ptr（ADR-082）

**风险**：
- 🟡 评审时若被要求合并 H4 到现有 mem_pool ops，本批次可能降至 2 个新 fn-ptr
- 🟡 模拟性能影响（H3 路由每 load/store 调用）—— v1 静态可忽略，v2 动态需优化

#### Migration
- **Phase 1**：Wave 2 启动前升至 Accepted
- **Phase 2**：实施
  - 修改 `plugins/gpu_driver/hal/gpu_hal_ops.h`：末尾追加 3 个 fn-ptr（per ADR-023 D4 append-only）
  - 修改 `plugins/gpu_driver/hal/gpu_hal.h`：追加 3 个 inline wrapper
  - 修改 `plugins/gpu_driver/sim/` + `plugins/gpu_driver/drv/` + `bin/switch_sim/sim/`：实现 + mock
  - 新增 `tests/test_fabric_hal_phase1_standalone.cpp`
- **测试**：5 个 IOMMU 测试 + 1 个 ATS 测试 + 现有 98 个测试 = 应绿；新增 fabric test 至少 3 个

**签名约束**：所有新 op 签名遵循 ADR-087 D2（no raw pointer in shared structures）；ADR-081 评审需逐 op 提供 ADR-059 D3 条件 4 证据。

### 7.4 审阅 Checklist
- [ ] D1-D3 fn-ptr 签名是否符合 ADR-023 命名 + 参数约定？
- [ ] D4 拒绝 `hal_mem_pool_attach_fabric` 的理由是否充分？是否需在 ADR-078 评审时同步确认？
- [ ] D5 append-only 实施路径是否清晰（`gpu_hal_ops.h` 结构变更最小化）？
- [ ] ADR-059 D3 条件 4 证据应在 ADR 文件中提供还是引用到实施 issue？

---

## §8 ADR-082 草案：Fabric HAL Phase-2 batch

### 8.1 文件名

`docs/00_adr/adr-082-fabric-hal-phase-2.md`

### 8.2 Frontmatter

```markdown
# ADR-082: Fabric HAL Phase-2 Batch（multicast + NIC + counted write trigger）

**状态**: 🔄 Proposed（Wave 3 启动前升至 Accepted）
**日期**: 2026-08-14
**提案人**: UsrLinuxEmu Architecture Team（基于 scale-up-fabric-architecture.md v0.2 §8 H6-H7 + ADR-023 append-only）
**评审者**: UsrLinuxEmu Architecture Team + Architecture Owner

**关联 ADR**:
- [ADR-023](adr-023-hal-interface.md) ✅ HAL 接口契约（追加 2-4 fn-ptr）
- [ADR-060](adr-060-message-notification-threading.md) ✅ kernel_workqueue（async 完成路径）
- [ADR-062](adr-062-hal-event-signal-extension.md) ✅ HAL event_signal（async 模式参考）
- [ADR-081](adr-081-fabric-hal-phase-1.md) ✅ Phase-1 batch（前置）
- [ADR-083](adr-083-pgas-multicast-semantics.md) 📋 多播语义（fn-ptr 行为依据）
- [ADR-084](adr-084-pgas-address-semantics.md) 📋 PGAS zone（zone 标识）
- [ADR-085](adr-085-consumer-contract.md) 📋 消费者契约（NIC 角色 gate）
- [ADR-086](adr-086-trigger-registry.md) ✅ Trigger registry（counted write trigger）
```

### 8.3 §结构大纲

#### Context
- Wave 3 实施需要 HAL 层 ops 支持 multicast + NIC RDMA
- ADR-081 Phase-1 已建立 switch route 基础
- ADR-083 多播语义需 HAL 层 fan-out op 表达
- ADR-085 消费者契约 gate 决定 NIC 角色实施时机

#### Decision（核心）
- **D1**: 新增 `hal_mcast_write(ctx, nfa, value, target_devices_bitmask)` —— replicated-object fan-out
- **D2**: 可选新增 `hal_mcast_route_set(ctx, nfa, target_devices)` —— 编程多播组（v1 用固定 window，无需该 op；trigger 保留）
- **D3**: 新增 `hal_fabric_rdma_send(ctx, nic_buf, remote_node, remote_buf, size)` —— NIC RDMA send
- **D4**: 新增 `hal_fabric_rdma_recv(ctx, nic_buf, remote_node, remote_buf, size)` —— NIC RDMA recv
- **D5**: **附条件扩展项**（trigger-gated）：若 ADR-086 "counted write trigger" 触发，新增 `hal_fabric_signal(ctx, nfa)` + `hal_fabric_wait(ctx, nfa, expected_counter)`（≤2 fn-ptrs，并入本批次评审）
- **D6**: 严格 append-only，HAL 70+ → 73-77

#### Consequences
**正面**：
- ✅ 多播 + NIC 核心 ops 落地
- ✅ 条件扩展项 D5 提供 NVSHMEM-like 消费者出现时的快速通道
- ✅ 同步原语层在 Phase-1 fence 基础上扩展到 signal/wait（trigger 时）

**负面**：
- ⚠️ HAL 接近 80 阈值，触发 SSOT 复用指引（须论证新增必要性）
- ⚠️ D5 trigger 触发时本 ADR 需 reopen + 追加 fn-ptr

**风险**：
- 🟡 D5 trigger 误触发 → ADR-082 多次 reopen
- 🟡 NIC role 与 ADR-078 双角色决策联动 —— 若 owner 否决双角色，本 ADR 需调整

#### Migration
- **Phase 1**：Wave 3 启动前升至 Accepted
- **Phase 2**：实施
  - 修改 `plugins/gpu_driver/hal/gpu_hal_ops.h`：追加 2-4 fn-ptr
  - 多播 op 走 ADR-083 §5.3 已定义的 replicated-object 语义
  - NIC op 走 ADR-078 §4.3 双角色定义
- **测试**：Phase-1 测试 + 新增多播测试 + 新增 NIC 测试

### 8.4 审阅 Checklist
- [ ] D1 多播 fn-ptr 是否需要 `value` 参数（vs 远端 pointer）？建议签名？
- [ ] D3/D4 NIC fn-ptr 签名是否对齐 RDMA-verbs 风格？
- [ ] D5 trigger 条件是否需要更明确（NVSHMEM consumer 是触发条件还是其他）？
- [ ] 是否需要拆分本 ADR 为 ADR-082a（mcast）+ ADR-082b（NIC）？

---

## §9 ADR-083 草案：PGAS 多播语义

### 9.1 文件名

`docs/00_adr/adr-083-pgas-multicast-semantics.md`

### 9.2 Frontmatter

```markdown
# ADR-083: PGAS 多播语义（replicated-object + fan-out + 错误处理 + reduction Non-Goal）

**状态**: 🔄 Proposed（Wave 3 启动前升至 Accepted）
**日期**: 2026-08-14
**提案人**: UsrLinuxEmu Architecture Team（基于 scale-up-fabric-architecture.md v0.2 §5.3 + Oracle 评审 U-06）
**评审者**: UsrLinuxEmu Architecture Team + Architecture Owner

**关联 ADR**:
- [ADR-023](adr-023-hal-interface.md) ✅ HAL 接口契约（hal_mcast_write 在 ADR-082）
- [ADR-036](adr-036-three-way-separation.md) ✅ 3 区分原则（多播 fan-out 走 ③ switch sim）
- [ADR-079](adr-079-coherence-protocol-non-goal.md) ✅ Coherence Non-Goal（多播副本一致性 vs coherent-line）
- [ADR-077](adr-077-node-fabric-address-model.md) 📋 NFA charter（多播 window 地址空间）
- [ADR-082](adr-082-fabric-hal-phase-2.md) 📋 HAL Phase-2（hal_mcast_write 实现）
- [ADR-086](adr-086-trigger-registry.md) ✅ Trigger registry（reduction/collectives trigger）

**关联文档**:
- [scale-up-fabric-architecture.md](../02_architecture/scale-up-fabric-architecture.md) v0.2 §5.3
- NVIDIA Rubin 深度调研报告 [R §2.8, §5.8]（CUDA multicast + multimem + SHARP 语义模型）
- Oracle 评审 `ses_00370deb6` §3.4, I-06
```

### 9.3 §结构大纲

#### Context
- scale-up 愿景要求 PGAS 多播（per `scale-up-fabric-research.md` §1.5）
- Rubin CUDA 多播 = replicated copies per device + multimem 指令 + SHARP 加速
- 原 `scale-up-fabric-architecture.md` §5.3 已声明"store-intercept fan-out" 机制，但未命名语义类型
- 用户 Q8 答案已锁定"emu 天然满足 GPU 直发多播"

#### Decision（核心）
- **D1**: **多播语义类型**：v1 实施 **replicated-object multicast with switch write fan-out**
  - **L1 Switch 进程内**（per arch v0.3 §4.5 FM v1 极简模型）fan-out：CPU-side driver load/store → Switch 进程拦截 → 循环写入目标 GPU 进程的 PGAS 接收区；完成经 CppLink-E 通道 fence_id 返回
  - 每设备一份物理副本（与 CUDA multicast object 语义同构）
  - **不隐含** coherent shared line（明确排除）
- **D2**: **完成机制**：同步 fence v1（复用 Stage 4.7 `fence_id_*`），通过 CppLink-E（GPU → Switch → CPU）异步 opt-in 后续（ADR-060/062 路径）
- **D3**: **错误处理**：v1 = all-or-nothing with explicit `-EIO`（deterministic sim）；per-target fault injection 是 test hook，非协议 feature
- **D4**: **SHARP 类 in-switch reduction**：❌ **Non-Goal**（v1），触发由 ADR-086 "in-network-compute trigger" 评估
- **D5**: **多播组配置**：v1 固定 multicast window（per `scale-up-fabric-architecture.md` §4.5 FM 表）；动态组配置为 v2 trigger
- **D6**: **多播 window 地址**：NFA 内的预留 multicast 子区（per ADR-077 D4 静态分区 + per `scale-up-fabric-architecture.md` §3.2 图）

#### Consequences
**正面**：
- ✅ 语义类型显式命名消除未来评审疑问（per Oracle I-06）
- ✅ 与 CUDA `multimem.st` 语义对齐，便于驱动移植对照
- ✅ reduction Non-Goal 清晰，避免 scope creep

**负面**：
- ⚠️ v1 不支持动态多播组配置（应用层写 PGAS 多播 VA 即可，无显式 API）
- ⚠️ all-or-nothing 错误语义可能让"部分成功"驱动移植时困惑

**风险**：
- 🟡 未来 SHARP-like 需求出现时本 ADR 需 reopen（D4 trigger 登记）

#### Migration
- **Phase 1**：Wave 3 启动前升至 Accepted
- **Phase 2**：实施（与 ADR-082 同步）
  - `bin/switch_sim/sim/`（L1 Switch 进程内）实现 multicast fan-out engine
  - `plugins/gpu_driver/sim/` 配置 multicast window per GPU（per-process）
  - `tests/test_pgas_mcast_standalone.cpp` 至少 5 个 case
- **测试**：5 个 multicast test cases（unicast PGAS / multicast broadcast / partial fail / fence 完成 / 多播 + 单播并发）

### 9.4 审阅 Checklist
- [ ] D1 replicated-object 语义命名是否清晰？是否需要更明确的 aliasing 规则？
- [ ] D3 all-or-nothing 错误处理是否符合 Linux 驱动期望？
- [ ] D6 多播 window 地址与 ADR-077 静态分区是否冲突（window 应在 NFA 哪个子区间）？
- [ ] 是否需要在 D2 增加"per-device fence completion"选项（部分完成即返回）？

---

## §10 ADR-084 草案：PGAS 地址语义

### 10.1 文件名

`docs/00_adr/adr-084-pgas-address-semantics.md`

### 10.2 Frontmatter

```markdown
# ADR-084: PGAS 地址语义（zone 划分 + 翻译规则 + NVSHMEM 对照）

**状态**: 🔄 Proposed（Wave 3 启动前升至 Accepted；可能与 ADR-077 合并评审）
**日期**: 2026-08-14
**提案人**: UsrLinuxEmu Architecture Team（基于 scale-up-fabric-architecture.md v0.2 §3.2 + §5.3 + Oracle 评审 U-06）
**评审者**: UsrLinuxEmu Architecture Team

**关联 ADR**:
- [ADR-077](adr-077-node-fabric-address-model.md) 📋 NFA charter（PGAS zone 在 NFA 内）
- [ADR-083](adr-083-pgas-multicast-semantics.md) 📋 多播语义（PGAS multicast 子区）
- [ADR-061](adr-061-hal-iommu-extension.md) ✅ HAL IOMMU ops（UVM 翻译基础）

**关联文档**:
- [scale-up-fabric-architecture.md](../02_architecture/scale-up-fabric-architecture.md) v0.2 §3.2（3 层地址模型）
- NVIDIA Rubin 深度调研报告 [R §4.6]（NVSHMEM symmetric heap 参照）
- Oracle 评审 `ses_00370deb6` §3.4
```

### 10.3 §结构大纲

#### Context
- scale-up 愿景要求 PGAS 地址模型（per `scale-up-fabric-research.md` §1.4.2）
- PGAS = Partitioned Global Address Space（per device 分区 + 多播区）
- NVSHMEM symmetric heap 是行业最成熟的 PGAS 实现
- ADR-077 已定义 NFA charter；PGAS zone 是 NFA 内的子区划分

#### Decision（核心）
- **D1**: UVM VA 空间内划分 PGAS zone = [SYS_BASE, SYS_END) + [PGAS_BASE, PGAS_END)
  - 系统地址区：传统 VA 行为（per-GPU 局部内存映射）
  - PGAS 地址区：fabric-level remote access 语义
- **D2**: PGAS zone 内部进一步划分：
  - 单播子区：每 device 一段（unicast）
  - 多播子区：replicated-object multicast 目标（per ADR-083 D6）
- **D3**: 翻译规则：
  - 系统地址区 VA → per-GPU MMU → 本地 HBM NFA partition
  - PGAS 单播 VA → per-GPU MMU → 远端设备 NFA partition（fabric routing）
  - PGAS 多播 VA → per-GPU MMU → 预留 multicast NFA window（switch fan-out）
- **D4**: PGAS API 暴露：自定义最小 ioctl 控制（`/dev/switch0` 管理 + per-GPU dev ioctl），OpenSHMEM 是 TaskRunner 层（per Q6 答案）
- **D5**: **NVSHMEM 对照**：本设计的 PGAS zone 概念上等同于 NVSHMEM symmetric heap，但实现简化（仿真器内是 mmap'd region，无 MIG/CUDA stream 复杂度）

#### Consequences
**正面**：
- ✅ 与 NVSHMEM 概念同构，库层可在其上分层
- ✅ UVM 翻译表 per-process + node-global NFA（per Q5 答案）
- ✅ PGAS zone 边界清晰，便于审计

**负面**：
- ⚠️ PGAS zone 在 UVM VA 内划分 = 消耗 VA 空间（仿真器无影响，真硬件需谨慎）
- ⚠️ 多播子区固定大小 = 多播规模受限（v1 = 64 KiB，per `scale-up-fabric-architecture.md` §5.3）

**风险**：
- 🟢 低：NVSHMEM 对照提供行业背书

#### Migration
- **Phase 1**：Wave 3 启动前升至 Accepted
- **可能合并 ADR-077**：因 PGAS zone 是 NFA charter 的具体化，可在 ADR-077 评审时合并讨论
  - 若合并：本 ADR 降级为 ADR-077 的 §D-decision
  - 若分离：本 ADR 独立 + 引用 ADR-077
- **Phase 2**：实施（与 ADR-083 同步）
  - per-GPU MMU 配置 PGAS zone page table
  - switch sim 识别单播 vs 多播 VA range
- **测试**：PGAS 单播 R/W + PGAS 多播 + zone boundary violation

### 10.4 审阅 Checklist
- [ ] 是否合并到 ADR-077？（推荐：分离，独立 ADR 更清晰）
- [ ] D1 zone 边界地址值（`SYS_BASE` / `PGAS_BASE`）是否需要在 ADR 内明确？
- [ ] D2 单播子区大小分配方案（每 device 等分 vs 按需）？
- [ ] D5 NVSHMEM 对照是否充分（库层分层的边界）？

---

## §11 ADR-085 草案：消费者契约 + TaskRunner ISwitchDriver（v0.3 扩展：合并原 ADR-090）

### 11.1 文件名

`docs/00_adr/adr-085-consumer-contract.md`

### 11.2 Frontmatter（v0.3 修订：含 ISwitchDriver）

```markdown
# ADR-085: 消费者契约（TaskRunner IFabricDriver + ISwitchDriver / IMEX 参照）

**状态**: 🔄 Proposed（Wave 4 gate 前升至 Accepted）
**日期**: 2026-08-14
**提案人**: UsrLinuxEmu Architecture Team（基于 scale-up-fabric-architecture.md v0.5 §10 + Oracle 评审 U-11 + **ADR-087 D6 IMP-4 决策**）
**评审者**: **3-owner critical path**
- UsrLinuxEmu Architecture Team
- TaskRunner owner（IGpuDriver + ISwitchDriver consumer-side）
- CppTLM owner（NVLink data plane 视角）

**关联 ADR**:
- [ADR-027](adr-027-linux-compat-strategy.md) ✅ Linux 兼容层扩展策略（spec-driven）
- [ADR-035](adr-035-governance-policy.md) ✅ 治理规则（§R5 4 步 cross-repo 流程）
- [ADR-076](adr-076-gpgpu-kernel-module-ioctl.md) ✅ 跨仓契约流程模板
- [ADR-086](adr-086-trigger-registry.md) ✅ Trigger registry（IMEX + ISwitchDriver trigger）
- [ADR-087](adr-087-multi-process-device-fabric-seam.md) 🔄 跨仓 seam（CppLink + D6 IMP-4 政策权威）
- [ADR-089](adr-089-l1-switch-device-driver.md) 📋 L1 Switch device driver（② 端 ISwitchDriver consumer 配合方）

**关联文档**:
- [scale-up-fabric-architecture.md](../02_architecture/scale-up-fabric-architecture.md) v0.5 §10
- NVIDIA Rubin 深度调研报告 [R §4.6, §6.5, §8.4]（IMEX + NVLink Fusion 对照）
- Oracle 评审 `ses_00370deb6` §10.2a + `ses_000f92e51` §6
- tadr-307（TaskRunner 跨仓 consumer-side 对偶文档）
- **原 ADR-090**（v0.3 合并到本 ADR §D8 ISwitchDriver 契约）
```

### 11.3 §结构大纲（v0.3 修订）

#### Context
- scale-up 轨道目前无消费者（GAP-3，per arch v0.5 §10.1）
- Wave 4 实施硬门 = 必须确定消费者契约
- ADR-027 spec-driven 扩展：需具体消费者拉动
- NVIDIA IMEX + NVLink Fusion 提供行业参照
- **v0.3 新增**：L1 Switch 跨 3 仓实现（per ADR-087 D6 + user 决策）—— TaskRunner 端需新增 ISwitchDriver 契约

#### Decision（核心，v0.3 修订）
- **D1**: 消费者契约层 = `IFabricDriver` 抽象接口（TaskRunner 侧 GPU consumer），UsrLinuxEmu 提供 canonical 定义
  - 范围：节点内 fabric 服务抽象（route / multicast / rdma_send/recv / fence）
  - 不包含：scale-out 网络协议（不属于 fabric 契约）
- **D2**: **v0.3 新增 ISwitchDriver** 抽象接口（TaskRunner 侧 Switch consumer）：
  - 范围：L1 Switch 设备抽象（attach/detach/route_query/multicast_subscribe/fence）
  - 借鉴 CUDA runtime shim 模式 + libswitch_taskrunner.so (LD_PRELOAD)
  - 与 IFabricDriver 正交（per arch v0.5 §10.2b）
- **D3**: 契约制定流程：ADR-035 §R5 4 步（canonical → consumer review → TADR 对偶 → commit 顺序协议）
- **D4**: 接口最小集：
  - IFabricDriver：`fabric_attach/detach/mcast_write/rdma_send/recv/fence`
  - ISwitchDriver：`switch_attach/detach/route_query/multicast_subscribe/fence`
  - Phase-2 加 `fabric_signal/wait`（trigger-gated）
- **D5**: **internal-only 备选**：若 Wave 4 之前无 TaskRunner/KFD consumer，明确走 internal-only（不创建 IFabricDriver / ISwitchDriver）
- **D6**: IMEX 对照：契约语义最小集应能表达"跨域 handle + 访问授权"（per arch v0.5 §10.2a）
- **D7**: NVLink Fusion 对照：契约本质是"受控开放的战略平台边界"（per NVIDIA 2024+ 战略）
- **D8**: **v0.3 新增 CppLink 契约层正交**：IFabricDriver + ISwitchDriver 是 TaskRunner 视角；CppLink 是 Switch ↔ GPU 传输层契约（per ADR-078 + ADR-087 D2）。两者独立评审。

#### Consequences
**正面**：
- ✅ 提供跨仓 spec-driven 治理验证
- ✅ IMEX + NVLink Fusion 提供行业先例背书
- ✅ 消费者契约硬门确保 Wave 4 不在无人驱动下实施

**负面**：
- ⚠️ 若 Wave 4 之前无 consumer，internal-only 路径需明确记录
- ⚠️ TaskRunner owner 评审可能要求本 ADR 创建 tadr-XXX 对偶（per ADR-076 模式）

**风险**：
- 🟡 Wave 4 触发可能需要 Phase-2 ADR 追加新 fn-ptr（D3 Phase-2）
- 🟢 内部备选（D4）风险低

#### Migration
- **Phase 1**：Wave 4 启动前升至 Accepted
- **Phase 2**：与 TaskRunner owner 协商 tadr-XXX 创建（per ADR-076 流程）
- **Phase 3**：Wave 4 实施以本契约为 gate

### 11.4 审阅 Checklist
- [ ] D1 抽象范围是否过大（应只含 fabric 内部，不含 scale-out）？
- [ ] D2 4 步流程是否需要 ADR-035 之外的额外检查？
- [ ] D4 internal-only 备选是否需要正式 ADR 而不是仅注释？
- [ ] 是否需要在 D5/D6 给出更具体的接口方法名清单？

---

## §12 ADR-086 草案：Trigger Registry 整合

### 12.1 文件名

`docs/00_adr/adr-086-trigger-registry.md`

### 12.2 Frontmatter

```markdown
# ADR-086: Scale-up Fabric Trigger Registry（单点 trigger 整合）

**状态**: 🔄 Proposed
**日期**: 2026-08-14
**提案人**: UsrLinuxEmu Architecture Team（基于 scale-up-fabric-architecture.md v0.2 §11.3 + Oracle 评审 U-07/U-08/U-12）
**评审者**: UsrLinuxEmu Architecture Team

**关联 ADR**:
- [ADR-034](adr-034-h7-deferred-registry.md) ✅ H-7 Deferred Registry（**模式借鉴**：单 trigger 入口治理）
- [ADR-035](adr-035-governance-policy.md) ✅ 治理规则
- [ADR-086-self](adr-086-trigger-registry.md) → 引用所有 scale-up ADR（077/078/080/081/082/083/084/085）

**关联文档**:
- [scale-up-fabric-architecture.md](../02_architecture/scale-up-fabric-architecture.md) v0.2 §11.3
- Oracle 综合评审 `ses_003b3fa7` §3.3（CONF-6 解决）
- Oracle 评审 `ses_00370deb6` U-07/U-08/U-12（Rubin 启示的单一泄洪口）
```

### 12.3 §结构大纲

#### Context
- scale-up 轨道有 8+ 类 trigger 候选（per `scale-up-fabric-architecture.md` §11.3）
- ADR-034 H-7 Deferred Registry 提供单点治理先例
- 避免每个 trigger 散落在不同 ADR 段落导致遗漏

#### Decision（核心）
- **D1**: 本 ADR 作为 scale-up 轨道的**单一 trigger 入口**（per ADR-034 模式）
- **D2**: trigger 表内容（per `scale-up-fabric-architecture.md` §11.3 已扩展 6 行）：
  | Trigger | 启动项 | Gate ADR |
  |---------|--------|---------|
  | SYN-1 trigger（标准化内存池保真度）| CXL Scope B + Scale-up 内存池 v2 | ADR-077 v2 + 独立 ADR |
  | SYN-2 trigger（fabric P2P DMA 路径出现）| ATS Phase 1+ | ATS/CXL doc §2.6 |
  | GAP-3 trigger（TaskRunner 或 KFD 出现 fabric 消费者）| Scale-up Wave 4 | ADR-085 |
  | Performance trigger（fabric 延迟成为瓶颈）| 顺序性 v2 升级 | ADR-079 v2 |
  | **Control-plane trigger**（多进程/隔离消费者出现）| 动态 FM + 访问控制 | ADR-077 v2 |
  | **IMEX trigger**（跨仓消费者需 fabric handle 共享）| IMEX-like 最小 handle 服务 | ADR-085 |
  | **counted-write trigger**（NVSHMEM-like 消费者出现）| hal_fabric_signal/wait | ADR-082 附条件扩展项 |
  | **in-network-compute trigger**（集合通信消费者出现）| SHARP-like 最小 reduction | ADR-083 Non-Goal 解除 |
| **RAS/降级 trigger**（故障注入测试消费者出现）| hot-swap / degraded op 模拟 | ADR-079 v2 |
| **拓扑分层 trigger**（N≥72 消费者出现）| switch sim 两层拓扑 | ADR-078 v2 |
| **Multi-process trigger**（Multi-Process Vision Phase 3 ship）| ADR-087 D4-D5 v2 实施（reconnect, checkpoint）| ADR-087 |
| **switch_sim repo trigger**（ADR-087 D6 触发 v2 评估）| switch_sim 仓迁移评估 | ADR-078 v2 |
| **CppLink ABI governance trigger**（任何仓 ABI drift）| CppLink version bump 评估 | ADR-078 |
- **D3**: 每条 trigger 触发时**重新打开**对应 ADR 评审（per ADR-035 R3）
- **D4**: 本 ADR 不直接实施任何代码 —— 仅治理整合
- **D5**: 与 ADR-034 H-7 Deferred Registry 的关系：H-7 是项目级 deferred 仓库，本 ADR 是 scale-up 轨道的局部触发集；H-7 已完成（已 Accepted），本 ADR 是其模式继承而非替代

#### Consequences
**正面**：
- ✅ 单一入口避免 trigger 遗漏
- ✅ ADR-034 模式继承，治理一致
- ✅ 10+ 类 trigger 一目了然

**负面**：
- ⚠️ 未来新增 trigger 需修改本 ADR（治理开销）
- ⚠️ 与项目级 H-7 边界需明确（D5 已声明）

**风险**：
- 🟢 低：H-7 模式已工作多年

#### Migration
- **零代码**：本 ADR 是治理决策
- **同步更新**：每次新增 scale-up ADR 时，审查是否需新增 trigger
- **与 `scale-up-fabric-architecture.md` §11.3 同步**：本 ADR 通过后，arch doc §11.3 标记 "（权威 trigger 列表见 ADR-086）"

### 12.4 审阅 Checklist
- [ ] D2 trigger 表是否完整？是否漏掉 NVLink Fusion / open interconnect trigger？
- [ ] D3 reopen 流程是否清晰？
- [ ] D5 与 ADR-034 关系是否需要更明确？
- [ ] 是否需要在 ADR 内提供 trigger 检测机制（脚本/命令）？

---

## §12b ADR-089 草案：L1 Switch Device Driver（v0.3 新增，UsrLinuxEmu）

### 12b.1 文件名

`docs/00_adr/adr-089-l1-switch-device-driver.md`

### 12b.2 Frontmatter

```markdown
# ADR-089: L1 Switch Device Driver（v0.3 新增，UsrLinuxEmu）

**状态**: 🔄 Proposed
**日期**: 2026-08-14
**提案人**: UsrLinuxEmu Architecture Team（基于 scale-up-fabric-architecture.md v0.5 §2.1 + ADR-087 D6 IMP-4 决策 + user 2026-08-14 决策）
**评审者**: **2-owner critical path**
- UsrLinuxEmu Architecture Team
- CppTLM owner（CppLink-D endpoint 端 + 跨仓契约评审）

**关联 ADR**:
- [ADR-023](adr-023-hal-interface.md) ✅ HAL 接口契约（H3/H4 端点约束）
- [ADR-035](adr-035-governance-policy.md) ✅ 治理规则
- [ADR-036](adr-036-three-way-separation.md) ✅ 3 区分原则（**② 端 = UsrLinuxEmu**）
- [ADR-038](adr-038-network-stack-three-way-separation.md) ✅ net_driver 模板（Plugin 模式参考）
- [ADR-076](adr-076-076-gpgpu-kernel-module-ioctl.md) ✅ 跨仓 HAL 扩展流程模板
- [ADR-077](adr-077-077-node-fabric-address-model.md) 📋 NFA charter
- [ADR-078](adr-078-l1-switch-process-and-cpplink.md) 📋 L1 Switch 进程 + CppLink 协议（v0.3 扩展 CppTLM SoC）
- [ADR-083](adr-083-083-multicast-semantics.md) 📋 多播语义（v0.3：fan-out 在 switch_sim 进程内）
- [ADR-085](adr-085-085-consumer-contract.md) 📋 消费者契约（含 ISwitchDriver 合并 ADR-090）
- [ADR-087](adr-087-multi-process-device-fabric-seam.md) 🔄 D2/D3 政策权威

**关联外部 ADR**:
- CppTLM ADR-SOC-04 (v1) + ADR-SOC-06 (v2 规划中) + ADR-SOC-07 (IPC seam)
- PTX-EMU ADR-0029 §D7（minimal split — 进程边界参考）
- TaskRunner tadr-307（IGpuDriver 姊妹文档）

**关联文档**:
- [scale-up-fabric-architecture.md](../02_architecture/scale-up-fabric-architecture.md) v0.5 §2.1, §9.2（② 端 UsrLinuxEmu 归属）
- NVIDIA Rubin 深度调研报告 [R §3.8]（NVSwitch 实证）
- Oracle 评审 `ses_000f92e51` §6 C2（descriptor-based signatures 政策）
- Multi-Process Vision Spec §3.1 D1.b（per-GPU process split 依据）
- ADR-087 D6（2026-08-14 决策记录）
```

### 12b.3 §结构大纲

#### Context
- L1 Switch 跨 3 仓实现（per ADR-087 D6 + user 2026-08-14 决策）：② 端 = UsrLinuxEmu
- UsrLinuxEmu 已有 `plugins/gpu_driver/` 完整 3-way separation 模板（per ADR-036 + ADR-038）
- L1 Switch driver 需：
  - 内核 device + ioctl 契约（`/dev/switch0` + `SWITCH_IOCTL_*`）
  - HAL bridge（switch_sim 进程 ↔ driver 进程 通过 CppLink-D/E）
  - 多实例支持（per ADR-080）
  - IPC readiness（per ADR-087 D2 + 087 D4 v1 fail-fast）

#### Decision（核心）
- **D1**: **L1 Switch device driver 路径**：`plugins/switch_driver/`
  - 与 `plugins/gpu_driver/` 完全同构（per ADR-036 + ADR-038 + ADR-076 模式）
  - 模块结构：
    ```
    plugins/switch_driver/
    ├── drv/         ② portable switch device driver
    │                • SwitchDevice (ioctl 派发表)
    │                • SWITCH_IOCTL_* 契约
    │                • VFS /dev/switch0 注册
    ├── hal/         HAL bridge ↔ switch_sim 进程
    │                • hal_user.cpp（与 gpu hal 同构）
    │                • CppLink-D endpoint 注册
    ├── sim/         ❌ 不在 UsrLinuxEmu（per ADR-087 D6，③ 归 CppTLM）
    ├── shared/      公共头：switch_ioctl.h, switch_types.h
    └── plugin.cpp   导出 `module mod` 符号
    ```
- **D2**: **借鉴 gpu_driver 模板**（per user 2026-08-14 决策）：
  - ioctl 派发表风格（类比 `GpgpuDevice::ioctl`）
  - HAL append-only（per ADR-023）
  - ModuleLoader 注册（per ADR-003）
  - 3-way separation（per ADR-036）
  - 失败模式：dual-track 评估（独立软件栈作为 v2 候选）
- **D3**: **SWITCH_IOCTL 契约**（类比 GPU_IOCTL，per ADR-039 模式）：
  - `SWITCH_IOCTL_CONNECT` —— driver ↔ switch_sim endpoint
  - `SWITCH_IOCTL_ROUTE_QUERY` —— NFA 路由查询（descriptor-based, per ADR-087 D2）
  - `SWITCH_IOCTL_MCAST_SUBSCRIBE` —— 多播组订阅
  - `SWITCH_IOCTL_FENCE_CREATE/WAIT/SIGNAL` —— fence 生命周期
  - `SWITCH_IOCTL_GET_DEVICE_INFO` —— 设备元数据
  - 后续: `SWITCH_IOCTL_*_v2` 演进（per ADR-023 append-only）
- **D4**: **HAL bridge（per ADR-078 §D1 v0.3 + ADR-087 D2 descriptor policy）**：
  - 通过 CppLink-D endpoint 注册到 switch_sim 进程
  - 通过 CppLink-E 接收 fence / 中断事件
  - 共享内存 region descriptors 由 switch_sim 通过 CppLink-C 传递
  - **不**直接调用 switch_sim 进程函数（CppLink 跨进程约束）
- **D5**: **多实例支持**（per ADR-080）：
  - `g_vram_store` 节点分区背景
  - 进程内可创建 N 个 SwitchDevice 实例（per multi-GPU 模式）
  - 每个实例通过独立 CppLink endpoint 接入 switch_sim
- **D6**: **IPC readiness + v1 fail-fast**（per ADR-087 D4）：
  - driver 启动时 verify CppLink-C 协商成功
  - 心跳 1s/超时 3s（per arch v0.5 §4.6.6）
  - 心跳超时 → driver process abort + state dump
- **D7**: **v2 评估项**（out-of-v1）：
  - 独立 switch driver 软件栈（vs 借鉴 GPU）
  - 多 switch 设备支持（per NVIDIA Rubin NVSwitch 拓扑）
  - 真实 RDMA wire protocol（v0.1 = 语义级）

#### Consequences
**正面**：
- ✅ 与 GPU 软件栈同构（per user 决策），复用大量模板
- ✅ ADR-036 3 区分原则得到严格实施（driving ② 在 UsrLinuxEmu，sim ③ 在 CppTLM）
- ✅ v1 实施可立即启动（借鉴已有 gpu_driver 模板）
- ✅ 后续评估独立软件栈的退路已记录

**负面**：
- ⚠️ 借鉴 GPU 模板的债务：未来 GPU 模板修改时需同步更新 switch driver（per ADR-087 D9 cross-repo policy）
- ⚠️ ioctl 编号预留需与 GPU_IOCTL 错开（per ADR-039 模式）
- ⚠️ CppTLM switch_sim 实施时序错位风险（W2 + W3 gates）

**风险**：
- 🟡 GPU driver 模板变更可能级联到 switch driver
- 🟡 真实硬件 archetype 缺失（v0.5 §15 风险 #1）
- 🟡 CppLink ABI 在 3 仓同步演化（per ADR-087 D3 versioning policy）

#### Migration
- **零代码**：本 ADR 是治理决策
- **实施触发**：Wave 2（per arch v0.5 §13.1）
- **代码归属**：
  - `plugins/switch_driver/` 新建（v1.0 创建）
  - 共享头 `plugins/switch_driver/shared/switch_ioctl.h` 公开化（TaskRunner consumer 引用）
  - 与 `plugins/gpu_driver/` 平行（不嵌套）
- **借鉴**：
  - `plugins/gpu_driver/drv/gpu_ioctl.cpp` → `plugins/switch_driver/drv/switch_ioctl.cpp`
  - `plugins/gpu_driver/hal/gpu_hal_user.cpp` → `plugins/switch_driver/hal/switch_hal_user.cpp`
  - 简化（driver 比 gpu_driver 简单：无 BO/VA Space/Queue 概念）

### 12b.4 审阅 Checklist
- [ ] D1 模块结构是否与 gpu_driver 完全同构？3-way separation 边界是否清晰？
- [ ] D2 借鉴范围是否合理？v1 vs v2 评估项边界？
- [ ] D3 SWITCH_IOCTL 编号是否错开 GPU_IOCTL？未来扩展路径清晰？
- [ ] D4 CppLink-D/E bridge 是否严格不调用 switch_sim 进程函数？
- [ ] D5 多实例支持是否与 ADR-080 spike 一致？
- [ ] D6 IPC readiness 检查是否完整？
- [ ] 共享头 `switch_ioctl.h` 是否 TaskRunner 侧也可引用（tadr-307 对偶）？

### 12b.5 与其他 ADR 的关系图

```
ADR-089 (Switch Device Driver)  ←——借鉴——→  plugins/gpu_driver/ (v0.3 模板)
       │
       ├─→ ADR-087 D2 (descriptor-only): 共享内存结构 no raw pointer
       ├─→ ADR-087 D3 (ABI versioning): SWITCH_IOCTL 编号预留
       ├─→ ADR-077 (NFA): SWITCH_IOCTL_ROUTE_QUERY 用 NFA
       ├─→ ADR-078 (L1 Switch 进程 + CppLink): CppLink-D bridge
       ├─→ ADR-083 (多播): SWITCH_IOCTL_MCAST_SUBSCRIBE 实现
       ├─→ ADR-085 (消费者契约): 共享 switch_ioctl.h 给 TaskRunner
       └─→ ADR-080 (多实例): g_vram_store 节点分区

外部：switch_sim 进程由 CppTLM 仓实施（per ADR-078 v0.3 §D1）
```

---

## §13 跨 ADR 主题（cross-cutting themes）

以下决策跨多个 ADR，需在创建过程中保持一致：

### 13.1 NFA vs BAR 命名一致性

**涉及 ADR**: 077（命名空间）+ 069（既有 BAR 物理 layout）

**关键点**：ADR-069 已定义 per-device BAR physical 在 `0x1000_0000+`；ADR-077 定义 NFA 在 `0x100_0000_0000` (1 TiB)。**两者严格互斥**（v0.2 §3.1 已确认）。

**风险**：未来 BAR 范围扩张需同步更新 ADR-077。

### 13.2 HAL append-only 一致性

**涉及 ADR**: 081（Phase-1）+ 082（Phase-2）

**关键点**：081 + 082 累计 5-9 个新 fn-ptr；HAL 68 → 73-77。SSOT §1.10.2 "> 50 → 优先复用"指引要求每 op 提供 ADR-059 D3 条件 4 证据。

**风险**：评审时被要求合并某些 op 到现有接口。

### 13.3 Spike GO → ADR-080 Accepted 链

**涉及 ADR**: 080（spike）+ 077/078（依赖 spike GO）

**关键点**：ADR-080 D2 spike NO-GO → 所有 Wave 2+ 阻塞；本计划假设 spike GO 路径。

**风险**：若 spike NO-GO，整个 10 ADR 计划中后 6 个需重新评估依赖。

### 13.4 消费者契约硬门

**涉及 ADR**: 085（消费者契约）+ 082（Wave 3 HAL Phase-2 NIC）+ Wave 4 实施

**关键点**：ADR-085 必须在 Wave 4 之前 Accepted；Wave 3 NIC 实施不依赖 ADR-085。

**风险**：Wave 4 gate 若不达成 → ADR-085 强制 internal-only 或 reopen。

### 13.5 Trigger Registry 治理一致性

**涉及 ADR**: 086（trigger registry）+ 所有 scale-up ADR

**关键点**：086 是单一 trigger 入口；新增任何概念需先查 086 trigger 表 → 决定是否新增 trigger vs 直接实施。

**风险**：若 086 与 arch doc §11.3 漂移 → 治理失序。

### 13.6 Rubin 行业证据一致性

**涉及 ADR**: 079（Coherence）+ 083（多播）+ 085（消费者契约）+ 086（trigger）

**关键点**：v0.2 已将 Rubin 实证写入 arch doc；每个 ADR 的 Context 应引用 [R §x.y] 强化。

**风险**：若 ADR Context 不引 [R §x.y]，评审者无法验证行业背书。

### 13.7 CppLink ABI Governance

**涉及 ADR**: 078 (wire artifacts) + 087 (policy)

**关键点**：CppLink ABI version 字段 + 能力协商 + deprecation 政策

**风险**：跨仓 ABI drift（per arch v0.3 risk #13）

**缓解**：docs-audit 校验 descriptor struct 在 UsrLinuxEmu / CppTLM / PTX-EMU 仓的一致性

---

## §14 审阅 Checklist（建议您重点关注）

### 14.1 整体层面

- [ ] 11 个 ADR 的**创建顺序**是否符合预期？（§2.1）
- [ ] 是否有**遗漏的 ADR**？（如 fabric 启动序列 ADR、模拟性能 ADR）
- [ ] **跨 ADR 主题**是否有遗漏？
- [ ] 本计划是否符合 ADR-035 治理规则？

### 14.2 单 ADR 层面（每个 ADR 都过）

- [ ] **Frontmatter 字段**是否完整？（状态/日期/提案人/关联/关联文档）
- [ ] **关联 ADR**列表是否完整？
- [ ] **Decision 数量**是否合理（3-7 个为佳）？
- [ ] **Consequences**是否平衡（正/负/风险）？
- [ ] **Migration**是否清晰（即使是零代码也要明确"零代码"理由）？

### 14.3 关键决策预审

| 决策 | 影响 ADR | 您需确认 |
|------|---------|---------|
| ADR-077 NFA base `0x100_0000_0000` | 077 | ✅/❌ |
| ADR-078 进程边界 | L1 Switch 独立 CPU 进程 + CppLink v1.0 协议（per arch v0.3）| ✅/❌ |
| ADR-078 双角色保留 + net_driver 备选 | 078 | ✅/❌ |
| ADR-079 Coherence Non-Goal + 行业证据 | 079 | ✅/❌ |
| ADR-080 spike 检查表 8 项（含 4 项 IPC readiness）| 080 | ✅/❌ |
| ADR-081 H3+H4 = 3 个新 fn-ptr（descriptor-based）| 081 | ✅/❌ |
| ADR-082 H6+H7 = 4 个新 fn-ptr + trigger D5 | 082 | ✅/❌ |
| ADR-083 replicated-object 语义类型命名 + switch 进程 fan-out | 083 | ✅/❌ |
| ADR-084 PGAS zone 划分（与 ADR-077 分离）| 084 | ✅/❌ |
| ADR-085 consumer = IFabricDriver + CppLink layer 正交 | 085 | ✅/❌ |
| ADR-086 trigger 表 13 项（+3 v0.3 triggers）| 086 | ✅/❌ |
| ADR-087 D6 IMP-4 | switch_sim / NVLink data plane 代码归属（关键路径）| ✅/❌（**必须 4-owner 共识**）|

### 14.4 文件名与命名约定

- [ ] 11 个文件名是否符合 `adr-NNN-slug.md` 约定？
- [ ] slug 是否足够描述 ADR 主题？
- [ ] ADR-078 文件名是否应改为 `adr-078-l1-switch-process-split.md`（替代 `adr-078-switch-driver-plugin.md`）？

---

## §15 待澄清项（创建前需要您先确认的）

### 15.1 必须在创建 ADR-077 之前确认

| # | 问题 | 选项 |
|---|------|------|
| Q1 | NFA base 值 `0x100_0000_0000`（1 TiB）是否可接受？ | A) 接受 / B) 调高（与 1 PiB BAR base 同区段）/ C) 调低（紧凑到 256 GiB）|
| Q2 | Memory pool backing 归属（gpu_driver/sim vs switch_sim/sim）| A) 维持 gpu_driver/sim + ServiceRegistry（节省 LOC）/ B) 迁到 switch_sim/sim（架构更对称）—— 必须创建前确认（已在 arch v0.3 §16 Q2 锁定）|

### 15.2 必须在创建 ADR-078 之前确认

| # | 问题 | 选项 |
|---|------|------|
| Q3 | 双角色 vs net_driver 分工最终决定 | A) 保留双角色（v1 简单优先）/ B) 改 net_driver 路径（Rubin 对齐）/ C) 推迟决策，先空 ADR 框架 |

### 15.3 必须在创建 ADR-081 之前确认

| # | 问题 | 选项 |
|---|------|------|
| Q5 | H3+H4 三个新 fn-ptr（descriptor-based 签名）是否够？是否需 `hal_mem_pool_attach_fabric`？ | A) 三 op 足够 / B) 增 H4 到 4 op |

### 15.4 必须在创建 ADR-082 之前确认

| # | 问题 | 选项 |
|---|------|------|
| Q6 | NIC 角色 fn-ptr 命名是否用 `hal_fabric_rdma_*` vs `hal_nic_rdma_*`？（per arch v0.3 §10.2b CppLink 契约层正交性）| A) hal_fabric_*（与平面边界一致）/ B) hal_nic_*（明确 NIC 命名）|

### 15.5 必须在创建 ADR-085 之前确认

| # | 问题 | 选项 |
|---|------|------|
| Q7 | 是否需要 TaskRunner tadr-XXX 对偶文档创建？| A) 是（按 ADR-076 模式）/ B) 否（internal-only）|

### 15.6 必须在创建 ADR-087 之前确认（4-owner 关键路径）

| # | 问题 | 选项 |
|---|------|------|
| Q8 | switch_sim 代码归属（IMP-4）| A) UsrLinuxEmu / B) CppTLM / C) 新仓 / D) 推迟 | **ADR-087 D6 关键路径**（不是 ADR-078） |
| Q9 | ADR-011 边界（吸收 vs 引用 vs 显式 defer）| A) 吸收其 fabric-seam 相关范围 / B) 引用 / C) 显式 defer 其余 | ADR-087 D8 |
| Q10 | CppLink v1.0 协议启动顺序 | A) Switch 监听（推荐）/ B) CPU 监听 / C) 各自监听 | ADR-078 + ADR-087 D3 |

---

## 附录 A 后续流程

### A.1 您的审阅路径

```
[本计划草案]
  ↓ 您审阅 §14 checklist + §15 待澄清项
  ↓ 提供反馈或直接批准
[创建第一个 ADR]
  ↓ 按 §2.1 顺序：ADR-087 → 077 → 078 → 080 → 086 → 079 → 081 → 083 → 084 → 085 → 082
  ↓ 每个 ADR 创建后：docs-audit 验证 + 索引同步
[全部 ADR Created]
  ↓ 状态升级：从 🔄 Proposed → ✅ Accepted（每个 ADR 单独评审升级）
[Wave 0 启动]
  ↓ ADR-087 4-owner Accepted → Wave 0b ADR Accept 解锁
  ↓ ADR-080 spike 创建 openspec/changes/2026-08-15-multi-instance-gpu-spike/
  ↓ spike GO → ADR-080 Accepted
[Wave 1-4 实施]
```

### A.2 您的输入期望

您审阅后可能的回复模式：

**模式 A: "全部 OK，按计划执行"** → 我开始按 §2.1 顺序创建 11 个 ADR
**模式 B: "修改 X/Y/Z 后执行"** → 我修改本计划 → 您二次审阅 → 批准后执行
**模式 C: "先讨论 §15 的 Q1-Q10"** → 我暂停创建流程，等您回答

---

## 附录 B 版本控制

| 版本 | 日期 | 状态 | 关键变更 |
|------|------|------|----------|
| v0.1 | 2026-08-14 | Plan Draft | 初版（10 ADR，per arch v0.2） |
| v0.2 | 2026-08-14 | Plan Draft | ADR-087 新增（D6 IMP-4 等 9 项决策）+ ADR-078 重命名 + ADR-081 签名改 descriptor-based + ADR-080 加 IPC readiness + ADR-083 fan-out 改 switch 进程 + 创建顺序：ADR-087 第一个 + §15 Q4 撤销 + Q8/Q10 新增 + §13 cross-cutting 加 CppLink ABI governance |
| **v0.3** | 2026-08-14 | Plan Draft | **ADR-087 D6 IMP-4 决策后修订**（per arch v0.5）：L1 Switch 跨 3 仓实现 = 与 GPU 软件栈同构；§1 总览 11 → 12 ADR（增 ADR-089）；**原 ADR-088（独立 CppTLM SoC spec）合并到 ADR-078 §D1**；**原 ADR-090（TaskRunner switch userspace）合并到 ADR-085 §D8（ISwitchDriver）**；§2.1 创建顺序：ADR-078 升为 3-owner critical path（与 ADR-087 并列）；§4 ADR-078 全面重写（含 CppTLM SoC spec + 跨仓 3-owner 评审 + 借鉴 GPU driver 模板）；§11 ADR-085 增 ISwitchDriver 契约；§12b 新增 ADR-089 完整草案；ADR 总数 10 → 12；零代码零 ADR 新增 |

---

**计划版本**: v0.3 Plan Draft
**下次更新触发**: 您审阅反馈
**维护者**: UsrLinuxEmu Architecture Team
**最后更新**: 2026-08-14