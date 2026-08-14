# Vision — Multi-Process GPU Simulator Stack

> **Status**: 🔄 Vision Draft (paused for owner review)
> **Date**: 2026-08-13
> **Author**: Sisyphus (基于 Oracle 跨仓独立评审 + 直接文件验证)
> **Scope**: 全栈 GPU 仿真栈的全局架构蓝图 — 从 CUDA runtime 到 GPU 设备
> **Audience**: UsrLinuxEmu / CppTLM / PTX-EMU / TaskRunner 四仓 owner + Architecture Team
> **Status Note**: 本文档**不 ship 代码**。它描述愿景、关键决策、阶段划分、文档清单，等四个 owner 评审后再决定 ship 顺序。

---

## 1. Context

### 1.1 现状

四个仓独立开发，各有重叠：

| 仓 | 当前职责 | 重叠 / 不一致 |
|---|---------|--------------|
| **UsrLinuxEmu** | Linux kernel sim + gpu_driver 插件 + HardwarePullerEmu (CP firmware) | 自有 GPU 仿真层 (sim/hardware/, sim/scheduler/) |
| **TaskRunner** | CUDA runtime + cu* shim (LD_PRELOAD) + IGpuDriver 抽象 | 接管部分 CUDA runtime 角色 |
| **CppTLM** | TLM 2.0 NoC + Cache/Memory + GPU cluster 仿真（黑盒） | **ADR-SOC-04** 永久排斥 doorbell/AQL/PIO |
| **PTX-EMU** | PTX kernel 解释执行 + libcudart.so (legacy) + GPUContext/SMContext | libptxemu_device.so 与 libcudart.so 路径重叠 |

ADR-076（2026-08-13 Accepted）尝试集成 UsrLinuxEmu + PTX-EMU，但 audit 暴露 3 个 critical defect：
- Defect 1: dlsym typedef 不匹配 (`hal_user.cpp:701-708`)
- Defect 2: `g_gpu_context` 在 dlsym 路径下不被创建
- Defect 3: HAL heap (256MB @ 0x100000000) 与 PTX-EMU SimpleMemory (4GB mmap) 不可桥接

### 1.2 限制

- 144/145 ctest 通过，但 mock 测试**无法 catch 真 .so 签名漂移**（audit §3.3）
- 跨仓 commit 顺序已通过 ADR-035 §R5.1 串行化
- CppTLM ADR-SOC-04 永久排斥真实 host software 接口（与全局 vision 冲突）
- 单仓视角无法解决"多 GPU / 多节点"演进需求

### 1.3 触发条件

- UsrLinuxEmu owner + CppTLM owner + PTX-EMU owner + TaskRunner owner 共识
- 目标：从"独立仓"演进为"产品形态集成仿真栈"

---

## 2. Vision

### 2.1 全栈仿真栈架构

```
┌─────────────────────────────────────────────────────────────────┐
│ Vision: 端到端 GPU 仿真栈（产品形态）                              │
└─────────────────────────────────────────────────────────────────┘

[Process 1] Software Stack Sim (host CPU 侧)
───────────────────────────────────────────── TaskRunner
    - cuLaunchKernel / cuMemcpy / cuModuleLoad
    - libcuda_taskrunner.so shim (LD_PRELOAD)
    ↓ IPC: MMIO + doorbell + fence + interrupt
  UsrLinuxEmu
    - gpu_driver plugin (GpgpuDevice, HAL)
    - HardwarePullerEmu (PM4 microcode / CP firmware)
    - Linux kernel sim (VFS, ModuleLoader, scheduler)

═══════════════ IPC / PCIe sim 边界 ═══════════════

[Process 2..N] GPU Device Sim (per-GPU)
─────────────────────────────────────────────
  CppTLM (SoC framework)
    - CppTLMBridge 端点 (driver→GPU MMIO 入口)
    - KernelLaunchTLM (PM4 dispatcher, AQL parser, doorbell handler)
    - ComputeUnitTLM × N (黑盒 SM)
    - TCC (GPU L2) + HBM mode
    - MemoryBridge (替代 PTX-EMU SimpleMemory)
  PTX-EMU libptxemu_device.so (minimal)
    - PtxContextAdapter (PTX-IR → StatementContext)
    - PtxInterpreter core (StatementContext 执行)
    - **不持有 SimpleMemory**（由 CppTLM MemoryBridge 提供）
    - **不持有 GPUContext singleton**（由 CppTLM KernelLaunchTLM 管）

═══════════════ Inter-GPU NVLink sim ═══════════════

[Process 2] GPU0  ←── NVLink IPC ──→ [Process 3] GPU 1
```

