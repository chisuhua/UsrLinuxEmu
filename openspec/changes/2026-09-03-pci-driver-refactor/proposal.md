# pci-driver-refactor: 4 象限重构（Stage 5.5.1 — 不改功能，仅迁移）

> **状态**: 🔄 **Proposed v0.3**（2026-09-03，Oracle v0.2 复审 INCONCLUSIVE 修复完成，待 v0.3 复审 + Metis 复审）
> **日期**: 2026-09-03
> **关联 ADR**: [ADR-091](../00_adr/adr-091-pci-driver-architecture-and-four-quadrant.md) ✅ Accepted v0.2
> **关联文档**: [4 象限架构文档](../02_architecture/four-quadrant-architecture.md) v0.2 + [实施路线图](../roadmap/pcie-bus-bridge-roadmap.md) v0.2
> **Stage**: 5.5.1（4 象限重构，不改功能）
> **工期**: 4-6 周
> **Owner**: UsrLinuxEmu Architecture Team
> **关联 Gate**: Gate 5.5.1-A（ctest 全 PASS）/ 5.5.1-B（ModuleLoader 拓扑排序）/ 5.5.1-C（L2 build 通过）

---

## Why

UsrLinuxEmu 经过 4 阶段演进，`src/kernel/` 出现**架构性问题**：既包含 VFS / ModuleLoader 等 Linux kernel env sim，又混入 PcieEmu 设备模型（~648 LOC）和 iommu_emu（~1,262 LOC）。真机 Linux 内核中，`drivers/pci/` 和 `drivers/iommu/` 都是 `drivers/` 下的子系统，**不属于 kernel core**。

当前问题：
1. **`src/kernel/pcie/` 错位**：是 PCI subsystem driver，不是 kernel sim
2. **`src/kernel/iommu/` 错位**：是 iommu framework + driver，不是 kernel sim
3. **真机对齐不足**：ADR-091 §C1.2 列出 7 处错位 / 缺失
4. **ADR-088/089 提出的 `src/system_hw/` 概念不精确**：与 CppTLM 桥接边界模糊
5. **缺 8 tier PCIe bus 仿真**：无法对接 CppTLM Phase 0-7 14 个组件

**触发条件**：用户 2026-09-03 提案 + ADR-091 ✅ Accepted v0.2

## What Changes

### 1. 新建 `plugins/pci_driver/` 目录

- 从 `src/kernel/pcie/`（5 文件，648 LOC）整体迁移至 `plugins/pci_driver/`
- 文件清单（实测）：
  - `pcie_emu.cpp` (156 LOC) → `plugins/pci_driver/probe.cpp`（**v0.2 保留 .cpp 扩展名**，因含 namespace/class，C 化显式推迟）
  - `pcie_emu_impl.h` (177 LOC) → `plugins/pci_driver/include/pcie_emu_impl.h`
  - `config_space.cpp` (80 LOC) → `plugins/pci_driver/pci_access.cpp`
  - `msi_x.cpp` (88 LOC) → `plugins/pci_driver/pci_msi.cpp`
  - `capability_walk.cpp` (147 LOC) → `plugins/pci_driver/pci_cap.cpp`
  - 公共头：`include/kernel/pcie/pcie_emu.h` (90 LOC) → `plugins/pci_driver/include/pcie_emu.h`
  - 公共头：`include/kernel/pcie_device.h` (28 LOC) → `plugins/pci_driver/include/pci_device.h`
- 命名空间：`usr_linux_emu::pcie_internal` → `usr_linux_emu::pci`

### 2. 新建 `plugins/iommu_driver/` 目录

- 从 `src/kernel/iommu/`（11 文件，1,262 LOC）拆分迁移至 `plugins/iommu_driver/`
- 文件清单（实测）：
  - `iommu_emu_state.cpp` (159 LOC) → `iommu.cpp`
  - `iommu_group.cpp` (158 LOC) → `iommu_group.cpp`
  - `iommu_domain.cpp` (75 LOC) → `iommu_domain.cpp`
  - `ats_protocol.cpp` (73 LOC) → `ats_protocol.cpp`
  - `dma_remap.cpp` (269 LOC) → `dma_remap.cpp`
  - `ioasid.cpp` (109 LOC) → `ioasid.cpp`
  - `pcie_integration.cpp` (111 LOC) → `plugins/pci_driver/pci_iommu_integration.cpp`
  - `vfio_bridge.cpp` (67 LOC) → `vfio_bridge.cpp`
  - `vfio_bridge.h` (17 LOC) → `plugins/iommu_driver/include/vfio_bridge.h`
  - 内部头：`iommu_internal.h` (115 LOC) → `plugins/iommu_driver/include/iommu_internal.h`
