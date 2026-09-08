# Scale-up Fabric 架构调研报告

> **状态**: 🟡 Living Document（持续更新中）— v0.2 (2026-08-14)
> **目的**: 调研 UsrLinuxEmu 视角下"节点内 L1 交换 + 内存池 + NIC 出口 + UVM/PGAS 地址模型"scale-up 架构的可行性、参考实现、关键挑战与实施路径。
> **数据来源**: 本文档为愿景驱动（用户提出架构 → 调研参考实现 → 评估可行性），由本会话综合知识 + 公开规范构成。**关键事实需后续 librarian agent 二次验证**。
> **Owner**: UsrLinuxEmu Architecture Team
> **愿景提出**: 用户（详细架构见 §1）
> **相关 ADR**:
> - [ADR-023](../00_adr/adr-023-hal-interface.md) ✅ HAL 接口契约（append-only）
> - [ADR-035](../00_adr/adr-035-governance-policy.md) ✅ 治理规则
> - [ADR-036](../00_adr/adr-036-three-way-separation.md) ✅ 3 区分原则
> - [ADR-058](../00_adr/adr-058-sim-mem-pool-real-va.md) ✅ sim_mem_pool Real VA（与内存池集成相关）
> **相关文档**:
> - [ats-cxl-30-implementation-research.md](ats-cxl-30-implementation-research.md) — ATS/CXL 调研（共享协议基础）
> - [kfd-nvidia-mempool-va-research.md](kfd-nvidia-mempool-va-research.md) — KFD/Nvidia UVM VA 分配模式（参考实现）
> - [core-architecture.md §1.10.2](../02_architecture/core-architecture.md) (HAL 列表) + [ADR-058](../00_adr/adr-058-sim-mem-pool-real-va.md) (sim_mem_pool) + [ADR-069](../00_adr/adr-069-bar-ioremap-emulation.md) (BAR 定基址) + [ADR-073](../00_adr/adr-073-dma-coherent-emulation.md) (DMA 独立命名空间) — 节点内架构的现有基线
> **目标读者**:
> - 架构师（评估 scale-up 架构选型）
> - 高级开发者（未来实施参考）
> - 探索者（GPU coherent fabric 调研）
> **最后更新**: 2026-08-14（v0.2 Oracle 评审后勘误）

## 目录

