# UsrLinuxEmu 4 象限目录布局（Four-Quadrant Directory Layout）

> **SSOT** | 最后验证: 2026-09-07（ADR-091 v0.2 ✅ Accepted + ADR-092 v0.1 🔄 Proposed 同期）| 对应 ADR: [ADR-091](../00_adr/adr-091-pci-driver-architecture-and-four-quadrant.md) ✅ Accepted v0.2 + [ADR-092](../00_adr/adr-092-hal-adapter-and-bypass-binding.md) 🔄 Proposed
>
> **作者**: UsrLinuxEmu Architecture Team
> **作用**: 把 UsrLinuxEmu 顶层目录按"Linux kernel sim vs portable driver vs PC system sim vs GPU-specific sim"**4 象限**划分，对应真机 Linux 内核目录结构
>
> **关系**: 本文是 [core-architecture.md](core-architecture.md)（v0.1.7）之后，针对 Stage 5.5+ 系统硬件仿真的**目录布局升级**

---

## 目录

- [§0 文档定位](#0-文档定位)
- [§1 4 象限总览](#1-4-象限总览)
  - [1.1 一张图](#11-一张图)
  - [1.2 与真机 Linux 内核目录对应](#12-与真机-linux-内核目录对应)
- [§2 Q1: src/kernel/ — Linux Kernel Env Sim](#2-q1-srckernel--linux-kernel-env-sim)
- [§3 Q2: plugins/*_driver/ — Portable Driver Code](#3-q2-plugins_driver--portable-driver-code)
- [§4 Q3: sim_hardware/ — PC System Hardware Sim](#4-q3-sim_hardware--pc-system-hardware-sim)
- [§5 Q4: plugins/gpu_driver/sim/ — GPU HW Sim](#5-q4-pluginsgpu_driversim--gpu-hw-sim)
- [§6 4 象限的依赖与调用规则](#6-4-象限的依赖与调用规则)
- [§7 ADR-036 演进（3 区分 → 4 象限）](#7-adr-036-演进3-区分--4-象限)
- [§8 迁移路径](#8-迁移路径)
- [§9 验收标准](#9-验收标准)

---

## §0 文档定位

### §0.1 为什么需要 4 象限

经过 4 阶段（Phase 1~2 / Stage 1~4）演进，UsrLinuxEmu 现有目录布局出现架构性问题：

1. **`src/kernel/` 范围失焦**：混入 PcieEmu（设备模型）和 iommu_emu（driver）
2. **真机对齐不足**：`drivers/pci/` ↔ UsrLinuxEmu 无对应位置
3. **ADR-088 `src/system_hw/` 概念不精确**：与 CppTLM 桥接边界模糊
4. **缺 8 tier PCIe 仿真分层**：无法对接 CppTLM Phase 0~7 14 个组件

### §0.2 阅读对象

- **架构师 / 维护者**: §1 总览 + §6 调用规则 + §8 迁移路径
- **新贡献者**: §1 一张图 + §2-5 各象限职责
- **实施者**: §8 迁移路径（具体文件 → 新位置）

### §0.3 关联文档

| 文档 | 关系 |
|------|------|
| [ADR-091](../00_adr/adr-091-pci-driver-architecture-and-four-quadrant.md) | 本文的 SSOT（Proposed v0.1） |
| [core-architecture.md](core-architecture.md) | 核心架构 SSOT（v0.1.7） |
| [roadmap.md](../../roadmap.md) | 顶层路线图（4 阶段） |
| [pcie-bus-bridge-roadmap.md](../../roadmap/pcie-bus-bridge-roadmap.md) | 本方案的实施路线图 |

---

## §1 4 象限总览

### 1.1 一张图

```
┌─────────────────────────────────────────────────────────────────────────┐
│ Q1: src/kernel/                    │ Q2: plugins/*_driver/                 │
│ Linux Kernel Env Sim               │ Portable Driver Code (Linux idioms)   │
│ ──────────────────────             │ ──────────────────────                │
│ ┌──────────────────────────────┐   │ ┌──────────────────────────────┐   │
│ │ VFS / ModuleLoader           │   │ │ pci_driver / vfio_driver    │   │
│ │ ServiceRegistry / IOCtl      │   │ │ iommu_driver / gpu_driver   │   │
│ │ notifier / wait_queue        │   │ │ net_driver / storage_driver │   │
│ │ Linux iommu framework        │   │ │ sample_memory / sample_serial│   │
│ └──────────────────────────────┘   │ └──────────────────────────────┘   │
│ ↑                                  │ ↑                                    │
│ Linux kernel API sim ONLY          │ Linux driver idioms ONLY            │
│ NO device models                   │ NO kernel env sim                   │
├────────────────────────────────────┼─────────────────────────────────────┤
│ Q3: sim_hardware/                  │ Q4: plugins/gpu_driver/sim/         │
│ PC System Hardware Sim             │ GPU HW Sim                           │
│ ──────────────────────             │ ──────────────────────                │
│ ┌──────────────────────────────┐   │ ┌──────────────────────────────┐   │
│ │ PCIe Host Bridge / Link      │   │ │ HardwarePullerEmu           │   │
│ │ Chipset / Northbridge        │   │ │ GlobalScheduler             │   │
│ │ IOMMU HW Model / CppTLM      │   │ │ CommandProcessor / MQD      │   │
│ │ Interrupt Ctrl / ATS         │   │ │ GpuQueueEmu / RingBuffer    │   │
│ │ Topology loader              │   │ │ DMA coherent pool / VRAM    │   │
│ └──────────────────────────────┘   │ └──────────────────────────────┘   │
│ ↑                                  │ ↑                                    │
│ NOT Linux kernel                   │ GPU-specific HW ONLY                │
│ PC platform emulation              │ Tightly coupled to gpu_driver        │
└────────────────────────────────────┴─────────────────────────────────────┘

HAL (Plugins/gpu_driver/hal/) — Q2↔Q4 之间桥接（per ADR-036 D2）
                                不属于任何单一象限，是 ②↔③ 桥接层
```

### 1.2 与真机 Linux 内核目录对应

| 真机 Linux 内核路径 | UsrLinuxEmu 4 象限 | 关系 |
|---------------------|---------------------|------|
| `arch/xxx/kernel/` | **Q1**: `src/kernel/` | kernel core sim |
| `include/linux/` | `include/linux_compat/` | API header-only compat |
| `kernel/` (sched/fs/mm) | `src/kernel/` | kernel core sim |
| `drivers/pci/` | **Q2**: `plugins/pci_driver/` | **1:1 对应** |
| `drivers/vfio/` | **Q2**: `plugins/vfio_driver/` | **1:1 对应**（Stage 5.5+）|
| `drivers/iommu/` | **Q2**: `plugins/iommu_driver/` | **1:1 对应** |
| `drivers/net/` | `plugins/net_driver/` | **1:1 对应** |
| `drivers/gpu/drm/amd/amdgpu/` | `plugins/gpu_driver/drv/` | **1:1 对应** |
| `arch/xxx/pci/` | **Q3**: `sim_hardware/pcie/` | 平台 PCI 仿真 |
| `arch/xxx/kernel/iommu.c` | `sim_hardware/dma/iommu_hw.cpp` | 平台 IOMMU 仿真 |
| **vendor 私有 GPU sim** | **Q4**: `plugins/gpu_driver/sim/` | GPU-specific sim |

**核心承诺**：
- `plugins/pci_driver/` 实现的代码**逻辑零修改**可 cp 到真机 `drivers/pci/` 编译
- `plugins/iommu_driver/` 实现的代码可 cp 到真机 `drivers/iommu/` 编译
- `plugins/vfio_driver/` 实现的代码可 cp 到真机 `drivers/vfio/` 编译

---

## §2 Q1: src/kernel/ — Linux Kernel Env Sim

### 2.1 范围

**只包含**：Linux kernel core API 在用户态的仿真实现。

### 2.2 现有内容（保留）

| 文件 | 角色 |
|------|------|
| `src/kernel/vfs.cpp` | 虚拟文件系统（VFS::instance() Meyers singleton） |
| `src/kernel/module_loader.cpp` | 插件动态加载（dlopen + dlsym("mod")） |
| `src/kernel/service_registry.cpp` | 跨模块服务注册 |
| `src/kernel/logger.cpp` | 统一日志 |
| `src/kernel/iocontroller.cpp` | ioctl 调度 |
| `src/kernel/iommu_framework.cpp` | Linux iommu framework（API 实现层）|
| `src/kernel/notifier.cpp` | notifier 子系统 |
| `src/kernel/wait_queue.cpp` | 同步原语 |
| `src/kernel/...` | 其他 kernel core sim |

### 2.3 待移除内容（迁移到 Q2/Q3）

| 现有文件 | 迁往 | 原因 |
|----------|------|------|
| `src/kernel/pcie/` (5 文件, 648 LOC, 含 `pcie_emu_impl.h` 私有头 177 LOC) | Q2: `plugins/pci_driver/` | 是 PCI subsystem driver，不是 kernel sim |
| `src/kernel/iommu/iommu_emu_state.cpp` | Q2: `plugins/iommu_driver/` | 是 iommu driver，不是 framework |
| `src/kernel/iommu/iommu_group.cpp` | Q2: `plugins/iommu_driver/` | 同上 |
| `src/kernel/iommu/iommu_domain.cpp` | Q2: `plugins/iommu_driver/` | 同上 |
| `src/kernel/iommu/ats_protocol.cpp` (73 LOC) | Q2: `plugins/iommu_driver/ats_protocol.c` | 同上 |
| `src/kernel/iommu/dma_remap.cpp` (269 LOC) | Q2: `plugins/iommu_driver/dma_remap.c` | 同上（与 ADR-069/073 关联）|
| `src/kernel/iommu/ioasid.cpp` (109 LOC) | Q2: `plugins/iommu_driver/ioasid.c` | 同上 |
| `src/kernel/iommu/invalidate.cpp` (109 LOC) | **Q3**: `sim_hardware/dma/iommu_hw.cpp` | 是 IOMMU 硬件模型，不是 API framework（v0.1 误写为 `iommu_invalidate.cpp`）|
| `src/kernel/iommu/iommu_internal.h` (115 LOC) | Q2: `plugins/iommu_driver/include/iommu_internal.h` | driver 内部头 |
| `src/kernel/iommu/vfio_bridge.cpp` (67 LOC) | Q2: `plugins/iommu_driver/vfio_bridge.c` | Stage 2 交付物（ADR-089 §D7）|
| `src/kernel/iommu/vfio_bridge.h` (17 LOC) | Q2: `plugins/iommu_driver/include/vfio_bridge.h` | 同上 |
| `src/kernel/iommu/pcie_integration.cpp` | Q2: `plugins/pci_driver/pci_iommu_integration.c` | 跨 driver 协作 |

### 2.4 不动

| 现有文件 | 状态 |
|----------|------|
| `include/linux_compat/*` (header-only) | ✅ 保留（API header，不动） |

### 2.5 Q1 的依赖规则

- Q1 可依赖 `include/linux_compat/`（API header 自引用）
- Q1 **不可**依赖 Q2 / Q3 / Q4（kernel sim 不依赖具体 driver 或 sim）
- Q1 **不可**依赖 `sim_hardware/`（kernel sim 是软件仿真，不依赖硬件平台仿真）

---

## §3 Q2: plugins/*_driver/ — Portable Driver Code

### 3.1 范围

**只包含**：用 Linux kernel 习语写的 driver 代码，可直接 cp 到真机 `drivers/` 编译。

### 3.2 现有内容（部分保留 + 部分新增）

| 现有 plugin | 状态 | 备注 |
|-------------|------|------|
| `plugins/gpu_driver/` | ✅ 保留 | 已经是 portable driver |
| `plugins/net_driver/` | ✅ 保留 | Stage 2 交付 |
| `plugins/storage_driver/` | ✅ 保留 | Stage 2 交付 |
| `plugins/sample_memory/` | ✅ 保留 | 示例 |
| `plugins/sample_serial/` | ✅ 保留 | 示例 |

### 3.3 新增 plugin（ADR-091）

| 新 plugin | 来源 | 职责 |
|-----------|------|------|
| **`plugins/pci_driver/`** | 🆕 迁移自 `src/kernel/pcie/` + 新增 Linux PCI subsystem 等价代码 | pci_scan_slot / pci_bus_assign_resources / BAR 配置 / MSI-X |
| **`plugins/iommu_driver/`** | 🆕 迁移自 `src/kernel/iommu/` 部分 + 新增 Linux IOMMU subsystem 等价代码 | iommu_domain / group / notifier / iommufd |
| **`plugins/vfio_driver/`** | 🆕 Stage 5.5+ 触发 | vfio / vfio_pci / iommufd 集成 |

### 3.4 plugins/pci_driver/ 内部结构

```
plugins/pci_driver/
├── CMakeLists.txt
├── plugin.cpp                    # module mod 导出
├── include/
│   ├── pcie_emu.h                # 迁移自 include/kernel/pcie/pcie_emu.h
│   └── pci_device.h              # 迁移自 include/kernel/pcie_device.h
├── probe.c                       # 迁移自 src/kernel/pcie/pcie_emu.cpp
├── pci_access.c                  # 迁移自 config_space.cpp
├── pci_msi.c                     # 迁移自 msi_x.cpp
├── pci_cap.c                     # 迁移自 capability_walk.cpp
├── pci_bus.c                     # 🆕 Linux drivers/pci/bus.c 等价
├── pci_probe.c                   # 🆕 Linux drivers/pci/probe.c 核心
├── pci_setup_bus.c               # 🆕 Linux drivers/pci/setup-bus.c
├── pci_iommu_integration.c       # 🆕 跨 driver 协作（调 iommu_register_pci_device）
├── pci_iov.c                     # 🆕 Wave 3：Linux drivers/pci/iov.c（SR-IOV）
└── pcie_portdrv.c                # 🆕 AER / PME / hotplug
```

### 3.5 plugins/iommu_driver/ 内部结构

```
plugins/iommu_driver/
├── CMakeLists.txt
├── plugin.cpp                    # module mod 导出
├── include/
│   └── iommu_driver.h
├── iommu.c                       # 迁移自 src/kernel/iommu/iommu_emu_state.cpp
├── iommu_group.c                 # 迁移自 iommu_group.cpp
├── iommu_domain.c                # 迁移自 iommu_domain.cpp
├── ats_protocol.c                # 🆕 迁移自 src/kernel/iommu/ats_protocol.cpp
├── dma_remap.c                   # 🆕 迁移自 src/kernel/iommu/dma_remap.cpp
├── ioasid.c                      # 🆕 迁移自 src/kernel/iommu/ioasid.cpp
├── iommu_internal.h              # 🆕 迁移自 src/kernel/iommu/iommu_internal.h
├── iommu-sva.c                   # 🆕 Linux drivers/iommu/iommu-sva.c 等价
├── amd_iommu.c                   # 🆕 stub（Stage 5 后）
├── intel_iommu.c                 # 🆕 stub（Stage 5 后）
└── iommufd.c                     # 🆕 Linux drivers/iommu/iommufd.c 等价
```

### 3.6 plugins/vfio_driver/ 内部结构（Stage 5.5+ 触发）

```
plugins/vfio_driver/
├── CMakeLists.txt
├── plugin.cpp
├── include/
│   └── vfio_driver.h
├── vfio.c                        # 🆕 Linux drivers/vfio/vfio.c 等价
├── vfio_pci.c                    # 🆕 Linux drivers/vfio/pci/vfio_pci.c 等价
├── vfio_iommufd.c                # 🆕 iommufd 集成
└── ... (其他 VFIO 子模块)
```

### 3.7 Q2 的依赖规则

- Q2 可依赖 Q1（kernel sim API）+ Q3（hardware sim API）
- Q2 **不可**依赖其他 Q2 plugin（driver 之间通过 Q1 通信）
- Q2 **不可**依赖 Q4（GPU-specific sim）
- Q2 → Q3 调用是单向的（driver 调 platform API）

### 3.8 Q2 与 Q4 的边界（关键）

| 维度 | Q2 (drv/) | Q4 (sim/) |
|------|-----------|-----------|
| 性质 | Linux kernel idioms | Hardware emulation |
| 真机对应 | `drivers/gpu/drm/amd/amdgpu/` | (无真机对应，是产品特定 sim) |
| 跨层依赖 | HAL（Q2↔Q4 桥）| HAL（Q2↔Q4 桥）|
| 物理隔离 | `plugins/gpu_driver/drv/` | `plugins/gpu_driver/sim/` |

**禁止规则**（per ADR-036）：
- `drv/` 不能 `#include "sim/"` 的代码
- `sim/` 不能 `#include "drv/"` 的代码
- 通信只能通过 HAL 的 `struct gpu_hal_ops`

---

## §4 Q3: sim_hardware/ — PC System Hardware Sim

### 4.1 范围

**只包含**：PC 平台硬件仿真（chipset + PCIe + interrupt + IOMMU + CppTLM 桥接）。

**关键性质**：**不是 Linux kernel**。这是 PC machine hardware model，对应真机 `arch/xxx/`。

### 4.2 sim_hardware/ 完整结构

```
sim_hardware/
├── include/
│   ├── pcie/
│   │   ├── host_bridge.h         # L2: PCIe host bridge / RC mirror
│   │   ├── link_layer.h          # L3: PCIe Link Layer
│   │   ├── phy.h                 # L4: PHY digital ctrl
│   │   ├── bypass.h              # PcieBypass Mux（Full/Bypass/Partial）
│   │   ├── completion.h          # L6: Completion tracking
│   │   └── sr_iov.h              # L5: SR-IOV VF pool
│   ├── cpptlm/
│   │   ├── bridge.h              # C ABI 桥接（封装 cpptlm_emulator.h）
│   │   ├── endpoint.h            # PcieEndpointIP 17-port composition
│   │   ├── axi_adapter.h         # PcieAxiAdapter 包装
│   │   └── tier.h                # 8 tier 运行时选择（v0.2 修正）
│   ├── chipset/
│   │   ├── northbridge.h         # 北桥仿真
│   │   ├── interrupt.h           # IOAPIC / MSI 控制器仿真
│   │   └── topology.h            # PCIe 拓扑描述加载器
│   ├── dma/
│   │   ├── iommu_hw.h            # IOMMU 硬件模型
│   │   └── ats.h                 # ATS/PASID 硬件模型
│   └── platform.h                # 平台抽象（x86 vs ARM 切换）
├── src/                          # 实现，对称结构
└── topology/                     # 配置
    ├── default_topology.json     # 默认 PC 拓扑
    └── cpptlm_dgpu_v1.json       # CppTLM dGPU profile
```

### 4.3 Q3 与 CppTLM 的集成

```
┌──────────────────────────────────────────────────────────────┐
│ Q3: sim_hardware/cpptlm/                                     │
│   - bridge.cpp: 封装 cpptlm_emulator.h C ABI                 │
│   - endpoint.cpp: 持有 PcieEndpointIP* 引用（17-port）      │
│   - axi_adapter.cpp: 持有 PcieAxiAdapter 引用                │
│   ──────────────────────────                                  │
│   通过 attach_to_endpoint() 静态注册表桥接                    │
└─────────────────────┬────────────────────────────────────────┘
                      │ 静态链接 cpptlm_core.so
                      ▼
┌──────────────────────────────────────────────────────────────┐
│ CppTLM (外部库)                                              │
│   include/abi/cpptlm_emulator.h (23 ABI 冻结)               │
│   include/tlm/pcie/*.hh (Phase 0-7 14 组件)                  │
│   - PcieEndpointTLM / PcieEndpointIP                         │
│   - PcieLinkLayer / PciePhyDigitalCtrl / PcieBypassMux       │
│   - HostBypassTLM / PcieRootComplexTLM                       │
│   - CompletionTracker / PcieSriovVfPool / PcieAxiAdapter     │
│   - Axi4StreamAdapter / Axi4Mapper / ...                     │
└──────────────────────────────────────────────────────────────┘
```

### 4.4 8 tier PCIe 仿真（v0.2 修正）

| Tier | 名称 | CppTLM 组件 | sim_hardware/pcie/ 文件 |
|------|------|-------------|--------------------------|
| **L1** | Software Bypass | `HostBypassTLM` (Phase 7) | `host_bridge.cpp`（用 BypassTLM 实现）|
| **L2** | Root Complex Mirror | `PcieRootComplexTLM` (Phase 7) | `host_bridge.cpp`（含 RC mode）|
| **L3** | Link Layer | `PcieLinkLayer` (Phase 1) | `link_layer.cpp` |
| **L4** | PHY Digital | `PciePhyDigitalCtrl` (Phase 2) | `phy.cpp` |
| **L5** | SR-IOV | `PcieEndpointIP` (Phase 4, 17-port) | `sr_iov.cpp` |
| **L6** | Completion Tracking | `CompletionTracker` (Phase 4) | `completion.cpp` |
| **L7** | AXI Adapter | `PcieAxiAdapter` (Phase 5) | `axi_adapter.cpp` |
| **L8** | AXI Mapper OOO | `Axi4Mapper` (Phase 6) | （在 cpptlm/axi_adapter.cpp）|

**累计行数**：~3,100-4,500 行（Wave 2-5 实施）

### 4.5 PCIe Bypass 形态（关键）

`sim_hardware/pcie/bypass.h` 暴露 3 态模式：

```cpp
namespace usr_linux_emu::sim_hardware::pcie {

// Canonical 枚举值（per Oracle Gate D 修正）：与代码 `bypass.h:8-12` 实测一致
enum class BypassMode : uint8_t {
    kFull    = 0,  // PHY + LL + TL + AXI（完整 PCIe 链路）
    kBypass  = 1,  // TL + AXI（直接事务层，软件 bring-up）
    kPartial = 2,  // LL + TL + AXI（跳过 PHY，保留 FC + ACK-NAK）
};

// 命名空间级自由函数（代码实测）：bypass_apply_mode/bypass_get_mode + drain accounting
int  bypass_apply_mode(BypassMode mode, DrainPolicy policy = DrainPolicy::kGracefulDrain);
BypassMode bypass_get_mode(void);
```

**实现**：`bypass.cpp` 封装 CppTLM `PcieBypassMux::apply_mode()` 的 10 步清理（DrainPolicy）。

**驱动零修改路径（per [ADR-092](../00_adr/adr-092-hal-adapter-and-bypass-binding.md) §D2）**：driver 通过 `linux_compat/pci`（`ioremap` + `readl`/`writel`）调用的 BAR MMIO，在 `PcieBypassController` 自动裁决下路由：
- **`kFull` / `kPartial`** → `cpptlm_emulator_mmio_*`（PCIe EP TLM 编解码）；
- **`kBypass`** → `cpptlm_emulator_backdoor_*`（跳过 PCIe EP TLM，直插 DGpuBoard AXI/VRAM）。

**Adapter 信息通道（per ADR-092 §D1）**：HAL `gpu_hal_ops` 68 → 71 fn-ptrs append-only 扩展 3 个 adapter 接口（`adapter_get_info`/`open`/`close`）；新文件 `hal_cpptlm.cpp` 通过 `sim_hardware::BackdoorEndpoint` + `CpptlmBridge` 真实调用 CppTLM 扩展后的 `cpptlm_emulator_open/close/get_adapter_info`。

**模式选择**（`topology/default_topology.json`）：
- **L1 (Bypass)**：driver bring-up，最快
- **L2 (Partial)**：driver 功能验证
- **L8 (Full)**：硬件协同验证

### 4.6 Q3 的依赖规则

- Q3 可依赖 `include/linux_compat/` 的 header-only 部分（仅类型/宏定义）
- Q3 **不可**依赖 Q1 / Q2 / Q4（sim_hardware 是 platform-level，不依赖具体 driver）
- Q3 **必须**通过 `cpptlm/bridge.cpp` 单一入口封装 CppTLM（保持 23 ABI 边界）

### 4.7 sim_hardware/ 与 ADR-088/089 的关系

| ADR | 旧 location | 新 location（本 ADR） |
|-----|-------------|----------------------|
| ADR-088 §C2 | `src/system_hw/` | `sim_hardware/` |
| ADR-089 v0.5 §C1 | `src/system_hw/` | `sim_hardware/` |
| ADR-089 v0.5 §C2 (`iommu/`) | `src/system_hw/iommu/` | 拆分为 `plugins/iommu_driver/` + `sim_hardware/dma/iommu_hw/` |
| ADR-089 v0.5 §C2 (`cxl_memdev/`) | `src/system_hw/cxl_memdev/` | `sim_hardware/chipset/cxl_memdev/` |

---

## §5 Q4: plugins/gpu_driver/sim/ — GPU HW Sim

### 5.1 范围

**只包含**：GPU-specific 硬件仿真（HardwarePullerEmu, GlobalScheduler, CommandProcessor 等）。

**关键性质**：紧耦合 gpu_driver，**不能跨 driver 复用**。

### 5.2 现有内容（全部保留）

| 文件 | 角色 |
|------|------|
| `plugins/gpu_driver/sim/scheduler/` | GlobalScheduler |
| `plugins/gpu_driver/sim/hardware/` | HardwarePullerEmu, channel_manager, mqd_state |
| `plugins/gpu_driver/sim/gpu_queue_emu.cpp` | Ring Buffer 消费者 |
| `plugins/gpu_driver/sim/bar_sim.cpp` | BAR 仿真 |
| `plugins/gpu_driver/sim/dma_coherent_pool.cpp` | DMA coherent pool |
| `plugins/gpu_driver/sim/page_fault_handler.cpp` | GPU page fault |
| `plugins/gpu_driver/sim/page_migration.cpp` | Page migration |
| `plugins/gpu_driver/sim/fence_id.cpp` | Fence ID tracking |
| `plugins/gpu_driver/sim/graph.cpp` | GPU graph |
| `plugins/gpu_driver/sim/green_context.cpp` | Green context |
| `plugins/gpu_driver/sim/mem_pool.cpp` | Memory pool |
| `plugins/gpu_driver/sim/semaphore_manager.cpp` | Semaphore |
| `plugins/gpu_driver/sim/vram_store.cpp` | VRAM storage |
| `plugins/gpu_driver/sim/backdoor_preempt.cpp` | Backdoor preempt |
| `plugins/gpu_driver/sim/pdl.cpp` | PDL (Preemption Data Loader) |
| `plugins/gpu_driver/sim/sim_device_va_allocator.cpp` | VA allocator |
| `plugins/gpu_driver/sim/stream_capture.cpp` | Stream capture |
| `plugins/gpu_driver/sim/timestamp_query.cpp` | Timestamp query |

### 5.3 Q4 的依赖规则

- Q4 可依赖 HAL（`gpu_hal.h`）作为对外接口
- Q4 **不可**依赖 Q1 / Q2 / Q3（GPU sim 是 plugin 内部，不依赖其他象限）
- Q4 **不可**被 Q2（drv/）访问（HAL 反向注入）

---

## §6 4 象限的依赖与调用规则

### 6.1 依赖图

```
                  ┌─────────────────┐
                  │ Q4: gpu_driver/ │
                  │     sim/        │
                  └────────▲────────┘
                           │ HAL 桥接（71 fn-ptrs，per ADR-092 68→71 append-only）
                  ┌────────┴────────┐
                  │ Q2: gpu_driver/ │
                  │     drv/        │
                  └────────▲────────┘
                           │ VFS / ModuleLoader / notifier
                           │
                  ┌────────┴────────┐
                  │ Q1: src/kernel/ │
                  │   (env sim)     │
                  └────────▲────────┘
                           │ 调 platform API
                           │
                  ┌────────┴────────┐
                  │ Q2: pci_driver/ │
                  │ iommu_driver/   │
                  └────────▲────────┘
                           │ 调 hardware sim API
                           │
                  ┌────────┴────────┐
                  │ Q3: sim_hardware/│
                  │  (PC platform)  │
                  └─────────────────┘
```

### 6.2 关键调用方向

| 调用方 | 被调用方 | 说明 |
|--------|----------|------|
| Q2 (gpu_driver/drv/) | Q1 (kernel sim) | driver 调 kernel API（VFS, notifier）|
| Q2 (gpu_driver/drv/) | Q4 (gpu sim) | **经 HAL 反向注入**（不是直接调用）|
| Q4 (gpu sim) | Q2 (gpu_driver/drv/) | 回调（HAL callback 路径）|
| Q2 (pci_driver/) | Q1 (kernel sim) | driver 调 kernel API |
| Q2 (pci_driver/) | Q3 (sim_hardware/pcie/) | driver 调 platform API |
| Q3 (sim_hardware/) | CppTLM (外部) | 调 CppTLM API（C ABI + C++ composition）|
| Q1 (kernel sim) | (无依赖) | Q1 是 base，不能依赖其他象限 |

### 6.3 禁止的依赖

| 禁止 | 原因 |
|------|------|
| Q1 → Q2 / Q3 / Q4 | kernel sim 不能依赖具体 driver 或 sim |
| Q3 → Q1 / Q2 / Q4 | hardware sim 不能依赖具体 software |
| Q4 → Q1 / Q2 / Q3 | GPU sim 不能跨 driver 复用 |
| Q2 (drv/) → Q4 (sim/) | 违反 ADR-036（drv 不能包含 sim）|

---

## §7 ADR-036 演进（3 区分 → 4 象限）

### 7.1 ADR-036 原版（3 区分）

```
① Linux Kernel Env Sim
② Portable Driver Code
③ Hardware Sim
```

### 7.2 本 ADR 扩展（4 象限）

```
① Linux Kernel Env Sim         → Q1: src/kernel/
② Portable Driver Code          → Q2: plugins/*_driver/
③a PC System Hardware Sim       → Q3: sim_hardware/
③b GPU-specific Hardware Sim    → Q4: plugins/gpu_driver/sim/
```

**核心改动**：③ 拆为 ③a（platform-level，独立顶级目录）和 ③b（GPU-specific，紧耦合 gpu_driver）。

### 7.3 ADR-036 后续更新

ADR-036 应在 Change-1 实施时同步更新（v0.2），增加 §Decision 4 描述 4 象限拆分。

---

## §8 迁移路径

### 8.1 迁移清单

| # | 现有路径 | 新路径 | 行数 | Wave |
|---|----------|--------|----:|------|
| 1 | `src/kernel/pcie/pcie_emu.cpp` | `plugins/pci_driver/probe.c` | 156 | Wave 1 |
| 2 | `src/kernel/pcie/pcie_emu_impl.h` | `plugins/pci_driver/include/pcie_emu_impl.h` | 177 | Wave 1 |
| 3 | `src/kernel/pcie/config_space.cpp` | `plugins/pci_driver/pci_access.c` | 80 | Wave 1 |
| 4 | `src/kernel/pcie/msi_x.cpp` | `plugins/pci_driver/pci_msi.c` | 88 | Wave 1 |
| 5 | `src/kernel/pcie/capability_walk.cpp` | `plugins/pci_driver/pci_cap.c` | 147 | Wave 1 |
| 6 | `src/kernel/iommu/iommu_emu_state.cpp` | `plugins/iommu_driver/iommu.c` | 159 | Wave 1 |
| 7 | `src/kernel/iommu/iommu_group.cpp` | `plugins/iommu_driver/iommu_group.c` | 158 | Wave 1 |
| 8 | `src/kernel/iommu/iommu_domain.cpp` | `plugins/iommu_driver/iommu_domain.c` | 75 | Wave 1 |
| 9 | `src/kernel/iommu/ats_protocol.cpp` | `plugins/iommu_driver/ats_protocol.c` | 73 | Wave 1 |
| 10 | `src/kernel/iommu/dma_remap.cpp` | `plugins/iommu_driver/dma_remap.c` | 269 | Wave 1 |
| 11 | `src/kernel/iommu/ioasid.cpp` | `plugins/iommu_driver/ioasid.c` | 109 | Wave 1 |
| 12 | `src/kernel/iommu/invalidate.cpp` | `sim_hardware/dma/iommu_hw.cpp` | 109 | Wave 1 |
| 13 | `src/kernel/iommu/iommu_internal.h` | `plugins/iommu_driver/include/iommu_internal.h` | 115 | Wave 1 |
| 14 | `src/kernel/iommu/pcie_integration.cpp` | `plugins/pci_driver/pci_iommu_integration.c` | 111 | Wave 1 |
| 15 | `src/kernel/iommu/vfio_bridge.cpp` | `plugins/iommu_driver/vfio_bridge.c` | 67 | Wave 1 |
| 16 | `src/kernel/iommu/vfio_bridge.h` | `plugins/iommu_driver/include/vfio_bridge.h` | 17 | Wave 1 |
| 17 | `include/kernel/pcie/pcie_emu.h` | `plugins/pci_driver/include/pcie_emu.h` | 90 | Wave 1 |
| 18 | `include/kernel/pcie_device.h` | `plugins/pci_driver/include/pci_device.h` | 28 | Wave 1 |
| **迁移小计** | | | **~2,028** | |
| 19 | ❌ 无 | `plugins/pci_driver/pci_bus.c` 等 | ~500 | Wave 1 |
| 20 | ❌ 无 | `plugins/iommu_driver/amd_iommu.c` 等 | ~300 | Wave 1 |
| 21 | ❌ 无 | `sim_hardware/pcie/host_bridge.cpp` 等 8 tier | ~3,100-4,500 | Wave 2-5 |
| 16 | ❌ 无 | `plugins/vfio_driver/` | ~1,500 | Wave 5 |
| **新增小计** | | | **~5,800** | |

### 8.2 命名空间映射

| 现有命名空间 | 新命名空间 | Wave |
|--------------|-------------|------|
| `usr_linux_emu::pcie_internal` | `usr_linux_emu::pci` | Wave 1 |
| `usr_linux_emu::iommu_emu_global_state` | `usr_linux_emu::iommu_driver::global_state` | Wave 1 |
| ❌ 无 | `usr_linux_emu::sim_hardware::pcie` | Wave 2 |
| ❌ 无 | `usr_linux_emu::sim_hardware::cpptlm` | Wave 2 |
| ❌ 无 | `usr_linux_emu::sim_hardware::chipset` | Wave 3 |
| ❌ 无 | `usr_linux_emu::sim_hardware::dma` | Wave 2 |

### 8.3 实施 Wave 划分

| Wave | 内容 | 工期 | 依赖 | Gate |
|------|------|-----:|------|------|
| **Wave 1** | Stage 1：代码迁移（不改功能）| 4-6 周 | 无 | 130+ ctest 全 PASS；ADR-072 L2 build 通过 |
| **Wave 2** | Stage 2：sim_hardware 基础 + L1+L2 | 6-8 周 | Wave 1 + CppTLM v0.5 stable | Tier 1+2 standalone 测试 PASS |
| **Wave 3** | Stage 3：L3+L5+L6 | 10-16 周 | Wave 2 | Tier 3+5+6 standalone 测试 PASS |
| **Wave 4** | Stage 4：L4+L7 | 8-12 周 | Wave 3 | Tier 4+7 standalone 测试 PASS |
| **Wave 5** | Stage 5：L8 + VFIO | 8-12 周 | Wave 4 | Tier 8 + VFIO standalone 测试 PASS；ADR-072 真机一致性 |

**总计**：36-54 周（约 9-13 月）

---

## §9 验收标准

### AC1: 4 象限布局合规（Wave 1）

- [ ] `src/kernel/` 不再含 PCI/IOMMU driver 代码
- [ ] `plugins/pci_driver/` + `plugins/iommu_driver/` 创建并加载成功
- [ ] `sim_hardware/` 顶级目录建立
- [ ] `sim_hardware/dma/iommu_hw.cpp` 封装完成
- [ ] 现有 130+ ctest 全部 PASS（**0 regression**）
- [ ] ModuleLoader 支持 depends 拓扑排序（含新 load_priority 字段，ABI 变更）

### AC2: 8 tier 仿真（Wave 2-5，v0.2 修正）

- [ ] L1 (Bypass) PASS
- [ ] L2 (RC Mirror) PASS — **首次验证：模拟 host 启动后枚举到 GPU 设备**
- [ ] L3 (Link Layer) PASS
- [ ] L4 (PHY) PASS
- [ ] L5 (SR-IOV) PASS
- [ ] L6 (Completion) PASS
- [ ] L7 (AXI Adapter) PASS
- [ ] L8 (AXI Mapper) PASS

### AC3: 真机一致性（Wave 5）

- [ ] `plugins/pci_driver/` 代码在 Linux 6.12 真机编译通过（ADR-072 L2 build 扩展）
- [ ] 真机 PCIe 设备扫描与 UsrLinuxEmu 行为一致
- [ ] `plugins/iommu_driver/` + `plugins/vfio_driver/` 同上

---

## 修订记录

- **v0.2** (2026-09-03, Proposed)：Oracle v0.1 复审 INCONCLUSIVE 修复完成
  - §8.1 迁移清单：补全 PCI 5 文件实测（648 LOC）+ IOMMU 11 文件实测（1,262 LOC）；删不存在的 `iommu_notifier.cpp`；修正 `invalidate.cpp` 文件名；补 7 个遗漏文件（ats_protocol/dma_remap/ioasid/iommu_internal.h/vfio_bridge.cpp+h）
  - 9 tier → 8 tier 命名统一（VFIO 不算 PCIe tier）
  - module struct 示例：`deps` → `depends`（实测 module_loader.h:10）；新增 `load_priority` 字段标注
  - 测试文件迁移：6 个 test_*_standalone 的去向说明

- **v0.1** (2026-09-03, Proposed): 初版
  - 4 象限总览 + 与真机 Linux 内核目录对应
  - 每个象限的范围 / 内容 / 依赖规则
  - 依赖图与禁止规则
  - ADR-036 演进路径
  - 完整迁移清单 + Wave 划分
  - 验收标准

---

**状态**: ✅ **Accepted v0.2**（与 ADR-091 v0.2 同步 — Oracle v0.2 复审 PASS；4 象限布局 SSOT）