- **v0.2 修订**：硬件模型 `invalidate.cpp` (109 LOC) 暂留 `plugins/iommu_driver/invalidate.cpp`（Q2 内部），**不**迁入 sim_hardware/dma/。理由：`invalidate.cpp` include `kernel/uvm/mmu_notifier_internal.h` + `kernel/uvm/mm_shim.h`（Q1 kernel sim），是 registration stub + dispatch 桥，不是纯硬件模型；Q2/Q3 归类决策推迟到 Change-2 (Stage 5.5.2) 显式裁决

### 3. 新建 `sim_hardware/` 顶级目录骨架

**v0.2 目录模型统一（CRITICAL fix）**：sim_hardware 采用**顶级目录模型**（与 ADR-091 §D2.3 + four-quadrant-architecture.md §4.2 SSOT 一致），内含 `include/` + `src/` + `topology/` 三个子目录。

```
sim_hardware/                           # 顶级目录（per ADR-091）
├── CMakeLists.txt                      # 新增
├── include/                            # Q3 公共 API 头
│   ├── pcie/bypass.h                   # 占位（enum-only，无类方法）
│   ├── pcie/host_bridge.h              # 占位
│   ├── cpptlm/bridge.h                 # 占位
│   ├── dma/iommu_hw.h                  # 占位（Stage 5.5.2 实施）
│   └── platform.h                      # 占位
├── src/                                # Q3 实现
│   └── （Stage 5.5.1 全部空，仅占位文件）
└── topology/                           # PC 拓扑配置
    └── default_topology.json           # 占位
```

- Stage 5.5.1 内**仅创建空目录骨架 + 占位头文件**（**不实现**任何 tier 仿真）
- `sim_hardware/dma/iommu_hw.cpp` 的 `invalidate.cpp` 迁移**已推迟到 Change-2**（见 §2 v0.2 修订）
- sim_hardware 在 5.5.1 为**静态库**（per task 4.4），是否升级为独立插件由 ADR-091 Open Questions 裁决（推迟 Change-2 决定）

### 4. ModuleLoader 升级

- struct module 新增字段：`uint32_t load_priority`（0=默认）
- 字段顺序：`name → load_priority（新）→ depends → init → exit`
- 新增拓扑排序算法（基于 `depends` 字段 + `load_priority` 权重）
- **这是 struct module 的 ABI 变更**，所有现有插件必须重编译
- Stage 5.5.1 工作项 4

### 5. 新建测试

- `tests/plugins/test_pci_driver_standalone.cpp`：PCI driver 基本功能测试
- `tests/plugins/test_iommu_driver_standalone.cpp`：IOMMU driver 基本功能测试
- 现有 6 个 test_*_standalone 保持路径不变（API 兼容）

### 6. 修改文档

- ADR-036 v0.2：3 区分 → 4 象限升级
- ADR-089 v0.6：location 引用 `src/system_hw/` → `sim_hardware/`
- ADR-072 v0.2：L2 build 目标集扩展（pci_driver/ + iommu_driver/）

## Capabilities

### New Capabilities

- `pci-driver-plugin`: Q2 portable driver for PCI subsystem（迁移自 src/kernel/pcie/）
- `iommu-driver-plugin`: Q2 portable driver for IOMMU subsystem（迁移自 src/kernel/iommu/，部分）
- `sim-hardware-skeleton`: Q3 PC system hardware simulation 顶级目录骨架
- `moduleloader-toposort`: ModuleLoader 拓扑排序 + load_priority 支持

### Modified Capabilities

- `module-loader-abi`: struct module 新增 `load_priority` 字段（**ABI 变更**，所有现有插件必须重编译）

## Impact

