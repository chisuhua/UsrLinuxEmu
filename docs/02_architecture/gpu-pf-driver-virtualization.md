# GPU PF 驱动虚拟化职责全景

> **状态**: 📋 Draft v0.1（2026-09-08，扩展核心架构 SSOT 的虚拟化维度）
> **角色**: GPU PF（Physical Function）驱动的虚拟化职责完整图谱——从基础 PCI 设备管理到 vGPU 实例创建 + Live Migration 状态迁移的全链路
> **作者**: UsrLinuxEmu Architecture Team
> **对应 commit**: HEAD (2026-09-08)
>
> **关系图**:
> ```
> core-architecture.md（项目级 SSOT，v0.1.7）
>      │
>      ├─ stage-5-5-2-driver-stack-flow.md（Stage 5.5.2 驱动栈 SSOT）
>      │       │
>      │       └─ driver-stack-flow-roadmap.md（P0-P4 修订实施路径）
>      │
>      ├─ four-quadrant-architecture.md（Stage 5.5+ 4 象限目录 SSOT）
>      │
>      └─ gpu-pf-driver-virtualization.md（本文件，GPU PF 虚拟化职责 SSOT）   ← NEW
>              │
>              ├─ ADR-088（CppTLM dGPU 仿真集成）
>              ├─ ADR-089（v5.5+ src/system_hw/ 仿真范围扩展 — 含 Live Migration）
>              ├─ ADR-091（PCI driver 架构 — 4 象限）
>              └─ ADR-092（HAL adapter + bypass binding）
> ```
>
> **关联 ADR**:
> - [ADR-036](../00_adr/adr-036-three-way-separation.md) ✅ 3 区分 → 4 象限架构原则
> - [ADR-088](../00_adr/adr-088-dgpu-complete-simulation.md) ✅ CppTLM 仿真集成
> - [ADR-089](../00_adr/adr-089-v55-system-hw-simulation.md) ✅ v5.5+ 仿真范围扩展（VFIO/IOMMUFD/Live Migration/vDPA）
> - [ADR-091](../00_adr/adr-091-pci-driver-architecture-and-four-quadrant.md) ✅ Accepted v0.2 — Stage 5.5.1 实施后升档
> - [ADR-092](../00_adr/adr-092-hal-adapter-and-bypass-binding.md) 🔄 Proposed — HAL adapter binding
>
> **目标读者**:
> - PF 驱动架构师（设计 GPU PF 虚拟化路径）
> - vGPU/GSP 固件协作开发者
> - Live Migration 实施者
> - 真机 + 仿真双轨协同设计者

---

## 目录

