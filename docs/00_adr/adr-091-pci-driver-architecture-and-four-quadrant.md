# ADR-091: 4 象限目录布局 + PCI/VFIO/IOMMU 驱动迁移 + sim_hardware/ 引入

**状态**: ✅ **Accepted v0.2**（2026-09-03，Stage 5.5.1 实施后升档；4 commits ship + 151/151 ctest PASS + Gate 5.5.1-A/B/C 全部通过；Oracle 实施后复审 Gate D 待触发）
**日期**: 2026-09-03
**版本**: v0.2（Accepted — Stage 5.5.1 实施升档）
**提案人**: UsrLinuxEmu Architecture Team
**评审者**: Oracle（v0.2 实施后复审 Gate D 待触发；proposal + design v0.3 经 Oracle + Metis 双审查通过）
**关联 ADR**:
- [ADR-036](adr-036-three-way-separation.md) ✅ Accepted — 3 区分原则（**本 ADR 将其扩展为 4 象限**）
- [ADR-088](../00_adr/adr-088-dgpu-complete-simulation.md) ✅ Accepted — dGPU 参考设计（明确 `src/system_hw/` 概念）
- [ADR-089](adr-089-v55-system-hw-simulation.md) ✅ Accepted — v5.5+ 系统硬件仿真（**本 ADR 调整 location**: `src/system_hw/` → `sim_hardware/`）
- [ADR-090](adr-090-ptxir-via-h2d-dma-v2.md) ✅ Accepted — H2D DMA PTXIR（依赖本 ADR 的 PCIe bus）
- [ADR-023](adr-023-hal-interface.md) ✅ Accepted — HAL 68 fn-ptrs append-only
- [ADR-061](adr-061-hal-iommu-extension.md) ✅ Accepted — HAL IOMMU ops 扩展
- [ADR-072](adr-072-portability-validation.md) ✅ Accepted — Linux 6.12 LTS L2 build（**本 ADR 扩展 L2 build 目标集**）
- [ADR-035](adr-035-governance-policy.md) ✅ Accepted — 治理规则

---

## Context

### C1: 现状问题

经过 4 阶段（Phase 1~2 / Stage 1~4）演进，UsrLinuxEmu 现有目录布局出现以下**架构性问题**：

#### C1.1: `src/kernel/` 范围失焦

`src/kernel/` 当前职责**模糊**：既包含 VFS / ModuleLoader 等 Linux kernel env sim，又混入 PcieEmu（设备模型，~648 LOC，5 文件）和 iommu_emu（~1262 LOC，11 文件）。**真机 Linux 内核中，`drivers/pci/` 和 `drivers/iommu/` 都是 `drivers/` 下的子系统**，不属于 kernel core。

#### C1.2: 真机对齐不足

| 真机 Linux 路径 | UsrLinuxEmu 当前路径 | 性质偏离 |
|----------------|---------------------|---------|
| `drivers/pci/probe.c` 等 | `src/kernel/pcie/pcie_emu.cpp` 等 | **错位**：应为 portable driver（②）而非 kernel sim（①）|
| `drivers/vfio/vfio.c` 等 | ❌ 不存在 | **缺失**：ADR-089 v5.5+ 触发 |
| `drivers/iommu/iommu.c` 等 | `src/kernel/iommu/iommu_emu_state.cpp` 等 | **错位**：同上 |
| `arch/xxx/pci/` 平台 PCI | ❌ 不存在 | **缺失**：PC 平台仿真无独立位置 |

#### C1.3: ADR-088 提出的 `src/system_hw/` 概念已不够精确

ADR-088 §C2 把 `src/system_hw/` 定义为"PC 系统硬件仿真"，但 ADR-089 v0.5 落地时发现：
- 该目录与 CppTLM 的桥接（23 ABI）需要清晰的物理边界
- 与 `plugins/gpu_driver/sim/`（GPU-specific 仿真）需要明确区分
- 与 Linux kernel env sim（`src/kernel/`）需要明确区分

#### C1.4: 缺 8 tier PCIe 仿真分层（v0.2 修正）

CppTLM 已交付 **Phase 0~7 共 14 个 PCIe 组件**（~3,684 LOC），覆盖 TLP / Link / PHY / SR-IOV / Completion / AXI / Host 桥接 / Root Complex。UsrLinuxEmu 当前**没有任何 PCIe bus 模型**，无法对接这些组件的 host 侧能力。

### C2: 用户提案（2026-09-03）

> *"`include/kernel`, `src/kernel` 只用于 linux kernel 仿真代码，而pci驱动的代码放在plugins/pci_driver下。从linux 驱动代码里，drivers/pci属于pci驱动，而在arch/xxx 目录下属于linux kernel。而vfio也应该放在drivers/vfio, 而PC机体系硬件系统的模拟应该单独放在独立的目录下，PC系统硬件用于和CppTLM端的dGPU对接，这里可以通过底层的Pcie链路层对接，也需要模拟PcieBypass形态的对接。"*

