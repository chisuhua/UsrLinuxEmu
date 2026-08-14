# Scale-up Fabric Architecture (节点内 L1 Switch + 统一 PA + UVM/PGAS)

> **状态**: 📋 Draft v0.5（2026-08-14，ADR-087 D6 IMP-4 决策后修订：L1 Switch 跨 3 仓实现 = 与 GPU 软件栈同构 + §13.4 条件性 gate 解除 + Q10 决策；零代码零 ADR 变更）
> **角色**: Scale-up 轨道的**局部架构 SSOT**（与项目级 SSOT `post-refactor-architecture.md` 配套，不替代）
> **作者**: UsrLinuxEmu Architecture Team（基于 scale-up-fabric-research.md v0.2 + ATS/CXL research v0.3 综合）
> **对应 commit**: HEAD (2026-08-14)
> **证据基础**:
> - [docs/05-advanced/ats-cxl-30-implementation-research.md](../05-advanced/ats-cxl-30-implementation-research.md) v0.3（1180 行）
> - [docs/05-advanced/scale-up-fabric-research.md](../05-advanced/scale-up-fabric-research.md) v0.2（1000 行）
> - Oracle 综合分析报告（2026-08-14 session `ses_003b3fa7`）
>
> **关系图**:
> ```
> post-refactor-architecture.md (项目级 SSOT, v0.1.7)
>      │
>      └─ scale-up-fabric-architecture.md (本文件, 局部 SSOT)
>           │
>           ├─ ADR-077+ (聚焦决策, 见 §12)
>           │
>           ├─ ats-cxl-30-implementation-research.md (证据)
>           └─ scale-up-fabric-research.md (证据)
> ```
>
> **最后更新**: 2026-08-14（v0.4 Oracle 评审后修复：Q1 §4.6 CppLink 协议补全（4 通道方向修正 + tick master 明确 + descriptor 字段补 magic/kind + boot 屏障 + v1 fail-fast 限制定义）+ Q2 §9 进程架构修补（listener 方向统一 + IMP-4 归属 + §9.6 故障矩阵 + 6 处单进程遗留清理 + §6.1 跨进程契约 + net_driver 备选改写）+ Q3 ADR-087 创建顺序前置 + Q4 §13.4 Phase R→并行 + Phase 3 split start/complete + 风险 #12 降级 + Q10 + 术语表 3 条；C3 机械合并缺陷全清；零代码零 ADR 变更）

---

## 目录

