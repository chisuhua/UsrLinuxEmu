# sim-hardware-foundation-tier1-tier2: sim_hardware 基础 + Tier 1+2 PCIe 仿真（mock-first，无外部 CppTLM 依赖）

> **状态**: 🔄 **Proposed v0.4**（2026-09-03，Metis 复审 A-01~A-14 + G-01~G-10 修复版；待 Oracle + Metis 双审查）
> **日期**: 2026-09-03
> **版本**: v0.4（从 v0.3 修订：消除虚构 CppTLM 依赖、新增 Phase 0 hard gate、mock-first、M2a/M2b/M2c 拆分、28-task 分解）
> **关联 ADR**: [ADR-091](../00_adr/adr-091-pci-driver-architecture-and-four-quadrant.md) ✅ Accepted v0.2
> **关联文档**: [4 象限架构文档](../02_architecture/four-quadrant-architecture.md) v0.2 + [实施路线图](../roadmap/pcie-bus-bridge-roadmap.md) v0.2 + [CppTLM v4 实施 Handoff](../05-advanced/cpptlm-v4-implementation-handoff.md) v4.0（Draft，**未验证/未合并**）
> **前置 Change**: [Change-1 pci-driver-refactor](../../archive/2026-09-03-2026-09-03-pci-driver-refactor/) ✅ Shipped（4 commits + 151/151 ctest PASS，已归档）
> **Stage**: 5.5.2
> **工期**: 4-6 周（Phase 0 hard gate 通过后）
> **Owner**: UsrLinuxEmu Architecture Team

---

## Current Facts（当前事实，v0.4 权威基线）

> 以下事实已逐一核对仓库现状（2026-09-03）。**任何与这些事实冲突的旧描述一律以本节为准。**

| # | 事实 | 状态 |
|---|------|------|
| F1 | 本仓库（UsrLinuxEmu）**不含任何 CppTLM 头文件、库、符号或 submodule**；`external/` 下仅有 `json/nlohmann/json.hpp` | ✅ 实测 |
| F2 | CppTLM 的 23 ABI（ADR-088 §D5 / cpptlm-v4-handoff 附录 A）**仍为 Draft for CppTLM Review**，无 CppTLM maintainer ack，无 release tag，无本地验证 | ✅ 实测（docs 声明 Draft）|
| F3 | `sim_hardware/` 全部 7 个实现文件（bridge/endpoint/host_bridge/bypass/topology/platform）均为 Wave 1C 占位（`-ENOSYS` / no-op / 不解析 JSON）| ✅ 实测 |
| F4 | `sim_hardware/CMakeLists.txt` 当前是 `add_library(sim_hardware INTERFACE)`，**未链接任何 cpptlm target**（cpptlm_core 链接行被注释）| ✅ 实测 |
| F5 | `sim_hardware/topology/default_topology.json` 当前是**截断的非法 JSON**（末尾 `"prefetcha` 被截断，无法解析）| ✅ 实测 |
| F6 | 当前真实 PCI 文件是 `plugins/pci_driver/probe.cpp`（**不是** `pci_probe.cpp`）；`pci_setup_bus.cpp` / `pcie_enable_device.cpp` **不存在**，本 Change 才创建 | ✅ 实测 |
| F7 | Change-1 已归档：路径为 `openspec/changes/archive/2026-09-03-2026-09-03-pci-driver-refactor/` | ✅ 实测 |
| F8 | 当前 ctest 基线 151/151 PASS（Change-1 归档记录）；新增测试后基线会增长 | ✅ 实测（ADR-091 v0.2）|
| F9 | 本 Change 是**计划文档**（Change-2），不修改任何 sim_hardware / plugins / tests / src / docs 应用代码 | ✅ 本任务约束 |

## Why

Change-1 完成 4 象限重构（`src/kernel/pcie/` → `plugins/pci_driver/`，`src/kernel/iommu/` → `plugins/iommu_driver/`，`sim_hardware/` 顶级目录骨架），但 `sim_hardware/` 仅含占位头文件与 `-ENOSYS` 实现，**未实现任何 tier 仿真**。