提案核心：
1. `src/kernel/` **只**用于 Linux kernel env sim
2. PCI subsystem driver → `plugins/pci_driver/`
3. VFIO subsystem driver → `plugins/vfio_driver/`（Stage 5.5+ 触发）
4. IOMMU subsystem driver → `plugins/iommu_driver/`
5. PC 系统硬件仿真 → **独立顶级目录** `sim_hardware/`
6. sim_hardware/ 对接 CppTLM dGPU，含 PCIe 链路层 + Bypass 形态

---

## Decision

### D1: 引入 4 象限目录布局（4-Quadrant Layout）

将 UsrLinuxEmu 顶层目录按**职责**划分为 4 象限，**1:1 对应真机 Linux 内核目录结构**：

```
┌─────────────────────────────────────────────────────────────────────────┐
│  Q1: src/kernel/         │ Q2: plugins/*_driver/                         │
│  Linux Kernel Env Sim    │ Portable Driver Code (Linux idioms)            │
│  ─────────────────       │ ──────────────────────                          │
│  VFS / ModuleLoader      │ pci_driver / vfio_driver / iommu_driver        │
│  ServiceRegistry / IOCtl │ gpu_driver / net_driver / storage_driver      │
├──────────────────────────────────────────────────┬──────────────────────┤
│  Q3: sim_hardware/                               │ Q4: plugins/gpu_driver/sim/│
│  PC System Hardware Sim                          │ GPU HW Sim           │
│  ──────────────────────                          │ ────────             │
│  PCIe Host Bridge / Link Layer / PHY             │ HardwarePullerEmu    │
│  Chipset / Northbridge / Interrupt Ctrl          │ GlobalScheduler      │
│  IOMMU HW Model / CppTLM SoC Bridge              │ CommandProcessor     │
│  ──────────────────────                          │ ────────             │
│  **NOT Linux kernel; PC platform emulation**     │ **GPU-specific only**│
└──────────────────────────────────────────────────┴──────────────────────┘
```

**核心原则**：
- **Q1 ↔ `arch/xxx/kernel/` + kernel core**：纯 Linux kernel env sim
- **Q2 ↔ `drivers/`**：Portable Linux driver code（Linux idioms）
- **Q3 ↔ `arch/xxx/`**：PC 平台仿真（chipset + PCIe + interrupt + CppTLM 桥接）
- **Q4 ↔ `drivers/gpu/` 内的 vendor 私有 sim**：GPU-specific 硬件仿真

**与 ADR-036 的关系**：ADR-036 是 3 区分（① kernel sim / ② portable driver / ③ hardware sim）。本 ADR 将 ③ 拆为 ③a（PC system sim = Q3）和 ③b（GPU-specific sim = Q4），共 **4 象限**。**ADR-036 后续需更新以反映此拆分**。

### D2: 代码迁移映射

> **v0.2 修订**（2026-09-03，Oracle v0.1 复审 INCONCLUSIVE 修复）：
> 1. **§D2.1 PCIe 迁移表**：补充 `pcie_emu_impl.h`（177 LOC 私有头）+ 锁定 LOC 实测数字
> 2. **§D2.2 IOMMU 迁移表**：删 `iommu_notifier.cpp`（不存在）+ 修正 `invalidate.cpp` 实际文件名 + 补 7 个遗漏文件 + 锁定 LOC 实测数字
> 3. **§D6 module struct 示例**：修正字段名为 `depends`（实际）+ 标注 `load_priority` 为新增字段
> 4. **§D7 不破坏承诺**：补 ADR-069/073
> 5. **§D6 依赖图**：补 `vfio_driver`
> 6. **9 tier → 8 tier PCIe**：修正命名矛盾（VFIO 不算 PCIe tier）
> 7. **测试文件迁移**：补 6 个 test_*_standalone 的迁移去向

#### D2.1: PCI subsystem（`src/kernel/pcie/` → `plugins/pci_driver/`）

**实测文件清单**（`ls -la src/kernel/pcie/`，2026-09-03）：

| 现有文件 | 字节 | 实际行数 | 新位置 | 备注 |
|----------|-----:|---------:|--------|------|
| `src/kernel/pcie/pcie_emu.cpp` | 4711 | 156 | `plugins/pci_driver/probe.c` | PcieEmuImpl → Linux driver 风格 |
| `src/kernel/pcie/pcie_emu_impl.h` | 6721 | 177 | `plugins/pci_driver/include/pcie_emu_impl.h` | **私有头文件**（v0.1 遗漏，v0.2 补）|
| `src/kernel/pcie/config_space.cpp` | 2317 | 80 | `plugins/pci_driver/pci_access.c` | Config space 读写 |
| `src/kernel/pcie/msi_x.cpp` | 2508 | 88 | `plugins/pci_driver/pci_msi.c` | MSI-X handler + PBA |
| `src/kernel/pcie/capability_walk.cpp` | 5053 | 147 | `plugins/pci_driver/pci_cap.c` | Capability chain walk |
| **小计（5 文件）** | **21,310** | **648** | | |
| `include/kernel/pcie/pcie_emu.h` | - | 90 | `plugins/pci_driver/include/pcie_emu.h` | 公共接口保留 |
| `include/kernel/pcie_device.h` | - | 29 | `plugins/pci_driver/include/pci_device.h` | PcieRootComplex 抽象 |