### 2.2 软件栈覆盖范围

| 层 | 真实硬件 | 仿真 | 备注 |
|---|---------|------|------|
| CUDA runtime | libcudart.so + libcuda.so | TaskRunner | 已基本完成 |
| 用户态 GPU 驱动 | libcuda.so / KFD ioctl | TaskRunner shim + UsrLinuxEmu | tadr-307 在 track |
| 内核态 GPU 驱动 | nvidia.ko / amdgpu.ko | UsrLinuxEmu gpu_driver plugin | HAL 已定义 |
| Linux 内核行为 | VFS / scheduler / VMA | UsrLinuxEmu kernel/ | 已实施 |
| PCIe 行为 | MMIO / DMA / MSI | **IPC 通道 (Phase 3)** | 待新增 |
| GPU 设备 | Silicon (SM/TMU/L2/HBM) | CppTLM cluster + PTX-EMU minimal | 已部分 (Phase 7.A) |

---

## 3. Key Design Decisions

### D1: 进程边界位置

| 选项 | 描述 | 推荐 |
|------|------|------|
| (a) 单进程 | CppTLM+PTX-EMU+UsrLinuxEmu+TaskRunner 全在 1 进程 | ❌ 不支持多 GPU |
| **(b) CPU/GPU 二分** | Software stack 1 进程，GPU sim 1 进程/卡 | ✅ **推荐** |
| (c) 全分（按模块） | 每模块独立进程 | ❌ 状态同步灾难 |
| (d) 同进程 + logical process 抽象 | CppTLM 内部 LP | ⚠️ 待评估 |

**理由 (b)**：真实硬件就是 CPU dies + GPU dies；多 GPU 自然延伸；IPC 开销 ~200cy/MMIO 对 GPU cycle-accurate 仿真可接受。

### D2: Memory 模型

| 选项 | 描述 | 推荐 |
|------|------|------|
| **(a) Shared memory mapping** | CPU/GPU 进程共享 mmap | ✅ **v1 推荐** |
| (b) Memory pass-through | GPU 进程持有 memory，CPU 通过 IPC 读写 | dGPU 模式 v2 |
| (c) Hybrid | APU 用 (a), dGPU 用 (b) | v2 演进**

**理由 (a) v1**：最简单，接近 APU 共享内存语义；可演进到 (b) 支持 dGPU 仿真。

### D3: PTX-EMU 拆分

| 选项 | 描述 | 推荐 |
|------|------|------|
| (a) 完整 PTX-EMU 保留 | 维持今天状态，HAL 走 dlsym | 改动最小但 SimpleMemory 重复 |
| **(b) PTX-EMU 内拆 minimal** | PtxContextAdapter + PtxInterpreter core 移到 `libptxemu_minimal.so` | ✅ **推荐** |
| (c) PTX-EMU 整体迁 CppTLM | 跨仓违反；PTX-EMU owner 反对 | ❌ |

**理由 (b)**：保留 PTX-EMU 仓所有权；与 vision 一致；工作量 Medium。

### D4: CppTLM Governance (ADR-SOC-04 拆分)

| 选项 | 描述 | 推荐 |
|------|------|------|
| (a) 保留 ADR-SOC-04 v1 | 黑盒简化适用 | 仅 TLM 验证 |
| (b) 撤销 ADR-SOC-04 | 全量实施 PM4/doorbell/AQL | 失去简化抽象 |
| **(c) ADR-SOC-04 拆 v1+v2** | v1 默认黑盒；v2 opt-in Host Adapter (DoorbellHandler + AQLDispatcher + PIOAdapter) | ✅ **推荐** |

**理由 (c)**：兼顾 TLM 黑盒验证（保留 v1 简化路径）与真实 host software 接入（v2 opt-in）。

---

## 4. Phased Implementation Path

| Phase | 范围 | 仓 | 工作量 | 关键 ship |
|-------|------|---|--------|----------|
| **R** | Audit 修复 + ADR-076 v2 | UsrLinuxEmu + PTX-EMU + TaskRunner | Quick-Medium | 真 .so e2e 通过 |
| **1** | CppTLM submodule 集成 | UsrLinuxEmu + CppTLM | Easy | 三仓 build 链接 |
| **2** | CppTLM GPU cluster API 化 | CppTLM | Medium | `gpu_simulator_init(config)` API |
| **3** | IPC seam（4 通道） | UsrLinuxEmu + CppTLM | Hard | CPU/GPU 二进程 e2e |
| **4** | Multi-GPU（同/跨进程） | All | Hard | 双 GPU + NVLink sim |
| **5** | Multi-node（跨机器） | All | Hard | 2 机 × 2 GPU 集群 |
| **6** | 真实 GPU ISA swap | PTX-EMU | Extreme | 替换 PTX-EMU 为 AMDGCN/Xe |