当前缺口（F3/F5/F6 实测）：
1. **无 Tier 1 (Software Bypass)**：`host_bridge_bypass_read/write` 返回 `-ENOSYS`，无 BAR bypass 通路
2. **无 Tier 2 (Root Complex Mirror)**：`host_bridge_enumerate` 返回 `-ENOSYS`，模拟 host 启动后无法枚举任何 PCIe 设备
3. **无 CpptlmBridge 真实实现**：`init/mmio_read/write/config_read/write/attach_endpoint` 全部 `-ENOSYS`
4. **无 topology JSON 解析**：`topology_load_json` 不真解析 JSON；默认 topology JSON 非法（F5）
5. **无端到端验证**：模拟 host 启动后不能枚举到任何（mock）GPU 设备

**关键澄清（v0.4）**：真实 CppTLM（23 ABI）当前**不存在且未验证**（F1/F2）。因此本 Change **不实施、不验证、不虚构** 任何真实 CppTLM 桥接；本 Change 交付的是**无外部依赖、可在当前仓库独立构建与测试**的 mock-first 基础，真实 CppTLM 对接是 **gated follow-up**（见 §CppTLM 状态与 Gate）。

**触发条件**：
- Change-1 完成 ✅（F7）
- **Phase 0 hard gate**：本 Change 首个 Gate（见 §Phase 0 Hard Gate），验证仓库现状（F1-F6）与设计契约一致
- 用户 2026-09-03 提案要求"PC 系统硬件模拟用于和 CppTLM 端的 dGPU 对接"（**对接本身在真实 CppTLM 可用后的 follow-up**）

---

## Phase 0 Hard Gate（v0.4 新增）

> 本 Change 的**第一个硬性验收 Gate**，在 Phase 1 实施开始前必须通过。用于在计划层面固化仓库现状，防止"把 mock 验证描述成真实 CppTLM 验证"。

| Gate | 检查项 | 通过标准 |
|------|--------|---------|
| P0-G1 | `sim_hardware/CMakeLists.txt` 是 INTERFACE；无 cpptlm target 链接 | grep 确认 `add_library(sim_hardware INTERFACE)` 且 `target_link_libraries(sim_hardware INTERFACE cpptlm_core)` 被注释或不存在 |
| P0-G2 | 仓库无 CppTLM 头/库/符号 | `find . -name '*cpptlm*'` 仅命中 `sim_hardware/` 自身文件，无 `external/` 或系统 include 中的 cpptlm_emulator.h |
| P0-G3 | `default_topology.json` 当前非法 | 运行 `nlohmann::json::parse`（或 `python3 -m json.tool`）失败；**不得声称已修复**（修复是 T2.1 任务） |
| P0-G4 | 真实 PCI 边界文件为 `plugins/pci_driver/probe.cpp` | 文件存在；`pci_setup_bus.cpp` / `pcie_enable_device.cpp` 不存在 |
| P0-G5 | 本 Change 不改任何 sim_hardware / plugins / tests / src / docs 文件 | `git diff --stat` 仅含 `openspec/changes/2026-09-03-sim-hardware-foundation-tier1-tier2/` 下 4 个 artifacts |

**通过标准**：P0-G1~G5 全部通过 → 记录 Phase 0 验证结论到任务 T0.1 的验证产物，解锁 Phase 1。

---

## CppTLM 状态与 Gate（v0.4 明确）