**PCI 现有测试**（迁移后必须仍 PASS）：
- `tests/test_pcie_emu_standalone.cpp` → 迁移后路径不变（API 兼容）
- `tests/test_pcie_gpu.cpp` → 迁移后路径不变

**新文件**（Linux PCI subsystem 等价）：
- `plugins/pci_driver/pci_bus.c` ↔ `drivers/pci/bus.c`
- `plugins/pci_driver/pci_probe.c` ↔ `drivers/pci/probe.c`（核心）
- `plugins/pci_driver/pci_setup_bus.c` ↔ `drivers/pci/setup-bus.c`
- `plugins/pci_driver/pci_iov.c` ↔ `drivers/pci/iov.c`（SR-IOV，L5 实施）
- `plugins/pci_driver/pcie_portdrv.c` ↔ `drivers/pci/pcie/portdrv.c`（AER/PME/hotplug stub）

**命名空间**：`usr_linux_emu::pcie_internal` → `usr_linux_emu::pci`

#### D2.2: IOMMU subsystem（`src/kernel/iommu/` 拆分）

**实测文件清单**（`ls -la src/kernel/iommu/`，2026-09-03）：

| 现有文件 | 字节 | 实际行数 | 新位置 | 归属象限 | 备注 |
|----------|-----:|---------:|--------|----------|------|
| `src/kernel/iommu/iommu_emu_state.cpp` | 4473 | 159 | `plugins/iommu_driver/iommu.c` | Q2 | Linux iommu framework driver |
| `src/kernel/iommu/iommu_group.cpp` | 3633 | 158 | `plugins/iommu_driver/iommu_group.c` | Q2 | |
| `src/kernel/iommu/iommu_domain.cpp` | 1957 | 75 | `plugins/iommu_driver/iommu_domain.c` | Q2 | |
| `src/kernel/iommu/ats_protocol.cpp` | 2271 | 73 | `plugins/iommu_driver/ats_protocol.c` | Q2 | **v0.1 遗漏**（v0.2 补）|
| `src/kernel/iommu/dma_remap.cpp` | 8335 | 269 | `plugins/iommu_driver/dma_remap.c` | Q2 | **v0.1 遗漏**（与 ADR-069/073 关联）|
| `src/kernel/iommu/ioasid.cpp` | 2305 | 109 | `plugins/iommu_driver/ioasid.c` | Q2 | **v0.1 遗漏**（IO Address Space ID）|
| `src/kernel/iommu/invalidate.cpp` | 3143 | 109 | `sim_hardware/dma/iommu_hw.cpp` | **Q3** | **v0.1 误写为 `iommu_invalidate.cpp`**，IOTLB flush 硬件模型 |
| `src/kernel/iommu/iommu_internal.h` | 3815 | 115 | `plugins/iommu_driver/include/iommu_internal.h` | Q2 | **v0.1 遗漏**，driver 内部头 |
| `src/kernel/iommu/pcie_integration.cpp` | 3653 | 111 | `plugins/pci_driver/pci_iommu_integration.c` | Q2 | 跨 driver 协作 |
| `src/kernel/iommu/vfio_bridge.cpp` | 1451 | 67 | `plugins/iommu_driver/vfio_bridge.c` | Q2 | **v0.1 遗漏**（Stage 2 交付，ADR-089 §D7 提及）|
| `src/kernel/iommu/vfio_bridge.h` | 333 | 17 | `plugins/iommu_driver/include/vfio_bridge.h` | Q2 | **v0.1 遗漏** |
| **小计（11 文件）** | **35,369** | **1,262** | | | |

**关键修正**（Oracle Blocking #1）：
- `iommu_notifier.cpp` **不存在**（v0.1 引用错误），实际无此文件。mmu_notifier 的实现可能在 `src/kernel/notifier.cpp`（属于 Q1 kernel sim 范围，**不迁移**）
- `iommu_invalidate.cpp` → 实际文件名是 `invalidate.cpp`

**IOMMU 现有测试**（迁移后必须仍 PASS）：
- `tests/test_iommu_emu_standalone.cpp`
- `tests/test_iommu_invalidate_runtime_standalone.cpp`
- `tests/test_iommu_notifier_standalone.cpp`
- `tests/test_iommu_priv_contract_regression_standalone.cpp`

**原则**：**API 实现**（driver，Q2）+ **硬件模型**（sim_hardware，Q3）。

**新文件**（Linux IOMMU 等价）：
- `plugins/iommu_driver/iommu.c` ↔ `drivers/iommu/iommu.c`
- `plugins/iommu_driver/iommu-sva.c` ↔ `drivers/iommu/iommu-sva.c`（Shared Virtual Addressing）
- `plugins/iommu_driver/amd_iommu.c` ↔ `drivers/iommu/amd/iommu.c`（stub）
- `plugins/iommu_driver/intel_iommu.c` ↔ `drivers/iommu/intel/iommu.c`（stub）
- `plugins/iommu_driver/iommufd.c` ↔ `drivers/iommu/iommufd.c`