### 4.1 Phase R 详情（现状 ship blocker）

- 修 audit Defect 1: `hal_user.cpp:701-708` typedef + call sites
- 修 audit Defect 2: `cpptlm_module.cpp::load_image` lazy-init `g_gpu_context` + `init()`
- 修 audit Defect 3: `ptxemu_mem_register(base, size)` ABI + `CPPTLM_MODULE_VERSION` 2→3
- `image_execute` 在调用内 drive-to-completion（避免跨 .so threading）
- ADR-076 v2 文档化 v1 scope 限制 + 双 ABI 关系
- 真 .so e2e gate 替代 mock-only gate

### 4.2 Phase 1 详情（submodule 集成）

- UsrLinuxEmu `external/CppTLM/` 子模块
- 三仓 build 系统链接（UsrLinuxEmu + CppTLM + PTX-EMU + TaskRunner）
- 验证：UsrLinuxEmu build 出 `libgpuhal.so` 链接 CppTLM cluster

### 4.3 Phase 2 详情（CppTLM GPU API）

- `KernelLaunchTLM` / `ComputeUnitTLM` / `TCC` / `MemoryBridge` 包装成单一 `gpu_simulator_init(config)` API
- 生命周期管理（init / tick / shutdown）
- 配置格式：JSON（per CppTLM `cpptlm/topo/layer.py`）

### 4.4 Phase 3 详情（IPC seam）

- 4 个 IPC 通道：MMIO/Doorbell/Fence/Interrupt
- 共享 memory mapping (per D2.a)
- Latency 模型：MMIO 200cy / Doorbell 50cy / Fence 50cy / Interrupt 100cy
- 同步原语：eventfd + shared memory completion queue

### 4.5 Phase 4-6 详情（略，留待 Phase 1/2/3 完成后再细化）

---

## 5. Cross-Repo Documentation Inventory

### 5.1 UsrLinuxEmu 仓（需创建/修改）

| 文档 | 类型 | 状态 |
|------|------|------|
| `docs/00_adr/adr-076-gpgpu-kernel-module-ioctl.md` | 修订 v2 | v1 Accepted → v2 修订 |
| `docs/02_architecture/multi-process-gpu-simulator-vision.md` | **新增** ← 本文档可迁 | 顶层 vision |
| `docs/00_adr/adr-XXX-cpptlm-integration-submodule.md` | **新增** | CppTLM submodule 集成 |
| `docs/00_adr/adr-XXX-multi-process-simulator-architecture.md` | **新增** | Phase 3 IPC seam 治理 |
| `openspec/changes/2026-XX-XX-phase-R-audit-fix/` | Change | Phase R ship |
| `openspec/changes/2026-XX-XX-phase-1-cpptlm-submodule/` | Change | Phase 1 |
| `openspec/changes/2026-XX-XX-phase-3-ipc-process-boundary/` | Change | Phase 3 |

### 5.2 CppTLM 仓

| 文档 | 类型 | 状态 |
|------|------|------|
| `docs/soc_arch/adr/ADR-SOC-04-hsapp-cp-dispatcher-simplification.md` | 修订 v2 | 拆 v1+v2（per D4.c） |
| `docs/soc_arch/adr/ADR-SOC-06-host-adapter-extension.md` | **新增** | DoorbellHandler / AQLDispatcher / PIOAdapter |
| `openspec/changes/cpptlm-soc-04-v2-host-adapter/` | Change | 实施 |
| `docs/soc_arch/adr/ADR-SOC-07-multi-process-ipc-seam.md` | **新增** | IPC 通道治理 |
| `docs/soc_arch/specs/multi-process-gpu-simulator.md` | **新增** | Phase 3+ spec |

### 5.3 PTX-EMU 仓

| 文档 | 类型 | 状态 |
|------|------|------|
| `docs/adr/ADR-0029-ptxemu-image-executor.md` | 修订 | §D 新 D7: libptxemu_minimal.so 拆分 |
| `openspec/changes/2026-XX-XX-phase-1-ptxemu-minimal-split/` | Change | 拆 PtxContextAdapter + PtxInterpreter |
| `openspec/changes/2026-XX-XX-phase-R-audit-fix/` | Change | 修 Defect 2 + 加 mem_register |

### 5.4 TaskRunner 仓

| 文档 | 类型 | 状态 |
|------|------|------|
| `docs/shared/adr/tadr-307-igpu-driver-kernel-module-extension.md` | 修订 v2 | 与 ADR-076 v2 同步 |
| `openspec/changes/igpu-driver-kernel-module-extension/` | Change | tadr-307 实施 |