| 项 | 声明 |
|----|------|
| 仓库内 CppTLM 是否存在 | ❌ **不存在**（F1）；任何"链接 libcpptlm_core.so / 调 cpptlm_emulator_create()"均为虚构，不得出现在本 Change 的实施任务中 |
| CppTLM 23 ABI 是否已验证 | ❌ **未验证**（F2：Draft for Review，无 maintainer ack / release / 本地构建）|
| 本 Change 是否实现真实 CppTLM 桥接 | ❌ **不实现**。真实 CppTLM 对接为 **gated follow-up**：需真实 CppTLM 可用（ack + release + 本地构建通过）后单独立项 |
| mock backend 是否视为真实 CppTLM 验证 | ❌ **不视为**。mock backend 只验证本仓库契约（API 签名 / 错误码 / 状态机 / topology 解析），**不是** CppTLM 验证 |
| M2a/M2b/M2c 与真实 CppTLM 的关系 | ✅ M2a（mock 枚举）/ M2b（BAR 往返）/ M2c（bypass 3 态）全部**在 mock backend 上达成**；真实 CppTLM 端到端（Gate 5.5.2-R 真实里程碑）不在本 Change 范围 |
| 对下游（ADR-088/090）的影响 | ✅ **无阻碍**。本 Change 交付的 API 契约是下游真实 CppTLM 对接的**适配面**；下游 gate 不受影响（它们各自有跨仓 work items）|

## What Changes

### 1. sim_hardware target 结构调整（INTERFACE API + mock STATIC 实现，v0.4 决策）

- `sim_hardware` 保持 **INTERFACE**（API 契约库：头文件 + 纯接口；不产出二进制）
- 新增 **`sim_hardware_mock` STATIC** 实现库：当前 7 个 stub 的 mock 真实实现 + topology JSON 解析
- 真实 CppTLM 对接为 **optional gated target**（`sim_hardware_cpptlm`，本 Change 不创建，属 follow-up）
- 理由：INTERFACE 保证"API 契约先行 + 零二进制耦合"；mock STATIC 保证"无外部依赖可独立测试"；真实 backend 未来以同 API 追加，不破坏契约

### 2. 子模块填充（mock-first，全部可在无 CppTLM 时构建测试）

| 文件 | 内容（mock-first） |
|------|-------------------|
| `sim_hardware/include/cpptlm/bridge.h`（修改）| 统一 API 签名（v0.4 契约，见 design.md §3）|
| `sim_hardware/src/cpptlm/bridge.cpp`（修改）| mock backend 实现：BAR buffer + config space + scalar adapter，**不调任何 cpptlm_emulator_* 符号** |
| `sim_hardware/src/cpptlm/endpoint.cpp`（修改）| 单 EP mock 实现（`pcie_endpoint_create` 返回有效对象；**不含真实 17-port composition**，后者 deferred）|
| `sim_hardware/src/pcie/host_bridge.cpp`（修改）| mock 枚举（读 topology 的 devices 数组）+ mock bypass 读写（直写 BAR buffer）|
| `sim_hardware/src/pcie/bypass.cpp`（修改）| Bypass 3 态真实切换（Full/Bypass/Partial 显式值 + DrainPolicy，见 design.md §8）|
| `sim_hardware/src/topology.cpp`（修改）| nlohmann/json 真实解析 + 合法 schema 校验 + `topology_write_default_json` 修复 |
| `sim_hardware/topology/default_topology.json`（修改）| 修复为**合法 JSON**（这是实施任务 T2.1；v0.4 现状是非法）|
| `sim_hardware/CMakeLists.txt`（修改）| INTERFACE 保持 + 新增 `sim_hardware_mock` STATIC |

### 3. plugins/pci_driver/ PCI probe 边界集成（v0.4 修正路径）

- 当前边界文件：`plugins/pci_driver/probe.cpp`（F6）
- 新增 `plugins/pci_driver/pci_setup_bus.cpp`：调 mock BAR allocate，实现 `pci_bus_assign_resources` 行为
- 新增 `plugins/pci_driver/pcie_enable_device.cpp`：调 config_write 写 `PCI_COMMAND_MEMORY|PCI_COMMAND_IO` 实现 enable
- **注意**：本 Change **不新增/不重命名** `pci_probe.cpp`；`probe.cpp` 的 enumerate 桥接以**最小侵入**方式接线（不修改现有 PcieEmuImpl 行为，仅新增可选调用点）