#### D2.3: sim_hardware/（Q3 新顶级目录）

```
sim_hardware/
├── include/
│   ├── pcie/
│   │   ├── host_bridge.h       # L2: PCIe host bridge / RC mirror
│   │   ├── link_layer.h        # L3: PCIe Link Layer
│   │   ├── phy.h               # L4: PHY digital ctrl
│   │   ├── bypass.h            # PcieBypass Mux（Full/Bypass/Partial）
│   │   ├── completion.h        # L6: Completion tracking
│   │   └── sr_iov.h            # L5: SR-IOV VF pool 仿真
│   ├── cpptlm/
│   │   ├── bridge.h            # C ABI 桥接（封装 cpptlm_emulator.h）
│   │   ├── endpoint.h          # PcieEndpointIP 17-port composition
│   │   ├── axi_adapter.h       # PcieAxiAdapter 包装
│   │   └── tier.h                # 8 tier 运行时选择
│   ├── chipset/
│   │   ├── northbridge.h       # 北桥仿真
│   │   ├── interrupt.h         # IOAPIC / MSI 控制器仿真
│   │   └── topology.h          # PCIe 拓扑描述加载器
│   ├── dma/
│   │   ├── iommu_hw.h          # IOMMU 硬件模型
│   │   └── ats.h               # ATS/PASID 硬件模型
│   └── platform.h              # 平台抽象
├── src/                        # 实现，对称结构
└── topology/                   # 配置
    ├── default_topology.json   # 默认 PC 拓扑
    └── cpptlm_dgpu_v1.json     # CppTLM dGPU profile
```

### D3: sim_hardware/pcie/ 与 plugins/pci_driver/ 的关系

```
┌─────────────────────────────────────────────────────────────┐
│ plugins/pci_driver/ (Q2: Linux driver idioms)             │
│   - pci_probe.c: pci_scan_slot()                         │
│   - pci_setup_bus.c: pci_bus_assign_resources()           │
│   - pci_access.c: pci_user_read_config_byte()             │
│   ──────────────────────────                                │
│   调 sim_hardware API (Q3)                                │
└─────────────┬──────────────────────────────────────────────┘
              │ 调用方接口
              ▼
┌─────────────────────────────────────────────────────────────┐
│ sim_hardware/pcie/ (Q3: 硬件平台仿真)                    │
│   - host_bridge: PcieRootComplexTLM composition           │
│   - link_layer: PcieLinkLayer 包装                         │
│   - bypass: PcieBypassMux 包装                             │
│   ──────────────────────────                                │
│   调 CppTLM / cpptlm_emulator ABI                           │
└─────────────────────────────────────────────────────────────┘
```

**对应真机关系**：
- `drivers/pci/` (真机) = `plugins/pci_driver/` (UsrLinuxEmu)
- `arch/xxx/pci/` (真机) = `sim_hardware/pcie/` (UsrLinuxEmu)
- 驱动调平台 API（**不是反过来**）

### D4: PCIe Bypass 形态对接（关键）

`sim_hardware/pcie/bypass.h` 暴露 3 态模式（封装 CppTLM `PcieBypassMux`）：

```cpp
namespace usr_linux_emu::sim_hardware::pcie {

// Canonical 枚举值（per Oracle Gate D 修正）：与代码 `sim_hardware/include/pcie/bypass.h:8-12` 实测一致
// kBypass=1, kPartial=2 是实现事实，文档/ADR 此前误写为 Partial=1, Bypass=2 — 已修正
enum class BypassMode : uint8_t {
    kFull    = 0,  // PHY + LL + TL + AXI（完整 PCIe 链路，最真）
    kBypass  = 1,  // TL + AXI（直接事务层，快速软件 bring-up）
    kPartial = 2,  // LL + TL + AXI（跳过 PHY，保留 FC + ACK-NAK）
};

// 命名空间级自由函数（代码实测，非 class）：命名空间 `sim_hardware::pcie` 暴露
// bypass_apply_mode(BypassMode, DrainPolicy) + bypass_get_mode() + drain accounting
int  bypass_apply_mode(BypassMode mode, DrainPolicy policy = DrainPolicy::kGracefulDrain);
BypassMode bypass_get_mode(void);
```

**说明（Gate D 修订）**：本 ADR v0.2 §D4 原生采用 `class PcieBypassController` 抽象设计，Stage 5.5.1 实施采用 namespace-scoped free functions（`bypass.cpp:43-80`），是简化版的等价实现。Gate D 验证时若 class 封装需求回归，可由 free functions  + 全局 atomic 重构回 class。

**模式选择策略**（`sim_hardware/topology/default_topology.json`）：
- **L1 (Bypass)**：driver bring-up，最快迭代
- **L2 (Partial)**：driver 功能验证
- **L8 (Full)**：硬件协同验证