---

## 6. Out of Scope (Explicit Non-Goals)

- ❌ **不实施 Phase 4-6** 在本 vision 文档评审通过之前
- ❌ **不撤销** ADR-SOC-04 v1 — v2 是 **opt-in 增量**，不替换 v1 黑盒路径
- ❌ **不替换 PTX-EMU 仓所有权** — 跨仓搬迁违反 ADR-035
- ❌ **不实施 Linux kernel-side full emulation** — UsrLinuxEmu 已有 linux_compat 层，不重做
- ❌ **不实施 dGPU 真实硬件协议**（PCIe 4/5, NVLink 物理层）— 仿真 latency 模型够用
- ❌ **不实施 ROCm/HSA runtime 100% 兼容** — ADR-SOC-04 v2 opt-in 已覆盖核心路径
- ❌ **不实施 cross-machine gRPC transport** — Phase 5 单独评估

---

## 7. Risks (R1-R5)

| # | 风险 | 严重度 | 缓解 |
|---|------|--------|------|
| R1 | CppTLM owner 不同意 ADR-SOC-04 v2 拆分 | 🔴 阻塞 | Vision doc 评审 + 共识先行 |
| R2 | PTX-EMU owner 不同意拆 minimal.so | 🟡 阻塞 | 通过 ADR-0029 §D7 同步 |
| R3 | IPC 性能退化（200cy/MMIO）影响 cycle 仿真准确性 | 🟡 中 | cycle 仿真范围限定 GPU 内部；MMIO 仅 driver 端 |
| R4 | 多 process 同步 bug 难调试 | 🟡 中 | 共享 memory state dump 工具 |
| R5 | TaskRunner tadr-307 与 vision 时间线错位 | 🟢 低 | ship 顺序按 ADR-035 §R5.1 |

---

## 8. References

### 8.1 内部文档

- UsrLinuxEmu `docs/00_adr/adr-076-gpgpu-kernel-module-ioctl.md` (v1 Accepted)
- UsrLinuxEmu `docs/02_architecture/post-refactor-architecture.md` (SSOT)
- UsrLinuxEmu `docs/00_adr/adr-035-governance-policy.md` (§R5.1 cross-repo 协议)
- UsrLinuxEmu `docs/00_adr/adr-036-three-way-separation.md`
- CppTLM `docs/soc_arch/specs/apu-soc-design.md` (Phase 7 vision)
- CppTLM `docs/soc_arch/adr/ADR-SOC-04-hsapp-cp-dispatcher-simplification.md`
- CppTLM `include/tlm/gpu/ptx_emu_driver.hh` (IPtxEmuDriver 窄接口)
- CppTLM `include/cudart/cpptlm_bridge.h` (CppTLMBridge + PtxEmuDriverApi)
- PTX-EMU `docs/adr/ADR-0029-ptxemu-image-executor.md` (§D8 HAL 扩展方案)
- PTX-EMU `include/cudart/cpptlm_module.h` (Image Executor ABI)

### 8.2 跨仓评审报告

- `PTX-EMU/docs/audits/2026-08-13-ptxemu-hal-backend-defect-audit.md` (3 critical defects)
- Oracle session: `ses_0036b6601ffeAEUEuvagSXjeg3` (本次 vision 评审)

### 8.3 阶段文档 (Phase 1-5 待写)

- Phase R: `openspec/changes/2026-XX-XX-phase-R-audit-fix/`
- Phase 1: `openspec/changes/2026-XX-XX-phase-1-cpptlm-submodule/`
- Phase 2: `openspec/changes/2026-XX-XX-phase-2-cpptlm-gpu-cluster-api/`
- Phase 3: `openspec/changes/2026-XX-XX-phase-3-ipc-process-boundary/`

---

## 9. Decision Required (下一步)

**Owner 评审本 vision doc 后的可能决策**:

1. ✅ 接受 → 进入 Phase R ship 流程
2. 🔄 修订 → 哪个设计决策 (D1-D4) 需调整？
3. ❌ 拒绝 → vision 范围需要重新定义

**本文档不 ship 代码，不替代任何 ADR/change 文档**。它是 4-owner 评审前的顶层 vision 基线。

---

**维护者**: Sisyphus（vision 起草，paused for review）
**最后更新**: 2026-08-13
**关联**: ADR-076 (v1 Accepted), CppTLM ADR-SOC-04 (v1 Accepted), PTX-EMU ADR-0029 (Accepted)
**下次 review gate**: 4仓 owner 评审（待安排）