### 4. MSI-X mock adapter（v0.4 新增明确范围）

- `sim_hardware_mock` 提供 `register_msix_callback` 的 **mock adapter**：vector 号 → 回调投递的模拟路径（无真实 pending bitmap，仅验证 host 侧 callback 契约）
- 真实 MSI-X 硬件仿真（pending bitmap / vector table / PBA）**deferred**（真实 CppTLM MSI-X 3 ABI 依赖）

### 5. 显式 Deferral（v0.4 明确，不纳入本 Change 实施）

| 项 | 状态 | 去向 |
|----|------|------|
| 真实 CppTLM 23 ABI 桥接 | ⏸️ **gated follow-up** | 需真实 CppTLM 可用后单独立项 |
| 真实 17-port PCIe endpoint composition（PcieEndpointIP）| ⏸️ **deferred** | Change-3（Tier 5 SR-IOV，ADR-091 §D5 L5）|
| IOMMU 硬件模型（sim_hardware/dma/iommu_hw）与真实 IOMMU invalidation / attach | ⏸️ **deferred** | 真实 CppTLM DMA translate cb 依赖；独立 follow-up |
| IOMMU Q2/Q3 拆分裁决（invalidate.cpp 归属）| ⏸️ **deferred** | 本 Change 保持 `plugins/iommu_driver/invalidate.cpp` 不动（沿用 ADR-091 v0.2 决策）|
| Tier 3-8（Link/PHY/SR-IOV/Completion/AXI/AXI Mapper）| ⏸️ deferred | Change-3/4/5（ADR-091 §D5）|

### 6. 新增测试（4 new + 1 modified）

| 测试 | 状态 | 内容 |
|------|------|------|
| `tests/sim_hardware/test_topology_standalone.cpp` | 🆕 NEW | 合法 topology 解析 / 非法 JSON 报错 / 默认 topology 往返 |
| `tests/sim_hardware/test_cpptlm_bridge_mock_standalone.cpp` | 🆕 NEW | mock backend：init / BAR 读写 / config 读写 / MSI-X callback 契约 |
| `tests/sim_hardware/test_pcie_host_bridge_mock_standalone.cpp` | 🆕 NEW | mock 枚举（M2a）/ mock bypass 读写（M2b 路径）|
| `tests/sim_hardware/test_pcie_bypass_mock_standalone.cpp` | 🆕 NEW | Bypass 3 态切换（M2c）+ DrainPolicy |
| `tests/plugins/test_pci_driver_standalone.cpp` | 🔁 MODIFIED | 既有（Change-1 新增）扩展：probe 边界 + setup_bus + enable_device 桥接 |

## Capabilities

### New Capabilities

- `sim-hardware-cpptlm-bridge`: 统一 bridge API（init/mmio/config/msix-callback）— **mock backend 实现**；真实 CppTLM 为 gated follow-up
- `sim-hardware-pcie-host-bridge`: Tier 1 (bypass) + Tier 2 (mock RC mirror enumerate) — mock-first
- `sim-hardware-bypass-controller`: PcieBypassMux 3 态运行时切换（Full/Bypass/Partial 显式值）
- `sim-hardware-topology-loader`: 从 JSON 加载合法 PC 拓扑（nlohmann/json）+ schema 校验
- `pci-driver-enumeration`: Linux pci_scan_slot 行为（调 sim_hardware mock 枚举）

### Modified Capabilities

- `pci-driver-plugin`: 增加 enumeration + BAR allocate 桥接（Change-1 仅有 PcieEmu 设备模型）
- `iommu-driver-plugin`: **不改**（IOMMU 桥接 deferred；见 §5 显式 Deferral）

## Impact