**运行时切换**：`bypass_apply_mode()`（`bypass.cpp:44`）实现 DrainPolicy 处理 in-flight TLP 的阻塞/超时逻辑（10 步清理，对齐 CppTLM `PcieBypassMux`）。

### D5: 8 tier PCIe 仿真分层

**v0.2 修正**（Oracle Finding #4）：原文"9 tier"实际只定义了 8 个 PCIe tier（L1-L8）。**统一命名为 "8 tier PCIe 仿真"**，VFIO 单独列出（不属于 PCIe tier，是 PCIe 之上的用户态 API）。

每个 tier 独立可交付、独立可测试，**互不破坏**（append-only）：

| Tier | 名称 | CppTLM 组件 | 真机对应 | 行数估算 | Stage |
|------|------|-------------|----------|--------:|-------|
| **L1** | Software Bypass | `HostBypassTLM` (Phase 7) | `drivers/pci/access.c` 简化模式 | 400-600 | 5.5.2 |
| **L2** | Root Complex Mirror | `PcieRootComplexTLM` (Phase 7) | `drivers/pci/probe.c` | 600-800 | 5.5.2 |
| **L3** | Link Layer | `PcieLinkLayer` (Phase 1) | PCIe Link Layer（部分） | 200-400 | 5.5.3 |
| **L4** | PHY Digital | `PciePhyDigitalCtrl` (Phase 2) | PHY 数字控制 + Rate Switch | 200-300 | 5.5.4 |
| **L5** | SR-IOV | `PcieEndpointIP` (Phase 4, 17-port) | `drivers/pci/iov.c` + `drivers/vfio/` | 800-1000 | 5.5.3 |
| **L6** | Completion Tracking | `CompletionTracker` (Phase 4) | NP↔CplD 关联 | 200-300 | 5.5.3 |
| **L7** | AXI Adapter | `PcieAxiAdapter` (Phase 5) | SoC AXI 边界 | 400-600 | 5.5.4 |
| **L8** | AXI Mapper OOO | `Axi4Mapper` (Phase 6) | outstanding tracking | 300-500 | 5.5.5 |

**累计 sim_hardware/pcie/ 行数**：~3,100-4,500 行

**Bypass mode 与 tier 编号严格区分**（Oracle Finding #4 修正）：
- **BypassMode**（Full/Partial/Bypass）是 `sim_hardware/pcie/bypass.h` 暴露的 3 态枚举，与 tier 编号**正交**
- **Tier 编号**（L1-L8）是 8 个独立可交付的功能层
- 默认模式选择：Bypass mode **按 tier 而定**（L1=Bypass mode、L8=Full mode 等），不是 L1 数值对应 BypassMode

### D6: 插件加载顺序（依赖图）

新架构下插件有**显式依赖**。

**现有 module struct**（`include/kernel/module_loader.h`，v0.2 实测）：
```c
typedef struct module {
  const char* name;       // 插件名称
  const char** depends;   // 依赖项列表（NULL 结尾）—— **字段名是 depends，非 deps**
  int (*init)(void);      // 初始化函数
  void (*exit)(void);     // 卸载函数
} module;
```

**v0.2 修正**（Oracle Blocking #3）：原 v0.1 示例中 `.deps` / `.load_priority` 字段与现有代码不符。**正确做法是 Stage 5.5.1 内**：
1. 保留 `depends` 字段名（不动 ABI 兼容已有插件）
2. **新增** `load_priority` 字段（uint32_t，0=默认，按拓扑排序权重）
3. 字段顺序：`name → load_priority（新）→ depends → init → exit`
4. **这是 struct module 的 ABI 变更**，所有现有插件必须重编译（Stage 5.5.1 工作项）

```c
// Stage 5.5.1 后的目标 module struct（新增字段）
typedef struct module {
  const char* name;
  uint32_t    load_priority;   // 新增（0 = 默认）
  const char** depends;
  int (*init)(void);
  void (*exit)(void);
} module;
```

**插件注册示例**（Stage 5.5.1 后）：

```cpp
// plugins/pci_driver/plugin.cpp
extern "C" {
    module mod = {
        .name = "pci_driver",
        .load_priority = 100,
        .depends = (const char*[]){"sim_hardware_pcie", nullptr},
        .init = pci_driver_init,
        .exit = pci_driver_exit,
    };
}

// plugins/gpu_driver/plugin.cpp
extern "C" {
    module mod = {
        .name = "gpu_driver",
        .load_priority = 300,
        .depends = (const char*[]){"pci_driver", "iommu_driver", nullptr},
        .init = gpu_driver_init,
        .exit = gpu_driver_exit,
    };
}

// plugins/vfio_driver/plugin.cpp（Stage 5.5.5 新增）
extern "C" {
    module mod = {
        .name = "vfio_driver",
        .load_priority = 250,
        .depends = (const char*[]){"pci_driver", "iommu_driver", nullptr},
        .init = vfio_driver_init,
        .exit = vfio_driver_exit,
    };
}
```

