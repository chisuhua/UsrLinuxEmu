# 阶段 5.5+: 4 象限布局 + PCIe Bus Bridge 9 Tier 仿真

> **状态**: ✅ **Accepted v0.2**（与 ADR-091 v0.2 同步 — Oracle v0.2 复审 PASS；Stage 5.5.1-5.5.5 实施路线图 SSOT）
> **目标**: 把 UsrLinuxEmu 顶层目录从"模糊 3 区分"重构为"清晰 4 象限布局"（ADR-091 v0.2），并对接 CppTLM Phase 0-7 14 个 PCIe 组件，实现 **8 tier PCIe bus** 仿真（v0.2 修正：原文误写 9 tier，实为 8 tier + VFIO 模块），最终让 `drv/` 代码**逻辑零修改**即可在 UsrLinuxEmu + CppTLM dGPU 仿真、真机 Linux + 真实 GPU 双目标运行
> **前置依赖**: Stage 4（4.1-4.7.2）✅ 已完成（2026-08-05）+ ADR-090 v2（PTXIR via H2D DMA）✅ Accepted
> **关联 ADR**:
> - [ADR-091](../00_adr/adr-091-pci-driver-architecture-and-four-quadrant.md) ✅ **Accepted v0.2**（2026-09-03 — Stage 5.5.1 实施升档 + 4 commits ship + 151/151 ctest PASS + Gate A/B/C 全部通过；Gate D Oracle 复审待触发）— **本路线图 SSOT**
> - [ADR-036](../00_adr/adr-036-three-way-separation.md) ✅ Accepted — 3 区分（**待更新为 4 象限**）
> - [ADR-088](../00_adr/adr-088-dgpu-complete-simulation.md) ✅ Accepted — dGPU 参考设计（明确 `src/system_hw/` 概念 + **CppTLM 23 ABI** = ADR-088 §D5 冻结）
> - [ADR-089](../00_adr/adr-089-v55-system-hw-simulation.md) ✅ Accepted — v5.5+ 系统硬件仿真（**待更新 location** → `sim_hardware/`）
> - [ADR-090 v2](../00_adr/adr-090-ptxir-via-h2d-dma-v2.md) ✅ Accepted — H2D DMA PTXIR
> - [ADR-076 v3](../00_adr/adr-076-gpgpu-kernel-module-ioctl.md) 🚫 Superseded by ADR-090 v2 — PTX-EMU HAL Backend（**HAL 65→68 fn-ptrs append-only** = `kernel_module_load/execute/unload`）
> - [ADR-023](../00_adr/adr-023-hal-interface.md) ✅ Accepted — HAL append-only 规则（**68 → 71 fn-ptrs = ADR-076 +3 + ADR-092 +3**）
> - [ADR-061](../00_adr/adr-061-hal-iommu-extension.md) ✅ Accepted — HAL IOMMU ops
> - [ADR-072](../00_adr/adr-072-portability-validation.md) ✅ Accepted — L2 build（**待扩展目标集**）
> - [ADR-092](../00_adr/adr-092-hal-adapter-and-bypass-binding.md) 🔄 **Proposed v0.1**（2026-09-07 — 实施已 ship 71 fn-ptrs 含 `adapter_get_info/open/close`，**Gate D Oracle 复审待触发升档**）
> **关联文档**:
> - [docs/02_architecture/four-quadrant-architecture.md](../02_architecture/four-quadrant-architecture.md) — 4 象限详细布局
> - [docs/02_architecture/core-architecture.md](../02_architecture/core-architecture.md) — 核心架构 SSOT
> - [ADR-089 关联调研](../05-advanced/system-hw-survey-2026-08-16.md) — v5.5+ 调研报告
> - [ADR-089 Live Migration 调研](../05-advanced/vfio-live-migration-research.md)
> **维护者**: UsrLinuxEmu Architecture Team
> **最后更新**: 2026-09-09（**v0.2.3 战略调整 — PCIe EP 优先 + CppTLM 跨仓前置阻塞**；详见 [§修订记录](#修订记录) v0.2.3 条目）

---

## 背景

经过 4 阶段（Phase 1~2 / Stage 1~4）演进，UsrLinuxEmu 现有目录布局出现**架构性问题**：

### 问题 1: `src/kernel/` 范围失焦

`src/kernel/` 当前职责模糊：既包含 VFS / ModuleLoader 等 Linux kernel env sim，又混入 PcieEmu（设备模型，~648 LOC，5 文件）和 iommu_emu（~1262 LOC，11 文件）。**真机 Linux 内核中，`drivers/pci/` 和 `drivers/iommu/` 都是 `drivers/` 下的子系统**，不属于 kernel core。

### 问题 2: 真机对齐不足

| 真机 Linux 路径 | UsrLinuxEmu 当前路径 | 性质偏离 |
|----------------|---------------------|---------|
| `drivers/pci/probe.c` 等 | `src/kernel/pcie/pcie_emu.cpp` 等 | **错位**：应为 portable driver（②）而非 kernel sim（①）|
| `drivers/vfio/vfio.c` 等 | ❌ 不存在 | **缺失**：ADR-089 v5.5+ 触发 |
| `drivers/iommu/iommu.c` 等 | `src/kernel/iommu/iommu_emu_state.cpp` 等 | **错位**：同上 |
| `arch/xxx/pci/` 平台 PCI | ❌ 不存在 | **缺失**：PC 平台仿真无独立位置 |

### 问题 3: ADR-088 提出的 `src/system_hw/` 概念已不够精确

ADR-088 §C2 把 `src/system_hw/` 定义为"PC 系统硬件仿真"，但 ADR-089 v0.5 落地时发现：
- 该目录与 CppTLM 的桥接（23 ABI）需要清晰的物理边界
- 与 `plugins/gpu_driver/sim/`（GPU-specific 仿真）需要明确区分
- 与 Linux kernel env sim（`src/kernel/`）需要明确区分

### 问题 4: 缺 8 tier PCIe 仿真分层（v0.2 修正）

CppTLM 已交付 **Phase 0~7 共 14 个 PCIe 组件**（~3,684 LOC），覆盖 TLP / Link / PHY / SR-IOV / Completion / AXI / Host 桥接 / Root Complex。UsrLinuxEmu 当前**没有任何 PCIe bus 模型**，无法对接这些组件的 host 侧能力。

### 触发条件（per 用户提案 + ADR-091 C2）

> *"`include/kernel`, `src/kernel` 只用于 linux kernel 仿真代码，而pci驱动的代码放在plugins/pci_driver下。从linux 驱动代码里，drivers/pci属于pci驱动，而在arch/xxx 目录下属于linux kernel。而vfio也应该放在drivers/vfio, 而PC机体系硬件系统的模拟应该单独放在独立的目录下，PC系统硬件用于和CppTLM端的dGPU对接，这里可以通过底层的Pcie链路层对接，也需要模拟PcieBypass形态的对接。"*

---

## 涉及层（按 4 象限）

| 象限 | 工作量占比 | 关键工作 |
|------|-----------|----------|
| **Q1**: Linux Kernel Env Sim | ~10% | ModuleLoader 升级（depends 拓扑排序 + 新增 load_priority）+ iommu framework 保留 |
| **Q2**: Portable Driver Code | ~40% | `plugins/pci_driver/` + `plugins/iommu_driver/` + `plugins/vfio_driver/` 创建 |
| **Q3**: PC System Hardware Sim | ~45% | `sim_hardware/` 顶级目录 + **8 tier PCIe 仿真** + CppTLM 桥接 |
| **Q4**: GPU-specific HW Sim | ~5% | 不动（保持原状） |

---

## 5 个 Stage 总览

| Stage | 主题 | 来源 | 关键交付 | 状态 | 工期 |
|-------|------|------|----------|------|-----:|
| **5.5.1** | 4 象限重构（不改功能）| ADR-091 §D1-D2 | `plugins/{pci,iommu}_driver/` + `sim_hardware/` 创建；ModuleLoader 升级 | ✅ 已归档（[Change-1](openspec/changes/archive/2026-09-03-2026-09-03-pci-driver-refactor/) ship + 164/164 ctest PASS）| 4-6 周 |
| **5.5.2** | sim_hardware 基础 + Tier 1+2 | ADR-091 §D5 L1+L2 | CpptlmBridge + HostBypass + RC Mirror；`plugins/pci_driver/` 调 sim_hardware | ✅ 已归档（[Change-2](openspec/changes/archive/2026-09-04-2026-09-03-sim-hardware-foundation-tier1-tier2/) ship，140/140 tasks）| 6-8 周 |
| **5.5.3** | Tier 3+5+6 | ADR-091 §D5 L3+L5+L6 | Link Layer + SR-IOV (PcieEndpointIP 17-port) + Completion | 📋 后台轨道 P1（降频，非 E2E 阻塞）| 10-16 周 |
| **5.5.4** | Tier 4+7 | ADR-091 §D5 L4+L7 | PHY Digital + AXI Adapter | 📋 后台轨道 P1 | 8-12 周 |
| **5.5.5** | Tier 8 + VFIO + 真机一致性 | ADR-091 §D5 L8 + VFIO + ADR-072 扩展 | AXI Mapper OOO + VFIO + L2 build 扩展 | 📋 后台轨道 P1 | 8-12 周 |
| **5.5.6** | **dGPU E2E 主线 #1 — 真实 CppTLM EP** | ADR-091 §D5 + Oracle 路线图分析 | `backdoor_endpoint.cpp` 真实现 + `bridge.cpp` kCpptlm dlopen **23 ABI（5.5.6 dlsym 绑定 22 符号子集）** + `hal_cpptlm.cpp` 3 op 真化 + `plugin.cpp` backend 切换 | ✅ **主线 P0 Archived**（commit `c263867` + Oracle 9.5/10；**接线真实但语义空转**，7 根本错误见 CppTLM change） | 4-6 周（已完成；需 follow-up 验证） |
| **5.5.7** | **dGPU E2E 主线 #2 — CommandProcessor 真实化** | 5.5.6 依赖 + **CppTLM PCIe EP 基础补完前置** | puller/queue submit 经 CppTLM TLP + doorbell；`HardwarePullerEmu` / CmdProcessor 双轨 | ⏸️ **Deferred**（[commit `2cf4bc5` 5.5.7.1 P5.NEW-A](openspec/changes/2026-09-09-5-5-7-cpptlm-cp-real-ification/) 验证 CP attach 假设错位；待 CppTLM [2026-09-09-cpptlm-pcie-ep-foundation](https://example/cpptlm-pcie-ep-foundation) 阶段 1.1-1.4 完成后启动） | 6-8 周 |
| **5.5.8** | **dGPU E2E 主线 #3 — kernel dispatch + DMA** | 5.5.7 依赖 + **CppTLM PCIe EP 基础补完前置** | gpgpu_device ioctl 表经 hal_cpptlm 全量穿透 + CppTLM backdoor DMA | ⏸️ **Deferred**（[commit `d4a98f7` 5.5.8 立项](openspec/changes/2026-09-09-5-5-8-cpptlm-kernel-dispatch-dma/) Oracle 8.7/10；阶段 1 cp_attach 待删除，需 CppTLM 修复 #1/#2/#4/#5/#6/#7 后重启） | 6-8 周 |
| **5.5.9** | **dGPU E2E 主线 #4 — 真机双轨验证** | 5.5.8 依赖 | drv/ 零修改 L2 build + 真机 CppTLM 对拍 | 📋 主线 P0（待 5.5.8 重启后启动） | 4-6 周 |
| **总计** | | | | **已 ship 2/9；主线 P0 4 个新增；2 个 Deferred 等 CppTLM 基础补完；后台 P1 3 个降频** |

### Stage 依赖路径图（双轨道：E2E 主线 P0 + PCIe 底层 P1）

```
[基础层] 5.5.1 (4 象限重构, ✅)
            ↓
       5.5.2 (sim_hardware mock-first 基础, ✅)
            │
            ├──[E2E 主线 P0 — 真实 CppTLM binding]──→
            │   5.5.6 (真实 EP + BackdoorEndpoint + bridge kCpptlm + hal_cpptlm 3 op)
            │        ↓ 真实 CppTLM TLP 通路
            │   5.5.7 (CommandProcessor — puller/queue 经 CppTLM TLP)
            │        ↓
            │   5.5.8 (kernel dispatch + DMA — ioctl 表穿透 + CppTLM backdoor DMA)
            │        ↓
            │   5.5.9 (真机双轨验证 — drv/ 零修改 L2 build)
            │
            └──[后台轨道 P1 — PCIe Tier 细节深化，非 E2E 阻塞]──→
                5.5.3 (Tier 3+5+6 Link/SR-IOV/Completion)
                    ↓
                5.5.4 (Tier 4+7 PHY/AXI Adapter)
                    ↓
                5.5.5 (Tier 8 + VFIO + 真机一致性)
                    ↓
                5.5.10+ (PF 虚拟化扩展轨道 — 原 5.5.6+ 改名，PF/VF 完善排在 E2E 之后)
```

**关键依赖要点**：

**E2E 主线（P0，硬依赖）**：
- **5.5.2 → 5.5.6**：硬依赖。`sim_hardware/` 目录结构 + CpptlmBridge 框架 + `pci_probe_enumerate_from_sim_hardware` 是 5.5.6 真实 CppTLM binding 的基础设施。
- **5.5.6 → 5.5.7**：硬依赖。真实 EP binding（backdoor_endpoint.cpp + bridge.cpp kCpptlm + hal_cpptlm.cpp 3 op 真化）必须先完成，CommandProcessor 才能走 CppTLM TLP 而非 mock FSM。
- **5.5.7 → 5.5.8**：硬依赖。puller/queue submit 经 CppTLM TLP 跑通后，kernel dispatch + DMA 才能在真实通路验证。
- **5.5.8 → 5.5.9**：硬依赖。ioctl 表全量穿透 hal_cpptlm + DMA backdoor 真机验证后，drv/ 零修改 L2 build 才能保证。

**后台轨道（P1，软依赖；仅在主线阻塞期插空推进）**：
- **5.5.3 → 5.5.4 → 5.5.5**：顺序软依赖。Tier 3/5/6 完成后 PHY/AXI Adapter 才有宿主，最终 Tier 8 + VFIO + L2 build 是 PF/VF 完善（5.5.10+）的仿真基础。
- **5.5.5 → 5.5.10+**：硬依赖。VFIO + IOMMU + PCI 底层就绪后，PF 虚拟化扩展轨道（[gpu-pf-driver-virtualization.md §3](../02_architecture/gpu-pf-driver-virtualization.md)）才有仿真底层支持。

**跨轨道独立性**：E2E 主线与后台轨道的依赖**互不耦合**——主线 5.5.6-5.5.9 不依赖 Link/PHY/AXI/SR-IOV/VFIO 任何 Tier（真实 CppTLM binding 通过 `backdoor_endpoint` + `bridge.cpp` kCpptlm 直连 CppTLM `.so`，不经过 5.5.3-5.5.5 的 PCIe 协议栈）；后台轨道 5.5.3-5.5.5 也不依赖主线（PCIe Tier 细节深化是 mock 通路足够）。

**🚀 v0.2.3 新增跨仓硬依赖**：5.5.6/5.5.7/5.5.8 实际依赖 [CppTLM `2026-09-09-cpptlm-pcie-ep-foundation`](https://github.com/CppTLM/openspec/changes/2026-09-09-cpptlm-pcie-ep-foundation/) change 完成（PCIe EP 基础必备 4 步：config space 真实化 + MSI-X 中断链 + DMA translate cb + 电源管理）。**原因**：Oracle 2026-09-09 三轮审查揭示 **23 ABI 契约中 7 个根本性错误**（5.5.6 dlsym 实际触及 22 符号子集）——`register_backdoor_cb` NO-OP / `register_dma_translate_cb` 硬编码 pa=0 / `pcie_config_read/write` -ENOSYS stub / `msix_update_pending` 中断链断裂 / `mmio_read` 数据缺口 + race / `backdoor_read` 返 len 伪装成功 / `mmio_write` 数据丢弃——5.5.6 ship 时仅验证接线真实未验证语义（5 个新测试仅断言 `ret != -ENOSYS`，对数据正确性零覆盖）；5.5.7 D.1 决策 X + 5.5.8 阶段 1 cp_attach 因果链错位（-ETIMEDOUT 根因是 sim_loop 调度 race，与 callback 注册无关，实测 2000 次 race 率仅 0.55%）。**后续路径**：① CppTLM 5 步实施（阶段 1.1-1.4 + 阶段 2.1，**5.0-6.0 周**）；② UsrLinuxEmu 端 follow-up（测试断言升级 `CHECK → REQUIRE + buf 内容验证`，独立 change）；③ 5.5.7 重启；④ 5.5.8 重启（cp_attach 阶段 1 删除）；⑤ 5.5.9 真机双轨验证。

---

## Stage 5.5.1: 4 象限重构（不改功能）

### 目标

把 UsrLinuxEmu 顶层目录从"模糊 3 区分"重构为"清晰 4 象限布局"，**不改任何业务功能**，仅做物理位置迁移。

### 范围

| # | 工作项 | 来源 | 工作量 |
|---|--------|------|--------|
| 1 | `src/kernel/pcie/` (5 文件, 648 LOC，含 `pcie_emu_impl.h` 私有头 177 LOC) → `plugins/pci_driver/` | ADR-091 §D2.1 | 1-2 周 |
| 2 | `src/kernel/iommu/` 拆分为 `plugins/iommu_driver/` + `sim_hardware/dma/` | ADR-091 §D2.2 | 2-3 周 |
| 3 | `include/kernel/pcie*/` → `plugins/pci_driver/include/` | ADR-091 §D2.1 | 并入 |
| 4 | ModuleLoader 升级支持 depends 拓扑排序 + 新增 load_priority 字段 | ADR-091 §D6（v0.2 ABI 变更） | 1 周 |
| 5 | ADR-036 更新到 4 象限版本 | ADR-091 §D7 | 0.5 周 |
| 6 | ADR-089 location 引用更新到 `sim_hardware/` | ADR-091 §D8 | 0.5 周 |
| 7 | ADR-072 L2 build 目标集扩展（pci_driver/ + iommu_driver/）| ADR-091 §D7 | 0.5 周 |

### 命名空间映射

| 现有命名空间 | 新命名空间 |
|--------------|------------|
| `usr_linux_emu::pcie_internal` | `usr_linux_emu::pci` |
| `usr_linux_emu::iommu_emu_global_state` | `usr_linux_emu::iommu_driver::global_state` |

### 验收标准

- [ ] 现有 130+ ctest 全部 PASS（**0 regression**）
- [ ] `plugins/pci_driver/plugin.cpp` 成功 dlopen + dlsym("mod")
- [ ] `plugins/iommu_driver/plugin.cpp` 成功 dlopen + dlsym("mod")
- [ ] ModuleLoader 按 depends 拓扑排序加载：`sim_hardware → pci_driver → iommu_driver → gpu_driver`（含 vfio_driver，load_priority=250）
- [ ] ADR-036 v0.2（4 象限版）创建并 Accepted
- [ ] ADR-089 v0.6（location 更新）创建并 Accepted
- [ ] ADR-072 v0.2（L2 build 目标扩展）创建并 Accepted
- [ ] `topology/default_topology.json` 初版定义
- [ ] **L2 build 验证**：`plugins/pci_driver/` 在 Linux 6.12 真机编译通过（即使 stub 也行）

### Gate

- **Gate 5.5.1-A**: 130+ ctest 全 PASS
- **Gate 5.5.1-B**: ModuleLoader 拓扑排序工作（手动测试 4 plugin 依赖图）
- **Gate 5.5.1-C**: L2 build 通过（即使是 stub 也行）

### 关联 OpenSpec Change

**Change-1**: `2026-09-03-pci-driver-refactor` — Stage 5.5.1 实施

---

## Stage 5.5.2: sim_hardware 基础 + Tier 1+2

### 目标

建立 `sim_hardware/` 顶级目录 + 实施 **Tier 1 (Software Bypass)** + **Tier 2 (Root Complex Mirror)**，让 UsrLinuxEmu 首次能够**模拟 host 启动后枚举到 GPU 设备**。

### 范围

| # | 工作项 | 来源 | 工作量 |
|---|--------|------|--------|
| 1 | `sim_hardware/cpptlm/bridge.cpp` 封装 `cpptlm_emulator.h` C ABI | ADR-091 §D5 + D7 | 2 周 |
| 2 | `sim_hardware/pcie/host_bridge.cpp` 封装 `HostBypassTLM`（Tier 1）| ADR-091 §D5 L1 | 2 周 |
| 3 | `sim_hardware/pcie/host_bridge_root.cpp` 封装 `PcieRootComplexTLM`（Tier 2）| ADR-091 §D5 L2 | 2 周 |
| 4 | `plugins/pci_driver/pci_probe.c` 调 sim_hardware 实现 Linux `pci_scan_slot` 行为 | 真机对齐 | 1 周 |
| 5 | `sim_hardware/topology/default_topology.json` 初版 | ADR-091 §D4 | 0.5 周 |
| 6 | `topology/cpptlm_dgpu_v1.json` 与 CppTLM profile 对接 | ADR-088 §D5 | 0.5 周 |
| 7 | 8 个新增 standalone Catch2 测试（每 tier 至少 1 个）| ADR-091 AC2 | 1 周 |

### 关键交付物

**新增文件**：
- `sim_hardware/include/cpptlm/bridge.h` + `sim_hardware/src/cpptlm/bridge.cpp`
- `sim_hardware/include/pcie/host_bridge.h` + `sim_hardware/src/pcie/host_bridge.cpp`
- `sim_hardware/include/pcie/bypass.h` + `sim_hardware/src/pcie/bypass.cpp`
- `sim_hardware/include/platform.h` + `sim_hardware/src/platform.cpp`
- `sim_hardware/topology/default_topology.json`
- `sim_hardware/topology/cpptlm_dgpu_v1.json`
- `plugins/pci_driver/pci_bus.c` + `plugins/pci_driver/pci_probe.c`
- `tests/sim_hardware/test_pcie_host_bridge_standalone.cpp`
- `tests/sim_hardware/test_pcie_bypass_standalone.cpp`
- `tests/sim_hardware/test_cpptlm_bridge_standalone.cpp`

**修改文件**：
- `plugins/pci_driver/plugin.cpp`：depends 加入 `"sim_hardware_pcie"`（v0.2 修正字段名）
- `CMakeLists.txt`：新增 `add_subdirectory(sim_hardware)`
- `sim_hardware/CMakeLists.txt`：链接 `cpptlm_core.so`

### 验收标准

- [ ] **首次验证**：模拟 host 启动后，`pci_scan_slot` 能枚举到 GPU 设备（`vendor_id=0x10DE, device_id=0x1234`）
- [ ] **首次验证**：BAR 分配（`bar_allocate` 写入 EP config space）
- [ ] **首次验证**：BAR 读写（`bar_read/write` 经 AXI master 路由到 EP BAR 空间）
- [ ] 8 个新增测试 PASS
- [ ] 130+ 现有 ctest 仍 PASS
- [ ] `pcie_bypass::PcieBypassController` 暴露 3 态切换（Bypass/Partial/Full）
- [ ] 拓扑加载器（`topology_loader.cpp`）从 JSON 加载默认拓扑

### Gate

- **Gate 5.5.2-A**: 首次枚举到 GPU 设备（**关键里程碑**）
- **Gate 5.5.2-B**: BAR 读写往返 PASS
- **Gate 5.5.2-C**: Bypass mode 切换工作（GRACEFUL_DRAIN + IMMEDIATE_ABORT 都验证）

### 关联 OpenSpec Change

**Change-2**: `2026-09-15-sim-hardware-foundation-tier1-tier2` — Stage 5.5.2 实施

---

## Stage 5.5.3: Tier 3+5+6

### 目标

实施 **Tier 3 (Link Layer)** + **Tier 5 (SR-IOV)** + **Tier 6 (Completion Tracking)**，让 UsrLinuxEmu 支持真机 PCIe 链路层 + SR-IOV。

### 范围

| # | 工作项 | 来源 | 工作量 |
|---|--------|------|--------|
| 1 | `sim_hardware/pcie/link_layer.cpp` 封装 `PcieLinkLayer` | ADR-091 §D5 L3 | 3-4 周 |
| 2 | `sim_hardware/pcie/sr_iov.cpp` 封装 `PcieEndpointIP` 17-port | ADR-091 §D5 L5 | 4-6 周 |
| 3 | `sim_hardware/pcie/completion.cpp` 封装 `CompletionTracker` | ADR-091 §D5 L6 | 2-3 周 |
| 4 | `plugins/pci_driver/pci_iov.c` 实现 Linux `pci_enable_sriov` 等价 | 真机对齐 | 2-3 周 |
| 5 | 3 tier × ≥3 测试 = ≥9 新增测试 | ADR-091 AC2 | 1-2 周 |

### 关键交付物

**新增文件**：
- `sim_hardware/include/pcie/{link_layer,sr_iov,completion}.h` + `sim_hardware/src/pcie/{link_layer,sr_iov,completion}.cpp`
- `plugins/pci_driver/pci_iov.c`（Linux drivers/pci/iov.c 等价）
- `plugins/pci_driver/pcie_portdrv.c`（AER / PME / hotplug stub）
- `tests/sim_hardware/test_pcie_{link_layer,sr_iov,completion}_standalone.cpp`

**修改文件**：
- `plugins/pci_driver/plugin.cpp`：depends 加入 `"sim_hardware_pcie"` 后可调 Link Layer
- `topology/default_topology.json`：增加 16 个 VF + per-VF config space 声明

### 验收标准

- [ ] **L3**: FC 信用度反压 PASS（P→NP→Cpl 三类 credit 独立计数）
- [ ] **L3**: ACK 累积确认 + NAK 重发 PASS
- [ ] **L3**: 错误注入（NAK / DLLP 丢包 / TLP 丢包）PASS
- [ ] **L5**: 16 个 VF 独立枚举 PASS
- [ ] **L5**: per-VF Config Space 独立读写 PASS
- [ ] **L5**: per-VF MSI-X PASS
- [ ] **L5**: FLR (Function Level Reset) PASS — `flr_pf()` 全复位 + `flr_vf(vfx)` 仅 VF 复位
- [ ] **L6**: NP↔CplD trans_id 关联 PASS
- [ ] **L6**: outstanding 容量上限（N+1 拒绝）PASS
- [ ] 现有 130+ ctest + Stage 5.5.2 8 个测试全 PASS（**0 regression**）

### Gate

- **Gate 5.5.3-A**: Link Layer FC + ACK-NAK PASS
- **Gate 5.5.3-B**: SR-IOV 16 VF 独立枚举 PASS
- **Gate 5.5.3-C**: Completion tracking PASS

### 关联 OpenSpec Change

**Change-3**: `2026-10-01-pcie-link-layer-sriov-completion` — Stage 5.5.3 实施

---

## Stage 5.5.4: Tier 4+7

### 目标

实施 **Tier 4 (PHY Digital Control)** + **Tier 7 (AXI Adapter)**，让 UsrLinuxEmu 支持 PCIe PHY 控制 + SoC AXI 事务边界。

### 范围

| # | 工作项 | 来源 | 工作量 |
|---|--------|------|--------|
| 1 | `sim_hardware/pcie/phy.cpp` 封装 `PciePhyDigitalCtrl` | ADR-091 §D5 L4 | 3-4 周 |
| 2 | `sim_hardware/pcie/axi_adapter.cpp` 封装 `PcieAxiAdapter` | ADR-091 §D5 L7 | 3-4 周 |
| 3 | `sim_hardware/cpptlm/axi_adapter.cpp` 包装 `Axi4StreamAdapter` 三端口 | ADR-091 §D7 | 1-2 周 |
| 4 | 2 tier × ≥3 测试 = ≥6 新增测试 | ADR-091 AC2 | 1-2 周 |

### 关键交付物

**新增文件**：
- `sim_hardware/include/pcie/phy.h` + `sim_hardware/src/pcie/phy.cpp`
- `sim_hardware/include/pcie/axi_adapter.h` + `sim_hardware/src/pcie/axi_adapter.cpp`
- `sim_hardware/include/cpptlm/axi_adapter.h` + `sim_hardware/src/cpptlm/axi_adapter.cpp`
- `tests/sim_hardware/test_pcie_{phy,axi_adapter}_standalone.cpp`

### 验收标准

- [ ] **L4**: Rate Switch 状态机 PASS（Gen1↔Gen2↔Gen3↔Gen4↔Gen5）
- [ ] **L4**: PHY ready 状态查询 PASS
- [ ] **L4**: Surprise Removal（C5 清理）PASS
- [ ] **L7**: 64-byte burst 写序列化 PASS（awlen=15, awsize=2 → 64 bytes total）
- [ ] **L7**: wlast 最后一拍置位 PASS
- [ ] **L7**: valid/ready 反压（不丢事务）PASS
- [ ] 现有 130+ ctest + Stage 5.5.2/5.5.3 全部测试 PASS（**0 regression**）

### Gate

- **Gate 5.5.4-A**: PHY Rate Switch PASS
- **Gate 5.5.4-B**: AXI 64-byte burst PASS

### 关联 OpenSpec Change

**Change-4**: `2026-12-01-pcie-phy-axi-adapter` — Stage 5.5.4 实施

---

## Stage 5.5.5: Tier 8 + VFIO + 真机一致性

### 目标

实施 **Tier 8 (AXI Mapper OOO)** + **VFIO** + **真机一致性验证**，完成 **8 tier 全覆盖**（v0.2 修正：原文 9 tier 错误）+ 真机 Linux 6.12 编译一致性。

### 范围

| # | 工作项 | 来源 | 工作量 |
|---|--------|------|--------|
| 1 | `sim_hardware/pcie/axi_mapper.cpp` 封装 `Axi4Mapper` | ADR-091 §D5 L8 | 3-4 周 |
| 2 | `plugins/vfio_driver/vfio.c` + `vfio_pci.c` 实现 Linux VFIO 等价 | ADR-089 §D7 | 4-6 周 |
| 3 | `plugins/vfio_driver/vfio_iommufd.c` iommufd 集成 | ADR-089 §D7 | 1-2 周 |
| 4 | ADR-072 L2 build 扩展：`plugins/{pci_driver,iommu_driver,vfio_driver}/` 在 Linux 6.12 真机编译 | ADR-091 §D7 | 1 周 |
| 5 | 3 模块 × ≥3 测试 = ≥9 新增测试 | ADR-091 AC3 | 1-2 周 |

### 关键交付物

**新增文件**：
- `sim_hardware/include/pcie/axi_mapper.h` + `sim_hardware/src/pcie/axi_mapper.cpp`
- `plugins/vfio_driver/` 完整目录（含 vfio.c / vfio_pci.c / vfio_iommufd.c 等）
- `tests/sim_hardware/test_pcie_axi_mapper_standalone.cpp`
- `tests/plugins/test_vfio_driver_standalone.cpp`

**修改文件**：
- `plugins/vfio_driver/plugin.cpp`：depends 声明 `"pci_driver"` + `"iommu_driver"`
- `tools/l2-build/`：扩展 L2 build 目标集

### 验收标准

- [ ] **L8**: outstanding tracking + OOO completion PASS
- [ ] **L8**: rid 关联 + 乱序完成匹配 PASS
- [ ] **VFIO**: `vfio_pci_open` / `vfio_pci_ioctl` / `vfio_pci_mmap` PASS
- [ ] **VFIO**: 与 iommufd 集成 PASS
- [ ] **L2 build**: `plugins/pci_driver/` 在 Linux 6.12 真机编译通过
- [ ] **L2 build**: `plugins/iommu_driver/` 在 Linux 6.12 真机编译通过
- [ ] **L2 build**: `plugins/vfio_driver/` 在 Linux 6.12 真机编译通过
- [ ] **L2 build**: 真机 PCIe 设备扫描与 UsrLinuxEmu 行为一致
- [ ] 现有 130+ ctest + Stage 5.5.2/5.5.3/5.5.4 全部测试 PASS（**0 regression**）

### Gate

- **Gate 5.5.5-A**: AXI OOO completion PASS
- **Gate 5.5.5-B**: VFIO 核心 ioctl PASS
- **Gate 5.5.5-C**: L2 build 全目标 PASS（**关键里程碑：真机一致**）

### 关联 OpenSpec Change

**Change-5**: `2027-01-15-pcie-axi-mapper-vfio-l2-build` — Stage 5.5.5 实施

---

## 9 Tier 实施总览

| Tier | 名称 | Stage | CppTLM 组件 | sim_hardware 文件 | Wave 工期 |
|------|------|-------|--------------|---------------------|----------:|
| **L1** | Software Bypass | 5.5.2 | `HostBypassTLM` (Phase 7) | `host_bridge.cpp`（BypassTLM mode）| 2 周 |
| **L2** | Root Complex Mirror | 5.5.2 | `PcieRootComplexTLM` (Phase 7) | `host_bridge.cpp`（RC mode）| 2 周 |
| **L3** | Link Layer | 5.5.3 | `PcieLinkLayer` (Phase 1) | `link_layer.cpp` | 3-4 周 |
| **L4** | PHY Digital | 5.5.4 | `PciePhyDigitalCtrl` (Phase 2) | `phy.cpp` | 3-4 周 |
| **L5** | SR-IOV | 5.5.3 | `PcieEndpointIP` 17-port (Phase 4) | `sr_iov.cpp` | 4-6 周 |
| **L6** | Completion Tracking | 5.5.3 | `CompletionTracker` (Phase 4) | `completion.cpp` | 2-3 周 |
| **L7** | AXI Adapter | 5.5.4 | `PcieAxiAdapter` (Phase 5) | `axi_adapter.cpp` | 3-4 周 |
| **L8** | AXI Mapper OOO | 5.5.5 | `Axi4Mapper` (Phase 6) | `axi_mapper.cpp` | 3-4 周 |
| **VFIO** | VFIO + iommufd | 5.5.5 | (不依赖 CppTLM) | (Q2 内部) | 4-6 周 |

---

## 与既有 ADR / Change 的关系

### 既有 ADR 联动

| ADR | 处理 |
|-----|------|
| ADR-036 ✅ | 3 区分 → 4 象限（Stage 5.5.1 同步更新） |
| ADR-088 ✅ | location `src/system_hw/` → `sim_hardware/`（Stage 5.5.1）|
| ADR-089 ✅ | location 更新 + 5.5.x 范围扩展（Stage 5.5.1-5）|
| ADR-090 v2 ✅ | 依赖 Stage 5.5.2 提供 PCIe bus 通路 |
| ADR-023 ✅ | HAL 71 fn-ptrs 不动 |
| ADR-061 ✅ | HAL IOMMU ops 不动；IOMMU impl 拆到 plugin |
| ADR-072 ✅ | L2 build 目标集扩展（Stage 5.5.5）|

### 既有 Change 关系

| 既有 Change | 关系 |
|-------------|------|
| `2026-07-02-stage-1-0-pcie-emu`（已归档）| **基础**：被 Stage 5.5.1 迁移 |
| `2026-07-02-stage-1-1-iommu-ats`（已归档）| **基础**：被 Stage 5.5.1 拆分迁移 |
| `2026-08-15-stage1-4-kfd-multi-file-integration`（已归档）| **依赖**：Stage 5.5.2 后可正常通过 PCIe bus |

### 新建 Change（5 个）

| # | Change 名 | Stage | 工期 |
|---|-----------|-------|-----:|
| 1 | `2026-09-03-pci-driver-refactor` | 5.5.1 | 4-6 周 |
| 2 | `2026-09-15-sim-hardware-foundation-tier1-tier2` | 5.5.2 | 6-8 周 |
| 3 | `2026-10-01-pcie-link-layer-sriov-completion` | 5.5.3 | 10-16 周 |
| 4 | `2026-12-01-pcie-phy-axi-adapter` | 5.5.4 | 8-12 周 |
| 5 | `2027-01-15-pcie-axi-mapper-vfio-l2-build` | 5.5.5 | 8-12 周 |
| **总计** | | | **36-54 周** |

---

## 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|:---:|:---:|------|
| 1 | CppTLM v0.5 MVP Release Gate 未闭环 | 中 | 高 | Stage 5.5.2 启动前确认 CppTLM stable；如未闭环，Stage 5.5.2 暂缓 |
| 2 | Wave 1 迁移回归（130+ ctest 不全 PASS）| 中 | 高 | 迁移一文件即跑一次全测试；Wave 1 完成立即 L2 build 验证 |
| 3 | plugin 加载顺序错乱（gpu_driver 先于 pci_driver）| 高 | 高 | ModuleLoader 必须支持拓扑排序（Stage 5.5.1 内实施）|
| 4 | iommu 拆分粒度争议 | 中 | 中 | 严格按"API 实现 vs 硬件模型"二分（参考真机 `include/linux/iommu.h` vs `drivers/iommu/`）|
| 5 | VFIO 强依赖 IOMMU + PCI | 中 | 中 | vfio_driver depends 声明 pci_driver + iommu_driver |
| 6 | 跨仓 ABI 变更导致对接断链 | 中 | 高 | 23 ABI 已冻结；新增能力走 C++ composition（`for_endpoint()`） |
| 7 | 长期工期低估 | 中 | 中 | Stage 5.5.1 + 5.5.2 完成后再评估后续 Stage |

---

## 验收里程碑

| 里程碑 | Stage | 关键事件 | 验收指标 |
|--------|-------|----------|----------|
| **M1** | 5.5.1 完成 | 4 象限重构 | 130+ ctest 全 PASS + ADR-036/089/072 v0.2 创建 |
| **M2** | 5.5.2 完成 | **首次枚举到 GPU 设备** | Gate 5.5.2-A PASS |
| **M3** | 5.5.3 完成 | SR-IOV 16 VF 独立枚举 | Gate 5.5.3-B PASS |
| **M4** | 5.5.4 完成 | AXI 64-byte burst | Gate 5.5.4-B PASS |
| **M5** | 5.5.5 完成 | **L2 build 全目标 PASS**（真机一致）| Gate 5.5.5-C PASS |

---

## 修订记录

- **v0.2.3** (2026-09-09, Accepted)：**🚀 战略调整 — PCIe EP 优先 + CppTLM 跨仓前置阻塞**
  - **用户决策（2026-09-09）**：先打通 CppTLM PCIe EP 协同流程，再考虑 CommandProcessor；在完全实现 PCIe EP 协同的情况下，再启动 5.5.7。
  - **Oracle 三轮审查揭示的 7 个根本性错误**（PCIe EP 评审 3.5/10 + CP attach 评审 3.5/10 + 5.5.8 评审 8.7/10）：5.5.6 ship 时"接线真实但语义空转"，7 个 ABI 函数为 NO-OP/stub/死路；5.5.7 D.1 决策 X + 5.5.8 阶段 1 cp_attach 因果链错位（-ETIMEDOUT 根因是 sim_loop race，与 callback 注册无关，实测 2000 次 race 率 0.55%）
  - **新增跨仓硬依赖**：[CppTLM `2026-09-09-cpptlm-pcie-ep-foundation`](https://example/cpptlm-pcie-ep-foundation) change（PCIe EP 基础必备 4 步 + 性能增强 1 步 + 7 错误修复 + 14 ADDED Requirements）—— 本路线图 5.5.7/5.5.8 必须等该 change 完成后启动
  - **5.5.6 状态调整**：🔄 主线 P0 → ✅ **主线 P0 Archived**（接线真实，调用真实化后行为自动生效）
  - **5.5.7 状态调整**：📋 主线 P0 → ⏸️ **Deferred**（commit `2cf4bc5` 5.5.7.1 P5.NEW-A 验证 CP attach 假设错位；D.1 决策 X 锁定"5.5.8 启动时 attach CP"但根因不在 callback）
  - **5.5.8 状态调整**：📋 主线 P0 → ⏸️ **Deferred**（commit `d4a98f7` 5.5.8 立项 Oracle 8.7/10；阶段 1 cp_attach 待删除）
  - **5.5.9 状态不变**：📋 主线 P0（待 5.5.8 重启后启动）
  - **3 个决策确认**（per Oracle 5.5.8 立项审查 + commit `d4a98f7` 修正）：
    - D.1 ✅ Accepted（选项 X：接受 -ETIMEDOUT 为正常返回，需 CppTLM 修复 race）
    - D.2 ✅ Accepted（选项 P：TaskRunner 直调 `gpu_hal_ops.adapter_*`，HAL 契约层直调）
    - D.3 ✅ Accepted（选项 D：保持 ctest opt-in 模式，零 169 baseline 影响）
  - **后续规划路径**：① CppTLM 5 步实施（阶段 1.1-1.4 + 阶段 2.1，2-3 周）；② UsrLinuxEmu 端 follow-up（测试断言升级 `CHECK → REQUIRE + buf 内容验证`，独立 change）；③ 5.5.7 重启；④ 5.5.8 重启（cp_attach 阶段 1 删除）；⑤ 5.5.9 真机双轨验证
  - **同步**：openspec/changes/2026-09-09-cpptlm-pcie-ep-foundation/（CppTLM 仓）+ openspec/changes/2026-09-09-5-5-7-cpptlm-cp-real-ification/ + openspec/changes/2026-09-09-5-5-8-cpptlm-kernel-dispatch-dma/

- **v0.2.2** (2026-09-08, Accepted)：**路线方针调整 — E2E 主线优先（P0）+ PCIe 底层降频（P1）+ 编号规范化**
  - 用户优先级调整：尽快打通 dGPU E2E（PCIe EP → CommandProcessor → kernel dispatch + DMA），再 PF/VF 完善
  - 新增 **5.5.6-5.5.9 dGPU E2E 主线**（P0）：
    - **5.5.6**：真实 CppTLM EP — `backdoor_endpoint.cpp` 真实现 + `bridge.cpp` kCpptlm dlopen **23 ABI**（5.5.6 绑定 22 符号子集） + `hal_cpptlm.cpp` 3 op 真化 + `plugin.cpp` backend 切换（工期 4-6 周，从零实现 4 组件）
    - **5.5.7**：CommandProcessor 真实化（puller/queue 经 CppTLM TLP + doorbell）
    - **5.5.8**：kernel dispatch + DMA（ioctl 表穿透 + CppTLM backdoor DMA）
    - **5.5.9**：真机双轨验证（drv/ 零修改 L2 build）
  - **5.5.3-5.5.5 降级为后台轨道 P1**（PCIe Tier 细节深化，非 E2E 阻塞；仅在主线阻塞期插空推进）
  - 5.5.3 状态从"🔄 进行中"改为"📋 后台轨道 P1"（语义修正：原"进行中"暗示核心 Tier 仿真即将启动，实际为后台轨道）
  - **5.5.6+ → 5.5.10+ 编号改名**（PF 虚拟化扩展轨道，避免与新主线 5.5.6-5.5.9 编号占位冲突）
  - Stage 依赖路径图改为双轨道（E2E 主线 P0 硬依赖 + 后台 P1 软依赖；明确跨轨道独立性）
  - **前置审计**：[openspec/changes/archive/2026-09-08-2026-09-08-kcpptlm-archive-audit/](openspec/changes/archive/2026-09-08-2026-09-08-kcpptlm-archive-audit/) 识别 kcpptlm-backend-binding 归档中 5 项虚假完成（33%），5.5.6 工期重估 4-6 周（vs 原始"增量"假设 1-2 周）

- **v0.2.1** (2026-09-08, Accepted)：**状态同步 + Stage 依赖路径图**
  - 5.5.1 → ✅ 已归档（Change-1 ship + 164/164 ctest PASS）
  - 5.5.2 → ✅ 已归档（Change-2 ship，140/140 tasks）
  - 5.5.3 → 🔄 进行中（前置 backlog 已 ship：bridge + 6 个稳定性 change）
  - 5.5.4 / 5.5.5 → 📋 待启动
  - 新增「Stage 依赖路径图」ASCII 图（5.5.1→5.5.5 顺序依赖 + 关键依赖要点）
  - **归档勾选审计残留**：Change-1 `tasks.md` 显示 32 项 `- [ ]`（外层 TDD 步骤全 `[x]`，仅 Implement 子步骤未全勾选）；代码本体已 ship，164/164 ctest PASS 验证功能完备，属于归档勾选粒度不一致，非实现缺口

- **v0.2** (2026-09-03, Proposed)：Oracle v0.1 复审 INCONCLUSIVE 修复完成
  - 9 tier → **8 tier PCIe** 命名统一（VFIO 单独列出，不算 PCIe tier）
  - ModuleLoader 示例：`deps` → `depends`（与实测 module_loader.h:10 一致）+ `load_priority` 新增字段标注
  - 数字修正：`~698 LOC`（pcie 4 文件）→ `~648 LOC`（pcie 5 文件，含 pcie_emu_impl.h 私有头 177 LOC）
  - Change 命名注解：Change-1 用今日日期 `2026-09-03`；Change-2~5 为占位日期（实际创建时按创建日定名）
  - 与 ADR-091 v0.2 保持完全一致

- **v0.1** (2026-09-03, Proposed): 初版
  - 5 Stage 总览（5.5.1~5.5.5）
  - 9 tier 实施总览（v0.2 修正为 8 tier PCIe）
  - 每个 Stage 的目标 / 范围 / 验收标准 / Gate / 关联 Change
  - 风险与缓解
  - 5 个里程碑（M1~M5）

---

**状态**: ✅ **Accepted v0.2.3**（Oracle v0.2.3 复审 PASS；2026-09-09 战略调整 v0.2.3 已合入）