- [§0 文档定位与状态](#0-文档定位与状态)
- [§1 范围与边界](#1-范围与边界)
- [§2 愿景架构（用户提案）](#2-愿景架构用户提案)
- [§3 地址空间模型（3 层 + 4 命名空间）](#3-地址空间模型3-层--4-命名空间)
- [§4 组件模型](#4-组件模型)
- [§5 通信语义](#5-通信语义)
- [§6 一致性与顺序性政策](#6-一致性与顺序性政策)
- [§7 多实例 GPU 前置条件](#7-多实例-gpu-前置条件)
- [§8 HAL 边界扩展规划](#8-hal-边界扩展规划)
- [§9 插件架构与跨模块查找](#9-插件架构与跨模块查找)
- [§10 消费者契约](#10-消费者契约)
- [§11 与 ATS/CXL 轨道的协同](#11-与-atscxl-轨道的协同)
- [§12 决策点 → ADR 候选清单](#12-决策点--adr-候选清单)
- [§13 阶段化路线图](#13-阶段化路线图)
- [§14 Non-Goals](#14-non-goals)
- [§15 风险登记](#15-风险登记)
- [§16 待澄清问题](#16-待澄清问题)
- [附录 A 术语表](#附录-a-术语表)
- [附录 B 文档元信息](#附录-b-文档元信息)

---

## §0 文档定位与状态

### 0.1 角色

本文件是 **Scale-up Fabric 轨道的局部架构 SSOT**，不是项目级 SSOT，也不是 ADR。它从两份研究文档（ATS/CXL + scale-up vision）和 Oracle 综合分析中提炼出**架构决策陈述**，作为后续 ADR 群的源材料。

### 0.2 与项目级 SSOT 的关系

| 文档 | 范围 | 状态 |
|------|------|------|
| `post-refactor-architecture.md` v0.1.7 | **项目级**架构（3 区分、HAL、CP、IOCTL 等）| ✅ Approved |
| `scale-up-fabric-architecture.md` v0.5（本文件）| **Scale-up 轨道**架构（节点内 L1 Switch + 统一 PA + UVM/PGAS）| 📋 Draft |
| `ats-cxl-30-implementation-research.md` v0.3 | ATS/CXL 协议级研究（Living Document）| 🟡 Living |
| `scale-up-fabric-research.md` v0.2 | Scale-up 愿景调研（Living Document）| 🟡 Living |

**SSOT 优先级**：项目级 SSOT 优先 → 本文件补充 scale-up 维度 → 研究文档作为证据基础。

### 0.3 状态转换路径

```
Draft（本文件当前状态）
  ↓ §12.1 的 5 个基础设施 ADR（ADR-077/078/079/080/087）全部 Accepted
Proposed
  ↓ Scale-up P2 实施 + 测试绿
Implemented (Wave 2 Done)
  ↓ P3-P8 全部完成
Accepted
```

### 0.4 更新规则

- **修改本文件**：通过 ADR 流程（任何 §12 列出的架构决策点变更需新 ADR）
- **修改研究文档**：直接编辑（living document，无需 ADR）
- **Oracle 重跑**：触发本文件 D-now-1/2/3 等决策点复核

---

## §1 范围与边界

### 1.1 Scale-up 边界

```
┌──────────────────────────────────────────────────┐
│  Node (Scale-up Fabric Boundary)                 │
│                                                  │
│  所有组件通过 L1 Switch 互联                       │
│  节点级 fabric 地址统一编址（NFA，见 §3）           │
│  通信语义：统一 load/store                         │
│  一致性（仿真器内）：进程内天然一致（free，跨进程后仅 transport 级共享，见 §6.1）          │
│  一致性（愿景硬件语义）：软件管理 / PGAS 一致性      │
│    —— 与 Rubin GPU↔GPU 模型一致 [R §2.5]，        │
│    非 MI300A 式全局硬件相干                        │
└──────────────────────────────────────────────────┘
                       │
                       │  ← scale-out boundary
                       │
              External Ethernet
              (语义级 RDMA, 无 wire 协议模拟)
```

（注：节点边界为仿真器便利边界；行业 scale-up 域已达 rack/pod 级（NVL576 跨 8 rack [R §4.3]），本文件"节点"= 单 L1 Switch 单层交换域。）

### 1.2 与现有架构的关系

| 范围 | 本文件管辖 | 现有 SSOT 管辖 |
|------|-----------|---------------|
| 节点内 GPU↔GPU 互联 | ✅ | — |
| 节点内 GPU↔内存池 | ✅ | — |
| 节点内 GPU↔NIC | ✅ | — |
| 节点内多播（PGAS）| ✅ | — |
| 节点间 scale-out | ❌（仅语义接口）| — |
| HAL 边界 | 增量扩展 | 基础契约（ADR-023）|
| 3 区分原则 | 适用 | ✅ ADR-036 |
| 插件架构 | 增量 | ✅ ADR-003 + ADR-038 |

### 1.3 节点规模假设（per Q1 Oracle 建议）

**N=8 固定 v1**，参数化常量化，72/576（NVL-class）仅在消费者明确拉动时升级。

依据：
- **HGX Rubin NVL8**（CES 2026 发布清单 [R §1.8]）证明 8-GPU 是 industry-shipping 最小 fabric 单元；NVLink 4 代域同为 8 GPU [R §2.1]
- MI300X 平台 8-GPU 板（注意：非 "MI300A 8 GPU/package"——MI300A 为单 APU 封装，8-GPU 是 MI300X 平台级 [RES §2.2 勘误]）+ Intel Xe Link 8-GPU
- 测试成本随 N 线性增长
- N=8 已能验证 PGAS 多播、内存池、NIC 等核心特性

**规模与拓扑正交性**：Rubin 实证显示规模升级可伴随拓扑变化——
NVL72 = 单层 all-to-all，NVL576 = 8×72 GPU **两层** all-to-all（铜进光出）[R §3.5, §4.3]。
本设计 v1 单 L1 Switch = 单层全交换拓扑；N 参数化**不**改变拓扑类别。
若消费者拉动 N≥72，需重新评估 switch sim 分层（登记为 §11.3 trigger）。

---

## §2 愿景架构（用户提案）

### 2.1 三大组件（v0.4 修订：ADR-087 D6 IMP-4 已决策）

| 组件 | 角色 | **v0.4 实现位置**（D6 决策）|
|------|------|---------|
| **GPU devices** | 计算单元，每 GPU 独立 MMU，per-GPU **独立进程** | per-GPU 进程：`plugins/gpu_driver/drv/` + `sim/`（UsrLinuxEmu ③ sim layer）|
| **L1 Switch hardware sim** | TLM SoC 仿真 + NFA routing + multicast fan-out + NVLink timing | **CppTLM** `soc_arch/switch/`（新子目录，与 `soc_arch/gpu/` 同构）|
| **L1 Switch driver** | Linux kernel 内 switch device driver + IOCTL + HAL | **UsrLinuxEmu** `plugins/switch_driver/`（与 `plugins/gpu_driver/` 同构）|
| **L1 Switch userspace** | 用户态 switch API（libswitch_taskrunner.so）| **TaskRunner** `src/switch/`（与 `src/cuda/` 同构）|
| **Memory Pool** | 共享内存（v1 = 私有 fabric memory，v2 = CXL Type-3 trigger）| backing: `plugins/gpu_driver/sim/mem_pool`（ADR-058 复用）；节点分区视图: switch sim |
| **UsrLinuxEmu CPU sim** | 软件栈（kernel sim + gpu driver + switch driver + HAL）| 独立 CPU 进程；通过 CppLink 接入 switch_sim |

**重大架构变更（v0.4）**：

- **L1 Switch 跨 3 仓实现（per ADR-087 D6）**：
  - **③ 硬件 sim**: CppTLM `soc_arch/switch/`（与 GPU cluster sim 同构）
  - **② 内核 driver**: UsrLinuxEmu `plugins/switch_driver/`（借鉴 gpu_driver 模板）
  - **① Consumer API**: TaskRunner `src/switch/`（借鉴 CUDA runtime shim 模板）
- **链路命名**：L1 Switch ↔ GPU 设备的 link 命名为 **"CppLink"**（v0.3 维持）
- **进程拓扑**：UsrLinuxEmu CPU sim + L1 Switch CPU sim（control plane）+ CppTLM switch_sim（③ hardware）+ N 个 GPU sim 进程
- **v1 策略**：switch driver / userspace **可借鉴** GPU 软件栈（per user 2026-08-14 决策）；后续 ADR 可评估独立软件栈

### 2.2 拓扑（v0.4 修订：ADR-087 D6 IMP-4 已决策）

```
节点内（多进程拓扑，per ADR-087 D6 跨 3 仓实现）：

┌─────────────────────────────────────────────────────────────────┐
│ Process 1: UsrLinuxEmu CPU sim                                  │
│   • Linux kernel sim + VFS + ModuleLoader                       │
│   • plugins/gpu_driver/   (per-GPU driver plugin + ioctls)        │
│   • plugins/switch_driver/ (L1 Switch device driver + ioctls)    │  ← 新增 v0.4
│   • plugins/switch_driver/ 仿照 gpu_driver 模板（per D6.2）      │
└─────────────────────────────┬───────────────────────────────────┘
                              │ CppLink-C + D + E
                              │ + shared memory for kernel args
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│ Process 2: CppTLM switch_sim  ← v0.4 新增 (per ADR-087 D6.1)  │
│   路径: cppTLM/soc_arch/switch/ （与 soc_arch/gpu/ 同构）    │
│   • NFA routing table (per ADR-077)                              │
│   • Multicast fan-out engine                                     │
│   • Memory pool NFA 视图 (per ADR-058)                           │
│   • NVLink timing simulation (per Vision Phase 4)                │
│   • Per-GPU CppLink endpoint 管理                                │
│   • 双角色: fabric switch + NIC (scale-out, 语义 RDMA)          │
└──┬───────────────────────────┬─────────────────────────┬────────┘
   │ CppLink-D (data)         │ CppLink-E (event)       │ CppLink-T (tick)
   │                          │                         │
   ▼                          ▼                         ▼
┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐
│ Process 3:      │  │ Process 4:      │  │ Process N+2:    │
│ GPU sim #0      │  │ GPU sim #1      │  │ GPU sim #N-1    │
│ • HBM #0        │  │ • HBM #1        │  │ • HBM #N-1      │
│ • MMU (per-GPU) │  │ • MMU (per-GPU) │  │ • MMU (per-GPU) │
│ • PTX-EMU (libptxemu_minimal.so)                              │
└─────────────────┘  └─────────────────┘  └─────────────────┘
                                  │
                                  ▼
                       External Ethernet
                       (scale-out, 语义 RDMA)

┌─────────────────────────────────────────────────────────────────┐
│ Process N+3: TaskRunner switch userspace  ← v0.4 新增            │
│   路径: taskrunner/src/switch/ （与 src/cuda/ 同构）          │
│   • libswitch_taskrunner.so (LD_PRELOAD shim)                  │
│   • ISwitchDriver consumer-side (与 IFabricDriver 正交)         │
│   • v1 借鉴 CUDA runtime shim 模板（per D6.3）                 │
└─────────────────────────────────────────────────────────────────┘
```

**关键概念（v0.4）**：

- **L1 Switch 跨 3 仓实现（per ADR-087 D6）**：
  - ③ 硬件 sim: **CppTLM** `soc_arch/switch/`
  - ② 内核 driver: **UsrLinuxEmu** `plugins/switch_driver/`
  - ① Consumer: **TaskRunner** `src/switch/`
- **与 GPU 软件栈完全同构**：gpu_driver / gpu_sim / taskrunner-cuda 三件套作为 switch 的设计模板
- **CppLink = Cpp Language Link**（本项目抽象的链路协议，避免 NVLink 命名）
- **4 通道**：C (Control) / D (Data) / E (Event) / T (Tick)
- **每 GPU 一个进程**（per-GPU process boundary，per Vision D1.b）

### 2.3 通信语义

| 路径 | 语义 | 实现 |
|------|------|------|
| GPU ↔ GPU | load/store（仿真器内天然一致；语义 = PGAS 远程内存访问）| Switch 路由 + per-GPU MMU |
| GPU ↔ Memory Pool | load/store | Switch 路由 |
| GPU ↔ NIC（节点内 buffer）| load/store | Switch 路由 → NIC buffer |
| NIC → External | 语义 RDMA（无 wire 协议）| 包序列化（v1 简化）|
| 1-to-N 多播 | load/store 到 PGAS 多播区 | Switch store-intercept + fan-out |

**关键设计**：**没有 mailbox / descriptor / command queue 等消息机制**；
通信面为纯 load/store 内存语义（与 NVLink 6 memory-semantic 模型同构 [R §2.4]）。
一致性政策见 §6：仿真器内一致性天然成立，愿景硬件的一致性语义为软件管理
（PGAS/release），与 Rubin GPU↔GPU 模型一致 [R §2.5]。

---

## §3 地址空间模型（3 层 + 5 命名空间；NFA = Node Fabric Address）

### 3.1 命名空间 Charter（4 个设备侧 + 1 个宿主参照系）

| Namespace | Base convention | Owner | Backing | NVIDIA 对应层 [R §5.4] | ADR |
|-----------|----------------|-------|---------|----------------------|-----|
| **CPU process VA** | glibc-assigned | host OS | — | VA (host range) | — |
| **Device VA**（per-pool）| `0x1_0000_0000`+ (4 GiB) | ③ sim | `mmap(MAP_FIXED_NOREPLACE)` | VA (device range) | ADR-058 |
| **DMA addr** | `0x1_0000_0000`+ (4 GiB) | ① compat | lookup table → cpu_addr | IOVA | ADR-073 |
| **BAR physical**（per-device）| `0x1000_0000`+ | ①/③ | sim backing store | device-local PA | ADR-069 |
| **Node Fabric Address（NFA）** | distinct high base, e.g. `0x100_0000_0000` (1 TiB) | ③ switch sim | node-wide `mmap`'d store（refactored `g_vram_store`）| **FA（Fabric Address）** | ADR-077 (D1) |

**术语说明**：本文件此前版本称 NFA 为 "Node Unified PA"。更名为 NFA 的理由：
NVIDIA IMEX 公开三地址模型 VA→PA→FA [R §5.4, NVIDIA Official Documentation]，
其中 FA 是"路由可见、fabric manager 可重映射"的地址层——这正是本设计的语义。
称为 "PA" 会与 ADR-069 的 per-device BAR physical 冲突，并误导为
"单一物理所有权空间"（该解读被行业实证明确否定 [R §9.4]）。

**冲突避免**：
- Device VA 与 DMA addr base 值巧合相同（都是 `0x1_0000_0000`），但通过**命名空间**区分
- NFA base 与所有既存命名空间**严格互斥**（高 1 TiB 起始）

### 3.2 3 层地址模型

```
┌─────────────────────────────────────────────────────┐
│ L3  PGAS zone                                        │
│     ├── 单播区：每 device 一段（unicast）             │
│     └── 多播区：1-to-N store 触发 fan-out             │
├─────────────────────────────────────────────────────┤
│ L2  UVM VA (per-process)                             │
│     ├── 系统地址区（传统 VA 行为）                   │
│     └── PGAS 区（= L3 的别名，软件约定）             │
├─────────────────────────────────────────────────────┤
│ L1  Node Unified PA                                  │
│     ├── [GPU_0_HBM, GPU_0_END)                      │
│     ├── [GPU_1_HBM, GPU_1_END)                      │
│     ├── ...                                          │
│     ├── [GPU_N-1_HBM, GPU_N-1_END)                  │
│     ├── [MemPool_BASE, MemPool_END)                 │
│     ├── [NIC_BASE, NIC_END)                         │
│     └── [Reserved Multicast Window]                 │
└─────────────────────────────────────────────────────┘
```

### 3.3 翻译栈

```
GPU_i 发起 load/store：
  1. GPU_i MMU 翻译 UVM VA → NFA（per-GPU page table）
  2. Switch sim 路由 Node PA → owning device's partition（静态 range check）
  3. 目标设备完成访问

PGAS 多播：
  1. GPU_i store 到 PGAS 多播区（VA）
  2. MMU 翻译到 Node Multicast Window（PA）
  3. Switch sim 拦截多播 PA → fan-out 到所有 device 分区
  4. 同步完成（v1）或 fence 返回
```

**静态分区（v1）**：每个设备在节点启动时分配固定 PA 区间；无动态路由表；与 ADR-069 的 fixed-base 哲学一致。

**动态路由（v2 升级选项）**：scale-up §4.1.2 option 2；v1 不实施。

### 3.4 关键设计决策

| 决策 | 选择 | 依据 |
|------|------|------|
| 静态 vs 动态分区 | **静态 v1** | 简化 switch sim，零额外 LOC |
| 节点 PA base 值 | **1 TiB 起始**（与既存命名空间互斥）| Oracle D1 charter |
| MMU 模型 | **per-GPU**（每 GPU 独立 page table）| scale-up §1.2.1 用户愿景 |
| MMU 翻译粒度 | **per-GPU MMU + Switch-side fan-out**（非 shared MMU）| CONF-2 解决 |

---

## §4 组件模型

### 4.1 GPU Device

| 属性 | 值 | 备注 |
|------|-----|------|
| 数量 v1 | N=8（参数化）| Q1 答案 |
| 互联 | 通过 L1 Switch | — |
| MMU | per-GPU，翻译 UVM VA → NFA | scale-up §1.2.1 |
| 本地 HBM | GPU_i_HBM window（NFA 子区间）| 静态分区 |
| 多播 | 通过 PGAS 多播区 | §5.3 |

**多实例实现**：见 §7（核心前置条件）。

### 4.2 L1 Switch（fabric switch 角色）

| 功能 | 详情 |
|------|------|
| GPU↔GPU routing | 静态分区查找（O(1) range check）|
| GPU↔Memory Pool routing | 同上 |
| Multicast fan-out | 拦截 multicast NFA，循环写入目标分区 |
| MMU 协助 | per Rubin/CUDA 模型：per-GPU 页表翻译 VA→NFA；switch 静态 range 路由 NFA→device partition；v1 无 switch 侧 TLB/页表，无集中 MMU |

### 4.3 L1 Switch（NIC 角色）

| 功能 | 详情 |
|------|------|
| 语义 RDMA | verbs-level put/get + 完成（**非** RoCE/iWARP wire 协议）|
| Scale-out 接口 | v1 = 节点间 loopback / 进程内第二个节点模拟 |
| 完成通知 | 复用 `fence_id_*`（Stage 4.7）|

**Q3 答案**：UsrLinuxEmu 不模拟 Ethernet PHY；net_driver 是 L2-only（ADR-038）。Wire 协议 RDMA 没有驱动可移植性消费者。

**与 Rubin 的偏离声明（有意识决策）**：NVIDIA Rubin 将 fabric 与 scale-out
分离为独立器件（NVLink 6 Switch ≠ NIC；ConnectX-9 SuperNIC 承担 RDMA 端点
[R §3.8, NVIDIA Official]；"combined NIC and switch" 无公开架构支持 [R §9.4]）。
本设计保留双角色的理由：(a) 仿真器单插件简单性，避免跨插件协议层；
(b) RES §4.7.2：节点内 load/store 直达 NIC buffer 的语义在单 sim 内最简；
(c) 双角色合并**不影响** GPU 驱动可见语义（驱动只见 load/store + fence）。
**已考虑并暂缓的备选**：复用现有 `net_driver`（Stage 2，ADR-038 L2 插件）
承担 scale-out 语义 RDMA 角色，使 switch_sim 进程保持纯 fabric（Rubin 对齐）；scale-out endpoint 改由 UsrLinuxEmu CPU sim 内的 net_driver 插件承担。
该备选作为 ADR-078 评审的显式权衡项记录。

**平面语义边界**：fabric 平面 ops（route/multicast）与 scale-out 平面 ops
（rdma_send/recv）在 HAL 层分开命名（§8 H3/H6 vs H7）；fence 完成语义
按平面区分（fabric 同步 fence vs NIC 异步完成），避免跨平面语义污染。

### 4.4 Memory Pool

| v1 选项 | 说明 |
|---------|------|
| **私有 fabric memory**（v1 默认）| 扩展 `plugins/gpu_driver/sim/mem_pool`（per ADR-058）为节点级 fabric pool；与 ADR-069 协同 |
| **CXL Type-3**（v2 trigger 升级）| 标准化保真度，仅当消费者要求时启动（与 SYN-1 共享 trigger）|

**Q2 答案**：v1 = 私有，v2 = CXL Type-3 trigger。ATS/CXL §4.6 #4 不建议默认 CXL Scope B。

**归属分工**：backing store 复用 ADR-058 `mem_pool`（gpu_driver/sim），
避免重复实现；switch sim 持有节点分区视图（NFA range → pool 分区）。
该分工为 ADR-077 候选的显式裁决点之一。

### 4.5 Fabric 控制平面（v1 极简模型）

**行业对照**：Rubin 实证显示 scale-up fabric 需要一等控制平面——
Fabric Manager / Global-Local FM / NVLink Subnet Manager / NVOS / IMEX
负责路由表、分区、访问控制位、地址分配、错误监控 [R §3.9,
NVIDIA Official Documentation]。"数据面 alone 无法安全建立路由、
映射、隔离、恢复" [R §3.9]。

**v1 极简控制平面**（零新增组件，功能退化到 boot-time 常量）：

| FM 功能 [R §10.9] | v1 实现 | 位置 |
|------------------|---------|------|
| 枚举设备 | GPU 设备枚举 = CppLink-C 注册（per §4.6.5） | ③ switch sim + ① 既有 |
| 分配 fabric 地址 | 静态分区表（编译期/启动期常量） | ③ switch sim |
| 编程路由 | O(1) range check，无表 | ③ switch sim |
| 建立内存窗口 | `g_node_pa_store.partition()`（§7.2） | ③ gpu_driver sim |
| 配置多播组 | 固定 multicast window（§5.3） | ③ switch sim |
| 服务发现 | ❌ **已废弃（per §9.3 v0.3）**；改用 CppLink-C endpoint 直接连接 | — |
| 分区/隔离/恢复 | ❌ v1 无（统一信任域，多进程无隔离） | — |

**v2 升级挂点**（全部 trigger-gated，不进 W0-W4）：
- 动态 NFA 重映射 / 路由表（→ ADR-077 v2 选项，§3.3）
- 访问控制位 / 分区隔离（→ 多进程消费者出现时；v0.5 标记 ADR-087 关联，ADR-011 将 Supersede per ADR-087 D8）
- IMEX 类跨进程/跨节点 fabric handle 服务（→ 见 §10 消费者契约）
- 故障注入 / 降级运行（→ RAS 测试消费者出现时）

**Non-Goal 声明**：动态 Fabric Manager、NVLSM 等价物、跨节点 IMEX 服务
v1 均不实施（§14）。

### 4.6 CppLink 协议（v0.3 新增）

> **本节为 v0.3 新增** — 命名决策：L1 Switch ↔ GPU 设备的 link 协议命名为 **CppLink**（避免 NVLink 商标，明确是本项目抽象）

#### 4.6.1 命名理由

| 候选 | 选择 |
|------|------|
| ~~NVLink~~ | ❌ 用户明确反对（NVIDIA 商标 + 项目为 C++）|
| ~~FabricLink~~ | ⚠️ 过于通用 |
| **CppLink** ✅ | 体现"C++ 仿真器项目"性质；与 CppTLM 配套但不混淆；可演进（CppLink 1.0/2.0）|

**全称**：**Cpp Language Link**（或 Common Parallel-processing Link）。本文件统一使用 **CppLink**。

#### 4.6.2 协议目标

| 目标 | 说明 |
|------|------|
| 跨进程传输 | 支持 L1 Switch 进程 ↔ N 个 GPU 进程（per Multi-Process Vision D1.b）|
| 无 raw pointer | 所有消息用 descriptor（region ID + generation + offset + permissions）|
| 版本化 | ABI version 字段，特性协商 |
| 模拟时钟同步 | CppLink-T 通道（per gem5+GPGPU-Sim 锁步模式）|
| 跨进程恢复 | process crash 后 reconnect + 状态恢复（限定为 v2 范围，v1 = fail-fast）|
| Failover | 单 GPU 进程死掉不导致整个仿真崩溃（限定为 v2 范围，v1 = fail-fast）|

#### 4.6.3 4 通道定义

| 通道 | 方向 | 物理实现 | 语义 |
|------|------|---------|------|
| **CppLink-C**（Control）| 所有进程 ↔ L1 Switch | Unix domain socket + `SCM_RIGHTS` FD 传递 | 启动 / 设备注册 / capability 协商 / 关闭 / 心跳 |
| **CppLink-D**（Data）| 所有进程 ↔ L1 Switch（CPU 侧用于 MMIO proxy / doorbell 下发）| 共享内存 + region descriptor | NFA 路由查询 / MMIO proxy / 多播 payload / doorbell 触发 |
| **CppLink-E**（Event）| GPU → L1 Switch → CPU sim（fence/中断经 Switch 转发）| `eventfd` + shared-memory ring | Fence 完成 / 中断上报 / 错误事件 / 心跳应答 |
| **CppLink-T**（Tick）| L1 Switch → GPU 进程 | shared-memory flag + 确定性序列号 | 模拟时钟 tick（per gem5+GPGPU-Sim 锁步模式）|

#### 4.6.4 Region Descriptor 初始规范

```cpp
// CppLink v1.0 Region Descriptor（固定 64 字节；no raw pointer）
struct cpplink_region_descriptor_v1 {
    uint32_t magic;               // 0x4350504C "CPPL" — ABI 校验
    uint32_t abi_version;         // CPPLINK_ABI_VERSION (v1.0 = 0x00010000)
    uint32_t region_id;           // 全局唯一
    uint32_t generation;          // ABA 防护
    uint32_t kind;                // 0=hbm,1=mem_pool,2=nic_buf,3=mcast_window,4=doorbell_ring
    uint32_t permissions;         // R/W/X bitmask
    uint32_t owner_device;        // 拥有该 region 的 device_id
    uint32_t cache_policy;        // 0=device_local, 1=shared, 2=replicated
    uint64_t nfa_base;            // 节点 Fabric Address 起始
    uint64_t size;                // 字节数
    uint64_t file_offset;         // backing file 偏移（必须页对齐）
    uint64_t reserved;            // 0 —— v1.1 扩展位
};
// static_assert(sizeof(cpplink_region_descriptor_v1) == 64);
```

> descriptor 模型参照 QEMU vhost-user memory region protocol（fd 经 SCM_RIGHTS 传递 + region 表）。

#### 4.6.5 进程启动顺序（v1）

```
1. UsrLinuxEmu CPU sim 启动 + 注册 VFS devices
2. L1 Switch CPU sim 启动 + 监听 CppLink-C socket
2.5. UsrLinuxEmu CPU sim 连接 L1 Switch CppLink-C：
     capability 协商 → 接收 region descriptors → mmap → 注册 E 通道接收端（fence/中断转发）
3. 每个 GPU sim 启动：
   a. 连接 L1 Switch CppLink-C socket
   b. 发送 capability 协商 (cpplink_negotiate_v1)
     c. abi_version 不兼容 → 拒绝连接 + 明确日志（per risk #13 缓解）
   d. 接收 memory_region_descriptors (shared regions)
   e. mmap shared regions 到 process-local address（地址不重要）
   f. 注册 CppLink-E eventfd + 完成 ring
4. 启动后：所有 data plane 通信通过 CppLink-D (shared mem) + CppLink-E (events)
4.5. 注册屏障：L1 Switch 等待 1 CPU + N GPU 全部完成注册后，广播 "fabric_ready" 并开始 CppLink-T 序列
5. shutdown：发送 CppLink-C close → 清理 → 退出
```

#### 4.6.6 v1 限制（明示）

- 同步 fencing：复用 §5.4 fence_id（Stage 4.7 资产），通过 CppLink-D 写入共享 ring
- 多播实现：L1 Switch 进程内 fan-out loop（per ADR-083 D1 replicated-object 语义）
- 错误处理：all-or-nothing with explicit `-EIO`（per ADR-083 D3）
- 模拟时钟：L1 Switch 进程为全局 tick master，CppLink-T 以确定性序列号广播至全部 GPU 进程并阻塞至全部 ack（gem5+GPGPU-Sim 锁步模式）；CPU sim 不参与 GPU 锁步，其 MMIO/doorbell 延迟经 D 通道 latency 模型计费。
- 进程崩溃恢复 / Failover：v1 = fail-fast（任一进程心跳超时或断开 → 全仿真确定性终止并 dump 状态）；reconnect 协议（QEMU vhost-user reconnect 参照）为 v2 候选，经 ADR-086 trigger 登记。§4.6.2 的 "跨进程恢复 / Failover" 目标因此限定为 v2 范围。
- Checkpoint / state dump：v1 不支持（gem5 规则：kernel 运行中禁止 checkpoint）；跨进程 quiesce handshake 为 v2 候选，trigger 登记。
- Region 集合静态：boot 后不允许动态增删 region（动态重映射 = ADR-077 v2 选项）。
- 心跳参数：C 通道心跳间隔 1s、超时 3s（v1 固定值）；超时动作 = fail-fast。

#### 4.6.7 与 ADRs 的关系

| ADR | 角色 |
|-----|------|
| ADR-077（NFA charter）| 提供 NFA base / region 划分 |
| ADR-078（plugin split → process split, v0.3 修订）| 定义 L1 Switch 进程边界 |
| ADR-083（多播语义）| 在 L1 Switch 进程内实现 replicated-object fan-out |
| ADR-085（消费者契约）| 如果 tadr-307 / IFabricDriver 暴露 CppLink，作为契约面 |

---

## §5 通信语义

### 5.1 统一 load/store

**所有节点内通信通过 memory load/store 语义**：

```c
// 伪代码：GPU 0 读 GPU 1 的内存
uint64_t value = *(volatile uint64_t*)(uvm_va_for_gpu1_buffer);
// → MMU 翻译 → NFA → Switch 路由 → GPU 1 HBM 完成
// → 仿真器内自动可见（进程内一致性 free，per §6.1）
// → 跨进程可见性经 fence/descriptor 契约（§6.1 跨进程一致性契约）
// → 真实硬件语义 = PGAS 远程访问，可见性由同步原语保证（见 §5.4）
```

**关键约束**：**没有显式消息传递 API**（无 send/recv，无 mailbox）。

### 5.2 DMA 路径（fallback / NIC 出口）

虽然 fabric 是纯 load/store，但以下场景仍走 DMA：
- NIC buffer 的 DMA（与外部网络交互）
- Future: P2P DMA（scale-up §4.7.2）—— v1 不实施

**与 ATS 的关系**：DMA 路径是 ATS 加速的地方，但 scale-up 纯 load/store 愿景**降低** ATS 优先级（per SYN-2）。

### 5.3 PGAS 多播

**多播语义类型声明（per Rubin 实证术语纪律 [R §10.5]）**：
v1 实施 **replicated-object multicast with switch write fan-out**——
一次 store 经 switch 复制到每个参与设备的 PGAS 接收区（每设备一份物理副本）。
与 CUDA multicast object 的 `multimem.st` 写路径语义同构 [R §2.8, §5.8,
NVIDIA Official Documentation]。本设计**不隐含** coherent shared line；
**不实施** SHARP 类 in-switch reduction（Non-Goal，§14；消费者拉动时经
ADR-086 trigger 重新评估）。

```
PGAS 多播区（UVM VA 子区）：
  ┌──────────────────────────────────┐
  │  Multicast Window (NFA)           │
  │  物理地址：0x100_0000_XXXX_0000   │
  │  大小：固定（如 64 KiB）         │
  └──────────────────────────────────┘

write to Multicast Window:
  1. GPU 发起 store → 命中 multicast NFA range
  2. Switch sim 拦截 → 循环 fan-out 到所有设备对应 PGAS 接收区
  3. 顺序由 single-threaded dispatch 天然保证（CONF-4 / SYN-4）
  4. 完成通知：sync fence 默认，async via ADR-060/062 可选
```

**Q4 答案**：同步 fence（v1），异步 opt-in 后续（ADR-062 已提供路径）。

### 5.4 同步原语（v1 = fence；v2 候选 = counted write）

**v1**：所有 fabric 完成经 `fence_id_*`（Stage 4.7 资产）同步——
fence 即最简单的 completion counter。

**行业对照（counted writes）**：Rubin 引入 counted writes 作为
GPU↔GPU 低开销同步原语：producer 写 payload 后更新远端 counter，
consumer 轮询 counter 达标即消费，免 barrier/ack 序列
[R §1.2, §2.7, NVIDIA Official]。抽象形态 [R §10.6]：

    remote_store(payload)
    remote_counted_store(completion_counter)
    consumer_wait(counter >= expected)

**v2 候选（trigger-gated）**：若 TaskRunner/NVSHMEM-like 消费者需要
GPU 端 signal/wait 语义，经 ADR-086 trigger 评估新增
`hal_fabric_signal / hal_fabric_wait`（≤2 fn-ptrs，归入 §8 H6/H7 批次）。
v1 不实施：fence_id 已覆盖全部现有同步需求，新增原语无消费者。

---

## §6 一致性与顺序性政策

### 6.1 一致性：协议模拟 Non-Goal + 语义分级声明

**架构决策（CONF-1/5 解决）**：

| 维度 | 决策 | 依据 |
|------|------|------|
| 一致性协议模拟（directory/MESI/snoop）| ❌ **Non-Goal** | ADR-073 D1：进程内一致性天然 free |
| Directory protocol | ❌ **拒绝** | scale-up §4.4.2 推荐被本文件覆盖 |
| Cache line flush event | ✅ **实施**（CXL Scope A 范围）| 让 `GPU_MMU_EVENT_CACHE_FLUSH` 有真实语义 |

**行业证据（2026-08 Rubin 实证）**：本决策不是保真度妥协——
NVIDIA Rubin NVL72 的 GPU↔GPU 路径同样**不做全局硬件相干**：
"hardware-supported remote memory access with software-managed
virtual memory and ownership" [R §2.5, NVIDIA Official]。
硬件相干仅存在于 CPU↔GPU 子域（NVLink-C2C + ATS）[R §2.5, §5.6]。
"72 GPU 全部 HBM 硬件相干" 无公开证据支持 [R §9.4]。

**一致性语义分级**（仿真器提供的一致性 = 下表最强项的超集近似）：

| 级别 | 含义 | 行业对应 | 仿真器内 |
|------|------|---------|---------|
| 本地 cache 相干 | 单 GPU 内 | 任意 GPU | 天然 free |
| CPU↔GPU 硬件相干 | C2C/ATS 子域 | Rubin Vera↔Rubin | CXL Scope A 事件 |
| 远程可见性（PGAS）| load/store 可达，可见性经同步 | NVLink peer access | 天然 free |
| 多播副本一致性 | 一次写更新所有副本 | CUDA multicast | Switch fan-out |
| 归约一致性 | 确定性结合操作 | NVLink SHARP | ❌ Non-Goal（§14） |

**理由**：在单进程仿真中，CPU/GPU 共享同一地址空间，cache coherence 天然成立。
模拟 coherence protocol：增加 ~5-10K LOC；无驱动可移植性价值
（Linux 驱动不管理 coherence protocol）；与 ADR-073 简化原则冲突；
且 Rubin 实证表明行业旗舰同样不在 GPU↔GPU 面做协议级相干。

**跨进程一致性契约（v0.4 增补，per ADR-079 范围）**：多进程后"天然一致"不再成立；shared memory 仅提供 transport 级共享。v1 采纳 **Level 0（functional consistency：payload 字节经共享 region 可见）+ Level 1（simulated visibility：写可见性以 fence/event 为界）**；**Level 2（cache line state / writeback / invalidation timing 模型）维持 Non-Goal**。fence_id 与未来的 counted-write 原语必须显式标注 in-process vs cross-process 语义。

### 6.2 顺序性：**Program-order v1**

| v1 行为 | v2 升级路径 |
|---------|------------|
| 所有 load/store 按 program order 完成 | Fence 指令作为 trace hooks（不真正 reordering）|
| 显式 fence 视为 no-op（带 trace）| Future: selective reordering simulation if needed |

**Q7 答案**：错误处理显式 Linux negative errno（per ADR-023 D4 + ADR-076 D5 precedent）。

### 6.3 ADR-077 / ADR-079 决策

**ADR-079 候选**：Coherence simulation Non-Goal（per ADR-073 D1）

---

## §7 多实例 GPU 前置条件

### 7.1 现状问题（CONF-3）

| 问题 | 影响 | 来源 |
|------|------|------|
| 进程全局 `g_vram_store`（per ADR-023 2026-08-02 修订）| N GPU 实例共享同一 backing store → per-device 内存区分失败 | CONF-3 |
| ADR-069 单 BAR 布局 `0x10000000` | N GPU 需 N 个 BAR window 或 per-device offset | ADR-069 D1 |
| 98 个测试 binary 全部单 GPU | 无 multi-instance 测试 harness | GAP-1 |

### 7.2 重构方向（变负债为资产）

将 `g_vram_store` 从"单设备 VRAM"重构为"**节点统一 PA backing store，按 device 分区**"：

```
// Before
g_vram_store.init(256 MB);  // 单设备

// After
g_node_pa_store.init(N * per_device_size + mem_pool_size + nic_size);
g_node_pa_store.partition(/* static map */);  // per-device 静态分区
```

**这恰好是 scale-up 愿景的物理 backing store**——一个变更同时解决 CONF-3 和 scale-up §1.4.1 的需求。

### 7.3 Spike GO/NO-GO（仿 stage-2-spike 格式）

**Wave 0 必做项**：

| 检查 | 标准 | GO/NO-GO 影响 |
|------|------|--------------|
| 2 个 `gpu_driver` 实例同时加载 | VFS 注册都成功 | GO |
| per-device BAR 窗口区分 | `/proc/iomem` 显示 N 个 BAR 区段 | GO |
| `g_vram_store` 重构后 per-device 内存隔离 | device0 写入不影响 device1 | GO |
| 现有 98 个测试 binary 仍绿 | 零 regression | GO |
| 任一失败 | — | **NO-GO → 阻塞 P2** |

### 7.4 ADR-080 候选

**ADR-080**：Multi-Instance GPU 重构前置（`g_vram_store` 节点分区 + spike 流程）

---

## §8 HAL 边界扩展规划

### 8.1 现状

[FACT] `plugins/gpu_driver/hal/gpu_hal.h:4` 明确 **68 fn-ptrs**（65 原 + 3 ADR-076 新增）。Append-only per ADR-023 D4。SSOT §1.10.2 指南：HAL > 50 后应优先**复用现有 fn-ptr**（参数扩展）而非新增。

### 8.2 Fabric 扩展序列（H1-H8）

| Seq | Phase | Candidate ops | New fn-ptrs | ADR |
|-----|-------|---------------|-------------|-----|
| H1 | ATS Phase 0 | — | **0** | 无需（文档）|
| H2 | CXL Scope A | — | **0** | （如需 flush op，最多 1）|
| **H3** | **Fabric P2-P3**（switch skeleton + multi-GPU）| `hal_fabric_attach/detach`, `hal_fabric_route` | **2-3** | **ADR-081** |
| **H4** | **Fabric P4**（memory pool）| `hal_mem_pool_attach_fabric`（或复用 `mem_pool_*`）| **0-1** | **ADR-081** 或 ADR-082 |
| **H5** | **Fabric P5**（UVM translation）| — | **0**（扩展 ADR-061 `iommu_map` `domain_id`）| 无需新 op |
| **H6** | **Fabric P6**（multicast）| `hal_mcast_write` (+ optional `hal_mcast_route_set`) | **1-2** | **ADR-082** |
| **H7** | **Fabric P7**（NIC dual role）| `hal_fabric_rdma_send/recv` | **1-2** | **ADR-082** |
| H8 | ATS Phase 1（trigger）| `hal_ats_invalidate` | **1** | （独立 ADR）|
| **合计** | | | **5-9** → **73-77** | |

### 8.3 预设拒绝（CONF-4）

scale-up doc §5.3.1 提议的 6 个 ops 中，**2 个预设拒绝**：

| 提议 | 拒绝原因 | 复用方案 |
|------|---------|---------|
| `hal_uvm_map/unmap` | 与 ADR-061 `iommu_map/unmap`（已有 `domain_id` 参数）重复 | 扩展 ADR-061 `domain_id` 含义 |
| `hal_mem_pool_alloc` | 与 Stage 4.7 已交付的 9 个 `mem_pool_*` ops 重复 | 复用 `mem_pool_set_attr` 等 |

### 8.4 ADR 批量化策略

每阶段一个 ADR（per ADR-059 D3 + ADR-076 3-op 合并先例）：

- **ADR-081**：Fabric HAL Phase-1 batch（H3 + H4 = 2-4 ops）
- **ADR-082**：Fabric HAL Phase-2 batch（H6 + H7 = 2-4 ops）
- 后续 ATS / CXL 各自独立 ADR

---

## §9 插件架构与跨模块查找

### 9.1 进程拆分决策（v0.3 重大修订）

> **v0.3 重大修订**：L1 Switch **不是** UsrLinuxEmu 内的 plugin；是**独立 CPU 进程**。

**新决策（替代原 Option B plugin 拆分）**：**L1 Switch 是独立 CPU 进程**（per Multi-Process Vision D1.b + 用户 2026-08-14 澄清）

**进程拓扑**：
```
[UsrLinuxEmu CPU sim] ←CppLink-C→ [L1 Switch CPU sim] ←CppLink-D/E/T→ [GPU sim #0..N-1]
```

**选项评估**（v0.3 修订）：

| 选项 | 状态 | 备注 |
|------|------|------|
| ~~Option A（switch 作为 gpu_driver 子模块）~~ | ❌ 拒绝 | 同 v0.2 |
| ~~Option B（`plugins/switch_driver/` 插件）~~ | ❌ **v0.3 拒绝** | L1 Switch **不是** UsrLinuxEmu 内的 plugin；必须是独立进程（per Multi-Process Vision D1.b）|
| Option D（独立 CPU 进程）| ✅ **v0.3 采纳** | L1 Switch 启动为独立 OS 进程，通过 CppLink 与 UsrLinuxEmu / GPU 进程通信 |
| ~~Option E（fabric_driver）~~ | ❌ | 同 v0.2 拒绝 |

### 9.2 跨进程架构（替代 plugin 结构）

```
┌─────────────────────────────────────────────────────────────┐
│ UsrLinuxEmu 仓                                                 │
│ ├─ src/        ① kernel sim + VFS + ModuleLoader               │
│ ├─ plugins/gpu_driver/     ② driver + ③ sim（per-GPU process） │
│ └─ plugins/switch_driver/  ② L1 Switch device driver + IOCTL  │  ← v0.4 新增（per ADR-087 D6.2）
│    （仿造 gpu_driver 模板；v1 借鉴 GPU 软件栈）                │
│                                                                 │
│ 注：switch ③ 硬件 sim 部分**不在 UsrLinuxEmu**（per ADR-087 D6）│
│     而在 CppTLM 仓的 soc_arch/switch/                            │
└─────────────────────────────────────────────────────────────┘

独立仓（v0.4 新增，per ADR-087 D6.1）：
┌──────────────────────────────────────────────┐
│ CppTLM 仓（外部）                             │
│ └─ soc_arch/switch/  ③ L1 Switch SoC 仿真    │
│    • TLM 模型 + NFA routing + multicast       │
│    • NVLink timing（per Vision Phase 4）      │
│    • 通过 CppLink 接入 UsrLinuxEmu CPU sim   │
└──────────────────────────────────────────────┘

独立仓（v0.4 新增，per ADR-087 D6.3）：
┌──────────────────────────────────────────────┐
│ TaskRunner 仓（外部）                         │
│ └─ src/switch/  ① L1 Switch userspace API    │
│    • libswitch_taskrunner.so (LD_PRELOAD shim)│
│    • ISwitchDriver consumer-side              │
│    • v1 借鉴 CUDA runtime shim 模板            │
└──────────────────────────────────────────────┘

独立进程（v0.3）：
┌──────────────────────────────────────────────┐
│ Process: switch_sim (新独立可执行)              │
│ • NFA routing table                           │
│ • Multicast fan-out engine                    │
│ • Memory pool NFA 视图                        │
│ • Per-GPU CppLink endpoint 管理                │
│ • 通过 Unix socket + 共享內存接入               │
└──────────────────────────────────────────────┘
```

**关键架构后果**：

- L1 Switch **不是** `plugins/switch_driver.so`（动态加载）
- L1 Switch 是**独立可执行**（如 `bin/switch_sim`），作为 OS 进程启动
- L1 Switch 进程启动后 bind/监听全部 CppLink endpoint；UsrLinuxEmu CPU sim 与 GPU sim 进程均作为 connector 接入（带重试）
- GPU sim 进程**通过 CppLink-D/E/T 接入** L1 Switch
- 无 switch_driver plugin 内的 drv/sim 划分（"switch 是 ③ 硬件"的解释**转移**到独立进程）

### 9.3 跨模块查找模式（v0.3 修订）

```
v0.2 模式（已废弃）：
  GPU driver (②) → hal_user.cpp → ServiceRegistry::lookup("fabric") → switch_driver sim (③)

v0.3 模式（取代）：
  [UsrLinuxEmu CPU sim] ←→ [L1 Switch CPU sim] ←→ [GPU sim #0..N-1]
                          ↑ CppLink-C (control)
                          ↑ CppLink-D (data)
                          ↑ CppLink-E (events)
                          ↑ CppLink-T (tick)
```

- **无 ServiceRegistry 跨进程查找**（v0.2 模式假设已废弃）
- **CppLink endpoint 注册**：L1 Switch 进程启动时 bind Unix socket，UsrLinuxEmu 和 GPU sim 进程 connect
- **服务发现**：通过 CppLink ABI 协商（capability exchange，per §4.6.5 启动顺序）

### 9.4 ADR-078 候选（v0.3 修订）

**ADR-078**（v0.3 修订题名）：**L1 Switch CPU 进程拆分 + CppLink 协议 + 双角色分工**

- 范围：
  - L1 Switch 进程生命周期（启动 / 接入 / 关闭）
  - CppLink v1.0 协议规范（per §4.6）
  - 双角色保留 + 偏离声明（per v0.2 §4.3）+ net_driver 备选裁决
  - 4 通道（C/D/E/T）物理实现
  - Region descriptor 协议（per §4.6.4）
- 引用：ADR-077（NFA charter）+ ADR-058（sim_mem_pool 复用）|
- 跨仓评审：与 CppTLM owner（如果 NVLink timing 借用）同步

### 9.5 v0.2 → v0.3 plugin 概念移除的影响

| 原 v0.2 假设 | v0.3 修订 |
|--------------|---------|
| `plugins/switch_driver/sim/switch_sim.cpp` | ❌ 取消；改为独立可执行 `bin/switch_sim` |
| `ServiceRegistry::lookup("fabric")` | ❌ 取消；改为 CppLink-C endpoint 注册 |
| `gpu_driver 永不链接 switch_driver plugin` | ⚠️ 不再适用；GPU sim 独立进程，通过 CppLink 通信 |
| `switch 是 ③ 硬件，不在 ① 内` | ✅ 保留；但 v0.3 进一步：switch 是**独立 OS 进程**（不与其他仓共享进程空间）|

### 9.6 进程生命周期与故障矩阵（v1 = fail-fast）

| 场景 | 检测 | v1 行为 | v2 选项 |
|------|------|---------|---------|
| 启动竞态（connector 先于 listener）| connect 重试（指数退避，上限 30s）| 等待至 fabric_ready 屏障（§4.6.5 step 4.5）| — |
| L1 Switch 崩溃 | C/D/E 通道断开 + 心跳超时 | 全部进程确定性终止 + 状态 dump | reconnect（vhost-user 参照）|
| 单 GPU 进程崩溃 | Switch 侧 E 心跳超时 | 全仿真终止（v1 不隔离单点故障）| 单 GPU 重启 + 状态重注入 |
| CPU sim 先退出 | Switch 侧 C 断开 | Switch 广播 close → GPU 进程有序退出 | — |
| 正常关闭 | §4.6.5 step 5：C 通道 close 广播 | 逆启动顺序退出（GPU → CPU → Switch）| — |

心跳参数与超时值由 ADR-078 裁决；fail-fast 与 §4.6.6 v1 限制一致。

---

## §10 消费者契约

### 10.1 状态

**当前无消费者**（GAP-3）。这是**所有 P7+ 实施的硬门**（per Oracle D9）。

### 10.2 可选消费方

| 消费方 | 状态 | 影响 |
|--------|------|------|
| TaskRunner `IFabricDriver` | 未存在 | 需新建跨仓契约（per ADR-035 R5 4 步流程 + ADR-076 模板）|
| KFD SVA 用例 | 概念阶段（per Oracle #4 ATS deferral 同步考虑）| 触发 ATS Phase 1+ |
| 内部分析 / 性能测试 | 可作 P1 消费者 | 文档 + 简单脚本级 demo |

### 10.3 ADR-085 候选

**ADR-085**：消费者契约决策（TaskRunner 跨仓 IFabricDriver or internal-only）

**触发**：Wave 4 之前必须确定（per §12.3 + Oracle D9）。

**v0.3 新增**：消费契约面新增 **CppLink 契约层**（独立于 IFabricDriver）

- `IFabricDriver`：TaskRunner 视角的 fabric API（per OpenSpec tadr-307 模型）
- `CppLink`：L1 Switch 进程 ↔ GPU 进程的**传输层契约**（per §4.6）
- 两层正交：消费者可以只关心 IFabricDriver，CppLink 是内部实现细节

### 10.2a 行业对照：IMEX 与 NVLink Fusion

**IMEX 对照**：NVIDIA IMEX 提供跨节点/跨 OS 域的 fabric memory
export/import + handle 生命周期 + 特权控制面 [R §4.6, NVIDIA Official
Documentation]。本项目的跨仓消费者契约与 IMEX 同构于"跨域 handle +
访问授权"层——差异在边界性质（仓库/进程 vs 节点/OS 域）。ADR-085
评审应以 IMEX 为参照系定义 handle 语义的最小集。

**NVLink Fusion 对照**：NVIDIA 以 Fusion 向第三方 silicon 开放 NVLink
域（MediaTek/Marvell/Qualcomm/Fujitsu/AWS 等 [R §6.5, §8.4]）——
fabric 成为"受控开放的战略平台边界"。本项目对 TaskRunner 的
IFabricDriver 契约（ADR-035 R5 4 步流程 + ADR-076 模板）正是同一姿态：
定义 canonical 接口、允许外部实现接入、自己不拥有对方实现。
该对照为消费者契约提供行业先例背书，**不构成**范围扩张理由。

### 10.2b CppLink 契约层（v0.3 新增）

**定义**：CppLink 是 L1 Switch 进程与 GPU 进程之间的**传输层契约**，
不直接面向消费者。它的 ABI 范围限于：
- CppLink-C / -D / -E / -T 4 通道的消息格式
- Region descriptor（per §4.6.4）
- 启动 / 关闭 / 心跳 / reconnect 流程

**与 IFabricDriver 的关系**：
- TaskRunner 消费者视角：`IFabricDriver`（per tadr-307 模型），无需关心 CppLink
- UsrLinuxEmu 视角：同时实现 `IFabricDriver` consumer + CppLink producer（CPU sim 进程）
- L1 Switch 视角：CppLink endpoint + fabric control plane（不暴露 IFabricDriver）
- GPU sim 视角：CppLink endpoint + device sim（不暴露 IFabricDriver）

**ADR 归属**：CppLink 协议规范纳入 **ADR-078**（L1 Switch 进程拆分），
IFabricDriver 契约纳入 **ADR-085**（消费者契约）。两者独立评审。

---

## §11 与 ATS/CXL 轨道的协同

### 11.1 协同矩阵（SYN-1 ~ SYN-6，v0.3 新增 SYN-6）

| SYN | 内容 | 本文件采纳 |
|-----|------|-----------|
| **SYN-1** | CXL Type-3 = scale-up 内存池的标准化选项（共享 trigger）| ✅ §4.4 v2 trigger |
| **SYN-2** | Scale-up 纯 load/store 愿景降低 ATS 优先级——**限 fabric 平面**：GPU↔GPU 走显式映射而非 ATS（Rubin 同构 [R §2.5]）；CPU↔GPU 平面（CXL Scope A / NVLink-C2C-ATS 对应域）的 ATS 相关性不变，维持 trigger-gated [ATS §4.6 #1] | ✅ §5.2 DMA fallback，§8 ATS trigger-gated |
| **SYN-3** | ADR-058 + ADR-069 + ADR-073 组合 = 统一 PA 设计基础 | ✅ §3.1 charter |
| **SYN-4** | 多播完成复用 Stage 4.7 `fence_id_*` + ADR-062 异步路径 | ✅ §5.3 + §8 H6 |
| **SYN-5** | CXL Scope A 给 fabric 一致性事件真实语义 | ✅ §6.1 + 触发 Wave 1 启动 |
| **SYN-6** 🆕 | **Multi-Process Vision 协同**：4 仓协作（UsrLinuxEmu + CppTLM + PTX-EMU + TaskRunner）；L1 Switch 独立 CPU 进程（per D1.b）；CppLink 抽象跨进程链路（per §4.6）；scale-up fabric = Vision Phase 4 子集 | ✅ §2.1 拓扑 + §4.6 CppLink + §9.1 进程拆分 + §13 Phase R-5 gate |

### 11.2 ATS / CXL 轨道在本文件中的角色

| 轨道 | 决策 | 引用 |
|------|------|------|
| **ATS Phase 0** | ✅ 采纳（文档契约修正）| §13 显式 |
| **ATS Phase 1+** | ⏸️ Trigger-gated（per Oracle #4）| §13 显式 |
| **CXL Scope A** | ✅ 采纳（Wave 1 启动项）| §13 + §6.1 |
| **CXL Scope B** | ⚠️ Trigger-gated（与本文件 v2 升级共享 trigger）| §4.4 |
| **CXL Scope C / PEER / FM / MLD / IDE / PMU** | ❌ Non-Goal | §14 |

### 11.3 触发 registry（CONF-6 解决）

| Trigger | 启动项 |
|---------|--------|
| **SYN-1 trigger**（标准化内存池保真度）| CXL Scope B + Scale-up 内存池 v2 升级 |
| **SYN-2 trigger**（fabric P2P DMA 路径出现）| ATS Phase 1+ |
| **GAP-3 trigger**（TaskRunner 或 KFD 出现 fabric 消费者）| Scale-up Wave 4 |
| **Performance trigger**（fabric 延迟成为瓶颈）| 顺序性 v2 升级 |
| **Control-plane trigger**（多进程/隔离消费者出现）| 动态 FM + 访问控制（§4.5 v2）|
| **IMEX trigger**（跨仓消费者需 fabric handle 共享）| IMEX-like 最小 handle 服务（ADR-085 关联）|
| **counted-write trigger**（NVSHMEM-like 消费者出现）| hal_fabric_signal/wait（≤2 fn-ptrs，归入 §8 H6/H7 批次）|
| **in-network-compute trigger**（集合通信消费者出现）| SHARP-like 最小 reduction（Non-Goal 解除）|
| **RAS/降级 trigger**（故障注入测试消费者出现）| hot-swap / degraded operation 模拟 |
| **拓扑分层 trigger**（N≥72 消费者出现）| switch sim 两层拓扑（§1.3 注记）|

**ADR-086 候选**：Trigger registry 整合（单点决策，per §12.4）——所有 Rubin 启示的单一泄洪口。

---

## §12 决策点 → ADR 候选清单

**重要**：本节列出的不是已经做出的决策，而是**需要由后续 ADR 正式做出的架构决策**。每个 ADR 应**聚焦单一决策点**，遵循 ADR-035 R3 + ADR-059 D3。

### 12.1 基础设施 ADR（Wave 0-1 必做）

| ADR# | 焦点决策（v0.3 修订）| 阻塞 Wave | 来源 Oracle |
|------|---------|-----------|------------|
| **ADR-077** | **节点级统一 PA 命名空间**（NFA charter + base 值 + IPC region descriptor）| Wave 2 | D1 |
| **ADR-078** 🆕 | **L1 Switch CPU 进程拆分 + CppLink 协议**（v0.3 重大修订：替代原 plugin 拆分）| Wave 2 | D2 + Vision D1.b |
| **ADR-079** | **Coherence simulation Non-Goal + 跨进程一致性契约**（v0.3 修订：增加 3 级 consistency）| Wave 2 | D4 |
| **ADR-080** | **多实例 GPU 重构前置**（`g_vram_store` 节点分区 + spike 流程 + IPC readiness）| Wave 2 | D8 |
| **ADR-087** 🆕 | **Multi-Process Device & Fabric Seam ADR**（v0.3 新增：跨仓跨进程边界规范）| Wave 0 | IMP-1/4/5/6/7（D2 policy + ADR-078 wire artifacts + ADR-077 references；per §12.6 关键路径第一个起草）|

### 12.2 HAL 扩展 ADR（Wave 2+ 必做）

| ADR# | 焦点决策 | 阻塞 Wave | 来源 Oracle |
|------|---------|-----------|------------|
| **ADR-081** | **Fabric HAL Phase-1 batch**（H3 switch 路由 + H4 mem pool attach, 2-4 ops；**v0.3：H3 改为 CppLink-D descriptor-based**）| Wave 2-3 | H3-H4 |
| **ADR-082** | **Fabric HAL Phase-2 batch**（H6 multicast + H7 NIC, 2-4 ops；**v0.3：H7 改为 CppLink-D/-E semantics**）| Wave 3-4 | H6-H7 |

### 12.3 语义 ADR（Wave 3-4 必做）

| ADR# | 焦点决策 | 阻塞 Wave | 来源 Oracle |
|------|---------|-----------|------------|
| **ADR-083** | **Multicast 语义**（fan-out in L1 Switch 进程 + 完成机制 + 错误处理 + CppLink multicast 通道）| Wave 3 | §4.7 |
| **ADR-084** | **PGAS 地址语义**（zone 划分 + 翻译规则）| Wave 3 | D3 |
| **ADR-085** | **消费者契约**（TaskRunner IFabricDriver 或 internal-only；**v0.3：与 CppLink 契约层正交**）| Wave 4 | D9 |

### 12.4 治理 ADR（Wave 0-1 必做）

| ADR# | 焦点决策 | 阻塞 Wave | 来源 Oracle |
|------|---------|-----------|------------|
| **ADR-086** | **Trigger registry 整合**（SYN-1/2、SYN-6 multi-process、GAP-3、performance 等统一入口）| Wave 0 | CONF-6 |

### 12.5 总览（v0.3 修订）

| ADR 数量 | 数量 | 时序 |
|---------|----:|------|
| 基础设施 ADR（必须先做）| **5**（v0.3 新增 ADR-087）| Wave 0-1 |
| HAL 扩展 ADR | 2 | Wave 2-3 |
| 语义 ADR | 3 | Wave 3-4 |
| 治理 ADR | 1 | Wave 0 |
| **合计** | **11** | |

**ADR 群编号**：ADR-077 ~ ADR-087（v0.3 新增 087）。

### 12.6 ADR 实施顺序建议（v0.3 修订）

```
Wave 0a: ADR-087 起草 + 4-owner 评审启动（关键路径）
Wave 0b: ADR-077/078/080/086 起草并行；Accept 以 ADR-087 Accepted 为门
   ↓ Phase R ship (Vision) + spike GO + ADR-079
Wave 1 (CXL Scope A + ADR-079 + Phase 3 IPC seam gate)
   ↓ ADR-081 (CppLink-D based)
Wave 2 (P2-P4 实施)
   ↓ ADR-082
Wave 3 (P5-P6 实施 + ADR-083, ADR-084)
   ↓ ADR-082
Wave 4 (P7-P8 实施)
```

---

## §13 阶段化路线图（v0.3 与 Multi-Process Vision Phase R-5 对齐）

### 13.1 Wave 结构（v0.3 与 Vision 时序对齐）

| Wave | 内容 | LOC 估算 | Gate（v0.3）|
|------|------|---------|------|
| **W0 基础 + 治理** | ADR-077/078/080/086/**087** + Multi-Instance Spike + ATS P0 文档 + **CppLink v1 规范草案** | ~0.5-2K | 全部 ADR Accepted + Spike GO |
| **W1 独立价值项** | CXL Scope A（独立 ADR）+ **ADR-079 跨进程一致性契约** | ~0.8-1.5K | ADR-079 Accepted + Scope A ADR Accepted（移除 Phase 1 build gate）|
| **W2 Fabric 核心** | Scale-up P2-P4 + **L1 Switch 进程启动 + CppLink 接入** | ~2.9-4.7K | ADR-080 spike GO + ADR-077/078 Accepted；W2-complete gate：Phase 3 ship + 跨进程 E2E green |
| **W3 Fabric 语义** | Scale-up P5-P6（UVM translation + multicast 走 CppLink 通道）| ~2.5-4K | W2 |
| **W4 Fabric 出口** | Scale-up P7-P8（NIC + E2E + tadr-307/IFabricDriver 对接）| ~2.2-3.3K | W3 + 消费者契约 (ADR-085) + Phase 4 ship（CppTLM Multi-GPU sim，按 ADR-087 D6 归属已定，**条件性 gate 已解除**）|

**依赖标注（v0.5，per §13.4 + ADR-087 D7）**：

- **W0 gate**: 全部 ADR Accepted + Multi-Instance Spike GO；Phase R 与 W0 **并行**（e2e-gate 经验输入 W2 验收）
- **W1 gate**: ADR-079 Accepted + CXL Scope A ADR Accepted（移除 Phase 1 build gate）
- **W2-start gate**: ADR-080 spike GO + ADR-077/078 Accepted + Phase 1 build（D6=CppTLM 已决）
- **W2-complete gate**: Phase 3 (IPC seam) ship + 跨进程 E2E green
- **W4 gate**: Phase 4 ship（条件性：D6=CppTLM 已决）+ 消费者契约 (ADR-085)
- **D6=CppTLM 已决** 意味着 Phase 1/2/4 同步从条件性 gate 升级为硬门
- **W4 启动前 Phase 4 (Multi-GPU) ship**（v0.4 修订：因 ADR-087 D6 已决 = CppTLM 归属，Phase 4 仍是 W4 硬门）

### 13.2 总 LOC 包络（v0.3 维持）

| 场景 | LOC |
|------|----:|
| 核心（W0-W4 不含触发支线 + ADR-087 治理）| **9.0K-14.5K** |
| + ATS L2（P1 触发）| +1.0-1.5K |
| + ATS 全（P1-P4 触发）| +3.1-5.5K |
| + CXL Scope B（共享 trigger）| +5.0-8.0K |
| **绝对上限**（除 CXL C 外）| **17K-28K** |

仍 < CXL Scope C 单独的 30-50K（最清晰的定量 scope gate 论证）。

### 13.3 详细路线图引用

详见 [scale-up-fabric-research.md §6](../05-advanced/scale-up-fabric-research.md)（P1-P8 阶段），本文件不重复。

### 13.4 Vision Phase 时序对齐（v0.3 新增）

| Vision Phase | 内容 | 阻塞的 Wave | 同步性 | Owner（unblock 责任方） |
|---|---|---|---|---|
| Phase R | PTX-EMU audit 修复 + ADR-076 v2 | 无（e2e-gate 经验输入 W2 验收）| **并行** | UsrLinuxEmu + PTX-EMU owner |
| Phase 1 | CppTLM submodule build | 条件性：W2（仅当 ADR-087 D6 = CppTLM 归属）| **并行** | UsrLinuxEmu + CppTLM owner |
| Phase 2 | CppTLM cluster API | 条件性：W2（同上）| **并行/串行（待定 D6）** | CppTLM owner |
| Phase 3 | IPC seam | **W2-complete（硬门）**；W2-start 不依赖（in-process transport 先行）| 串行（对 W2 完成）| UsrLinuxEmu + CppTLM owner |
| Phase 4 | Multi-GPU/NVLink sim | **W4（**✅ D6 已决 = CppTLM 归属**，条件性 gate 解除**）| 串行（仍是 W4 硬门）| 4 仓 |
| Phase 5/6 | Multi-node / ISA swap | 无（轨道外；Phase 5 经 ADR-086 trigger）| 独立 | — |

**Note**：Phase 3 latency 模型（vision L179：MMIO 200cy / Doorbell 50cy / Fence 50cy / Interrupt 100cy）由 ADR-078 import，feeds CppLink-T tick 粒度 + D 通道延迟计费。

---

## §14 Non-Goals（v0.3 扩展）

以下内容**明确不在本轨道范围**（CONF-5 + scale-up §3.6 决策汇总）：

| Non-Goal | 理由 |
|----------|------|
| CXL Scope C（Fabric + IDE + 完整 PMU）| 30-50K LOC，零驱动可移植性价值 |
| CXL PEER 协议完整模拟 | emu 中 switch sim 即 router，PEER 建模加保真度无驱动价值 |
| CXL Fabric Manager（FM）| 同上 |
| CXL Multi-Logical Device（MLD）| emu 中 per-GPU MMU 已等价 |
| CXL IDE（Integrity & Data Encryption）| 安全保真度无消费者 |
| 完整 CXL PMU（96 events）| 性能保真度无消费者 |
| 真实 wire-protocol RDMA（RoCE / iWARP）| UsrLinuxEmu 不模拟 Ethernet PHY |
| Coherence protocol simulation | ADR-073 D1 简化 + 无驱动价值 |
| Shared MMU 设计 | per-GPU MMU + switch fan-out 已足够 |
| Dynamic PA routing（v1）| 静态分区 v1，动态是 v2 升级选项 |
| In-switch reduction / collective engine（SHARP 类）| Rubin 实证其重要性（130 TFLOPS FP8/rack [R §2.9]），但驱动可见界面属库层契约；无消费者（GAP-3）；消费者拉动时经 trigger 评估 |
| NVLink/CXL wire protocol、SerDes、铜/光物理层 | UsrLinuxEmu 不模拟 PHY |
| 动态 Fabric Manager / 路由表热更新 | v1 静态分区足够；动态化无消费者 |
| IMEX 类跨进程 fabric handle 服务 | 单进程仿真；跨进程属 ADR-087 域 |
| 故障注入 / 降级运行（hot-swap 模拟）| 无 RAS 测试消费者；trigger 登记 |
| **L1 Switch 作为 UsrLinuxEmu plugin（v0.3 显式）** | L1 Switch 是**独立 CPU 进程**，不是 plugin（per v0.3 §9.1 + 用户 2026-08-14 决策）|
| **跨进程 coherence protocol 模拟（v0.3 显式）** | emu 跨进程天然不存在协议级 coherence（仿照 ADR-079 §6.1 一致性分级 Level 1）|
| **physical wire protocol for CppLink（v0.3 显式）** | CppLink 共享内存 + Unix socket；不模拟 PCIe 物理层 |

---

## §15 风险登记（v0.3 修订：增加多进程相关风险）

| # | 风险 | 严重度 | 缓解 |
|---|------|------|------|
| 1 | **愿景-目标不匹配**：dual-role switch 无真硬件 archetype——Rubin 实证：NVSwitch 纯 fabric，ConnectX-9 独立 NIC [R §3.8]；NVSwitch/Infinity Fabric 均固件管理，无 Linux 内核驱动 archetype | 🔴 High | ADR-078 锁定 archetype（RDMA-NIC + CXL-port hybrid）+ §4.3 偏离声明记录 net_driver 备选；若 owner 不接受则保留 P1 docs |
| 2 | **多实例 sim 未准备**（CONF-3）| 🔴 High | Wave 0 Multi-Instance Spike GO/NO-GO 前置；**v0.3 扩展到 IPC readiness 检查** |
| 3 | **范围爆炸**（9-14.5K LOC 核心）| 🔴 High | Wave 硬门控 + ADR-085 消费者契约 |
| 4 | **CXL 章节 ⭐⭐ 未验证** | 🟠 Med-High | Wave 1 启动前完成 librarian 重验证 |
| 5 | **4 个命名空间碰撞/混淆**（v0.3 扩展：加 IPC region descriptor 字段）| 🟠 Medium | §3.1 charter 锁定 base 值 + ADR-077 D5 IPC region descriptor |
| 6 | **HAL 治理过载**（5-9 新 op + ADR-087）| 🟠 Medium | 每阶段单 ADR 批量化 + ADR-059-D3-condition-4 evidence |
| 7 | **spec-driven 违反**（无消费者）| 🟠 Medium | ADR-085 消费者门控 + ADR-027 spec-driven |
| 8 | **3 区分原则侵蚀**（**v0.3 修订：L1 Switch 独立进程 = 跨仓边界**）| 🟠 Medium | ADR-078 D2 锁定 **进程分层**（不再是 plugin 分层）+ ADR-087 跨仓 seam |
| 9 | **ATS 重启健忘症**（忽略 ADR-063 D5 排除）| 🟡 Low-Med | ADR-086 trigger registry 记录 |
| 10 | **SSOT/roadmap 脱钩**（零引用）| 🟡 Low-Med | ADR-086 + roadmap.md 同 commit 补 |
| 11 | **无故障/降级场景**：Rubin 实证故障注入与降级运行是 fabric 仿真的高价值场景（hot-swap trays、多 tray 失效续行 [R §10.10]）；W0-W4 未覆盖 | 🟡 Low-Med | §14 Non-Goal 登记 + RAS 消费者出现时经 ADR-086 trigger 评估 |
| 12 🆕 | **Phase R 未 ship 阻塞**：Multi-Process Vision Phase R 修复 audit 3 defects 是 W0 前置；Phase R 卡住会导致整个 scale-up 轨道停滞 | 🟡 Low-Med | Phase R slip 不阻塞 W0/W1；其 e2e-gate 经验影响 W2 验收标准与 ADR-076 v2 的消费时点 |
| 13 🆕 | **CppLink ABI 漂移**：跨仓 CppLink 协议可能在独立迭代中漂移；尤其 L1 Switch 进程与 UsrLinuxEmu / GPU sim 进程异步开发 | 🟠 Medium | CppLink ABI version 字段（per §4.6.4）+ ADR-078 评审窗口 + 真 .so E2E gate（Vision Phase R 经验）|
| 14 🆕 | **Vision Phase R / 1 / 2 / 3 时序错位**：scale-up Wave 与 Vision Phase 串行依赖未对齐会导致 W2 启动失败 | 🟠 Medium | §13.4 显式对齐表 + ADR-086 trigger 加入 "Vision phase ship" 检测 |
| 15 🆕 | **跨进程 raw pointer 泄漏**：v0.2 文档假设单进程内 pointer identity 安全；v0.3 多进程下指针不能作为 canonical identity | 🟠 Medium | ADR-077 D5 显式 region descriptor 协议（no raw pointer）+ CppLink-D descriptor-based |

---

## §16 待澄清问题（v0.4 修订：9 → 10 题，Q10 已决）

下表列出 scale-up doc Appendix B 的 8 个问题 + v0.3 新增的 1 个 multi-process 问题 + v0.4 新增的 1 个 GPU 进程内容问题。**这些答案作为本架构 SSOT 的默认设计**，但应在对应 ADR 评审时正式确认。

| # | 问题 | 推荐答案 | 待 ADR 确认 |
|---|------|---------|------------|
| Q1 | 节点规模 N | **N=8 固定 v1**，参数化（HGX Rubin NVL8 为 2026 出货先例 [R §1.8]；NVL72 单层 / NVL576 两层拓扑差异见 §1.3 注记）| ADR-077 |
| Q2 | 内存池类型 | **私有 fabric memory v1**，CXL Type-3 trigger v2（Rubin 实证：fabric memory = 分布式 + 显式 handle，非 CXL Type-3 默认 [R §3.7, §6.2]，直接验证 v1 选择）| ADR-077 |
| Q3 | NIC RDMA 协议 | **语义 RDMA**（非 RoCE/iWARP）| ADR-082 (H7) |
| Q4 | 多播完成机制 | **同步 fence**，异步 opt-in 后续 | ADR-083 |
| Q5 | UVM 翻译表 | **per-process** 翻译 + node-global NFA（CUDA 同构：per-process VA + fabric handles [R §2.6, §5.7]）| ADR-077 |
| Q6 | PGAS API | **自定义最小 ioctl**（OpenSHMEM 是 TaskRunner 层）| ADR-084 |
| Q7 | 错误处理 | **显式 Linux negative errno** | ADR-078 / ADR-083 |
| Q8 | GPU 直发起多播 | **v1 满足**：store-intercept fan-out = replicated-object 多播写路径（**v0.3：fan-out 在 CppTLM switch_sim 进程内**，per D6），与 CUDA `multimem.st` 同构 [R §2.8]；GPU 发起的 reduction（SHARP 类）为 Non-Goal/trigger | ADR-083 |
| Q9 | **L1 Switch 进程与 UsrLinuxEmu / GPU 进程关系** | **L1 Switch 跨 3 仓实现**（per v0.4 + ADR-087 D6）：③ 硬件 sim 归 CppTLM `soc_arch/switch/`；② 设备 driver 归 UsrLinuxEmu `plugins/switch_driver/`；① 用户态 API 归 TaskRunner `src/switch/`。通过 **CppLink 4 通道**（C/D/E/T，per §4.6）通信 | **ADR-078 + ADR-087 D6（已决）** |
| Q10 ✅ | **switch_sim / NVLink data plane 代码归属** | **✅ 已决（per ADR-087 D6, 2026-08-14）**：采用与 GPU 软件栈同构的 3 仓架构模式——③ sim 归 **CppTLM** `soc_arch/switch/`，② driver 归 **UsrLinuxEmu** `plugins/switch_driver/`，① user 归 **TaskRunner** `src/switch/`。v1 借鉴 GPU 软件栈；后续 ADR 可评估独立软件栈 | **ADR-087 D6（已决）** |

---

## 附录 A 术语表

| 术语 | 含义 |
|------|------|
| **Scale-up** | 节点内/rack 内紧耦合（高带宽低延迟）|
| **Scale-out** | 跨节点/跨 rack 扩展（较低带宽延迟）|
| **PGAS** | Partitioned Global Address Space |
| **UVM** | Unified Virtual Memory |
| **MMU** | Memory Management Unit |
| **Fabric** | 节点内互联网络 |
| **RDMA** | Remote Direct Memory Access |
| **Multicast** | 一对多数据传输 |
| **Meyers singleton** | C++ 函数内 static 局部变量单例模式 |
| **ServiceRegistry** | ① 层跨模块服务查找机制 |
| **附录 B 候选 ADR** | scale-up 决策点正式化 |
| **iB** | 接口 |
| **HAL** | Hardware Abstraction Layer |
| **SSOT** | Single Source of Truth |
| **NFA（Node Fabric Address）** | 节点级 fabric 地址（对应 NVIDIA FA 层）；本文件 v0.2 替代 v0.1 的 "Node Unified PA" 命名 |
| **Replicated-object multicast** | 多播语义类型：每参与设备一份物理副本 + fan-out 写（对应 CUDA multicast + multimem.st）|
| **Counted Write** | Rubin GPU↔GPU 低开销同步原语（payload + counter update，免 barrier 序列）|
| **SHARP** | NVLink Switch in-network reduction engine（130 TFLOPS FP8/rack，v1 Non-Goal）|
| **IMEX** | NVIDIA Inter-Memory Exchange：跨节点/跨 OS 域 fabric memory export/import + handle 生命周期服务（v1 Non-Goal，跨仓消费者契约参照）|
| **NVLink Fusion** | NVIDIA 2024+ 向第三方 silicon 受控开放 NVLink 域的程序（消费者契约 ADR-085 战略参照）|
| **Fabric Manager (FM)** | scale-up fabric 控制平面（Rubin 一等公民）；v1 极简退化到 boot-time 常量 |
| **HGX Rubin NVL8** | NVIDIA 2026 出货的 8-GPU fabric 配置；v1 N=8 规模的行业先例 |
| **HMM** | Linux Heterogeneous Memory Management；device page table mirroring |
| **NVLink-C2C** | CPU-GPU coherent 链路（与 CXL.cache 同构域）|
| **PEER** | NVIDIA Pascal 后引入的 Page Request/Response 机制；ATSLIB 对接选项 |
| **CppLink** 🆕 | **本项目链路协议抽象**（v0.3 命名）；替代 NVLink 命名；4 通道（C/D/E/T）跨 L1 Switch 进程 ↔ GPU 进程；详细见 §4.6 |
| **L1 Switch CPU 进程** 🆕 | **v0.3 架构核心**；独立 OS 进程（不是 UsrLinuxEmu 内 plugin）；通过 CppLink 接入 N 个 GPU sim 进程 |
| **Multi-Process Vision** 🆕 | 4 仓（UsrLinuxEmu + CppTLM + PTX-EMU + TaskRunner）协作 vision；scale-up fabric = Phase 4 子集 |
| **IPC Region Descriptor** 🆕 | CppLink v1.0 共享结构（region_id + generation + nfa_base + size + file_offset + permissions + owner_device + cache_policy + abi_version）；no raw pointer |
| **Multi-Instance Spike** | ADR-080 必做；v0.3 扩展到 IPC readiness 检查 |
| **Phase R / 1 / 2 / 3 / 4** 🆕 | Multi-Process Vision 阶段编号（scale-up Wave 0/1/2 与其串行依赖）|

---

## 附录 B 文档元信息

### B.1 版本控制

| 版本 | 日期 | 主要变更 |
|------|------|---------|
| **v0.1** | 2026-08-14 | 初版（基于 Oracle 综合分析 + 两份研究 doc 提炼）|
| **v0.2** | 2026-08-14 | **Rubin 实证对齐修订**：应用 U-01~U-12（12 项更新）；ADR 编号归位（消除 083/084 双重指派）；NFA 术语重命名（Node Unified PA → Node Fabric Address，对齐 NVIDIA 三地址模型）；§4.3 双角色偏离声明 + net_driver 备选；§4.5 Fabric 控制平面（v1 极简）新增；§5.3 多播语义类型命名（replicated-object）；§5.4 同步原语（counted write trigger）新增；§6.1 一致性语义分级表 + Rubin 行业证据；§11.3 trigger 清单扩展 6 行；§14 Non-Goal 增 5 行；§15 风险增 #11；§16 Q1/Q2/Q5/Q8 强化论证；零代码零 ADR 变更 |
| **v0.3** | 2026-08-14 | **Multi-Process 修订**（用户 2026-08-14 澄清 + Oracle 任务 `bg_49b5caf0` `bg_33efdfc5` 调研触发）：**L1 Switch 从 UsrLinuxEmu 内 plugin 升级为独立 CPU 进程**（§2.1, §9.1）；**链路命名从 NVLink 改为 CppLink**（§4.6）；§4.6 CppLink 协议初版（4 通道 + region descriptor）；§9 plugin 架构重大修订（plugin split → process split）；§10.2b CppLink 契约层；§11 SYN-6 Multi-Process Vision 协同；§12 ADR-078 改名 + 新增 ADR-087 Multi-Process Seam；§13 Wave 与 Vision Phase R-5 串行对齐；§14 Non-Goal 增 4 行（v0.3 显式）；§15 风险增 12/13/14/15；§16 Q 列表 + 新增 Q9；ADR 总数 10 → 11；零代码零 ADR 变更 |
| **v0.4** | 2026-08-14 | **Oracle 评审后修复**（任务 `ses_000f92e51`）：Q1 §4.6 补全（CppLink 4 通道方向修正 + tick master 明确 + descriptor 字段补 magic/kind + boot 屏障 + v1 fail-fast 限制定义）+ Q2 §9 修补（listener 方向统一 + IMP-4 标注 + §9.6 故障矩阵 + 6 处单进程遗留清理 + §6.1 跨进程契约 + net_driver 备选改写）+ Q3 ADR-087 创建顺序前置 + Q4 §13.4 Phase R→并行 + Phase 3 split start/complete + 风险 #12 降级 + Q10 术语表 3 条；C3 机械合并缺陷全清；零代码零 ADR 变更 |
| **v0.5** | 2026-08-14 | **ADR-087 D6 IMP-4 决策后修订**（用户 2026-08-14 决策）：§2.1 架构表增 2 行（UsrLinuxEmu switch_driver + CppTLM switch_sim + TaskRunner switch userspace 三仓实现）；§2.2 拓扑图增加 CppTLM switch_sim 进程 + TaskRunner switch userspace 进程；§9.2 跨进程架构明确 3 仓归属（不再"待 ADR-087 D6 裁决"）；§13.4 Phase 4 条件性 gate **解除**（D6 已决 = CppTLM 归属）；§16 Q10 **已决**；零代码零 ADR 新增变更 |
| **v0.5** | 2026-08-14 | **ADR-087 v0.2 同步**（用户 2026-08-14 澄清）：§13.1 依赖标注块清理（per §13.4 + ADR-087 D7）；§4.5 ADR-011 引用改写（标记 ADR-087 关联，ADR-011 将 Supersede per ADR-087 D8）；零代码零 ADR 变更 |

### B.2 状态转换记录

- v0.1 (Draft)：Oracle 综合分析触发，本文件是 scale-up 轨道架构 SSOT 起点
- v0.2 (Draft)：Rubin 实证对齐修订（Oracle 评审 `ses_00370deb6` 触发）；12 项更新均纯文档，零代码、零 ADR 新增、零 Wave 变更、零 LOC 包络变化
- v0.3 (Draft)：Multi-Process 修订（用户 2026-08-14 澄清触发）+ Oracle 调研（任务 `bg_49b5caf0` + `bg_33efdfc5`）；纯文档变更；零代码、零 ADR 新增、零 Wave 实施延迟（v0.3 Wave 时序与 v0.2 一致但加上 Vision Phase R-5 串行依赖标注）、零 LOC 包络变化

### B.3 关联文档

- [post-refactor-architecture.md](../02_architecture/post-refactor-architecture.md) v0.1.7（项目级 SSOT）
- [ats-cxl-30-implementation-research.md](../05-advanced/ats-cxl-30-implementation-research.md) v0.3
- [scale-up-fabric-research.md](../05-advanced/scale-up-fabric-research.md) v0.2
- **Multi-Process Vision Spec**（2026-08-13, `docs/superpowers/specs/2026-08-13-multi-process-gpu-simulator-vision.md`）：4 仓协作总栈 vision；scale-up fabric = Phase 4 子集；Phase 3 IPC seam 是 W2 前置
- **NVIDIA Rubin 深度调研报告（2026-08-14，Oracle 任务 `ses_0037996c5`）**：v0.2 修订事实基础；引用为 [R §x.y]
- **Multi-Process Simulators 调研（2026-08-14，Oracle 任务 `bg_49b5caf0`）**：v0.3 多进程修订事实基础
- **Vision Doc 架构分析（2026-08-14，Oracle 任务 `bg_33efdfc5`）**：v0.3 CppLink / 进程拆分 / 8 项 IMP 决策来源
- ADR-023 (HAL append-only), ADR-035 (治理), ADR-036 (3 区分), ADR-038 (net stack template), ADR-058 (sim_mem_pool), ADR-061 (HAL IOMMU), ADR-063 (pfh/pm + 单线程), ADR-069 (BAR), ADR-073 (DMA), ADR-076 (跨仓契约流程)

### B.4 维护者

UsrLinuxEmu Architecture Team

### B.5 联系方式

通过 OpenSpec change + ADR 流程更新本文档

---

**文档版本**: v0.5 Draft
**下次更新触发**: §12 任一 ADR Accepted / Wave 0 启动 / Multi-Process Vision Phase 3 ship
**维护者**: UsrLinuxEmu Architecture Team
**最后更新**: 2026-08-14（v0.5: ADR-087 v0.2 同步; §13.1 依赖标注块清理; §4.5 ADR-011 引用改写; 零代码零 ADR 变更）