**加载流程**（依赖图）：
```
sim_hardware_pcie (Q3, 启动时由 sim_hardware_init 加载)
       ↓
pci_driver → iommu_driver → vfio_driver (Stage 5.5.5)
       ↓
gpu_driver (load_priority=300，最后加载，依赖 pci+iommu)
       ↓
net_driver / storage_driver (现有，无 PCI 依赖)
```

**ModuleLoader 必须升级**为拓扑排序加载（**Stage 5.5.1 内实施**）。

### D7: 关键不破坏承诺

| 现有架构契约 | 处理 |
|--------------|------|
| **HAL 68 fn-ptrs append-only**（ADR-023）| ✅ 不动 |
| **3 区分原则**（ADR-036）| ✅ **扩展**为 4 象限（后续 ADR-036 更新） |
| **23 ABI 边界**（ADR-088 §D5）| ✅ 通过 `cpptlm_emulator.h` C ABI 桥接，**不动** |
| **HAL 65 → 68 fn-ptrs**（ADR-076 追加 kernel_module_*）| ✅ 不动 |
| **HAL IOMMU ops**（ADR-061）| ✅ 不动；IOMMU impl 拆到 plugin |
| **BAR/ioremap 仿真**（ADR-069）| ✅ **不动**（已在 Stage 4.1 实施）；iommu `dma_remap.cpp` 迁移不触及 BAR 仿真语义 |
| **DMA coherent emulation**（ADR-073）| ✅ **不动**（已在 Stage 4.1 实施）；iommu `dma_remap.cpp` 迁移不触及 DMA coherent 语义 |
| **kernel_workqueue**（ADR-060）| ✅ 不动 |
| **cpptlm_emulator ABI version 2**（ADR-090 v2）| ✅ 不动 |
| **Linux 6.12 LTS L2 build**（ADR-072）| ✅ **扩展**：pci_driver/ + iommu_driver/ + vfio_driver/（Stage 5.5.5）也加入 L2 build 目标 |
| **`drv/` 代码零修改** | ✅ **强化**：现在 `pci_driver/` + `iommu_driver/` + `vfio_driver/` 也是零修改目标 |
| **struct module ABI**（ModuleLoader）| ⚠️ **Stage 5.5.1 内 ABI 变更**：新增 `load_priority` 字段（详见 §D6），所有现有插件必须重编译 |

### D8: ADR-088 `src/system_hw/` 概念变更

ADR-088 §C2 提出 `src/system_hw/` 概念，ADR-089 v0.5 实施时已部分采纳。**本 ADR 正式将 location 从 `src/system_hw/` 改为顶级目录 `sim_hardware/`**：

| 旧路径（ADR-088/089） | 新路径（本 ADR）| 原因 |
|-----------------------|----------------|------|
| `src/system_hw/iommu/` | `plugins/iommu_driver/` + `sim_hardware/dma/iommu_hw/` | API 实现归 Q2 driver；硬件模型归 Q3 sim_hardware |
| `src/system_hw/cxl_memdev/` | `sim_hardware/chipset/cxl_memdev/` | 纯硬件模型，归 Q3 |
| ❌ 无 | `sim_hardware/pcie/` | 新增（**8 tier PCIe 仿真**）|
| ❌ 无 | `sim_hardware/cpptlm/` | 新增（CppTLM 桥接）|
| ❌ 无 | `sim_hardware/chipset/` | 新增（北桥/中断控制器）|

**ADR-089 v0.5 同步更新**：location 引用从 `src/system_hw/` → `sim_hardware/`。

---

## Consequences

### Co1: 现有代码迁移

| 迁移项 | LOC | 工作量 |
|--------|----:|--------|
| `src/kernel/pcie/` → `plugins/pci_driver/` | 648 | 1-2 周 |
| `src/kernel/iommu/` → `plugins/iommu_driver/` + `sim_hardware/dma/` | 1262 | 2-3 周 |
| `include/kernel/pcie*/` → `plugins/pci_driver/include/` | 118 | 并入 |
| **现有代码迁移小计** | **~2,000** | **4-6 周** |
| `sim_hardware/` 新增（8 tier PCIe + cpptlm + chipset） | ~3,500 | 24-34 周（Wave 2-5） |
| VFIO 新增（`plugins/vfio_driver/`） | ~1,500 | 8-12 周（Wave 5） |
| **总新增** | **~5,000** | **32-46 周** |
| **总计** | **~7,000** | **36-52 周**（约 9-12 月） |

### Co2: ADR 联动更新

| ADR | 更新 |
|-----|------|
| ADR-036 | 3 区分 → 4 象限（① + ② + ③a PC sim + ③b GPU sim）|
| ADR-089 | location 引用 `src/system_hw/` → `sim_hardware/` |
| ADR-072 | L2 build 目标集扩展（pci_driver/ + iommu_driver/ 加入）|

### Co3: 与 CppTLM 集成的清晰边界

通过 `sim_hardware/cpptlm/bridge.cpp` 单一入口封装 `cpptlm_emulator.h`（19 forward 函数 + 4 callback），**严格遵守 23 ABI 边界**。

### Co4: 实施依赖