- **新增 sim_hardware_mock 代码**：~900-1,100 LOC（mock 实现 + topology 解析 + bypass + platform）
- **修改 pci_driver/**：~250-350 LOC（probe 桥接 + setup_bus + enable_device）
- **新增/修改测试**：~700-800 LOC（4 new + 1 modified standalone binary）
- **HAL 影响**：**无**（per ADR-023 append-only）
- **下游解锁**：Change-3（Stage 5.5.3 Tier 3+5+6）与真实 CppTLM follow-up 的 API 契约面
- **风险**：
  1. mock 与真实 CppTLM 行为差异（latency/时序/边界语义）→ 设计契约中显式标注"latency informational"
  2. 真实 CppTLM ABI 未冻结 → 以本 Change API 契约为适配面，未来 ABI 变化被隔离在 `sim_hardware_cpptlm` backend 内
  3. `plugins/pci_driver/` 链接 sim_hardware_mock 的静态耦合 → mock 通过显式 backend 注入，生产路径（真实 CppTLM）不依赖 mock
  4. 符号解析：pci_driver 插件 dlopen 模型 → sim_hardware_mock 以静态链接进插件或显式依赖注入

## Milestones（v0.4 拆分 M2a/M2b/M2c）

| 里程碑 | 定义 | 验证测试 |
|--------|------|---------|
| **M2a** | mock 首次枚举到 GPU 设备（topology 解析 + host_bridge_enumerate 返回 ≥1 设备）| `test_pcie_host_bridge_mock_standalone` |
| **M2b** | BAR 读写往返（mock buffer 写入读回一致）| `test_cpptlm_bridge_mock_standalone` + `test_pcie_host_bridge_mock_standalone` |
| **M2c** | Bypass 3 态切换工作（Full/Bypass/Partial + DrainPolicy）| `test_pcie_bypass_mock_standalone` |
| **M2**（组合）| M2a + M2b + M2c 全部通过 = **M2 达成**（首次模拟 host 启动后经 mock 枚举到 GPU 设备）| 三者合跑 |

> **latency informational**：性能/延迟数据（如"250-700ns 区间"）在本 Change 中**仅作信息记录**，不作为验收标准（mock backend 无时钟模型，时序无意义）。真实 CppTLM 延迟验证属 follow-up。

## Out of Scope

- ❌ 真实 CppTLM 23 ABI 桥接 / 验证（gated follow-up，需真实 CppTLM 可用）
- ❌ 真实 17-port PcieEndpointIP composition（deferred 到 Change-3）
- ❌ 真实 IOMMU 硬件模型 + invalidation + attach（deferred）
- ❌ Tier 3-8（Link Layer / PHY / SR-IOV / Completion / AXI / AXI Mapper）— Change-3/4/5
- ❌ latency 性能验收（latency informational only）
- ❌ AMD/Nvidia 真实硬件 topology profile（仅 pc-x86-mock）
- ❌ 真机 Linux L2 build 验证 — Change-5

## 关联 ADR / Change

- **前置**：
  - [ADR-091](../00_adr/adr-091-pci-driver-architecture-and-four-quadrant.md) ✅ Accepted v0.2
  - [Change-1 pci-driver-refactor](../../archive/2026-09-03-2026-09-03-pci-driver-refactor/) ✅ Shipped（已归档，F7）
- **关联**：[ADR-088](../00_adr/adr-088-dgpu-complete-simulation.md)（CppTLM 23 ABI 边界，Draft 未验证）+ [CppTLM v4 Handoff](../05-advanced/cpptlm-v4-implementation-handoff.md)（Draft，未合并）
- **关联**：[ADR-090 v2](../00_adr/adr-090-ptxir-via-h2d-dma-v2.md)（H2D DMA 走 BAR0 MMIO，本 Change 提供 PCIe bus 契约面）
- **下游**：Change-3（Tier 3+5+6）+ 真实 CppTLM follow-up

---

**状态**: 🔄 Proposed v0.4（Metis 复审修复版；待 Oracle + Metis 双审查）