- **代码迁移**：~2,028 LOC（648 PCI + 1,262 IOMMU + 118 头文件）
- **新增骨架**：sim_hardware/ 顶级目录骨架（< 200 LOC）
- **新增测试**：2 个独立 Catch2 binary
- **构建系统**：3 个新 CMakeLists.txt（plugins/pci_driver/, plugins/iommu_driver/, sim_hardware/）
- **文档变更**：3 个 ADR 升级（036/089/072）
- **HAL 影响**：**无**（per ADR-023 append-only）
- **下游解锁**：Change-2（Stage 5.5.2 Tier 1+2 实施）
- **风险**：
  1. 迁移映射表准确性（ADR-091 v0.2 已修复）
  2. struct module ABI 变更 → 所有插件重编译
  3. 测试引用内部头文件 → Stage 5.5.1 内修测试

## Tests

### Gate 5.5.1-A：现有 ctest 全 PASS（基线 = `ctest -N | wc -l`）

- 148 测试源文件 + 46 add_test 全部 PASS
- 现有 6 个 test_pcie_* + test_iommu_* 必须 0 regression
- 关键：迁移后 PcieEmu API 兼容（公共头文件名 + 命名空间双重保证）

### Gate 5.5.1-B：ModuleLoader 拓扑排序工作

- 新增 `tests/test_moduleloader_toposort_standalone.cpp`
- 测试场景：
  - 单 plugin 无依赖 → load_priority 决定顺序
  - 多 plugin 链式依赖 → depends 数组决定顺序
  - 循环依赖检测 → 返回错误
  - load_priority 冲突时按 depends 排序
  - 现有 5 个 plugin（gpu/net/storage/sample_*）重编译后仍可加载

### Gate 5.5.1-C：L2 build 通过（即使 stub 也行）

- `plugins/pci_driver/` 在 Linux 6.12 真机编译通过（替换 #include 路径）
- `plugins/iommu_driver/` 在 Linux 6.12 真机编译通过
- per ADR-072 v0.2

## Out of Scope

- ❌ sim_hardware/pcie/ 实际 tier 仿真（Stage 5.5.2）
- ❌ sim_hardware/cpptlm/ 实际桥接（Stage 5.5.2）
- ❌ Stage 5.5.3+ Link Layer / SR-IOV 等（后续 Change）
- ❌ VFIO driver 创建（Stage 5.5.5，单独 Change）
- ❌ iommu vendor stub（amd_iommu/intel_iommu，延后）

## 关联 ADR / Change

- **前置**：[ADR-091](../00_adr/adr-091-pci-driver-architecture-and-four-quadrant.md) ✅ Accepted v0.2
- **关联**：[ADR-023](../00_adr/adr-023-hal-interface.md) ✅（HAL 68 fn-ptrs 不动）
- **关联**：[ADR-036](../00_adr/adr-036-three-way-separation.md) ✅（本 Change 升级到 4 象限）
- **关联**：[ADR-072](../00_adr/adr-072-portability-validation.md) ✅（L2 build 扩展）
- **关联**：[ADR-089](../00_adr/adr-089-v55-system-hw-simulation.md) ✅（location 更新）
- **下游**：[Change-2 Stage 5.5.2](../2026-XX-XX-sim-hardware-foundation-tier1-tier2/)（Tier 1+2 实施）

---

**状态**: 🔄 Proposed v0.3（2026-09-03，Oracle v0.2 复审 INCONCLUSIVE 修复完成，待 v0.3 复审 + Metis 复审）

---

## 修订记录

- **v0.3** (2026-09-03)：Oracle v0.2 复审 INCONCLUSIVE 修复（4 项 BLOCKING 残留）
  - §1 + §2：所有 `.c` 扩展名 → `.cpp`（C++ 内容禁止 C 编译）
  - §2 invalidate.cpp 修订：暂留 Q2（Q2/Q3 拆分决策推迟 Change-2）
  - §3 sim_hardware 修订：统一为顶级目录模型（per ADR-091 §D2.3 SSOT）
- **v0.2** (2026-09-03)：Oracle + Metis 双审查 v0.1 INCONCLUSIVE 修复（仅 proposal.md）
- **v0.1** (2026-09-03)：初版