| 依赖项 | 状态 |
|--------|------|
| CppTLM v0.5 MVP Release Gate | ⏳ 进行中（最新 commit `ca7040a` 2026-08-31）|
| CppTLM Phase 7（HostBypassTLM + PcieRootComplexTLM）| ✅ 已交付 |
| CppTLM Phase 4-6（SR-IOV + AXI）| ✅ 已交付 |
| UsrLinuxEmu ADR-090 v2 | ✅ Accepted（依赖本 ADR 的 PCIe bus）|

---

## Alternatives Considered

### A1: 维持 `src/kernel/pcie/` 不动

**否定理由**：违背真机 Linux 内核目录约定，破坏"drivers/pci/ 可移植到真机"承诺。L1+ PCIe 仿真无法落地。

### A2: 把 sim_hardware/ 放在 `src/sim_hardware/` 而非顶级目录

**否定理由**：ADR-088 原文是 `src/system_hw/`（在 src 下），但 sim_hardware 与 kernel sim 同级（都是 platform 性质），**顶级目录**更清晰表达"PC platform 仿真"的独立地位。

### A3: 把 iommu 完全搬到 plugin

**否定理由**：IOMMU framework（API 实现）是 kernel sim 性质（Q1），IOMMU driver（vendor 私有）是 plugin 性质（Q2）。**必须拆分**为两部分。

### A4: 跳过 L4（PHY）和 L8（AXI Mapper），只做 L1+L2+L3+L5+L6

**否定理由**：L4+L8 是真机 PCIe 协议栈的边角特性，**跳过会导致"真机一样驱动栈"承诺有缺口**。但**可作为 P3 延后**。

---

## Acceptance Criteria（验收标准）

### AC1: 现有代码迁移（Stage 1）

- [ ] `src/kernel/pcie/` 全部文件迁移到 `plugins/pci_driver/`
- [ ] `src/kernel/iommu/` 拆分为 `plugins/iommu_driver/` + `sim_hardware/dma/`
- [ ] 现有 130+ ctest 全部 PASS（**0 regression**）
- [ ] ModuleLoader 支持 depends 拓扑排序（含新 load_priority 字段，ABI 变更）
- [ ] ADR-036 更新到 4 象限版本
- [ ] ADR-089 location 引用更新到 `sim_hardware/`
- [ ] ADR-072 L2 build 目标扩展（pci_driver/ + iommu_driver/）

### AC2: sim_hardware 基础（Stage 2）

- [ ] `sim_hardware/` 顶级目录建立 + CMakeLists
- [ ] `sim_hardware/cpptlm/bridge.cpp` 封装 `cpptlm_emulator.h`
- [ ] L1（Software Bypass via HostBypassTLM）实施
- [ ] L2（Root Complex Mirror via PcieRootComplexTLM）实施
- [ ] `plugins/pci_driver/` 调 sim_hardware 完成 Linux pci_scan_slot 行为
- [ ] 8 个新增 standalone Catch2 测试 PASS
- [ ] **首次验证：模拟 host 启动后枚举到 GPU 设备**

### AC3: 真机一致性（Stage 5）

- [ ] ADR-072 L2 build：plugins/pci_driver/ 在 Linux 6.12 真机编译通过
- [ ] 真机 PCIe 设备扫描与 UsrLinuxEmu 一致

---

## Open Questions

### OQ1: vfio_driver 何时启动？（v0.2 关闭）

- **候选 A**：Wave 2 与 L2 一起启动（早期集成）
- **候选 B**：Wave 5（Stage 5.5+）单独启动
- **决议**：**B**（VFIO 强依赖 PCI + IOMMU，地基就绪后再上）
- **理由**：ADR-091 §D6 依赖图与路线图 Stage 5.5.5 已将 VFIO 排在最后，OQ1 已通过路线图决策关闭

### OQ2: sim_hardware/topology.json 配置格式

- **候选 A**：完全自创 JSON schema
- **候选 B**：受 CppTLM `configs/dgpu_soc_v1.json.in` 启发
- **候选 C**：参考 Linux ACPI 表（MCFG / DSDT / CRAT）
- **倾向**：**B**（与上游一致，未来易扩展）

### OQ3: AMD/Intel vendor-private iommu driver stub 优先级

- **候选 A**：Stage 5.5.1 仅 framework，vendor stub 延后
- **候选 B**：Stage 5.5.1 即含 amd_iommu + intel_iommu stub
- **倾向**：**A**（YAGNI 原则，vendor 私有不阻塞主线）

### OQ4 (v0.2 新增): ctest 基线数字

- **背景**：原文"130+ ctest"未验证。实测 `tests/CMakeLists.txt` 中 `add_test` 仅 46 处，但测试源文件 152 个，README 称 98 binaries
- **决议**：以 `ctest -N` 实际输出为基准。Gate 5.5.1-A 修订为"**现有 ctest 全 PASS（基线 = `ctest -N | wc -l`，建立 baseline 数字**）"

### OQ5 (v0.2 新增): vfio_bridge.cpp 归属