- [§0 文档元信息与版本控制](#0-文档元信息与版本控制)
- [§1 愿景架构（用户提案）](#1-愿景架构用户提案)
  - [1.1 总体拓扑](#11-总体拓扑)
  - [1.2 三大组件](#12-三大组件)
  - [1.3 通信语义（统一 load/store）](#13-通信语义统一-loadstore)
  - [1.4 地址空间模型](#14-地址空间模型)
  - [1.5 PGAS 多播语义](#15-pgas-多播语义)
- [§2 现有 scale-up 架构调研](#2-现有-scale-up-架构调研)
  - [2.1 NVIDIA NVLink/NVSwitch + GPUDirect](#21-nvidia-nvlinknvswitch--gpudirect)
  - [2.2 AMD Infinity Fabric + MI300A](#22-amd-infinity-fabric--mi300a)
  - [2.3 Intel Xe Link / Ponte Vecchio](#23-intel-xe-link--ponte-vecchio)
  - [2.4 Google TPU v4/v5 Pod + ICI](#24-google-tpu-v4v5-pod--ici)
  - [2.5 Fujitsu Tofu + A64FX](#25-fujitsu-tofu--a64fx)
  - [2.6 Cerebras WSE](#26-cerebras-wse)
  - [2.7 综合对比矩阵](#27-综合对比矩阵)
- [§3 PGAS 编程模型调研](#3-pgas-编程模型调研)
  - [3.1 OpenSHMEM 标准](#31-openshmem-标准)
  - [3.2 UPC / Co-array Fortran](#32-upc--co-array-fortran)
  - [3.3 NVSHMEM（NVIDIA 闭源）](#33-nvshmemnvidia-闭源)
  - [3.4 ROCm SHMEM](#34-rocm-shmem)
  - [3.5 GASNet（PGAS 底层传输）](#35-gasnetpgas-底层传输)
- [§4 关键技术挑战](#4-关键技术挑战)
  - [4.1 统一物理地址空间](#41-统一物理地址空间)
  - [4.2 MMU 设计（per-GPU vs shared）](#42-mmu-设计per-gpu-vs-shared)
  - [4.3 多播硬件支持](#43-多播硬件支持)
  - [4.4 Cache 一致性边界](#44-cache-一致性边界)
  - [4.5 内存顺序模型](#45-内存顺序模型)
  - [4.6 Fault / Error 处理](#46-fault--error-处理)
  - [4.7 Scale-out vs Scale-up 边界](#47-scale-out-vs-scale-up-边界)
- [§5 UsrLinuxEmu 实施参考](#5-usrlinuxemu-实施参考)
  - [5.1 与现有架构的对齐](#51-与现有架构的对齐)
  - [5.2 3 区分原则的影响](#52-3-区分原则的影响)
  - [5.3 HAL 边界扩展点](#53-hal-边界扩展点)
  - [5.4 插件架构适配](#54-插件架构适配)
  - [5.5 测试策略](#55-测试策略)
- [§6 阶段化路线图](#6-阶段化路线图)
- [§7 未来更新指南](#7-未来更新指南)
- [附录 A 术语表](#附录-a-术语表)
- [附录 B 待澄清问题](#附录-b-待澄清问题)
- [附录 C 局限性](#附录-c-局限性)

---

## §0 文档元信息与版本控制

### 0.1 数据来源矩阵

| 章节 | 主要内容 | 数据来源 | 可信度 |
|------|---------|---------|--------|
| §1 愿景架构 | 用户提案 | 用户输入 | ⭐⭐⭐ 高（用户确认） |
| §2.1 NVIDIA NVLink/NVSwitch | 公开产品文档 + 行业分析 | 综合知识 | ⭐⭐⭐ 高 |
| §2.2 AMD MI300A | AMD 产品发布 + AMD ROCm 文档 | 综合知识 | ⭐⭐⭐ 高 |
| §2.3 Intel Xe Link | Intel 公开信息 | 综合知识 | ⭐⭐ 中（产品已停产）|
| §2.4 Google TPU v4 ICI | Google research 论文 + TPU 公开文档 | 综合知识 | ⭐⭐⭐ 高 |
| §2.5 Fujitsu Tofu | Fujitsu 技术文档 + 论文 | 综合知识 | ⭐⭐ 中 |
| §2.6 Cerebras WSE | Cerebras 公开资料 | 综合知识 | ⭐⭐ 中 |
| §3 PGAS 模型 | OpenSHMEM/UPC/GASNet 标准文档 | 综合知识 | ⭐⭐⭐ 高（成熟标准） |
| §4 关键挑战 | 架构分析 | 综合知识 | ⭐⭐⭐ 高 |
| §5 UsrLinuxEmu 集成 | 项目实地 + ADR | 综合知识 | ⭐⭐⭐ 高 |

### 0.2 版本控制

| 版本 | 日期 | 主要变更 |
|------|------|---------|
| **v0.1** | 2026-08-13 | 初版：用户愿景 + 6 大 scale-up 架构调研 + PGAS 模型调研 + 7 类关键技术挑战 + 6 阶段路线图 |
| **v0.2** | 2026-08-14 | Oracle 评审后勘误：frontmatter 引用 `§1.10 节点内架构基线` → `§1.10.2 + ADR-058/069/073`；§5.1 内存池路径 `src/kernel/iommu/` → `plugins/gpu_driver/sim/`；§6.1 P2 switch sim 路径 `src/kernel/iommu/switch_sim.cpp` → `plugins/switch_driver/sim/switch_sim.cpp`（per ADR-077 D2 plugin split）|

### 0.3 待二次验证项

| 编号 | 待验证内容 | 优先级 |
|------|----------|------|
| V1 | NVIDIA Rubin (NVL576) 实际硬件参数 | 中 |
| V2 | AMD MI300A 互联细节（Infinity Fabric bandwidth）| 高 |
| V3 | UALink 2024 规范细节 | 高 |
| V4 | OpenSHMEM 1.7+ 规范对 GPU-aware 的扩展 | 中 |
| V5 | NVIDIA NVSwitch 多播硬件支持细节 | 中 |

---

## §1 愿景架构（用户提案）

### 1.1 总体拓扑

```
┌─────────────────────────────────────────────────────────────┐
│                        Node (Scale-up Domain)                │
│                                                               │
│  ┌──────────┐     ┌──────────┐     ┌──────────┐              │
│  │  GPU 0   │     │  GPU 1   │     │  GPU N   │              │
│  │  + MMU   │     │  + MMU   │     │  + MMU   │              │
│  └────┬─────┘     └────┬─────┘     └────┬─────┘              │
│       │                │                │                    │
│       └────────┬───────┴───────┬────────┘                    │
│                │               │                             │
│                ▼               ▼                             │
│        ┌───────────────────────────────────┐                │
│        │         L1 Switch (Fabric)        │                │
│        │                                   │                │
│        │   • GPU↔GPU switching             │                │
│        │   • Load/Store 协议处理           │                │
│        │   • 多播支持                      │                │
│        │   • MMU routing                   │                │
│        └───┬─────────────────────┬─────────┘                │
│            │                     │                          │
│            ▼                     ▼                          │
│     ┌──────────────┐    ┌──────────────────┐                │
│     │ Memory Pool  │    │   NIC (RDMA)     │                │
│     │ (CXL Type-3?)│    │   (scale-out)    │                │
│     └──────────────┘    └────────┬─────────┘                │
│                                  │                           │
└──────────────────────────────────┼──────────────────────────┘
                                   │
                          External Ethernet
                          (Scale-out Domain)
```

**关键设计点**：

1. 节点内**所有组件**通过 L1 Switch 互联（统一 fabric）
2. Switch 同时是 **fabric switch + NIC 网卡**（双角色）
3. 内存池、GPU、NIC **共享同一物理地址空间**
4. 所有通信都是 **memory load/store 语义**（无消息传递）

### 1.2 三大组件

#### 1.2.1 GPU 设备（Node 内）

| 属性 | 值 |
|------|-----|
| 数量 | N（典型 8 / 16 / 72）|
| 互联 | 通过 L1 Switch |
| MMU | **每 GPU 一个独立 MMU**（per-GPU translation）|
| 内存 | 各自 HBM（本地）|
| 通信语义 | load/store via fabric |

#### 1.2.2 L1 Switch（核心 fabric）

| 功能 | 详情 |
|------|------|
| GPU↔GPU switching | 全连接或 partial |
| 内存池接入 | 共享内存总线 |
| NIC 功能 | scale-out 到外部 Ethernet |
| 多播支持 | PGAS 多播必备 |
| MMU routing | 协助 GPU MMU 做地址翻译 |
| 协议 | 私有 / CXL-like / 开放 |

**注意**：Switch 在用户愿景中是"双角色"——既是 fabric switch（节点内）又是 NIC（节点外）。这是与 NVSwitch 的关键差异（NVSwitch 仅做节点内）。

#### 1.2.3 内存池（Memory Pool）

| 可能性 | 说明 |
|--------|------|
| CXL Type-3 设备 | 标准 CXL 内存模块 |
| 私有 fabric memory | 定制内存池（如 NVSwitch-attached memory）|
| DDR5 大容量 RDIMM | 通过 fabric 接入 |

**当前 UsrLinuxEmu 已有 sim_mem_pool**（per ADR-058）—— 可作为内存池基础设施。

### 1.3 通信语义（统一 load/store）

**所有通信都通过 memory load/store 语义**，无消息传递：

| 通信类型 | 语义 | 路径 |
|---------|------|------|
| GPU ↔ GPU | 对方 GPU HBM 的 load/store | Switch fabric |
| GPU ↔ Memory Pool | 内存池地址空间 load/store | Switch fabric |
| GPU ↔ NIC（外部） | NIC DMA buffer load/store | Switch fabric → NIC → Ethernet |
| Switch ↔ 任意 | 同上 | Switch 内部 |
| 多播 | **PGAS 多播地址 load/store** | Switch 多播 |

**关键设计选择**：
- 没有 mailbox / descriptor / command queue 等消息机制
- 完全 memory-coherent fabric（hardware coherency by Switch + MMU）
- 这与 NVLink 完全不同（NVLink 既有 load/store 也有 NCCL message-passing）

### 1.4 地址空间模型

#### 1.4.1 物理地址（PA）：统一编址

```
Physical Address Space (Node-wide flat):
┌─────────────────────────────┐
│  [0x0000_0000_0000_0000)    │ ← GPU 0 HBM
│  [GPU_0_BASE, GPU_0_END)    │
├─────────────────────────────┤
│  [GPU_1_BASE, GPU_1_END)    │ ← GPU 1 HBM
├─────────────────────────────┤
│  ...                        │
├─────────────────────────────┤
│  [GPU_N_BASE, GPU_N_END)    │ ← GPU N HBM
├─────────────────────────────┤
│  [MEM_POOL_BASE, MEM_POOL_END) ← Memory Pool
├─────────────────────────────┤
│  [NIC_BASE, NIC_END)        │ ← NIC DMA buffer（如果 MMIO）
└─────────────────────────────┘
```

**核心特性**：所有 GPU / 内存池 / NIC 的物理地址在同一线性空间内，**无需复杂路由表**。

#### 1.4.2 虚拟地址（VA）：UVM

```
UVM Virtual Address Space:
┌─────────────────────────────┐
│  System Address Zone        │ ← 系统地址区
│  [SYS_BASE, SYS_END)        │   （类似传统虚拟地址）
├─────────────────────────────┤
│  PGAS Address Zone          │ ← PGAS 地址区
│  [PGAS_BASE, PGAS_END)      │   （分区全局地址）
│                             │
│  Sub-zones:                 │
│  • 单播区（每个 device 一段）│
│  • 多播区（同步多设备）     │
└─────────────────────────────┘
```

#### 1.4.3 MMU 翻译路径

```
GPU 0 MMU (per-GPU):
  VA (UVM) → PA (unified node-wide)

翻译表项：
  - VA range → [device_id, PA]
  - 系统地址区：单设备映射
  - PGAS 地址区：可能是多设备映射（多播）

Switch 的角色：
  - 协助 GPU MMU 解析 PA 路由（哪个 device）
  - 多播时复制/转发到多个目标
```

### 1.5 PGAS 多播语义

**PGAS (Partitioned Global Address Space)** 是一种并行编程模型：

- 全局地址空间被**分区**给每个 device 一段
- 每个 device 直接 load/store 自己的分区
- 通过**多播地址区**做 one-to-many 同步写入

#### 1.5.1 单播语义

```
GPU 0 写入 PGAS 单播地址 (GPU 2 的分区):
  write(0xPGAS_GPU2_OFFSET, value);
  → Switch 路由 → GPU 2 HBM 写入
  → cache coherence: GPU 2 本地 cache invalidate
```

#### 1.5.2 多播语义

```
GPU 0 写入 PGAS 多播地址 (所有 GPU 的同步区):
  write(0xPGAS_MCAST_OFFSET, value);
  → Switch 检测多播 → 复制到所有 GPU 同步区
  → 所有 GPU 看到同一写入，顺序一致
  → cache coherence: 所有 GPU cache invalidate
```

**实现要求**：
- Switch 必须支持 **多播路由**（硬件级）
- 多播顺序保证（所有目标 device 按相同顺序收到）
- 错误处理（部分设备 fault 时如何处理）

---

## §2 现有 scale-up 架构调研

### 2.1 NVIDIA NVLink/NVSwitch + GPUDirect

#### 2.1.1 拓扑

```
┌────────────────────────────────────────────────┐
│  NVL72 Rack (Hopper/Blackwell)                 │
│                                                │
│  72 GPU + 18 CPU + Switch Tray                 │
│  全连接 via NVSwitch                           │
│                                                │
│  GPU ↔ GPU: NVLink                            │
│  GPU ↔ CPU: NVLink (C2C)                      │
│  GPU ↔ External: GPUDirect RDMA via NIC       │
└────────────────────────────────────────────────┘
```

#### 2.1.2 关键参数（NVL72 / NVLink 5）

| 维度 | 值 |
|------|-----|
| 单 GPU NVLink 带宽 | ~1.8 TB/s 双向 |
| NVSwitch 端口数 | 72 |
| 最大域 | NVL72 / NVL576（Rubin）|
| GPU↔GPU 延迟 | ~100-300 ns |
| 内存一致性 | **软件管理**（CUDA UVA + Managed Memory）|
| 多播硬件 | ✅ NVSwitch 多播 |
| NIC 集成 | **独立**（GPUDirect 通过 PCIe/NVLink 到外部 NIC）|
| 软件栈 | CUDA + NCCL + NVSHMEM |

#### 2.1.3 与用户愿景的关键差异

| 差异点 | NVLink | 用户愿景 |
|--------|--------|---------|
| 内存一致性 | 软件管理 | **硬件 cache coherent** |
| Switch 是否作 NIC | ❌ 否（独立 NIC）| ✅ 是（双角色）|
| 通信语义 | load/store + message-passing | **纯 load/store** |
| 多播 | ✅ 硬件多播 | ✅ PGAS 多播 |

### 2.2 AMD Infinity Fabric + MI300A

#### 2.2.1 拓扑（MI300A）

```
┌────────────────────────────────────────────────┐
│  MI300A APU (Coherent)                         │
│                                                │
│  CPU + GPU 共享内存（in-package coherent）      │
│  6 x CDNA3 GPU + 4 x Zen4 CPU                  │
│  Infinity Fabric（coherent bus）                │
│  共享 128GB HBM3                               │
│                                                │
│  Scale-up: MI300A ↔ MI300A via Infinity Fabric │
│  Scale-out: via PCIe NIC                       │
└────────────────────────────────────────────────┘
```

#### 2.2.2 关键参数

| 维度 | 值 |
|------|-----|
| Package 内部带宽 | ~5.3 TB/s（共享 HBM3）|
| Scale-up 互联带宽 | ~128 GB/s（per-package，估计）|
| 内存一致性 | **硬件 cache coherent**（与用户愿景一致）|
| GPU↔CPU 延迟 | ~ns（package 内）|
| 多播 | ✅ Infinity Fabric 多播支持 |
| NIC | 独立（PCIe NIC）|
| 软件栈 | ROCm + RCCL + ROCm SHMEM |

#### 2.2.3 与用户愿景的契合度

| 契合点 | 说明 |
|--------|------|
| ✅ 硬件 cache coherent | 与用户愿景核心一致 |
| ✅ Package 内统一地址 | 用户愿景是 node 内统一 |
| ⚠️ Scale-up 互联性能 | Infinity Fabric 是 packet-based，bandwidth 比 NVLink 低 |
| ⚠️ NIC 双角色 | 不支持（独立 NIC）|

### 2.3 Intel Xe Link / Ponte Vecchio

#### 2.3.1 拓扑（已停产）

```
Ponte Vecchio (PVC) Data Center GPU:
  - 47 tiles（含 CPU/GPU/HBM/IO）
  - Xe Link 互联 tile-to-tile
  - 8 GPU 通过 Xe Link 全连接
  - 已停产（2023），但架构有参考价值
```

#### 2.3.2 关键参数

| 维度 | 值 |
|------|-----|
| Tile-to-tile 带宽 | 高（具体未公开）|
| 内存一致性 | **硬件 cache coherent**（Intel Mesh）|
| 软件栈 | oneAPI + Level Zero + SYCL |

#### 2.3.3 与用户愿景的契合度

| 契合点 | 说明 |
|--------|------|
| ✅ 硬件 cache coherent | 一致 |
| ✅ Tile 统一地址 | 类似用户愿景 |
| ⚠️ 已停产 | 不再演进，仅参考 |

### 2.4 Google TPU v4/v5 Pod + ICI

#### 2.4.1 拓扑

```
TPU v4 Pod:
  - 4096 chips (TPU v4)
  - 2D torus topology
  - ICI (Inter-Chip Interconnect)
  - Coherent across pod
```

#### 2.4.2 关键参数

| 维度 | 值 |
|------|-----|
| ICI 带宽 | ~656 GB/s per chip（TPU v4）|
| ICI 延迟 | ~1-2 us（estimate）|
| 最大域 | 4096 chips in pod |
| 内存一致性 | **硬件 coherent**（XLA SPMD 分区）|
| 多播 | ✅ torus 上的 broadcast 支持 |
| 软件栈 | JAX/XLA + GSPMD |

#### 2.4.3 与用户愿景的契合度

| 契合点 | 说明 |
|--------|------|
| ✅ 硬件 coherent | 一致 |
| ✅ Pod-level 统一地址 | 类似用户愿景 |
| ✅ 大规模多播 | 一致 |
| ⚠️ 网络拓扑 | 2D torus，vs 用户愿景的 L1 switch |
| ⚠️ Switch 双角色 | TPU 没有传统 NIC 概念（光模块直接接入）|

### 2.5 Fujitsu Tofu + A64FX

#### 2.5.1 拓扑

```
A64FX (Fugaku):
  - 48 cores + 4 assistant cores
  - HBM2 32GB
  - TofuD 互联（6D mesh/torus）
  - 16,000 nodes scale-up via TofuD
```

#### 2.5.2 关键参数

| 维度 | 值 |
|------|-----|
| TofuD 带宽 | ~6.8 GB/s per link × 4 links |
| TofuD 延迟 | ~0.5-1 us |
| 最大域 | 16,384 nodes |
| 内存一致性 | **软件 PGAS**（PGAS language model）|
| 多播 | ✅ TofuD collective ops |
| 软件栈 | Fortran + MPI + PGAS languages |

#### 2.5.3 与用户愿景的契合度

| 契合点 | 说明 |
|--------|------|
| ✅ PGAS 原生 | 完全契合用户愿景 |
| ✅ 大规模多播 | TofuD collective 成熟 |
| ⚠️ 软件一致性 | vs 用户愿景的硬件一致性 |
| ⚠️ GPU 不直接参与 | A64FX 是 CPU |

### 2.6 Cerebras WSE

#### 2.6.1 拓扑

```
WSE-2 (Cerebras):
  - 单 wafer 850,000 cores
  - Mesh interconnect
  - 单一物理地址空间（wafer-scale）
  - 所有 core 在同一 fabric
```

#### 2.6.2 关键参数

| 维度 | 值 |
|------|-----|
| Fabric 带宽 | ~220 Pb/s aggregate |
| Fabric 延迟 | ~ns 级 |
| 内存一致性 | **硬件 coherent** |
| 软件栈 | Cerebras SDK（自定义）|

#### 2.6.3 与用户愿景的契合度

| 契合点 | 说明 |
|--------|------|
| ✅ 单一物理地址空间 | 完全契合 |
| ✅ 硬件 coherent | 一致 |
| ✅ 多播 | hardware broadcast |
| ⚠️ Scale 是 wafer-scale | vs 用户愿景的 node-scale |
| ⚠️ 没有 GPU | compute-only cores |

### 2.7 综合对比矩阵

| 特性 | 用户愿景 | NVLink | MI300A | Intel Xe Link | TPU v4 | TofuD | Cerebras WSE |
|------|---------|--------|--------|---------------|--------|-------|--------------|
| 节点内 GPU 数 | N | 8-72 | 8（in package）| 8 | 4 per host | N/A (CPU) | wafer |
| L1 Switch | ✅ 核心 | ✅ NVSwitch | ✅ Infinity Fabric | ✅ Xe Link | ✅ ICI | ✅ TofuD | ✅ mesh |
| Switch 作 NIC | ✅ 双角色 | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ |
| 内存一致性 | **硬件** | 软件 | **硬件** | **硬件** | **硬件** | 软件 PGAS | **硬件** |
| 多播硬件 | ✅ PGAS | ✅ | ✅ | ✅ | ✅ torus | ✅ collective | ✅ broadcast |
| 物理地址统一 | ✅ | ⚠️ 部分 | ✅ | ✅ | ✅ | ⚠️ PGAS | ✅ |
| UVM / UVA | ✅ UVM | UVA | ROCm UVM | oneAPI UVM | SPMD | PGAS lang | SDK |
| 成熟度 | 愿景 | ⭐⭐⭐ | ⭐⭐ | ⭐ (停产) | ⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐ |

---

## §3 PGAS 编程模型调研

### 3.1 OpenSHMEM 标准

#### 3.1.1 概述

**OpenSHMEM** 是 PGAS 编程的开放标准，由 OpenSHMEM.org 维护。最新版本 OpenSHMEM 1.7（2024）。

#### 3.1.2 核心 API

```c
// 单播：对称地址空间
int my_pe = shmem_my_pe();       // 当前 PE（处理单元）ID
int npes = shmem_n_pes();         // PE 总数

// 单播 put/get
void shmem_put32(int *dest, int *src, size_t nelems, int pe);
void shmem_get32(int *dest, int *src, size_t nelems, int pe);

// 远程原子
void shmem_atomic_add(int *dest, int value, int pe);

// 同步
void shmem_barrier_all();          // 全 PE 同步
void shmem_quiet();                // 远程写入完成
```

#### 3.1.3 GPU-aware OpenSHMEM

- **OpenSHMEM 1.7+** 开始引入 GPU-aware 扩展
- 允许 symmetric heap 分配在 GPU 内存
- API：例如 `shmemx_get32` 支持 device pointer

#### 3.1.4 适用评估

| 维度 | OpenSHMEM | 用户愿景 PGAS |
|------|-----------|--------------|
| 单播 | ✅ 标准 put/get | ✅ 兼容 |
| 多播 | ✅ `shmem_broadcast` / collectives | ✅ 契合 |
| GPU 支持 | ⚠️ 早期（1.7+）| ✅ 原生设计 |
| 硬件加速 | 软件层（依赖底层网络）| Switch 硬件支持 |
| 成熟度 | ⭐⭐⭐ 成熟 | N/A |

### 3.2 UPC / Co-array Fortran

#### 3.2.1 UPC（Unified Parallel C）

```c
// UPC 共享数组
shared int x[N_THREADS];     // 每个 thread 一段
x[MYTHREAD] = value;          // 本地访问
x[(MYTHREAD + 1) % N] = ...; // 远程访问（PGAS 语义）
upc_barrier;                  // 同步
```

#### 3.2.2 Co-array Fortran

```fortran
! Co-array Fortran
real :: x[*]    ! 每个 image 一段
x[THIS_IMAGE()] = ...        ! 本地
x[OTHER_IMAGE()] = ...       ! 远程
sync images                   ! 同步
```

#### 3.2.3 适用评估

| 维度 | UPC/CAF | 用户愿景 |
|------|---------|---------|
| 语言级 PGAS | ✅ 原生 | 用户愿景不限语言 |
| 多播 | ⚠️ 通过循环 + barrier | ✅ 硬件多播 |
| GPU 支持 | ❌ 不原生 | ✅ 必需 |
| 适用 | 学术 / HPC | 用户愿景更广义 |

### 3.3 NVSHMEM（NVIDIA 闭源）

#### 3.3.1 概述

**NVSHMEM** 是 NVIDIA 在 NVLink 之上的 PGAS 实现，基于 OpenSHMEM 扩展。

#### 3.3.2 关键 API

```cpp
#include <nvshmem.h>

// 初始化
nvshmem_init();

// Symmetric memory allocation
void *ptr = nvshmem_malloc(size);

// 单播
nvshmem_put32(dest, src, nelems, pe);
nvshmem_get32(dest, src, nelems, pe);

// 多播（collective）
nvshmem_broadcast32(dest, src, nelems, pe_root);
nvshmem_team_alltoall(team, ...);

// 同步
nvshmem_barrier_all();
```

#### 3.3.3 适用评估

| 维度 | NVSHMEM | 用户愿景 |
|------|---------|---------|
| 性能 | ⭐⭐⭐（NVLink 优化）| 取决于 Switch 性能 |
| 多播 | ✅ 通过 collective | ✅ 契合 |
| 硬件 | NVLink 私有 | 用户愿景开放 |
| 厂商绑定 | NVIDIA | 用户愿景开放 |
| 可移植性 | ❌ NVIDIA only | ✅ 开放 |

### 3.4 ROCm SHMEM

#### 3.4.1 概述

AMD ROCm 的 SHMEM 实现，与 NVSHMEM 对偶但基于 Infinity Fabric / ROCm UVM。

#### 3.4.2 状态

- ROCm 5.7+ 提供 rocSHMEM
- 基于 OpenSHMEM 标准 + ROCm UVM 集成
- 性能接近 NVSHMEM（Infinity Fabric 带宽低一些）

### 3.5 GASNet（PGAS 底层传输）

#### 3.5.1 概述

**GASNet** 是 PGAS 系统的底层通信库，被 UPC/Co-array Fortran/OpenSHMEM 实现使用。

#### 3.5.2 角色

```
┌─────────────────────────────────┐
│  UPC / OpenSHMEM (高层 API)     │
├─────────────────────────────────┤
│  GASNet (中层 transport)         │
├─────────────────────────────────┤
│  Hardware fabric (TofuD/IB/...) │
└─────────────────────────────────┘
```

#### 3.5.3 适用评估

- GASNet 是 **transport layer**，不直接对应用户愿景
- 但用户愿景的 Switch 可以提供 **GASNet conduit** 接口
- 这是 OpenSHMEM/UPC 在用户愿景上的**实现路径**

---

## §4 关键技术挑战

### 4.1 统一物理地址空间

#### 4.1.1 挑战

| 子挑战 | 说明 |
|--------|------|
| 地址空间大小 | N GPU × HBM size + 内存池 + NIC → 数十 TB 级 PA |
| Hole 管理 | 不同设备大小不一致，需 hole avoidance |
| Switch 路由表 | Switch 必须知道 PA → device 映射 |
| Cache line 大小 | 不同设备可能不一致（HBM 64B / DRAM 64B / NIC 256B）|

#### 4.1.2 解决方案方向

1. **静态分区**（最简）：预先分配 `[GPU_i_BASE, GPU_i_END)` 等固定区间
2. **动态分配**（复杂）：Switch 维护路由表，支持 hot-plug
3. **层次化 PA**（混合）：节点内统一 PA + 节点间扩展 PA

### 4.2 MMU 设计（per-GPU vs shared）

#### 4.2.1 三种可能架构

| 架构 | 描述 | 优劣 |
|------|------|------|
| **A. Per-GPU MMU（用户愿景）** | 每个 GPU 独立 MMU 翻译 | ✅ 隔离好 / ❌ 多播时 TLB 同步问题 |
| **B. Shared MMU** | Switch 维护全局 MMU | ✅ 多播简单 / ❌ Switch 复杂 |
| **C. Hybrid** | Per-GPU MMU + Switch 协助 | ✅ 平衡 / ⚠️ 实现复杂 |

#### 4.2.2 per-GPU MMU 多播时的同步问题

```
场景：GPU 0 多播写 0xPGAS_MCAST → 所有 GPU 同步区

如果只有 per-GPU MMU：
  - GPU 0 TLB: 0xPGAS_MCAST → [多设备, 需要复制]
  - GPU 1 TLB: 0xPGAS_MCAST → [自己的同步区]
  - Switch 检测到多播 → 复制写入 → GPU 1-7 收到
  - GPU 1-7 各自更新本地 cache/TLB

问题：
  - 复制期间的 cache 一致性？
  - 部分设备 fault 时如何处理？
  - 多播完成的确认机制？
```

### 4.3 多播硬件支持

#### 4.3.1 Switch 多播能力需求

| 能力 | 必要性 |
|------|--------|
| 多播路由表 | 必须 |
| 多播顺序保证（按目标 device 顺序）| 必须 |
| 多播完成通知 | 必须 |
| 部分故障处理（部分 device 收到部分失败）| 必须 |
| 多播优先级 | 可选 |

#### 4.3.2 与现有方案对比

| 方案 | 多播实现 |
|------|---------|
| NVSwitch | 硬件 collective engine（SHARP-like）|
| Infinity Fabric | hardware multicast |
| TPU ICI | torus broadcast |
| TofuD | collective operations library |
| 用户愿景 | Switch 内多播引擎（待设计）|

### 4.4 Cache 一致性边界

#### 4.4.1 一致性范围

```
强一致性：所有 GPU 看到同一写入 → Switch + MMU 协同
弱一致性：写入立即对发起者可见，远程延迟可见 → 性能更好
最终一致性：仅保证最终一致 → 不适合 GPU compute
```

#### 4.4.2 用户愿景推荐模型

- **强一致性**（per AMD MI300A 模型）：Switch + per-GPU MMU 通过 directory-based protocol 实现
- **目录位置**：Switch 内或分布式
- **一致性粒度**：cache line（64B 或 128B）

### 4.5 内存顺序模型

#### 4.5.1 内存模型选项

| 模型 | 说明 | 适用 |
|------|------|------|
| **Sequential Consistency（SC）** | 最强，所有访问全局序 | 编程简单，性能差 |
| **Total Store Order（TSO, x86）** | 写序保证，读可乱序 | GPU compute 通常足够 |
| **Release/Acquire（ARM/RISC-V）** | 显式 fence | 灵活，性能好 |
| **GPU relaxed** | 显式 fence | NVIDIA 默认 |

#### 4.5.2 用户愿景建议

- **TSO-like**（写序保证 + fence 控制读序）：平衡编程模型与性能
- 配合 `__threadfence_system()`（CUDA 风格）做显式同步

### 4.6 Fault / Error 处理

#### 4.6.1 Fault 类型

| 类型 | 例子 | 处理 |
|------|------|------|
| 多播部分失败 | GPU 3 在多播时 crash | 其他 GPU 收到 partial commit |
| Switch 内部错误 | 多播路由失败 | 整批多播失败 + 上报 |
| 内存池不可达 | CXL Type-3 设备断开 | 切换到 fallback |
| NIC 错误 | scale-out 包丢失 | 重传 + 上报 |

#### 4.6.2 恢复策略

- **多播**：`shmem_quiet()` 返回错误码 → 应用层决定 retry
- **单播**：目标 GPU fault → 异常传播到源 GPU（类似 page fault）
- **内存池**：fallback 到 GPU 本地 HBM

### 4.7 Scale-out vs Scale-up 边界

#### 4.7.1 边界设计

| 边界位置 | 影响 |
|---------|------|
| **Switch NIC 内部**（用户愿景）| ✅ 节点内 load/store 可直接读 NIC buffer |
| **PCIe 独立 NIC**（传统）| ❌ GPU 需通过 GPUDirect RDMA 走 PCIe |
| **网络协议转换**（需 RDMA verbs）| ⚠️ 增加 CPU 介入开销 |

#### 4.7.2 用户愿景的双角色 Switch

```
GPU load/store → Switch → NIC DMA buffer → Ethernet RDMA → 远端 Switch → 远端 GPU

关键点：
  - 节点内 load/store 到 NIC buffer 是同 fabric
  - NIC 发起 Ethernet RDMA 是 scale-out
  - 远端 NIC 写入 → 远端 Switch → 远端 GPU 是反向路径
```

---

## §5 UsrLinuxEmu 实施参考

### 5.1 与现有架构的对齐

| 现有组件 | 适配 |
|---------|------|
| `plugins/gpu_driver/sim/` | sim_mem_pool 扩展为节点级 fabric memory pool（per ADR-058）；`plugins/switch_driver/sim/`（P2 新建）做 fabric switch 模拟 |
| `plugins/gpu_driver/` | GPU driver 适配 |
| `plugins/gpu_driver/hal/` | Switch ops 走 HAL |
| `plugins/gpu_driver/sim/` | Switch sim 实现 |
| `include/linux_compat/pci/` | Switch 通过 PCIe capability 暴露 |
| `src/kernel/pcie/` | Switch 设备模型 |

### 5.2 3 区分原则的影响（ADR-036）

| 层 | 角色 | 实施位置 |
|-----|------|---------|
| **① 内核环境模拟** | Linux UVM/MMU/SHMEM API | `src/kernel/` + `include/linux_compat/` |
| **② 可移植驱动** | GPU driver 调 UVM/SHMEM API | `plugins/gpu_driver/drv/` |
| **③ 硬件模拟** | Switch sim + 内存池 sim + NIC sim | `plugins/gpu_driver/sim/` |
| **HAL** | Switch ops、内存池 ops、multicast ops | `plugins/gpu_driver/hal/`（append-only）|

### 5.3 HAL 边界扩展点

#### 5.3.1 候选新增 HAL ops（每条需独立 ADR）

| op | 用途 |
|-----|------|
| `hal_switch_route(va, pa, device_mask)` | Switch 路由表配置 |
| `hal_mcast_write(va, value, target_devices)` | 多播写入 |
| `hal_uvm_map(va, pa, size, perms)` | UVM 映射 |
| `hal_uvm_unmap(va, size)` | UVM 解除映射 |
| `hal_nic_rdma_send(nic_buf, remote_node, remote_buf, size)` | NIC RDMA 发送 |
| `hal_mem_pool_alloc(pool_id, size, alignment)` | 内存池分配 |

#### 5.3.2 与现有 HAL ops 的关系

| 已有 ops | 关系 |
|---------|------|
| `iommu_map/unmap`（ADR-061）| **基础**：UVM 映射基于此 |
| `iommu_iova_to_phys`（ADR-023）| UVM 翻译基础 |
| `fence_create/read`（ADR-023）| 多播 fence 可基于此 |

### 5.4 插件架构适配

#### 5.4.1 插件选项

| 选项 | 描述 |
|------|------|
| **A. 集成到 `gpu_driver`** | Switch sim 作为 gpu_driver 子模块 |
| **B. 独立 `switch_driver` 插件** | 新插件 `plugins/switch_driver/` |
| **C. 独立 `fabric_driver` 插件** | 更通用的 fabric 抽象 |

**推荐**：选项 B（独立 `switch_driver` 插件），遵循 net_driver/storage_driver 的现有模式。

#### 5.4.2 插件接口

```cpp
// plugins/switch_driver/plugin.cpp
extern "C" {
    struct module mod = {
        .name = "switch_driver",
        .register_devices = switch_register_devices,  // 创建 /dev/switch0
    };
}

// /dev/switch0 的 IOCTL 契约
#define SWITCH_IOCTL_CONNECT_GPU      0x01  // GPU ↔ Switch
#define SWITCH_IOCTL_CONNECT_MEMPOOL  0x02  // Memory Pool ↔ Switch
#define SWITCH_IOCTL_CONFIGURE_NIC    0x03  // Switch NIC 配置
#define SWITCH_IOCTL_MCAST_WRITE      0x04  // 多播写入（PGAS）
```

### 5.5 测试策略

#### 5.5.1 测试层次

| 层次 | 测试类型 |
|------|---------|
| 单元 | UVM 翻译、多播路由、MMU TLB |
| 集成 | GPU↔GPU load/store、内存池接入、NIC RDMA |
| 端到端 | PGAS 多播程序、scale-out RDMA 程序 |
| 性能 | fabric 延迟、带宽、多播吞吐 |

#### 5.5.2 参考测试模式

- 借鉴 `tests/test_iommu_emu_standalone` 模式
- 多 PGAS 测试：`test_pgas_unicast`、`test_pgas_mcast`、`test_uvm_multinode`

---

## §6 阶段化路线图

### 6.1 总体阶段

| 阶段 | 目标 | 关键交付 | LOC 估算 |
|------|------|---------|---------|
| **P1: 概念模型** | 文档 + 架构决策 | 本文档 + ADR-077 (scale-up fabric) | 0（仅文档）|
| **P2: Switch 基础** | Switch sim skeleton + 单一 GPU | `plugins/switch_driver/sim/switch_sim.cpp` + 基础 IOCTL（**per ADR-077 D2 plugin split**；Switch 是 ③ 硬件，不在 ① 内）| 1500-2500 |
| **P3: 多 GPU 互联** | 多 GPU load/store | HAL ops（`hal_switch_route`）| 800-1200 |
| **P4: 内存池集成** | 内存池 device + Switch 连接 | 复用 ADR-058 sim_mem_pool | 600-1000 |
| **P5: UVM 翻译层** | per-GPU MMU + UVM 翻译 | `uvm_mmu.cpp` + HAL ops | 1000-1500 |
| **P6: PGAS 多播** | 多播硬件 + API | HAL ops（`hal_mcast_write`）+ OpenSHMEM 子集 | 1500-2500 |
| **P7: NIC 双角色** | Switch NIC + scale-out RDMA | `nic_sim.cpp` + HAL ops | 1200-1800 |
| **P8: 端到端验证** | 完整 PGAS 程序运行 | `test_pgas_*.cpp` × 5+ | 1000-1500 |
| **总计** | | | **~8000-13000** |

### 6.2 与现有阶段对齐

| 现有阶段 | 用户愿景阶段 |
|---------|--------------|
| Stage 1.4 Tier-2（mmu_notifier 完整化）| P5 前置依赖 |
| Stage 4（BAR/ioremap + CP）| 独立轨道 |
| Stage 5（multi-engine + PM4）| 独立轨道 |
| C-12 (KFD multi-file) | 部分依赖（P3）|
| **新轨道：scale-up fabric** | **P1-P8**（trigger-gated）|

### 6.3 触发条件

| 触发 | 启动阶段 |
|------|---------|
| 项目目标增加 "GPU coherent fabric" | P1 |
| AMD MI300A 类似场景出现 | P1-P3 |
| NVIDIA Rubin 替代需求 | P1-P7（full）|
| 用户/合作方明确需求 | P1（最小）|

---

## §7 未来更新指南

### 7.1 何时更新本文档

| 触发条件 | 更新动作 |
|---------|---------|
| 用户架构愿景调整 | 更新 §1 + 同步 §6 |
| 新发现 scale-up 架构（如 UALink）| 追加 §2.8+ |
| OpenSHMEM GPU-aware 新版本 | 更新 §3.1 |
| ADR 新增（scale-up 相关）| 追加相关 ADR |
| UsrLinuxEmu sim_mem_pool 变更 | 同步 §5.1 |
| 关键事实验证完成（V1-V5）| 移除附录 C 标记 |

### 7.2 更新流程

1. 修改本文档对应章节
2. 更新文档顶部"最后更新"日期
3. 更新 §0.2 版本控制表（新增版本行）
4. 如果新增重大发现，追加 §0.1 数据来源矩阵

---

## 附录 A 术语表

| 术语 | 含义 |
|------|------|
| **Scale-up** | 节点内 / rack 内紧耦合（高带宽低延迟）|
| **Scale-out** | 跨节点 / 跨 rack 扩展（较低带宽延迟）|
| **PGAS** | Partitioned Global Address Space — 全局地址空间被分区给每个 device |
| **UVM** | Unified Virtual Memory — 统一虚拟内存（CUDA/ROCm 概念）|
| **MMU** | Memory Management Unit — 内存管理单元（VA→PA 翻译）|
| **ICI** | Inter-Chip Interconnect（TPU 内部互联）|
| **TofuD** | Fujitsu 6D torus 互联 |
| **NVSwitch** | NVIDIA fabric switch |
| **Infinity Fabric** | AMD coherent fabric |
| **Conduit** | GASNet 术语：底层 transport 抽象 |
| **Symmetric Heap** | OpenSHMEM 概念：所有 PE 都能寻址的地址空间 |

---

## 附录 B 待澄清问题

| 编号 | 问题 | 影响 |
|------|------|------|
| Q1 | 目标节点规模？N = 8 / 16 / 72 / 576？| 决定 Switch 端口数 |
| Q2 | 内存池是 CXL Type-3 还是私有 fabric？| 决定一致性协议 |
| Q3 | NIC 双角色的 RDMA 是 RoCE 还是 iWARP 还是自定义？| 决定 scale-out 协议 |
| Q4 | 多播完成通知机制：synchronous fence 还是 async callback？| 决定编程模型 |
| Q5 | UVM 翻译表是每进程独立还是全局？| 决定 MMU 设计 |
| Q6 | PGAS API 用 OpenSHMEM 子集还是自定义？| 决定软件兼容性 |
| Q7 | 错误处理策略：silent retry 还是显式 error？| 决定 fault 模型 |
| Q8 | 是否需要 GPU 直接发起多播（不经过 Switch CPU）？| 决定硬件设计 |

---

## 附录 C 局限性

| 局限 | 影响 | 缓解 |
|------|------|------|
| 愿景架构是用户提出，可能与项目目标不完全匹配 | 实施优先级待定 | §6.3 触发条件 |
| §2 现有架构调研基于综合知识，部分细节需验证 | 关键数字可能有偏差 | V1-V5 二次验证 |
| §3 PGAS 模型对 GPU 扩展较新 | 成熟度不足 | 关注 OpenSHMEM 1.7+ 演进 |
| §4 关键挑战部分（多播硬件、MMU 同步）需深入设计 | 实施复杂 | §6 P5-P6 阶段 |
| §5 UsrLinuxEmu 集成未做 ADR | 治理缺失 | 需要 ADR-077 (scale-up fabric) |
| §6 路线图 LOC 估算基于经验 | 可能偏差 ±50% | 实施时精细化 |

---

**文档版本**: v0.2 (Living Document)  
**下次更新触发**: §7.1 任一条件  
**维护者**: UsrLinuxEmu Architecture Team  
**最后更新**: 2026-08-14（v0.2 Oracle 评审后勘误）