- [§0 文档定位与读者指南](#0-文档定位与读者指南)
- [§1 GPU PF 驱动角色总览](#1-gpu-pf-驱动角色总览)
- [§2 PF 驱动 6 大核心责任](#2-pf-驱动-6-大核心责任)
  - [§2.1 基础 PCI 设备管理](#21-基础-pci-设备管理)
  - [§2.2 SR-IOV Core 管理](#22-sr-iov-core-管理)
  - [§2.3 硬件资源调度与隔离](#23-硬件资源调度与隔离)
  - [§2.4 虚拟化扩展（面向 vGPU）](#24-虚拟化扩展面向-vgpu)
  - [§2.5 状态保存与恢复（Live Migration）](#25-状态保存与恢复live-migration)
  - [§2.6 宿主机原生 I/O + GSP 固件协同](#26-宿主机原生-io--gsp-固件协同)
- [§3 4 阶段发展路径](#3-4-阶段发展路径)
- [§4 驱动接口总览（PF ↔ 内核/用户态契约）](#4-驱动接口总览pf--内核用户态契约)
- [§5 UsrLinuxEmu 项目对应实施位置](#5-usrlinuxemu-项目对应实施位置)
- [§6 与现有 ADR / Roadmap / pcie-bus-bridge-roadmap 的关系](#6-与现有-adr--roadmap--pcie-bus-bridge-roadmap-的关系)
- [§7 实施状态映射](#7-实施状态映射)
- [§8 Non-Goals](#8-non-goals)
- [§9 风险登记](#9-风险登记)
- [附录 A 术语表](#附录-a-术语表)

---

## §0 文档定位与读者指南

### 0.1 角色

GPU **PF（Physical Function）驱动**作为物理 PCIe 设备的**全权管理者**，需实现从硬件初始化到虚拟化资源管理的**完整链路**。本文档系统化梳理 PF 驱动的 6 大核心责任 + 4 阶段发展路径 + 在 UsrLinuxEmu 仿真栈中的对应实施位置。

### 0.2 与现有文档的关系

| 文档 | 范围 | 与本文关系 |
|---|---|---|
| [core-architecture.md](core-architecture.md) | 项目级 SSOT（HAL/IOCTL/CP 等） | **基础层**——提供 HAL 契约、IOCTL 体系 |
| [stage-5-5-2-driver-stack-flow.md](stage-5-5-2-driver-stack-flow.md) | Stage 5.5.2 驱动栈数据流 SSOT | **同层**——覆盖基本驱动职责（PCIe/固件/中断/显存/命令/同步） |
| [four-quadrant-architecture.md](four-quadrant-architecture.md) | Stage 5.5+ 4 象限目录 SSOT | **目录层**——说明 PF driver 物理位置（Q2 plugins/gpu_driver/drv/） |
| [pcie-bus-bridge-roadmap.md](../roadmap/pcie-bus-bridge-roadmap.md) | PCIe 子系统仿真路线图 | **仿真层**——提供 PCIe 8 tier + VFIO 仿真路径 |
| **本文档** | GPU PF 虚拟化职责全景 | **职责层**——完整覆盖从基础 PCI 到 Live Migration 的 PF 责任地图 |

### 0.3 阅读对象

- **PF 驱动架构师**：§1 总览 + §2 6 大责任 + §3 4 阶段路径
- **vGPU 开发者**：§2.4 虚拟化扩展 + §2.5 Live Migration + §4 接口总览
- **GSP 固件协作**：§2.6 GSP 协同 + §3 阶段 2-3
- **仿真栈协同设计者**：§5 UsrLinuxEmu 实施位置 + §6 与 ADR/Roadmap 关系

---

## §1 GPU PF 驱动角色总览

### 1.1 PF vs VF 角色对照

| 角色 | 职责 | 资源 | 数量 | 生命周期 |
|---|---|---|---|---|
| **PF（Physical Function）** | 物理设备的**全权管理者**，含资源分配、VF 创建/销毁、宿主 I/O、SR-IOV 治理 | 全设备资源（BAR、MSI-X、引擎、显存） | 1（每物理设备） | 设备生命周期 |
| **VF（Virtual Function）** | 虚拟化切片，呈现给 VM 的 PCIe 设备 | 资源子集（BAR 切片、MSI-X 子集、显存区域） | N（PF 决定，TotalVFs 上限） | 动态创建/销毁 |

**PF 的 3 个不可替代职责**：
1. **资源仲裁者**（allocator）：所有 VF 共享的物理资源由 PF 切分
2. **配置空间代理**（PF-mediated config space）：部分 VF 配置空间需 PF 间接读写
3. **错误恢复主导**（error recovery）：VF 错误通过 PF 复位链恢复

### 1.2 PF 驱动在 UsrLinuxEmu 4 象限中的位置

```
Q1: src/kernel/                      Q2: plugins/*_driver/
Linux Kernel Env Sim                 Portable Driver Code
┌─────────────────────────────┐    ┌──────────────────────────────┐
│ VFS / ModuleLoader            │    │ ⭐ pci_driver                │
│ ServiceRegistry               │    │ ⭐ iommu_driver              │
│ Linux PCI 子系统 API           │    │ ⭐ vfio_driver                │
│ - pci_register_driver()        │◄───┤ - sriov_configure / enable   │
│ - sriov_configure()             │    │ - mdev_register_driver       │
│ - mdev_parent_ops               │    │ - vfio_device_ops            │
└─────────────────────────────┘    │ ⭐ gpu_driver/drv/ (PF here) │
                                    │   GpgpuDevice::ioctl 派发表   │
                                    └──────────────────────────────┘
                                          │
                                          │ HAL 反向注入
                                          ▼
                                    ┌──────────────────────────────┐
                                    │ Q4: plugins/gpu_driver/sim/   │
                                    │ GPU HW Sim                    │
                                    │ HardwarePullerEmu FSM         │
                                    │ GSP firmware emulation        │
                                    └──────────────────────────────┘
                                          │
                                          │ 调 platform API
                                          ▼
                                    ┌──────────────────────────────┐
                                    │ Q3: sim_hardware/             │
                                    │ PC System Hardware Sim        │
                                    │ PCIe Host Bridge / IOMMU HW  │
                                    └──────────────────────────────┘
```

---

## §2 PF 驱动 6 大核心责任

### §2.1 基础 PCI 设备管理

**责任定义**：所有后续功能的基础——把物理 PCIe 设备正确接入系统。

#### 2.1.1 设备探测与初始化

| 子项 | 接口/操作 | 面向 |
|---|---|---|
| Vendor ID / Device ID 匹配 | `pci_register_driver()` + `.id_table` | Linux PCI 子系统 |
| PCI 配置空间读取 | `pci_read_config_*()` 系列 | BAR/Command/Status/PCIe Cap |
| BAR 空间映射 | `pci_ioremap_bar()` | MMIO 寄存器访问 |
| MSI/MSI-X 中断注册 | `pci_alloc_irq_vectors()` + `devm_request_threaded_irq()` | 内核中断子系统 |

#### 2.1.2 寄存器访问层

```c
/* 寄存器封装典型形态 */
struct gpu_reg_ops {
    u32  (*read32)(void __iomem *base, u32 offset);
    void (*write32)(void __iomem *base, u32 offset, u32 value);
    void (*set_bits)(void __iomem *base, u32 offset, u32 mask);
    void (*clear_bits)(void __iomem *base, u32 offset, u32 mask);
    int  (*poll)(void __iomem *base, u32 offset, u32 mask,
                 u32 expected, u32 timeout_us);
};
```

#### 2.1.3 固件加载与管理

- 加载 GPU 固件（如 NVIDIA GSP、Intel GuC、AMD SMU firmware）
- 通过 DMA 或 MMIO 写入 GPU 指定内存区域
- 与片上微控制器建立通信通道（Mailbox 寄存器或共享内存）
- 完成版本协商和初始化确认
- 各引擎（计算、显存、命令处理器）初始化序列

#### 2.1.4 电源管理

- PCIe PM 状态切换：D0 / D3hot / D3cold
- 设备级 Runtime PM：空闲降功耗、有任务恢复全功率
- ASPM（Active State Power Management）：L0s / L1 / L1.1 / L1.2

#### 2.1.5 错误处理与恢复

- PCIe AER（Advanced Error Reporting）：Uncorrectable / Correctable 错误捕获
- 设备级 + 引擎级复位（Function-Level Reset / Bus Reset）
- 错误恢复流程：错误检测 → 状态保存 → 复位 → 状态恢复

**UsrLinuxEmu 实施位置**：
- `plugins/pci_driver/`（Stage 5.5.1 已建立，含 probe.cpp / pci_access.cpp / pci_msi.cpp / pci_cap.cpp / pci_setup_bus.cpp / pcie_enable_device.cpp / pci_iommu_integration.cpp）
- `plugins/gpu_driver/drv/gpgpu_device.cpp` 的 ioctl 派发表（41 entry per Oracle 第 1 轮审查）

### §2.2 SR-IOV Core 管理

**责任定义**：PF 区别于普通设备驱动的**关键部分**——管理 VF 生命周期和资源分配。

#### 2.2.1 SR-IOV Capability 解析

```
PCIe Extended Capability ID = 0x10（SR-IOV）
┌─────────────────────────────────────────┐
│ SR-IOV Control (offset 0x08)             │
│   ├─ NumVFs (0-7)        : 实际启用的 VF 数 │
│   ├─ VF Enable (bit 7)   : 1=enabled        │
│   ├─ Migration Enable    : Live Migration   │
│   ├─ Interrupt Enable    : 中断使能          │
│   └─ MSE / ARI            : 寻址能力           │
├─────────────────────────────────────────┤
│ SR-IOV Status (offset 0x0A)              │
│   ├─ VF Migration Status                │
│   └─ ...                                 │
├─────────────────────────────────────────┤
│ InitialVFs / TotalVFs / NumVFs           │
│ First VF Offset / VF Stride / VF Device ID│
│ Supported Page Size                    │
│ System Page Size                        │
└─────────────────────────────────────────┘
```

#### 2.2.2 VF 生命周期管理

| 阶段 | 操作 | 关键寄存器/接口 |
|---|---|---|
| 创建 VF | 写 NumVFs + 置 VF Enable | `pci_enable_sriov()` → 写 NumVFs |
| 销毁 VF | 写 NumVFs = 0 | `pci_disable_sriov()` |
| 状态查询 | 读 SR-IOV Status | `pci_sriov_get_totalvfs()` / `pci_sriov_get_numvfs()` |

#### 2.2.3 VF 资源分配策略

每个 VF 需分配独立的：

- **BAR 空间切片**（PF BAR 偏移 + 步长）
- **MSI-X 中断向量**（MSI-X Table 项子集）
- **DMA 队列**（command queue、page table）
- **配置空间**（VF config space，由 PF Mediated）

**关键决策点**（属于 PF 设计阶段）：
- BAR 切片粒度（按 VF 数量等分 vs 按 Profile 配置）
- MSI-X 分配算法（连续 vs 间隔）
- 显存切分（按 VF 配额 vs 按需分配）

#### 2.2.4 VF 配置空间访问代理

部分 VF 配置空间字段（如 Mailbox 通信）需要 PF 间接读写，PF 实现 `vfio_device_ops` + `pci_user` 回调。

**UsrLinuxEmu 实施位置**：
- **❌ 全未实施**：全仓搜索 `sr_iov` 文件为空；仿真侧亦属 Stage 5.5.3 Tier 5 规划（pcie-bus-bridge-roadmap Change-3 `2026-10-01`，未来日期）
- PF driver 侧 **未实施**（属 Stage 5.5.6+ 待规划；注：`Stage 5.5.6+` 为本文档延续编号，pcie-bus-bridge-roadmap 覆盖至 Stage 5.5.5，5.5.6+ 是 pcie-bus 之后的扩展轨道）

### §2.3 硬件资源调度与隔离

**责任定义**：PF 把物理资源切分给各 VF，确保算力/显存/带宽隔离。

#### 2.3.1 计算引擎分区

- 将 GPU 计算核心（NVIDIA SM / AMD CU / Intel EU）按策略分配给不同 VF
- 算力隔离：每个 VF 独占一组 SM/CU，避免跨 VF 干扰
- 超配（over-subscription）：总配额 > 物理核心数时需调度

#### 2.3.2 显存管理

- 帧缓冲（Frame Buffer）切分为多个区域，每个 VF 独占一段
- 硬件地址映射实现隔离（VFIO 1.x type1 IOMMU 或 vIOMMU）
- 显存回收 / 跨 VF 借用策略

#### 2.3.3 命令队列管理

- 每个 VF 独立硬件命令队列（Command Queue / Ring Buffer）
- 数据通路隔离：VF 提交的 ring buffer 独立
- 优先级调度：QoS 优先级

#### 2.3.4 带宽与优先级控制

- PCIe 带宽限制（per-VF Max Payload Size / Max Read Request Size）
- 显存带宽 QoS（NVIDIA GPU MMIO Engine / AMD RLC）
- 调度延迟保证（实时场景）

**UsrLinuxEmu 实施位置**：
- 当前为整体驱动（GpgpuDevice）独占资源
- VF 切片**未实施**——属 Stage 5.5.6+ 待规划（见 §3.1 编号说明：5.5.6+ 为本文档延续编号）

### §2.4 虚拟化扩展（面向 vGPU）

**责任定义**：将 PF 资源暴露为可被 QEMU/libvirt 创建的 vGPU 实例。

#### 2.4.1 mdev 设备注册

```c
/* mdev parent 驱动注册 */
struct mdev_driver gpu_mdev_driver = {
    .device_driver = {
        .name = "gpu_mdev",
        .owner = THIS_MODULE,
    },
    .probe    = gpu_mdev_probe,
    .remove   = gpu_mdev_remove,
    .supported_type_groups = gpu_mdev_type_groups,
};

/* vGPU Profile 定义 */
struct mdev_type gpu_mdev_type_a = {
    .name = "gpu-profile-A",
    .api = "vfio-gpu",
    .description = "8GB VRAM, 50% SM quota",
    .device_api = VFIO_DEVICE_API_PCI_STRING,
};
```

#### 2.4.2 VFIO 设备操作回调

| 回调 | 用途 |
|---|---|
| `open()` | 用户态进程打开 VFIO 设备（QEMU/libvirt） |
| `release()` | 关闭 |
| `read() / write()` | MMIO 读写 |
| `mmap()` | BAR 空间映射（含 DMA region） |
| `ioctl()` | VFIO_IOC_* 命令（设备特定扩展） |

#### 2.4.3 Trap-and-Emulate 支持

对 VF 的敏感寄存器访问进行拦截和模拟：

- **Trap 路径**：VFIO IOMMU fault / fault-on-access
- **Emulate 路径**：PF 驱动接管，模拟目标寄存器行为
- 控制路径请求转发：部分 GSP 命令需 PF 决策后转发

#### 2.4.4 GSP 固件协同

- GPU System Processor（GSP）作为 GPU 内部微处理器
- PF ↔ GSP 通过 Mailbox/RPC 通信
- vGPU 实例创建/调度/销毁由 PF + GSP 协同完成

**UsrLinuxEmu 实施位置**：
- `plugins/vfio_driver/`（**Stage 5.5.5 规划中**，Change-5 `2027-01-15`；将含 vfio.c / vfio_pci.c / vfio_iommufd.c）
- mdev 注册 **未实施**（`plugins/vfio_driver/` 目录未创建；pcie-bus Stage 5.5.5 Change-5 `2027-01-15` 是未来日期；当前无 vGPU profile）
- Trap-and-Emulate **未实施**

### §2.5 状态保存与恢复（Live Migration）

**责任定义**：实现 VF/vGPU 实例的完整状态快照，支持热迁移场景下的状态迁移。

#### 2.5.1 状态快照内容

```
vGPU 实例状态快照
├─ 显存内容（VRAM dump + 增量压缩）
├─ 寄存器状态（GPU 全局寄存器 + per-engine 寄存器）
├─ 命令队列状态（pending commands + ring buffer 内容）
├─ fence / 时间戳（同步原语）
├─ 中断状态（pending IRQs）
├─ GSP 内部状态（固件协作上下文）
├─ 页表（GPUVA → 物理地址映射）
└─ 元数据（profile, qos, permissions）
```

#### 2.5.2 Live Migration 流程

```
源端                                目标端
─────                                ──────
1. Pre-copy
   - 同步冻结 VF（quiesce）
   - 发送初始状态（stop-and-copy）
   - VM 在源端继续运行
2. Iterative pre-copy
   - 跟踪 dirty pages
   - 增量传输
   - 多轮迭代（reduce dirty set）
3. Stop-and-copy
   - 停止源端 VF
   - 发送剩余 dirty state + device state
4. Source resume
   - 目标端恢复 VF
   - 验证完整性
5. Connection migration
   - 网络连接迁移（如 vGPU 网络重定向）
```

#### 2.5.3 增量优化技术

- **脏页跟踪**（Dirty Page Tracking）：硬件支持或 MMU notifier
- **压缩**：zlib / zstd / LZ4
- **XOR 缓存**：相同 page 跨实例去重
- **自适用传输**：网络带宽感知

**UsrLinuxEmu 实施位置**：
- 调研基础：[vfio-live-migration-research.md](../05-advanced/vfio-live-migration-research.md)（18KB）+ [ADR-089](../00_adr/adr-089-v55-system-hw-simulation.md) 阶段 v5.5.4（Live Migration 仿真，4-6 周）
- 实施路径：iommufd 集成（Stage 5.5.5，pcie-bus-bridge-roadmap Change-5 `2027-01-15` 未来日期）
- GPU 专属状态快照 **未实施**

### §2.6 宿主机原生 I/O + GSP 固件协同

#### 2.6.1 宿主机直接访问

即使启用 SR-IOV，PF 仍为宿主机自身提供完整设备访问：

- 宿主机 CUDA 程序直接访问 GPU
- 宿主机 VF 流量代理（未分配 VF 的 VM 或宿主机网络/显示流量）
- PF driver 同时支持**作为 PF（管理角色）**和**作为宿主机消费者**

#### 2.6.2 GSP 固件协同架构

```
PF Driver (Kernel)
    ↕ Mailbox/RPC（硬件寄存器 + 共享内存）
GSP Firmware (GPU 内部微处理器)
    ├─ Resource Manager（资源仲裁）
    ├─ Scheduler（命令调度）
    ├─ Isolation Engine（VF 隔离）
    ├─ Migration Coordinator（Live Migration）
    └─ Power Manager（电源状态）
```

**GSP 协议典型命令**：
- `GSP_CREATE_VF` / `GSP_DESTROY_VF`
- `GSP_ASSIGN_RESOURCE` / `GSP_RELEASE_RESOURCE`
- `GSP_SAVE_STATE` / `GSP_RESTORE_STATE`
- `GSP_POWER_TRANSITION`

**UsrLinuxEmu 实施位置**：
- 宿主机原生 I/O：`GpgpuDevice` 已支持（per `core-architecture.md §1.3`）
- GSP 固件仿真：**无 HAL fw 组（非 stub）**——`gpu_hal.h` fw 组 5 个 op 全部缺失（per [stage-5-5-2-driver-stack-flow.md §-1.3.9](stage-5-5-2-driver-stack-flow.md) fw ❌ 全缺），无明确 stub 代码依据
- GSP 完整仿真：**待规划**（蓝图后扩展轨道，未定义编号；详见 §3.1 编号说明）

---

## §3 4 阶段发展路径

### 3.1 阶段全景图

```
┌────────────────────────────────────────────────────────────────────┐
│ 阶段 1: 基础 PCI 设备管理（PF 入门）                                │
│ probe / BAR / MSI-X / 固件加载 / 电源管理 / AER 错误恢复              │
│ ⬇ Stage 5.5.1 已建 pci_driver（基本）                               │
│ ⬇ Stage 5.5.2+ 补 GPU PF 专属固件加载 + 电源管理 + AER              │
├────────────────────────────────────────────────────────────────────┤
│ 阶段 2: SR-IOV Core 管理（PF 虚拟化基础）                           │
│ Capability 解析 / VF 生命周期 / 资源分配策略 / VF Mediated Config  │
│ ⬇ 全未实施：仿真侧亦属 Stage 5.5.3 Tier 5 规划（Change-3 2026-10-01）│
│ ⬇ 待规划 PF driver 侧实现（Stage 5.5.6+，延续编号）                │
├────────────────────────────────────────────────────────────────────┤
│ 阶段 3: 虚拟化扩展（vGPU 暴露）                                     │
│ mdev 注册 / VFIO 设备操作 / Trap-and-Emulate / GSP 协同             │
│ ⬇ vfio_driver 目录未建；Stage 5.5.5 是 Change-5 2027-01-15 未来日期│
│ ⬇ mdev + Trap-and-Emulate 待规划（Stage 6+，蓝图后未编号轨道）     │
├────────────────────────────────────────────────────────────────────┤
│ 阶段 4: 状态保存/恢复（Live Migration）                              │
│ 状态快照 / 增量传输 / 脏页跟踪 / 自适应压缩 / 连接迁移              │
│ ⬇ 调研基础：ADR-089 阶段 v5.5.4 + vfio-live-migration-research.md  │
│ ⬇ iommufd 集成属 Stage 5.5.5 规划（pcie-bus-bridge Change-5）       │
│ ⬇ GPU 专属状态快照 待规划（Stage 6+，蓝图后未编号轨道）             │
└────────────────────────────────────────────────────────────────────┘
```

> **编号说明**：
> - **Stage 5.5.6+** = 本文档延续编号，pcie-bus-bridge-roadmap 覆盖至 Stage 5.5.5（5 个 Stage），5.5.6+ 是 pcie-bus 之后的扩展轨道
> - **Stage 6+** = 蓝图后扩展轨道（根 roadmap.md 仅到 Stage 5 + 蓝图，无 Stage 6 定义）

### 3.2 各阶段交付物与验收

| 阶段 | 核心交付物 | 验收标准 | 预计工期 |
|---|---|---|---|
| **阶段 1：基础 PCI** | `plugins/pci_driver/` 完整版（含 GPU PF probe/BAR/MSI-X/AER） | 真机一致性 + ctest 全 PASS | 8-12 周 |
| **阶段 2：SR-IOV Core** | GPU PF sriov_configure/sriov_enable + 资源分配策略 | VF 创建/销毁 100% PASS + 资源隔离验证 | 10-16 周 |
| **阶段 3：vGPU 暴露** | `gpu_mdev_driver` + Trap-and-Emulate + GSP 仿真 | mdev type profile 创建 + QEMU 启动 vGPU VM | 12-20 周 |
| **阶段 4：Live Migration** | vGPU state save/restore + 增量传输 + 自适应压缩 | 端到端 VM 跨节点迁移验证 | 16-24 周 |

**总计**：46-72 周（约 11-18 月）

### 3.3 阶段依赖关系

```
阶段 1 (PCI)
    ↓ 基础 + 真机一致
阶段 2 (SR-IOV)
    ↓ Capability + 资源分配
阶段 3 (vGPU)
    ↓ mdev + VFIO 回调
阶段 4 (Live Migration)
```

---

## §4 驱动接口总览（PF ↔ 内核/用户态契约）

| 类别 | 接口/操作 | 调用方 | PF 实现 |
|---|---|---|---|
| **PCI 基础** | `pci_register_driver()` / `probe` / `remove` | Linux PCI 子系统 | ✅ Stage 5.5.1 |
| **PCI 基础** | `pci_enable_device()` / `pci_set_master()` | Linux PCI 子系统 | ✅ Stage 5.5.1 |
| **电源管理** | `suspend` / `resume` / `runtime_suspend` | Linux PM 子系统 | ❌ 框架接口位 / 功能未实施（无电源域；stage-5-5-2 §-1.2 排除） |
| **错误处理** | `pci_error_handlers` → `error_detected` / `mmio_enabled` / `slot_reset` | Linux AER 子系统 | ❌ 框架接口位 / 功能未实施（ADR-055 Deferred-Never） |
| **SR-IOV** | `pci_enable_sriov()` / `pci_disable_sriov()` | Linux SR-IOV 框架 | ❌ Stage 5.5.6+ |
| **SR-IOV** | `numvfs` sysfs / `sriov_totalvfs` / `sriov_numvfs` | 用户态（libvirt） | ❌ Stage 5.5.6+ |
| **mdev** | `mdev_register_driver()` + `supported_type_groups` | Linux mdev 框架 | ❌ Stage 6+ |
| **VFIO** | `vfio_device_ops` → `open` / `release` / `ioctl` / `mmap` | QEMU / 用户态 | ❌ 待 Stage 5.5.5（Change-5 2027-01-15 未启动） |
| **VFIO** | `vfio_pci_open()` / `vfio_pci_ioctl()` / `vfio_pci_mmap()` | QEMU | ❌ 待 Stage 5.5.5 |
| **硬件通信** | MMIO 读写 / Mailbox / GSP RPC | PF Driver | ✅ 已实施 |
| **中断处理** | ISR / tasklet / workqueue / threaded IRQ | Linux IRQ 子系统 | ✅ 已实施 |
| **DMA** | `dma_map_single()` / `dma_unmap_*()` / IOMMU | Linux DMA 子系统 | ✅ 已实施 |

**符号说明**：
- ✅ 已实施
- ⚠️ 框架在位指 Stage 5.5.5 计划中（**非已 ship**，vfio_driver 目录未建）；PF 专属细节待补
- ❌ 未实施（属未来阶段）

---

## §5 UsrLinuxEmu 项目对应实施位置

### 5.1 4 象限定位

| PF 责任 | 4 象限位置 | 当前状态 |
|---|---|---|
| §2.1 基础 PCI 设备管理 | Q2 `plugins/pci_driver/` + Q4 `plugins/gpu_driver/drv/` | ✅ Stage 5.5.1 |
| §2.1.4 电源管理 | Q4 `plugins/gpu_driver/sim/` | ❌ 未实施（无电源域；[stage-5-5-2-driver-stack-flow.md §-1.2](stage-5-5-2-driver-stack-flow.md) 明确"本项目用户态仿真无需 D0/D3 状态切换"） |
| §2.1.5 AER 错误处理 | Q2 `plugins/pci_driver/` + Q1 `src/kernel/` | ⚠️ ADR-055 ⏸️ Deferred-Never |
| §2.2 SR-IOV Core | Q3 `sim_hardware/pcie/sr_iov.cpp`（**未建立**；Stage 5.5.3 Change-3 `2026-10-01` 未来日期） + Q2 `plugins/pci_driver/`（driver） | ❌ 全未实施 |
| §2.3 资源调度隔离 | Q4 `plugins/gpu_driver/sim/` | ❌ 待规划 |
| §2.4 mdev | Q2 `plugins/gpu_driver/drv/` | ❌ Stage 6+ |
| §2.4 VFIO | Q2 `plugins/vfio_driver/`（**未建立**；Stage 5.5.5 Change-5 `2027-01-15` 未来日期） | ❌ 待规划 |
| §2.4 Trap-and-Emulate | Q4 `plugins/gpu_driver/sim/` | ❌ Stage 6+ |
| | §2.5 Live Migration | Q2 `plugins/vfio_driver/`（**未建立**；iommufd 属 Stage 5.5.5 规划） + Q4（GPU state） | ⚠️ ADR-089 阶段 v5.5.4 + vfio-live-migration-research.md 调研完成，实施 ❌ |
| §2.6 宿主机原生 I/O | Q4 `GpgpuDevice` | ✅ 已实施 |
| §2.6 GSP 固件协同 | Q4 `plugins/gpu_driver/sim/` | ❌ 无 HAL fw 组（stage-5-5-2 §-1.3.9 fw ❌ 全缺） |
| §2.6 GSP 完整仿真 | Q4 + CppTLM 扩展 | ❌ Stage 6+（蓝图后未编号轨道） |

### 5.2 已 ship 实施 vs 待规划

**已 ship**（与本文档对应）：
- Stage 5.5.1：Q2 `plugins/pci_driver/` + `iommu_driver/` 建立（基础 PCI 框架）
- Stage 5.5.2：Q3 `sim_hardware/` 顶级目录 + Tier 1+2 PCIe Bypass

**待规划**（与本文档对应）：
- 阶段 1 完整化：GPU PF 专属 probe/AER/电源管理
- 阶段 2：`gpu_pf_sriov_configure` + 资源分配策略
- 阶段 3：`gpu_mdev_driver` + Trap-and-Emulate + GSP 仿真
- 阶段 4：vGPU state save/restore

---

## §6 与现有 ADR / Roadmap / pcie-bus-bridge-roadmap 的关系

### 6.1 ADR 联动

| ADR | 与本文关系 |
|---|---|
| ADR-036 ✅ | 3 区分 → 4 象限架构（本文 §1.2 引用） |
| ADR-088 ✅ | CppTLM 仿真集成（§5.1 Q4 sim 路径） |
| ADR-089 ✅ | v5.5+ 仿真范围扩展——含 VFIO/IOMMUFD/Live Migration/vDPA（§2.4/§2.5 实施路径） |
| ADR-091 ✅ | PCI driver 架构——4 象限（§1.2 + §5.1） |
| ADR-092 🔄 Proposed | HAL adapter binding（PF driver 调用 HAL） |

### 6.2 Roadmap 联动

- [`docs/roadmap/pcie-bus-bridge-roadmap.md`](../roadmap/pcie-bus-bridge-roadmap.md)（435 行）：PCIe 子系统**仿真**路线图（8 tier PCIe sim + VFIO + 真机一致性）
- [`docs/roadmap/driver-stack-flow-roadmap.md`](../roadmap/driver-stack-flow-roadmap.md)（306 行）：Stage 5.5.2 驱动栈**基本**职责修订路径
- [`docs/roadmap/stage-5-multi-engine-pm4.md`](../roadmap/stage-5-multi-engine-pm4.md)（124 行）：Stage 5 触发门控（multi-engine Puller + PM4 microcode）

### 6.3 本文档与 pcie-bus-bridge-roadmap 的分工

| 维度 | pcie-bus-bridge-roadmap.md | 本文档 |
|---|---|---|
| **范围** | PCIe 子系统仿真 + VFIO | GPU PF 驱动开发全链路 |
| **视角** | 自底向上（PCIe PHY → Link → Transaction → VFIO） | 自顶向下（PF 责任 → 分配到象限） |
| **产出** | 8 tier sim + L2 build | 4 阶段交付物 + 4 象限定位 |
| **用户** | 仿真栈开发者 | PF 驱动开发者 + 架构师 |

**互补关系**：pcie-bus-bridge-roadmap 提供**仿真底层**（PF driver 调用的 Q3 sim API），本文档说明**PF driver 上层责任**。

---

## §7 实施状态映射

| 责任 | 子项 | UsrLinuxEmu 状态 | 阻塞项 |
|---|---|---|---|
| §2.1 基础 PCI | probe / BAR | ✅ Stage 5.5.1 | — |
| §2.1 基础 PCI | MSI-X | ✅ Stage 5.5.1 | — |
| §2.1 基础 PCI | 固件加载（GSP） | ❌ 无 HAL fw 组（stage-5-5-2 §-1.3.9 fw ❌ 全缺） | GSP 仿真完整化 |
| §2.1 基础 PCI | 电源管理（D0/D3） | ❌ 未实施（无电源域；stage-5-5-2 §-1.2 明确不实施） | 电源域仿真 |
| §2.2 SR-IOV | Capability 解析 | ❌ 未实施（全仓 sr_iov 文件为空；Stage 5.5.3 规划） | driver 侧实施 |
| §2.2 SR-IOV | VF 生命周期 | ❌ | Stage 5.5.6+ |
| §2.2 SR-IOV | 资源分配策略 | ❌ | Stage 5.5.6+ |
| §2.2 SR-IOV | VF Mediated Config | ❌ | Stage 5.5.6+ |
| §2.3 引擎分区 | SM/CU 分配 | ❌ | Stage 6+ |
| §2.3 显存分区 | VF 显存切片 | ❌ | Stage 6+ |
| §2.3 队列隔离 | per-VF ring buffer | ❌ | Stage 6+ |
| §2.3 带宽 QoS | PCIe bandwidth | ⚠️ Tier 1+2 部分 | Tier 4+7 完整化 |
| §2.4 mdev | profile 注册 | ❌ | Stage 6+ |
| §2.4 VFIO | open/release/ioctl/mmap | ❌ 待 Stage 5.5.5（Change-5 `2027-01-15` 未启动） | GPU 专属回调 |
| §2.4 Trap-and-Emulate | 敏感寄存器拦截 | ❌ | Stage 6+ |
| §2.5 Live Migration | state save/restore | ⚠️ ADR-089 阶段 v5.5.4 + vfio-live-migration-research.md 调研完成 | iommufd 集成后（Stage 5.5.5） |
| §2.6 GSP 完整仿真 | RPC + Mailbox | ❌ | Stage 6+ |

---

## §8 Non-Goals

明确**本文档不覆盖**的内容，避免范围蔓延：

- ❌ **GPU 用户态驱动开发**（CUDA / OpenCL / ROCm）：属 TaskRunner 集成范围
- ❌ **GPU 编译器栈**（LLVM/PTX/NVVM）：不在 UsrLinuxEmu 范围
- ❌ **GPU 性能优化**（per-kernel 优化、调度策略）：属 Stage 5+ 触发门控
- ❌ **GPU 计算引擎微架构仿真**（SM 内部调度、warp scheduler）：属 GSP 仿真完整化（Stage 6+）
- ❌ **多 GPU 拓扑**（NVLink / Infinity Fabric）：不在阶段 1-4 范围，属 [`scale-up-fabric-architecture.md`](scale-up-fabric-architecture.md) Scale-up 轨道（节点内 L1 Switch + 统一 PA）

---

## §9 风险登记

| 风险 | 概率 | 影响 | 缓解 |
|---|---|---|---|
| GSP 完整仿真工作量大 | 高 | 高 | 分阶段：基础 RPC → Mailbox → 完整固件协议 |
| mdev 注册与 VFIO 集成复杂 | 中 | 中 | 待 Stage 5.5.5 VFIO 框架落地后 + 参考真机 amdgpu mdev 实现 |
| Live Migration 状态快照粒度难定 | 中 | 高 | 先实现基本快照，再优化增量跟踪 |
| 真机 SR-IOV 测试环境搭建成本高 | 高 | 中 | L2 build 提供真机一致性 + CppTLM 仿真提供功能验证 |
| 4 阶段实施周期长（46-72 周） | 中 | 中 | 与 pcie-bus-bridge-roadmap Stage 5.5.x 并行 |

---

## 修订记录

- **v0.1** (2026-09-08): 初版
  - §0-§9 全章节覆盖 GPU PF 驱动 6 大核心责任 + 4 阶段路径
  - §5 实施位置映射（基于实际代码/文档状态）
  - §7 状态矩阵（覆盖 4 象限 + 6 责任）
  - 与 pcie-bus-bridge-roadmap 分工明确（仿真底层 vs 驱动上层）

---

**最后更新**: 2026-09-08
**关联 commit**: HEAD
**维护者**: UsrLinuxEmu Architecture Team
**下一版本计划**: v0.2 — 待 GPU PF SR-IOV driver 侧实施启动后，补 §2.2/§2.3 实操细节