- **背景**：Stage 2 交付的 `vfio_bridge.cpp/h`（67 + 17 LOC）与 `iommu_emu_state.cpp` 同目录
- **候选 A**：迁往 `plugins/iommu_driver/vfio_bridge.c`（作为 iommu driver 的辅助）
- **候选 B**：迁往 `plugins/vfio_driver/vfio_bridge.c`（作为 vfio driver 前身，Stage 5.5.5 时并入）
- **决议**：**A**（更早可用，且 vfio_bridge 是 iommu API 的辅助而非 vfio 核心）

### OQ6 (v0.2 新增): `iommu_internal.h` 处理

- **背景**：`tests/test_iommu_*` 引用此头（内部头）。迁移后该头在 `plugins/iommu_driver/include/`
- **候选 A**：保留为 plugin 内部头，测试改为引用 plugin 公共 API（更隔离）
- **候选 B**：保留为 plugin 内部头，测试仍引用（破坏隔离）
- **倾向**：**A**（正确做法，Stage 5.5.1 内修测试）

---

## Revision History

- **v0.2** (2026-09-03, Proposed)：Oracle v0.1 复审 INCONCLUSIVE 修复
  - §D2.1：补 `pcie_emu_impl.h`（177 LOC）+ 锁定 PCIe 5 文件实测 LOC（648）
  - §D2.2：补 7 个遗漏文件（ats_protocol/dma_remap/ioasid/invalidate/iommu_internal.h/vfio_bridge.cpp+h）+ 修正 `invalidate.cpp` 文件名 + 删不存在的 `iommu_notifier.cpp` + 锁定 IOMMU 11 文件实测 LOC（1,262）
  - §D6：修正 module struct 字段名 `deps` → `depends`，新增 `load_priority` 字段标注 + 说明 ABI 变更 + 依赖图补 `vfio_driver`
  - §D7：补 ADR-069（BAR/ioremap）+ ADR-073（DMA coherent）
  - §D5：9 tier → 8 tier 命名统一（VFIO 单独列出）+ 严格区分 BypassMode 与 tier 编号
  - §C2/Q4：图笔误 `plugins/gpu/sim/` → `plugins/gpu_driver/sim/`
  - OQ1：关闭（VFIO 时机已通过路线图决策）
  - 新增 OQ4（ctest 基线）+ OQ5（vfio_bridge 归属）+ OQ6（iommu_internal.h 处理）
  - 测试文件迁移清单：补 6 个 test_*_standalone 的去向说明

- **v0.2 (Accepted)** (2026-09-03, Stage 5.5.1 实施升档)：4 commits ship + 151/151 ctest PASS
  - 实施 commit: `8c4ee2f`（Wave 1A+1B PCI/IOMMU 迁移）/ `485de1e`（Wave 1C sim_hardware 骨架）/ `dd70988`（Wave 1D ModuleLoader toposort）/ `1db07d1`（Wave 1E ADR + L2 build）
  - Gate 验证：5.5.1-A (151/151 ctest PASS) / 5.5.1-B (test_moduleloader_toposort_standalone 5 测试场景 PASS) / 5.5.1-C (tools/l2-build/build_*.sh 创建) / 5.5.1-D (Oracle 实施后复审 — Gate D 待触发)
  - OpenSpec change [2026-09-03-pci-driver-refactor](../../changes/2026-09-03-pci-driver-refactor/) 状态：126/126 tasks 已 ship，3 new tests + Gate A/B/C 验证全部通过
  - 关联 ADR 升档：[ADR-036 v0.2](adr-036-three-way-separation.md)（3 区分 → 4 象限）/ [ADR-072 v0.2](adr-072-portability-validation.md)（L2 build 扩展 pci_driver + iommu_driver）/ [ADR-089 v0.6](adr-089-v55-system-hw-simulation.md)（location `src/system_hw/` → `sim_hardware/`）

- **v0.1** (2026-09-03, Proposed)：初版
  - 4 象限目录布局
  - PCI/VFIO/IOMMU 迁移映射
  - sim_hardware/ 顶级目录
  - 9 tier PCIe 仿真分层（v0.2 修正为 8 tier）
  - Bypass 形态对接
  - 插件加载顺序
  - 依赖与验收标准

---

## Related Documents

- [docs/02_architecture/four-quadrant-architecture.md](../02_architecture/four-quadrant-architecture.md) — 4 象限详细说明
- [docs/02_architecture/core-architecture.md](../02_architecture/core-architecture.md) — 重构后架构 SSOT（v0.1.7）
- [docs/roadmap/pcie-bus-bridge-roadmap.md](../roadmap/pcie-bus-bridge-roadmap.md) — 实施路线图（4 Wave + Stage）
- [ADR-036](adr-036-three-way-separation.md) — 3 区分（待更新为 4 象限）
- [ADR-089](adr-089-v55-system-hw-simulation.md) — v5.5+ 系统硬件（待更新 location）

---

**状态**: ✅ **Accepted v0.2**（2026-09-03 Oracle v0.2 复审 PASS — 2 处单行残留已修，3 份文档完全一致；可起草 Change-1）
