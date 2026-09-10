# PCIe Endpoint 实施入口文档（双仓 SSOT）

> **定位**: 本文档是 UsrLinuxEmu ↔ CppTLM 双仓 **PCIe EP 驱动到硬件链路**所有实施工作的**集中入口**（Single Source of Truth Entry Point）。
> **状态**: v0.2.3 (2026-09-09, post-CppTLM v0.4 闭环登记)
> **维护**: CppTLM + UsrLinuxEmu 架构组（跨仓同步）
> **目的**: 让任何进入 PCIe EP / dGPU E2E 主线工作的工程师，能够**从这里找到所有需要的文档、openspec change、实施路径、同步点、验证清单**，而不需要在双仓搜索
> **关联索引**:
> - 本文档**不是**架构 SSOT——架构 SSOT 见 [`https://github.com/chisuhua/CppTLM/blob/main/docs/soc_arch/architecture/16-pcie-endpoint-architecture.md`](https://github.com/chisuhua/CppTLM/blob/main/docs/soc_arch/architecture/16-pcie-endpoint-architecture.md)（**CppTLM 硬件侧**）+ [`pcie-endpoint-architecture.md`](pcie-endpoint-architecture.md)（**UsrLinuxEmu 驱动侧**，本地相对路径）
> - 本文档**不是**roadmap——roadmap 见 [`pcie-bus-bridge-roadmap.md`](../roadmap/pcie-bus-bridge-roadmap.md)（总 roadmap，5.5.1-5.5.10+ 全覆盖）+ [`pcie-ep-cross-repo-implementation-path.md`](../roadmap/pcie-ep-cross-repo-implementation-path.md)（5+4 步聚焦实施路径）
> - 本文档**不是**SDMA 内部设计——见 [`https://github.com/chisuhua/CppTLM/blob/main/docs/soc_arch/architecture/17-sdma-engine-design.md`](https://github.com/chisuhua/CppTLM/blob/main/docs/soc_arch/architecture/17-sdma-engine-design.md)（CppTLM 仓，17-编号）
> - 本文档**是**索引 + 路径图 + 同步点 + 验证清单的"导航器"

---

## §1 双仓文档地图

### §1.1 CppTLM 仓文档（5 核心文档）

> **路径约定**：CppTLM 仓文档路径使用 `https://github.com/chisuhua/CppTLM/blob/main/docs/soc_arch/architecture/NN-*.md` 编号式（NN = 16/17/18 连续），与 UsrLinuxEmu 仓无编号 `docs/02_architecture/` 风格不同。openspec change 用 `https://github.com/chisuhua/CppTLM/blob/main/openspec/changes/...`。

| 文档 | 路径 | 角色 | 何时读 |
|------|------|------|-------|
| **pcie-endpoint-entry.md** | [`https://github.com/chisuhua/CppTLM/blob/main/docs/soc_arch/architecture/18-pcie-endpoint-entry.md`](https://github.com/chisuhua/CppTLM/blob/main/docs/soc_arch/architecture/18-pcie-endpoint-entry.md) | **入口（SSOT for navigation）** | 任何 PCIe EP 工作的**起点** |
| **pcie-endpoint-architecture.md** | [`https://github.com/chisuhua/CppTLM/blob/main/docs/soc_arch/architecture/16-pcie-endpoint-architecture.md`](https://github.com/chisuhua/CppTLM/blob/main/docs/soc_arch/architecture/16-pcie-endpoint-architecture.md) | **架构 SSOT（硬件侧）** | 了解跨仓架构 + 数据流/控制流时 |
| **sdma-engine-design.md** | [`https://github.com/chisuhua/CppTLM/blob/main/docs/soc_arch/architecture/17-sdma-engine-design.md`](https://github.com/chisuhua/CppTLM/blob/main/docs/soc_arch/architecture/17-sdma-engine-design.md) | **SDMA 内部设计**（§1-§14 全 14 章节）| 阶段 1.3a-1.3d 实施时 |
| **pcie-ep-cpptlm-collaboration-roadmap.md** | [`https://github.com/chisuhua/CppTLM/blob/main/docs/roadmap/pcie-ep-cpptlm-collaboration-roadmap.md`](https://github.com/chisuhua/CppTLM/blob/main/docs/roadmap/pcie-ep-cpptlm-collaboration-roadmap.md) | **5+4 步实施 roadmap** | 路径规划 + 工期估算时 |
| **openspec/changes/2026-09-09-cpptlm-pcie-ep-foundation/** | [`https://github.com/chisuhua/CppTLM/blob/main/openspec/changes/2026-09-09-cpptlm-pcie-ep-foundation/`](https://github.com/chisuhua/CppTLM/blob/main/openspec/changes/2026-09-09-cpptlm-pcie-ep-foundation/) | **openspec change**（proposal + design + tasks + spec）| change 提案 + 13 ADDED Requirements 时 |
| **openspec/changes/2026-09-10-cpptlm-stage-1-1-pcie-ep-fixes/** | [`https://github.com/chisuhua/CppTLM/blob/main/openspec/changes/2026-09-10-cpptlm-stage-1-1-pcie-ep-fixes/`](https://github.com/chisuhua/CppTLM/blob/main/openspec/changes/2026-09-10-cpptlm-stage-1-1-pcie-ep-fixes/) | **聚焦子集 change**（阶段 1.1 4 bug 修复）| 实施修复 #3/#5/#6/#7 + 4 ADDED Requirements 时 |

### §1.2 UsrLinuxEmu 仓文档（3 文档 + 3 openspec change = 6 条目）

| 文档 | 路径 | 角色 | 何时读 |
|------|------|------|-------|
| **pcie-endpoint-entry.md**（本文档同源）| `docs/02_architecture/pcie-endpoint-entry.md` | **入口（SSOT for navigation）** | 任何 PCIe EP 工作的**起点** |
| **pcie-endpoint-architecture.md** | `docs/02_architecture/pcie-endpoint-architecture.md` | **架构 SSOT（驱动侧）** | 了解驱动/HAL/bridge 架构时 |
| **pcie-bus-bridge-roadmap.md** | `docs/roadmap/pcie-bus-bridge-roadmap.md` | **总 roadmap**（v0.2.3 战略调整）| 阶段关系 + 跨轨道依赖时 |
| **openspec/changes/2026-09-09-5-5-7-cpptlm-cp-real-ification/** | `openspec/changes/2026-09-09-5-5-7-cpptlm-cp-real-ification/` | **CP 真实化 change**（Deferred 待 CppTLM 5 步完成）| 5.5.7 重启时 |
| **openspec/changes/2026-09-09-5-5-8-cpptlm-kernel-dispatch-dma/** | `openspec/changes/2026-09-09-5-5-8-cpptlm-kernel-dispatch-dma/` | **kernel dispatch + DMA change**（Deferred 待 CppTLM 5 步完成）| 5.5.8 重启时 |
| **openspec/changes/2026-09-10-ue-stage-1-1-bridge-sync/** | `openspec/changes/2026-09-10-ue-stage-1-1-bridge-sync/` | **聚焦子集 change**（UE 侧桥接层断言升级 + 跨仓集成测试，P0 前置）| 5.5.7 启动 gate 解锁条件之一 |

### §1.3 跨仓引用关系

```
┌─────────────────────────────────────────────────────────┐
│ 入口（SSOT for navigation）                                │
│ CppTLM/docs/soc_arch/architecture/18-pcie-endpoint-entry.md         │
│ UsrLinuxEmu/docs/02_architecture/pcie-endpoint-entry.md    │
└────────┬───────────────────────────────────────────────────┘
         │
         ├─→ CppTLM 架构 SSOT ─→ 跨仓引用 ─→ UsrLinuxEmu 架构 SSOT
         │   pcie-endpoint-architecture.md (硬件侧)            │
         │                                  ↓                  │
         │                            pcie-endpoint-architecture.md (驱动侧) │
         │
         ├─→ CppTLM SDMA 内部设计（阶段 1.3 实施）
         │   sdma-engine-design.md
         │
         ├─→ CppTLM 5+4 步 roadmap
         │   pcie-ep-cpptlm-collaboration-roadmap.md
         │
         ├─→ UsrLinuxEmu 总 roadmap
         │   pcie-bus-bridge-roadmap.md (v0.2.3)
         │
         ├─→ CppTLM openspec change (13 ADDED Requirements)
         │   2026-09-09-cpptlm-pcie-ep-foundation/
         │
         ├─→ UsrLinuxEmu openspec change (CP + DMA)
         │   2026-09-09-5-5-7-cpptlm-cp-real-ification/
         │   2026-09-09-5-5-8-cpptlm-kernel-dispatch-dma/
         │
          └─→ 5.5.6 P4.NEW-A/B/C/D + B.5 已 ship 代码（接线真实）
              sim_hardware/src/cpptlm/ (backdoor_endpoint.cpp + bridge.cpp + endpoint.cpp)
              + sim_hardware/src/pcie/ (host_bridge.cpp)
```

---

## §2 实施路径图（5+4 步）

### §2.1 总览（来自 roadmap）

```
阶段 1 基础必备（4.0-5.0 周，跨仓 blocker）        阶段 2 性能增强（1 周）
┌─────────────────┬─────────────────┬─────────────────┐
│  阶段 1.1 PCIe EP │  阶段 1.2 MSI-X  │  阶段 1.3 DMA  │
│  基础（0.5-1 周）│  中断（0.5 周）  │  引擎（2.5-3 周）│
│                  │                  │  ┌────┬────┬───┐│
│  修复 #3 #5 #6 #7 │  修复 #4        │  │1.3a│1.3b│1.3│
│                  │                  │  │    │    │c/d│
│                  │                  │  └────┴────┴───┘│
│                  │                  │                 │
│  阶段 1.4 电源管理│  阶段 2.1 P2P   │                 │
│  （0.5 周）       │  + Resizable    │                 │
│                  │  BAR（1 周）    │                 │
└─────────────────┴─────────────────┴─────────────────┘
```

### §2.2 5+4 步详细映射

| 步骤 | 标题 | 工期 | 执行仓（实施→验证）| 对应 openspec change | 关键模块（5 端口 + Ring + Doorbell）| 当前状态 |
|------|------|:---:|------------------|----------------------|----------------------------------------|----------|
| **阶段 1.1** | PCIe EP 基础 | 0.5-1 周 | CppTLM → UsrLinuxEmu | `2026-09-09-cpptlm-pcie-ep-foundation` §1.1 | config space + 4 data path + race 修复 | 🔄 Proposed |
| **阶段 1.2** | MSI-X 中断 | 0.5 周 | CppTLM → UsrLinuxEmu | 同上 §1.2 | intr_cb 真实触发 + trigger_irq_async 接线 | 🔄 Proposed |
| **阶段 1.3a** | PCIe SDMA 基础 | 1 周 | CppTLM → UsrLinuxEmu | 同上 §1.3 + `sdma-engine-design.md §2-§6` | Ring Buffer + RPTR/WPTR + Doorbell + SG | 🔄 Proposed |
| **阶段 1.3b** | D2D SDMA 路径 | 0.5-1 周 | CppTLM → UsrLinuxEmu | 同上 §1.3 + `sdma-engine-design.md §10` | NoC 数据面 + 显存控制器 bypass | 🔄 Proposed |
| **阶段 1.3c** | dma_translate_cb + GART/IOMMU + CP→SDMA | 0.5 周 | CppTLM → UsrLinuxEmu | 同上 §1.3 + `sdma-engine-design.md §8+§11` | 地址翻译链 + PM4 DMA opcode 0x4600-0x4900 | 🔄 Proposed |
| **阶段 1.3d** | SDMA 完成通知 | 0.5 周 | CppTLM → UsrLinuxEmu | 同上 §1.3 + `sdma-engine-design.md §9` | Fence + MSI-X 接线（#4）| 🔄 Proposed |
| **阶段 1.4** | 电源管理 | 0.5 周 | CppTLM → UsrLinuxEmu | 同上 §1.4 | D0/D3 + ASPM | 🔄 Proposed |
| **阶段 2.1** | P2P + Resizable BAR | 1 周 | CppTLM → UsrLinuxEmu | 同上 §2.1 | ARI 路由 + Resizable BAR Cap | 🔄 Proposed |
| **总计** | | **5.0-6.0 周** | | | |

### §2.3 关键路径（5+4 → UsrLinuxEmu 5.5.7/8/9 重启）

```
CppTLM 5+4 步 (5.0-6.0 周)
  ├─→ [聚焦] 阶段 1.1 (staged via cpptlm-stage-1-1-pcie-ep-fixes, 0.5-1 周): 修复 #3/#5/#6/#7 + Oracle Gate E
  ├─→ 阶段 1.2 (MSI-X 修复 #4)
  ├─→ 阶段 1.3a 完成后: UsrLinuxEmu profile 测试升级 (data assertion)
  ├─→ 阶段 1.3c 完成后: dma_translate_cb 真实化（#2 修复）
  ├─→ 阶段 1.3d 完成后: MSI-X 中断链真实（#4 修复）
  ├─→ 阶段 1.4 + 2.1 完成后:
  │     └─→ [UE 同步] ue-stage-1-1-bridge-sync (0.5-1 周): UE 侧桥接层断言升级 + 跨仓集成测试
  │           验证 CppTLM 4 bug 修复在 UE 进程端到端正确
  └─→ 5.5.7 启动 gate (阶段 1.1+1.2+1.3a ship + bridge-sync ship):
        ├─→ UsrLinuxEmu 5.5.7 重启 (CommandProcessor 真实化, 1-2 周)
        ├─→ UsrLinuxEmu 5.5.8 重启 (kernel dispatch + DMA, 2-3 周)
        └─→ UsrLinuxEmu 5.5.9 真机双轨验证 (4-6 周)
```

---

## §3 openspec change 全景

### §3.1 CppTLM 仓 change（当前 2 个，17 ADDED Requirements）

| Change | 范围 | 阶段覆盖 | 状态 |
|--------|------|---------|------|
| [`2026-09-09-cpptlm-pcie-ep-foundation`](https://github.com/chisuhua/CppTLM/blob/main/openspec/changes/2026-09-09-cpptlm-pcie-ep-foundation/) | CppTLM PCIe EP 基础必备 + 性能增强 + 电源管理 + 完成通知 | **§1.1-1.4 + §2.1**（8 步全覆盖）| 🔄 Proposed |
| [`2026-09-10-cpptlm-stage-1-1-pcie-ep-fixes`](https://github.com/chisuhua/CppTLM/blob/main/openspec/changes/2026-09-10-cpptlm-stage-1-1-pcie-ep-fixes/) | **聚焦子集**：阶段 1.1 4 bug 修复（#3 pcie_config_read/write / #6 backdoor_read miss / #5 mmio_read 数据 / #7 mmio_write async 文档澄清）| §1.1（父 change 阶段 1.1 聚焦实施，0.5-1 周）| 🔄 Proposed（Blocked-by：父 change `pcie-ep-foundation` 同步验证）|

**关键文档**：
- `proposal.md` — Why / What / Capabilities / Impact（含 7 修复 + 5 步建议）
- `design.md` — 技术设计（含 7 错误定位 + 阶段 1.3 细分 4 子阶段）
- `tasks.md` — TDD 5 步结构（5 个阶段章节 §2-§6；阶段 1.3 已按 design.md §3.3 拆分为 §4.1-§4.4 即 1.3a/1.3b/1.3c/1.3d 4 子阶段）
- `specs/cpptlm-pcie-ep-foundation/spec.md` — 13 ADDED Requirements（覆盖 7 修复）
- `specs/cpptlm-stage-1-1-fixes/spec.md` — 4 ADDED Requirements（聚焦 4 修复）

### §3.2 UsrLinuxEmu 仓 change（当前 4 个，Archived 1 + Deferred 2 + Proposed 1）

> **状态词双口径说明**：
> - **change proposal 状态**：`🔄 Proposed v1.0`（在 5.5.7/5.5.8/bridge-sync proposal.md 顶部）= 提案待批/启动
> - **实施状态**：`⏸️ Deferred`（在 entry/bus-bridge）= 实施被 CppTLM 5+4 步阻塞；`🚧 Blocked-by <上游 change>` = 实施被聚焦 change 阻塞
> - **两词共存合法**：proposal 在审实施待 CppTLM 完成；归档后状态合并为 ✅ Archived

| Change | 范围 | 状态 |
|--------|------|------|
| [`2026-09-08-5-5-6-cpptlm-ep-binding`](../../openspec/changes/archive/2026-09-08-2026-09-08-5-5-6-cpptlm-ep-binding/)（archive 实目录名含冗余日期前缀 `2026-09-08-2026-09-08-`，待整改）| 5.5.6 dGPU E2E 主线 #1（背门端点 + bridge dlopen 23 ABI 绑定 22 符号子集 + hal_cpptlm 3 op + backend 选择）| ✅ Archived（接线真实，Oracle 9.5/10）|
| [`2026-09-09-5-5-7-cpptlm-cp-real-ification`](../../openspec/changes/2026-09-09-5-5-7-cpptlm-cp-real-ification/) | 5.5.7 dGPU E2E 主线 #2（CommandProcessor 真实化）| ⏸️ Deferred（待 CppTLM 5+4 步完成 + bridge-sync ship）|
| [`2026-09-09-5-5-8-cpptlm-kernel-dispatch-dma`](../../openspec/changes/2026-09-09-5-5-8-cpptlm-kernel-dispatch-dma/) | 5.5.8 dGPU E2E 主线 #3（kernel dispatch + DMA）| ⏸️ Deferred（待 5.5.7 ship）|
| [`2026-09-10-ue-stage-1-1-bridge-sync`](../../openspec/changes/2026-09-10-ue-stage-1-1-bridge-sync/) | **聚焦子集**：UE 侧桥接层断言升级（`ret != -ENOSYS` → 数据正确性）+ 跨仓集成测试（dlopen `libcpptlm_emulator.so` 验证 4 修复端到端）| 🔄 Proposed v1.0 + 🚧 Blocked-by `cpptlm-stage-1-1-pcie-ep-fixes`（P0 前置，5.5.7 启动 gate 解锁条件之一）|
| **5.5.7.1 P5.NEW-A**（已在 `2026-09-09-5-5-7-cpptlm-cp-real-ification/specs/`）| Profile 真实化验证（独立 P5.NEW-A 任务）| ✅ Oracle 9.4/10 |

### §3.3 5.5.6-5.5.9 状态（来自 UsrLinuxEmu roadmap v0.2.3）

| 阶段 | 标题 | 状态 | 阻塞条件 |
|------|------|------|---------|
| **5.5.6** | dGPU E2E 主线 #1 — 真实 CppTLM EP | ✅ Archived | — |
| **5.5.7** | dGPU E2E 主线 #2 — CommandProcessor | ⏸️ Deferred | CppTLM 5+4 步完成 |
| **5.5.8** | dGPU E2E 主线 #3 — kernel dispatch + DMA | ⏸️ Deferred | CppTLM 5+4 步完成 |
| **5.5.9** | dGPU E2E 主线 #4 — 真机双轨验证 | 📋 待启动 | 5.5.7 + 5.5.8 完成 |

---

## §4 架构核心概念（5 分钟理解）

### §4.1 双层 DMA 架构

```
┌─────────────────────────────────────────┐
│ CommandBuffer (PM4/AQL) — 调度层        │
│ 驱动构造命令序列，提交到 CP 解析分发      │
└──────────┬──────────────────────────────┘
           │ PM4 命令包
           ▼
┌──────────────────────────────────────────────┐
│ Command Processor (CP) — 翻译官              │
│ Fetch PM4 → Decode → Dispatch 到各引擎     │
│ dma_req[2] 端口转发 DMA 类到 SDMA          │
└──────────┬───────────────────────────────┘
           │ SdmaRingEntry (CP→SDMA)
           ▼
┌──────────────────────────────────────────────┐
│ SDMA Engine — 执行层                          │
│ Ring Buffer + RPTR/WPTR + Doorbell + FSM      │
│  5 端口: desc_in/mem_in/mem_out/host_out/done │
└──────────┬───────────────────────────────┘
           │ PCIe TLP (H2D/D2H) / NoC (D2D)
           ▼
┌──────────────────────────────────────────────┐
│ GPU 内部 NoC → 显存控制器 → VRAM            │
│ 或 PCIe Controller → 系统内存               │
└──────────────────────────────────────────────┘
```

> **⚠️ 目标态 vs 现状**：上图为 5+4 步**完成后**的目标架构。**现状**（2026-09-09）：SDMA 为 descriptor 直投（`sdma_engine_tlm.cc`，Ring Buffer/RPTR/WPTR/Doorbell **尚不存在**，阶段 1.3a 待建）；CP 无 DMA 类分发（阶段 1.3c 待建）。找符号/测试 target 前先看 §11.3 代码事实与 §2.2 状态列。

### §4.2 4 层 PCIe 能力框架

| 层级 | 范围 | 本 roadmap 实施 |
|------|------|----------------|
| **基础必备** | PCIe EP + MSI-X + DMA + 电源 | ✅ §2 5+4 步 |
| **性能增强** | P2P + Resizable BAR + 原子操作 + 带宽优化 | ✅ 阶段 2.1 |
| **虚拟化必备** | SR-IOV + VF 配置 + VF 中断隔离 | ❌ 排除（移交 VFIO） |
| **高级可选** | CXL/NTB/TPH/ATS/PRI/PASID | ❌ 排除（后续阶段） |

### §4.3 跨仓边界（23 ABI）

> **ABI 数字三口径说明**：本仓文档统一以 **23 ABI** 为契约边界口径（per [ADR-088 §D5](../../00_adr/adr-088-dgpu-complete-simulation.md) 冻结契约：基础 6 + callback typedef 4 + register 1 + 板卡扩展 8 + MSI-X 3 + DMA translate 1 = 23）。其他出现数字：
> - **22** = 5.5.6 `bridge.cpp` dlsym 实际绑定数（19 契约函数 + 3 adapter 扩展 `open/close/get_adapter_info`，见 ADR-092）；经 `nm -D libcpptlm_emulator.so` 实测（2026-09-09），.so 实际导出亦为 **22**，两者相等——契约 23 中 4 个 callback typedef 非函数符号，不参与 dlsym/导出计数
> - **结论**：23 = 契约（19 函数 + 4 typedef）；22 = 绑定数 = 导出数（19 契约函数 + 3 adapter 扩展）。原"24"口径经实测不存在，已删除

```
UsrLinuxEmu (driver)                  CppTLM (hardware 仿真)
  GpgpuDevice (drv/ioctl)              23 ABI functions
    ↓                                     ↑
  HAL struct (71 fn-ptrs)              cpptlm_emulator_*
    ↓                                     ↑
  CpptlmBridge (23 ABI dlopen)         DGpuBoard / PcieEndpointIP / SDMA / CmdProc
                                         ↑
                               dlopen("libcpptlm_emulator.so")
```

**核心约束**：23 ABI 函数签名不变（5 端口 wire-format 冻结，HAL append-only）

> **🚧 可改 / 禁改边界（全局，非仅验证项）**：
> - ❌ **禁改**：本仓 `plugins/gpu_driver/drv/`（ADR-036 三区分：驱动代码须可零修改移植内核，5+4 步全程含完成后）；23 ABI 签名；5 端口 wire-format；HAL 既有 71 fn-ptrs 签名（append-only per ADR-023）
> - ✅ **可改**：`tests/`（断言升级）、`hal/hal_cpptlm.cpp`（backend 组合）、CppTLM 侧 `src/tlm/` + `test/`

---

## §5 跨仓同步点（实施时序）

### §5.1 同步检查清单

| 时机 | UsrLinuxEmu 端 | CppTLM 端 | 验证命令 |
|------|----------------|----------|---------|
| **阶段 1.1 完成后** | 测试断言升级（CHECK → REQUIRE + buf）| cfg space + 4 data path + race 修复 | `ctest -R profile_real` |
| **阶段 1.2 完成后** | — | msix intr_cb 触发验证（200ms 内 ≥1） | `test_msix_*` |
| **阶段 1.3a 完成后** | — | Ring Buffer + RPTR/WPTR + SG | `test_sdma_ring_rptr_wptr` |
| **阶段 1.3b 完成后** | D2D 不走 host_out 断言 | NoC 数据面 + 显存控制器 | `test_d2d_noc_path` |
| **阶段 1.3c 完成后** | dma_translate identity 断言 | translate_cb 真实化（#2）| `test_dma_translate_iommu` |
| **阶段 1.3d 完成后** | msix 触发后 driver ISR | Fence + MSI-X 接线（#4）| `test_sdma_fence` |
| **阶段 1.4 完成后** | reset path 验证 | D0/D3 + ASPM | `test_pm_state` |
| **阶段 2.1 完成后** | P2P 接入点 + Resizable BAR | ARI + Resizable BAR Cap | `test_p2p_dma` |
| **全 5+4 步完成后** | 5.5.7 重启 + 5.5.8 重启 + 5.5.9 启动 | openspec change archive | Oracle 终审 ≥9.0 |

### §5.2 实施同步约束

- **CppTLM 端先行**（架构补完 + 代码实施）
- **UsrLinuxEmu 端同步**（每子阶段完成后升级断言）
- **commit 节奏**：每个子阶段一个 commit（CppTLM）+ 一个 commit（UsrLinuxEmu）
- **Oracle 复审点**：每个阶段完成后触发

---

## §6 关键决策（已固化）

> **选择列图例**：单字母为该决策在 Oracle/设计评审中的**选项代号**——**X** = 选项 X（接受现状语义 + 对方仓修复）；**P** = 选项 P（TaskRunner 直调，不经 drv/）；**D** = 选项 D（保持既有 opt-in 默认）；其余行为文字描述即最终选择。§7.2 的"（X/P/D）"引用同此。

| ID | 决策 | 选择 | 影响 |
|----|------|------|------|
| **D.1** | -ETIMEDOUT 语义 | X（接受 + CppTLM 修复 race）| 5.5.7 CP attach 不变 |
| **D.2** | adapter op 接入点 | P（TaskRunner 直调 HAL adapter_*）| 零 drv/ 改动 |
| **D.3** | ctest WORKING_DIRECTORY | D（保持 opt-in 模式）| 169 baseline 不变 |
| **D.4** | 5 端口 wire-format | 冻结（5 端口索引顺序锁定）| 23 ABI 兼容（22 = 5.5.6 绑定子集）|
| **D.5** | Ring Buffer 引入 | 作为 desc_in 前端（不变 wire-format）| 阶段 1.3a 双轨过渡 |
| **D.6** | SDMA 完成通知 | MSI-X + Fence 双轨 | 阶段 1.3d |

---

## §7 验证清单（用户必查）

### §7.1 阶段完成验证

- [ ] 阶段 1.1 完成：4 数据通路 roundtrip + config space 真实化
- [ ] 阶段 1.2 完成：msix_update_pending 触发 intr_cb（200ms 内 ≥1 次）
- [ ] 阶段 1.3a 完成：Ring Buffer + RPTR/WPTR + SG + Doorbell 绑定
- [ ] 阶段 1.3b 完成：D2D 路径不经过 PCIe（host_out 零事务断言）
- [ ] 阶段 1.3c 完成：dma_translate_cb 真实调用（identity mapping）
- [ ] 阶段 1.3d 完成：Fence + MSI-X 接线（#4 修复）
- [ ] 阶段 1.4 完成：D0 ↔ D3 切换 + ASPM
- [ ] 阶段 2.1 完成：P2P + Resizable BAR
- [ ] 全 5+4 步 Oracle 审查 ≥ 9.0/10
- [ ] UsrLinuxEmu 5.5.7 P5.NEW-A profile 测试稳定（mmio ret==0 + buf 真实数据）

### §7.2 跨仓集成验证

- [ ] CppTLM `test/test_cpptlm_emulator_abi.cc` 全 PASS
- [ ] UsrLinuxEmu `test_bridge_kcpptlm_profile_real_standalone` 5/5 PASS + data assertion
- [ ] `ctest` 双向全绿
- [ ] docs-audit 双仓 PASS
- [ ] D.1/D.2/D.3 决策不变（X/P/D）

### §7.3 架构约束保持

- [ ] UsrLinuxEmu `drv/` 零修改（5+4 步实施期间 + 完成后）
- [ ] UsrLinuxEmu HAL append-only（ADR-023 §D4 不变）
- [ ] 23 ABI 函数签名不变（仅行为从 stub → 真实；22 = 5.5.6 绑定子集快照，见 §4.3 脚注）
- [ ] CppTLM `include/abi/cpptlm_emulator.h` 不变（声明冻结；`src/abi/cpptlm_emulator.cc` 实现可改）
- [ ] 5 端口 wire-format 冻结（D.4）
- [ ] Ring Buffer 作为 desc_in 前端双轨过渡后 5 端口 wire-format 不变（D.5）

---

## §8 工作场景速查（"我在做 X，应该看哪些文档"）

### §8.0 我是新人，第一次进入 PCIe EP 工作

1. **读序**（约 40 分钟）：§1 文档地图 → §4 核心概念（注意 §4.1 目标态注记）→ §2.3 关键路径 → §7.3 架构约束
2. **当前能做什么**（全部 5+4 步 🔄 Proposed 期间）：
   - ✅ 阅读 + 挑错：双仓文档/spec 评审（本文档 §11、CppTLM spec 13 ADDED Requirements）
   - ✅ 跑现状验证：`ctest -R profile_real`（本仓）、`test/test_cpptlm_emulator_abi.cc`（CppTLM 仓）
   - ✅ 认领预备任务：§8.3 测试断言升级预案的 dry-run（不改代码，先写 diff 草稿）
   - ⏸️ 不可认领：阶段 1.1-2.1 实施（待 CppTLM change 批准启动）；5.5.7/5.5.8（⏸️ Deferred）
3. **可动 / 禁动区域**：见 §4.3 末尾边界框（SSOT）；新人请先读该节再动手
4. **第一联系人**：跨仓同步问题 → 见 §5 同步点负责人（架构组）

### §8.1 我是 CppTLM 开发者，要实施阶段 1.1（PCIe EP 基础）

1. **先读**：[`pcie-endpoint-architecture.md`](pcie-endpoint-architecture.md) §2.1-2.2（数据流）
2. **再读**：[`https://github.com/chisuhua/CppTLM/blob/main/openspec/changes/2026-09-09-cpptlm-pcie-ep-foundation/specs/cpptlm-pcie-ep-foundation/spec.md`](https://github.com/chisuhua/CppTLM/blob/main/openspec/changes/2026-09-09-cpptlm-pcie-ep-foundation/specs/cpptlm-pcie-ep-foundation/spec.md)（13 ADDED Requirements 中关于 PCIe EP 基础的部分）
3. **实施**：`src/tlm/gpu/dgpu_board_shell.cc`（修复 #3 + #5 + #6 + #7）
4. **测试**：新建 `test/test_cpptlm_emulator_abi_*.cc`（CppTLM 测试目录为单数 `test/`，无 `abi/` 子目录）
5. **验证**：从 UsrLinuxEmu 仓跑 `ctest -R profile_real`

### §8.2 我是 CppTLM 开发者，要实施阶段 1.3a（SDMA Ring Buffer）

1. **必读**：[`https://github.com/chisuhua/CppTLM/blob/main/docs/soc_arch/architecture/17-sdma-engine-design.md`](https://github.com/chisuhua/CppTLM/blob/main/docs/soc_arch/architecture/17-sdma-engine-design.md) §2-§6（核心 5 章节）
2. **对照**：[`https://github.com/chisuhua/CppTLM/blob/main/openspec/changes/2026-09-09-cpptlm-pcie-ep-foundation/design.md`](https://github.com/chisuhua/CppTLM/blob/main/openspec/changes/2026-09-09-cpptlm-pcie-ep-foundation/design.md) §3.3（阶段 1.3 细分）
3. **实施**：`src/tlm/gpu/sdma_engine_tlm.cc` 扩展 + 新建 `sdma_ring_buffer.h/cc`
4. **测试**：新建 `test/test_sdma_ring_rptr_wptr.cc`（CppTLM 测试目录为单数 `test/`，扩展名 `.cc`，无 `_standalone` 后缀——与 §8.1 约定一致）
5. **验证**：5 端口 wire-format 不变 + Ring→descriptor 转换内部完成

### §8.3 我是 UsrLinuxEmu 开发者，要升级测试断言

1. **必读**：[`pcie-endpoint-architecture.md`](pcie-endpoint-architecture.md) §2.4.1（SDMA 接入点）
2. **必读**：[CppTLM/docs/soc_arch/architecture/17-sdma-engine-design.md](https://github.com/chisuhua/CppTLM/blob/main/docs/soc_arch/architecture/17-sdma-engine-design.md) §6（Packet 格式）
3. **修改**：`tests/sim_hardware/test_bridge_kcpptlm_profile_real_standalone.cpp`：
   - `CHECK(ret != -ENOSYS)` → `REQUIRE(ret == 0)` + `INFO("ret=" << ret)`
   - 加 buf 内容断言（写入 0xDEADBEEF → 读回相等）
4. **验证**：从 UsrLinuxEmu 仓跑（`cd /workspace/project/UsrLinuxEmu && ./build/bin/test_bridge_kcpptlm_profile_real_standalone`；该二进制是本仓 Catch2 产物，CppTLM 仓无此文件）

### §8.4 我是架构师，要做跨仓对齐

1. **入口**：本文档（`pcie-endpoint-entry.md`）
2. **双仓架构 SSOT**：
   - CppTLM: [`https://github.com/chisuhua/CppTLM/blob/main/docs/soc_arch/architecture/16-pcie-endpoint-architecture.md`](https://github.com/chisuhua/CppTLM/blob/main/docs/soc_arch/architecture/16-pcie-endpoint-architecture.md)（硬件侧）
   - UsrLinuxEmu: [`pcie-endpoint-architecture.md`](pcie-endpoint-architecture.md)（驱动侧，本地相对）
3. **跨仓引用**：两个 SSOT 互相 cross-reference（⚠️ 已知问题：CppTLM 16-doc L10 回指误写为 CppTLM 自身路径 `docs/soc_arch/architecture/16-...`，正确目标为本仓 `docs/02_architecture/pcie-endpoint-architecture.md`，根治需 CppTLM 侧提交）
4. **变更影响评估**：任何 PCIe EP 改动需同步更新两个 SSOT

### §8.5 我是 PM，要看进度

1. **总 roadmap**：[`pcie-bus-bridge-roadmap.md`](../roadmap/pcie-bus-bridge-roadmap.md)（5.5.1-5.5.10+ 全覆盖）+ [`pcie-ep-cross-repo-implementation-path.md`](../roadmap/pcie-ep-cross-repo-implementation-path.md)（5+4 步 + 5.0-6.0 周工期）
2. **当前状态**：
   - 阶段 1.1-1.4 + 2.1：🔄 Proposed（待 CppTLM 实施）
   - 阶段 1.3 已细分 4 子阶段（1.3a/1.3b/1.3c/1.3d）
   - UsrLinuxEmu 5.5.7/5.5.8：⏸️ Deferred（待 CppTLM 5+4 步完成）
3. **里程碑**（**累计周** = 阶段完工时点；M6-M8 是启动时点而非"+X 周"）：
   - M1 (阶段 1.1): 0.5-1 周
   - M2 (阶段 1.2): 1.0-1.5 周
   - M3 (阶段 1.3 4 子步完成): 3.5-4.5 周（= M2 + 1.3a 1 + 1.3b 0.5-1 + 1.3c 0.5 + 1.3d 0.5 = 2.5-3.0）
   - M4 (阶段 1.4): 4.0-5.0 周（= M3 + 0.5）
   - M5 (5+4 步全部完成 / 阶段 2.1): **5.0-6.0 周**（5.5.7 启动 gate 之一）
   - M5.5 (UE bridge-sync 同步): M5 + 0.5-1 周（5.5.7 启动 gate 关键前置，per `2026-09-10-ue-stage-1-1-bridge-sync/` change）
   - M6 (5.5.7 启动): M5.5 完成时（per Q1 裁决口径：阶段 1.1+1.2+1.3a + bridge-sync 全部 ship）
   - M7 (5.5.7 完成 / 5.5.8 启动): M5 + 1-2 周（per 5.5.7 proposal L3 工期）= **6.0-8.0 周**
   - M8 (5.5.8 完成 / 5.5.9 启动): M7 + 2-3 周（per 5.5.8 proposal L3 工期）= **8.0-11.0 周**
   - M9 (5.5.9 真机双轨验证完成): M8 + 4-6 周 = **12.0-17.0 周**

---

## §9 风险与回退

| 风险 | 等级 | 回退 |
|------|:----:|------|
| 阶段 1.1 mmio race 修复影响 5.5.7.1 profile 测试 | 中 | timeout 延长 + sim_loop tick 频率提升 |
| 阶段 1.3 SDMA 实施量大（现有 descriptor 直投 → Ring Buffer 重构）| 高 | 阶段 1.3a 双轨过渡保留 5 端口 wire-format（per §6 D.5）|
| 阶段 1.3c dma_translate_cb 真实调用触发 UsrLinuxEmu cb 错误处理 | 中 | cb 失败 fallback（phys = iova identity）|
| 阶段 1.3d 中断链修复后 msix 测试不稳定 | 中 | 测试用 200ms 超时 + retry 1（对齐 §5.1/§7.1"200ms 内 ≥1"） |
| 阶段 1.4 电源管理状态切换影响 profile 加载 | 低 | profile 加载时强制 D0 |
| 跨仓协调延迟（CppTLM 实施 → UsrLinuxEmu 升级断言）| 中 | 每子阶段独立 commit，可异步同步 |
| Oracle 复审不通过 | 中 | 5+4 步全部交付 + 5.5.7/5.5.8/5.5.9 三轮终审达 9.0+。**现状**：5.5.6 9.5/10 与 5.5.7.1 P5.NEW-A 9.4/10 已过 9.0 门槛（属 5.5.x 工作线，非本 PCIe EP 协同线）；PCIe EP 协同线三轮 Oracle 评审（PCIe EP 评审 3.5/10 + CP attach 评审 3.5/10 + 5.5.8 评审 8.7/10，见 `pcie-bus-bridge-roadmap.md` L469 修订记录）当前最佳 8.7/10，仍 < 9.0 门槛；本轮预期：补 SDMA 内部设计（`17-sdma-engine-design.md`，§1-§14 完整章节)后该工作线下一轮评审有望突破 9.0；如仍未达 9.0 则升级为"高"并触发 §10.3 文档回退预案（归档本 change → 启 v0.3 修订）|

---

## §10 文档维护规则

### §10.1 新增 PCIe EP 文档时

1. **先更新本文档 §1**（文档地图）
2. **再更新对应 SSOT 文档**（架构 / SDMA 设计 / roadmap / change）
3. **commit 节奏**：本文档 + 受影响文档同 commit
4. **命名规范**：
   - `pcie-*.md` — PCIe EP 相关
   - `sdma-*.md` — SDMA 相关
   - 跨仓引用用完整 URL（`https://github.com/chisuhua/CppTLM/blob/main/docs/...`）

### §10.2 修改现有 PCIe EP 文档时

1. **更新文档地图**：在本文档 §1 标记版本变化
2. **更新修订记录**：在文档自身 §修订记录
3. **跨仓同步**：涉及双仓 API 改动时同步两个仓的文档

### §10.3 归档已废弃 PCIe EP 文档时

1. 移到 `archive/` 目录
2. 在本文档 §1 标记 "已归档"
3. 保留链接（Git 历史可访问）

### §10.4 结构性章节镜像规则（双仓同步防漂移）

> **来源**: Sprint C.2（Oracle Gate Review ses_f77b67eb6ffe4bhFqs4qQJ3Woy + ses_f77b65cf4ffecmczxhrNwP4yfg 闭环）。背景：v0.4 (84350648) 与 dca050a2 两次半成品教训显示，结构性章节 + 数字口径变更需双仓严格镜像，否则下次 sync 会把已修正数字再拉回。

**镜像清单（结构性章节）**：

| 章节 | 类型 | 镜像源/镜像目标 |
|---|---|---|
| §1 文档地图 | 结构 | CppTLM 18-doc ↔ UsrLinuxEmu entry |
| §4 架构核心概念 | 结构 + 数字 | 镜像（包含 §4.1 目标态注记 / §4.3 三口径脚注 / §4.3 边界框）|
| §6 决策图例 | 结构 | X/P/D 单字母代号必须双仓一致 |
| §8.0 新人场景 | 结构 | 必须双仓镜像（否则 CppTLM 侧新人零入口）|
| §8.3 验证指令 | 可执行命令 | `cd <仓> && <path>` 必须指向真实存在仓 + 文件 |
| §8.5 里程碑 | 数字 + 格式 | M1-M9 累计周格式 + M9 (12-17 周) 双仓一致 |
| §9 Oracle 风险 | 数字 | 等级 + 评分数字双仓一致 |
| §11.3 代码事实 | 数字 | 行数 + IOCTL 必须与 `wc -l` 实测一致 |
| §12 修订记录 | 结构 | v0.x 条目双向登记对方仓 commit hash |

**触发条件**：
1. 上述清单任一项变更 → **同 PR** 必须镜像双仓（不得单仓先 commit）
2. 数字口径变更（71 fn-ptrs / 22/23 ABI / 41 IOCTL / 5-6 周工期）→ 同上
3. 新增 §8.x 工作场景 / §10.x 维护规则 / §4.x 子章节 → 同上

**CI 检查（绑定 `tools/docs-audit.sh`）**：
- `§1.6` 章节存在性 grep（每仓 entry 必备章节清单）
- `§11.3` 行数 vs `wc -l` 实测差异告警（>5% 偏差触发）
- `§11.2` 死链检查（`openspec/specs/` + GitHub URL HEAD 请求）
- `§1.2` 文档计数自洽（标题数字 vs 表格行数一致）

**失败处理**：
- `tools/docs-audit.sh --strict` 返回非零 → pre-commit hook 阻断提交
- 文档层 + CI 层双重失败 → 必须先修复再 commit，不允许 `SKIP_DOCS_AUDIT=1` 绕过（仅 env 警告可绕过）

**例外**：
- CppTLM 仓独有 change（如本仓 spec 提前 archive 等）→ 镜像责任在本仓
- 跨仓 API 改动 → §10.2 跨仓同步已覆盖

---

## §11 关联资源

### §11.1 ADR（架构决策）

| ADR | 内容 | 关联 |
|-----|------|------|
| [ADR-023 HAL append-only](../../00_adr/adr-023-hal-interface.md) | HAL fn-ptrs append-only（ADR-023 文本记 64+1；代码 2026-09-09 实测 **71**，per `tools/docs-audit.sh §1.5` 权威 SSOT；73 错算已撤回）| §4 §6 D.4 |
| [ADR-088 dGPU 完整仿真](../../00_adr/adr-088-dgpu-complete-simulation.md) | dGPU 仿真边界 + 23 ABI | §4 §1 |
| [ADR-091 4 象限布局](../../00_adr/adr-091-pci-driver-architecture-and-four-quadrant.md) | 4 象限 + PCIe tier | §4.2 |
| [ADR-092 HAL adapter + bypass binding](../../00_adr/adr-092-hal-adapter-and-bypass-binding.md) | ✅ **Accepted v0.2**（2026-09-09 — Gate D Oracle 复审通过升档：4/4 checklist PASS ① UE tasks 100% ② CppTLM `bab64dd5` ship 8 项 ③ nm 22 fn ④ BypassMode canonical 双仓对齐；详见 ADR-092 §v0.2 修订段 + commit `ec672ab`） | §4.3 |

### §11.2 spec（功能规范）

| spec | 内容 |
|------|------|
| [`cpptlm-pcie-ep-foundation/spec.md`](https://github.com/chisuhua/CppTLM/blob/main/openspec/changes/2026-09-09-cpptlm-pcie-ep-foundation/specs/cpptlm-pcie-ep-foundation/spec.md) | 13 ADDED Requirements（CppTLM 仓）|
| [`kcpptlm-backend-binding`](../../openspec/specs/kcpptlm-backend-binding/) | UsrLinuxEmu 仓 ABI 契约扩展（5.5.6 已 ship）|

### §11.3 代码事实

| 模块 | 文件 | 行数 | 说明 |
|------|------|------|------|
| SDMA 引擎（现有）| `src/tlm/gpu/sdma_engine_tlm.cc` | 420 | descriptor 直投，阶段 1.3a 待扩展 |
| Command Processor（现有）| `src/tlm/gpu/command_processor_mvp.cc` | 149 | 5-state FSM，阶段 1.3c 待扩 DMA 类 |
| DGpuBoard（现有）| `src/tlm/gpu/dgpu_board_shell.cc` | 445 | 7 错误修复点 |
| CpptlmBridge（UsrLinuxEmu 已 ship）| `sim_hardware/src/cpptlm/bridge.cpp` | 405 | 23 ABI dlopen + 4 data path（22 = 5.5.6 dlsym 绑定子集）|
| BackdoorEndpoint（UsrLinuxEmu 已 ship）| `sim_hardware/src/cpptlm/backdoor_endpoint.cpp` | 390 | 5 ule_dgpu_* functions |
| Endpoint（UsrLinuxEmu 已 ship）| `sim_hardware/src/cpptlm/endpoint.cpp` | 18 | PcieEndpointIP 接入点 |
| host_bridge（UsrLinuxEmu 已 ship）| `sim_hardware/src/pcie/host_bridge.cpp` | 111 | bypass/full dispatch（位于 pcie/ 而非 cpptlm/）|
| HAL cpptlm（UsrLinuxEmu 已 ship）| `plugins/gpu_driver/hal/hal_cpptlm.cpp` | 79 | 3 adapter op 真化 |
| GpgpuDevice（UsrLinuxEmu 已 ship）| `plugins/gpu_driver/drv/gpgpu_device.cpp` | 1137 | ioctl 派发表（**41** IOCTL，per `gpgpu_device.h:27 kNumIoctls = 41`）|

---

## §12 修订记录

- **v0.1** (2026-09-09, Draft): 初版,基于 2026-09-08 文档重命名 + 2026-09-09 战略调整 + 5+4 步 roadmap + SDMA 内部设计
   - §1 双仓文档地图（5 CppTLM + 5 UsrLinuxEmu 条目）
  - §2 实施路径图（5+4 步 + 5.0-6.0 周）
  - §3 openspec change 全景（CppTLM 1 + UsrLinuxEmu 1 Archived + 2 Deferred）
  - §4 架构核心概念（双层 DMA + 4 层 PCIe + 23 ABI 边界）
  - §5 跨仓同步点（9 个时序检查清单）
  - §6 关键决策（6 条固化决策）
  - §7 验证清单（阶段完成 + 跨仓集成 + 架构约束）
  - §8 工作场景速查（6 个常见场景：§8.0 新人 / §8.1 CppTLM 1.1 / §8.2 CppTLM 1.3a / §8.3 UsrLinuxEmu 断言升级 / §8.4 架构师对齐 / §8.5 PM 进度）
  - §9 风险与回退（7 条）
  - §10 文档维护规则
   - §11 关联资源（4 ADR + 2 spec + 9 代码模块）

- **v0.2** (2026-09-09, post-Oracle 复审): 双轮评审后修订（Oracle 评分 6.5 → 目标 8.5+）
  - **must-fix**:
    - §7.3 冻结约束路径 `src/abi/cpptlm_emulator.h` → `include/abi/cpptlm_emulator.h`（声明冻结；`.cc` 实现可改）
    - §9 Oracle 风险行: 重写回退字段，区分两条评审线（5.5.x 已过 9.0 vs PCIe EP 协同线当前 8.7/10 < 9.0），风险等级"低"→"中"
    - §1.1 / §3.1 / §8.2 / §11.2 共 16 处跨仓 URL `github.com/CppTLM/...` → `github.com/chisuhua/CppTLM/blob/main/...`（owner = `chisuhua`，与 git remote 一致），并更新 §1.1 路径约定说明 + §10.1 命名规范
    - §8.2 测试路径 `tests/sim_hardware/test_sdma_ring_rptr_wptr_standalone.cpp` → `test/test_sdma_ring_rptr_wptr.cc`（与 §8.1 单数 `test/` + `.cc` 约定一致）
  - **cosmetic**:
    - §1.1 SDMA "（11 章节）" → "（§1-§14 全 14 章节）"
    - §4.3 图内 "71 fn-ptrs" → "73 fn-ptrs"（与同节 73 一致；71 是 ADR-092 记录时点旧值）
    - §9 风险行 2 回退字段 "阶段 1.3c 过渡期" → "阶段 1.3a 双轨过渡"（per §6 D.5）
    - §12 §8 描述 "5 个常见场景" → "6 个常见场景" + 列举 §8.0-§8.5
    - §12 §3 描述 "UsrLinuxEmu 3 已 ship + Deferred" → "1 Archived + 2 Deferred"
  - **已知遗留（需 CppTLM 仓侧提交）**: 见 CppTLM 16-doc / 18-doc（"14 ADDED" 实际 13；18-doc §1.2 UsrLinuxEmu 路径错写；18-doc 头部 markdown `> **> **` bug；16-doc L10 UsrLinuxEmu 回指错误）→ **全部已闭环**: CppTLM `dca050a2` (v0.3.1) 完成 73→71 错误链闭环登记 + 死 spec 替换；`84350648` (v0.4) 完成 §8/§9/§1.3/§2.2/§4/§11.3 全套镜像同步（详见 CppTLM 18-doc §12 v0.4 条目 + 本仓 v0.2.3 闭环登记）。

- **v0.2.1** (2026-09-09, commit `12a3e4b`): 回滚 HAL fn-ptr 73 → 71（grep 错算修正）
  - §4.3 图内 "73 fn-ptrs" → "71 fn-ptrs"
  - §4.3 边界框 "HAL 既有 73 fn-ptrs" → "71"
  - §11.1 ADR-092 行 "ship 73 fn-ptrs" → "ship 71 fn-ptrs"
  - ADR-023 L434 演进注记 "73" → "71" + 加 `grep -c "(\*"` 错算说明 + 引用 `tools/docs-audit.sh §1.5`
  - AGENTS.md CODE MAP "73 函数指针" → "71"
  - 错误链溯源：`8cc29c3 F-4 → 58a89b0 ADR-023 → 570b977 §4.3 图 → 8461887 AGENTS.md → 7f4d26a4 CppTLM 镜像 → 300ddc45 CppTLM v0.3 自愈`
  - **Oracle 复审发现遗漏（本仓 v0.2.2 补）**: §11.1 ADR-023 行 L418 仍写"实测 73"——rollback 半成品

- **v0.2.2** (2026-09-09, 本 commit): 补全 v0.2.1 rollback + 流程纪律修复
  - §11.1 ADR-023 行 L418 "实测 73" → "实测 71" + 引用 §1.5 SSOT（mirror CppTLM `300ddc45` L391 措辞）
  - §12 还原 v0.2 原 bullet 文本（"71 → 73"叙事 + "71 是 ADR-092 记录时点旧值" rationale），恢复历史真实性
  - §12 新增独立 v0.2.1 条目（如上），记录 12a3e4b 的全部变更
  - 文档头部状态 v0.2 → v0.2.2（与 commit message + ADR-023 注记一致）
  - **跨仓闭环**: CppTLM `300ddc45` (v0.3) 独立完成相同回滚；本 commit 引用该 commit 标记双仓对齐 71
  - **Oracle 复审 session**: `bg_0eca263a`（Oracle 独立确认 71 为 SSOT + 指出 v0.2.1 三处遗漏）

- **v0.2.3** (2026-09-09, post-CppTLM v0.4 闭环登记): CppTLM `84350648` (v0.4) 已完成本仓镜像同步
  - **CppTLM 18-doc 已镜像**:
    - §8.0 新人场景（mirror UE entry）
    - §8.1/§8.2 测试路径 → 单数 `test/` + `.cc`（实测 CppTLM 真实路径）
    - §8.3 验证指令 → `cd /workspace/project/UsrLinuxEmu && ./build/bin/...`（该二进制仅在 UE 仓）
    - §8.5 里程碑 → M1-M9 累计周格式 + M9 (5.5.9 完成 12-17 周)
    - §9 Oracle 风险 → "中 / 协同线当前最佳 8.7/10 < 9.0"
    - §4.1 目标态注记 / §4.3 边界框 / §6 X/P/D 决策图例
    - §2.2 执行仓列 / §1.3 图 host_bridge 归 `src/pcie/`
    - §11.3 行数 + IOCTL 41（实测）/ URL owner → `chisuhua/CppTLM/...`
  - **本仓同步修正**: §11.3 GpgpuDevice "38 IOCTL" → **41**（per `gpgpu_device.h:27 kNumIoctls = 41`）
  - **§12 v0.2 已知遗留 4 项全部闭环**：CppTLM `dca050a2` (v0.3.1) + `84350648` (v0.4) 已完成 §11.1 ADR-023 行 71 / §11.2 死 spec 替换 / §12 历史还原 / 跨仓镜像全套
  - **剩余 0 项已知遗留**

- **v0.3** (2026-09-09, post-Sprint C 治理闭环): Sprint C 全套治理修订落地 + P1.1 量化 AC 复审
  - **ADR-023 v3 修订** (UE `b9df3d1`):
    - 文末追加 "## v3 修订 2026-09-09" 段（per ADR-024/064 修订格式）
    - 主段 64+1 → 71+1 = **72 callable entries**
    - 15 组契约分布细化（base 11→14, graph 7→8, memory pool 9→10, queue 5→6, adapter 独立列 3）
    - L434 演进注记更新为指向 v3 修订段
    - 影响面登记：双仓 7 处 SSOT 全部已对齐 71（无需修改）
  - **tasks.md §4 拆分** (CppTLM `c9ce049c`):
    - 3 任务 → 4 子阶段（1.3a PCIe SDMA 基础 / 1.3b D2D 路径 / 1.3c dma_translate_cb+GART/IOMMU+CP→SDMA / 1.3d SDMA 完成通知）
    - 总 checkbox 41 → 55（+14）；总工期 3-4 周 → **4-5 周**
    - 量化 AC 标注"待复审确认"
  - **CppTLM 18-doc v0.4.1 cleanup** (CppTLM `aa1a22a6`):
    - 删除 §8.5 L362 残留旧 M8 格式行
  - **§10.4 双仓镜像规则** (UE `0ef42ff` + CppTLM `b1814313`):
    - 双仓 entry §10 新增"结构性章节镜像规则"段（mirror 同步）
    - 镜像清单 9 项 + 触发条件 + CI 检查绑定 docs-audit + 失败处理 + 例外
    - 防 v0.4 (84350648) + dca050a2 半成品教训复发
  - **P1.1 量化 AC 复审** (CppTLM `d99ab2e0`, Oracle `ses_f77b67eb6ffe4bhFqs4qQJ3Woy`):
    - 9 项量化 AC 复审裁决：**7 ACCEPT + 5 AMEND + 2 REJECT**
    - 5 AMEND 应用到 §4.1-§4.4：
      - Ring Buffer 256KB → `cfg.ring_size ∈ {4KB, 8KB, 16KB, 64KB}` 最大 1024 entries @64B
      - Doorbell `0x18` → `BAR1 + 0x10010000`（per §3.3 / §5.1 L296）
      - SG 链长 ≥16 → ≥8（per §2 / §13 MAX_SG_ENTRIES=8）
      - NoC ≥32 GB/s → ≥100 GB/s（per §10.4 "数百 GB/s" 保守下限）
      - cb fallback: "0 错误码（IOMMU 失败）" 0=成功 语义颠倒 → identity 模式 pa=iova 返回 0；IOMMU 模式传播负 errno（-ENOSYS/-EIO）至 error_cb
    - 2 REJECT 改为非量化：
      - MSI-X ≤1ms → "200ms 超时窗口内触发 ≥1 次"（引用链断裂 + TLM wall-clock 无可重复性）
      - vector 0-3 → "vector 由 entry/驱动指定 + msix_init table_size 合法范围"
    - 任务清单同步对齐（6 处子任务描述修订）
    - **实施就绪度**：阶段 1.1 可立即启动；阶段 1.3a 在此 commit 后启动
  - **§10.4 镜像规则首次应用**: 本次 P1.4 commit 按 §10.4 镜像规则双仓同步落地
  - **剩余 0 项已知遗留**

- **v0.3.1** (2026-09-09, post-Gate-Review 跨仓同步登记, Oracle `ses_f76ad2e6effeBzU10vj2ruu0Pm` + Metis `ses_f76acfffaffeNN0jWV9GMOXORn`): 登记 2 个聚焦 changes + 关键路径补 bridge-sync + M5.5 插入
  - **聚焦 changes 登记**（双仓 entry §1 + §3 同 PR 镜像, per §10.4 镜像规则）:
    - CppTLM `2026-09-10-cpptlm-stage-1-1-pcie-ep-fixes`（阶段 1.1 4 bug 修复聚焦子集, 4 ADDED Requirements, 0.5-1 周）
    - UsrLinuxEmu `2026-09-10-ue-stage-1-1-bridge-sync`（UE 侧桥接层断言升级 + 跨仓集成测试, 6 ADDED Requirements, 0.5-1 周, 🚧 Blocked-by `cpptlm-stage-1-1-pcie-ep-fixes`）
  - **关键路径补全**（§2.3 + §8.5）: 插入 `[cpptlm-stage-1-1-pcie-ep-fixes]` 聚焦节点 + `[ue-stage-1-1-bridge-sync]` UE 侧 gate; 5.5.7 启动条件 Q1 裁决 = 阶段 1.1+1.2+1.3a + bridge-sync 全部 ship
  - **里程碑 M5.5 插入**（§8.5）: M5 + 0.5-1 周 = bridge-sync gate; M6 (5.5.7 启动) = M5.5 完成时
  - **Q-A-A 决策**（Oracle Gate Review Q1/Q2/Q3）: gate 口径 = 宽松（1.1+1.2+1.3a+bridge-sync ship）; 工期口径 = entry 优先（5.0-6.0 周）; 聚焦 changes 登记 = 是（§10.4 触发）
  - **Commit 链**:
    - UE `375cad5` docs(pcie-ep): v0.3.1 cross-repo entry sync — register 2 focused changes + key path
    - CppTLM `d8f39276` docs(cpptlm): v0.5.1 cross-repo entry sync — mirror UsrLinuxEmu v0.3.1
  - **跨仓 commit**（Oracle 评审复审后落地）:
    - CppTLM `cb82146c` fix(cpptlm): ADR-092 baseline + timeline alignment + cross-repo URL fix
    - UE `3a7f7b3` fix(ue-stage-1-1-bridge-sync): correct §2.5 dangling reference → §2.3
  - **0 项已知遗留**

- **待 v0.4**: 阶段 1.3a 实施后追加（实际 Ring Buffer wire-format 验证 + 性能基准 + 5.5.7 Oracle 复审反馈）

---

**入口文档使用提示**：
- **首次进入 PCIe EP 工作**：从 §1 文档地图开始，依次读 §4 核心概念 → §2 实施路径 → §8 工作场景速查
- **跨仓协调**：以本文档 §5 同步点为准，每子阶段 commit 前检查同步清单
- **问题排查**：先看 §6 决策 → 再看 §9 风险 → 最后看具体文档章节

**SSOT 边界**：
- **本文档** = 入口 + 索引 + 同步点（不存架构/设计 SSOT 内容）
- **pcie-endpoint-architecture.md**（双仓）= 架构 SSOT
- **sdma-engine-design.md**（CppTLM 仓）= SDMA 内部设计 SSOT
- **pcie-bus-bridge-roadmap.md** = 总 roadmap SSOT（5.5.1-5.5.10+）
- **pcie-ep-cross-repo-implementation-path.md** = 5+4 步聚焦实施路径
- **openspec change** = change 提案 + 13 ADDED Requirements