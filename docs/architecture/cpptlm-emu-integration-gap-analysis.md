# Architecture Gap Analysis: CppTLM ↔ UsrLinuxEmu GPU Emulation Integration

**Document ID**: GAP-088-cpptlm-emu
**Status**: 🚨 **已过时**（**请勿作为实施依据**；仅 §2 现状快照可参考）
**Date**: 2026-08-14（初版）；2026-08-16（**重新定位**为已过时）
**Owner**: UsrLinuxEmu Architecture Team + CppTLM maintainer
**Driver**: Stage 5 (post-Stage-4 closeout) — trigger-gated by ADR-049/052 conditions

---

## 🚨 重要：本文档已完全过时（2026-08-16 重新定位）

> **🚨 本文档不应作为任何实施决策的依据**——所有设计冲突均已由 [ADR-088](../00_adr/adr-088-dgpu-complete-simulation.md)（**✅ Accepted，2026-08-15 Oracle 二次评审通过；2026-08-16 范围收窄修订**）解决。
>
> **本文档与 ADR-088 的核心冲突**：
>
> | 维度 | 本文档（过时） | ADR-088（权威） |
> |------|---------------|----------------|
> | **ABI 总数** | 55-57 ABI（v1 + v2 扩展）| **23 ABI**（仅 dGPU 板卡；范围收窄后）|
> | **CppTLM 范围** | 仿真完整 PCIe/IOMMU/CXL（含 PciHostTLM / PciDeviceTLM / DmaDeviceTLM / ComputeUnitTLM）| **仅仿真 dGPU 板卡**（BAR MMIO + Config Space + MSI-X + 多板卡 + backdoor + DMA translate cb）|
> | **IOMMU 归属** | CppTLM 实现（`cpptlm_iommu_domain`）| **UsrLinuxEmu `src/system_hw/iommu/`**（功能级保真）|
> | **CXL.mem 归属** | CppTLM 实现（`cpptlm_cxl_memdev`）| **UsrLinuxEmu `src/system_hw/cxl_memdev/`**（功能级保真）|
> | **新增 ABI** | SMU/PSP/vm_handle_fault/tlb_flush/kfd_signal/hqd_save/hmm_register 等 15 项 v2 扩展 | **无**（v5.5+ 评估，per ADR-088 §Open Questions）|
> | **DMA translate cb** | ❌ 缺失 | **新增**（`cpptlm_emulator_register_dma_translate_cb`，per §D3.8）|
> | **HAL fn-ptr 数** | "68 fn-ptrs（per ADR-023 §D4）" | **68 fn-ptrs 保持不变**（per ADR-088 P4）— 此项一致 |
> | **模式** | dGPU amdgpu mode (48 forward) + nouveau mode (26 forward) | **单一模式**（dGPU 参考设计，不分 vendor）|
> | **版本字符串** | `"v2.0-amdgpu"` / `"v2.0-nouveau"` | **`"v1.0-dgpu-v0"`** |
>
> **保留价值**：
> - **§2 现状快照**（UsrLinuxEmu HAL Surface + Sim Module Surface + CppTLM Current State）有事实参考价值——可作为背景资料阅读
> - **§3.11-3.25 Linux 内核深度调研**（SMU/PSP/HMM/PRAMIN 等）有事实价值——但所有"Resolution: 新增 forward `cpptlm_emulator_*`"已**全部作废**（这些项目归 v5.5+ 评估或由 UsrLinuxEmu `src/system_hw/` 处理）
>
> **后续行动**：
> - ❌ 本文档**不**进入 `docs/05-advanced/` 实施 spec 链路（那是 ADR-088 + [cpptlm-v4-implementation-handoff.md](../05-advanced/cpptlm-v4-implementation-handoff.md) 的角色）
> - ❌ 本文档**不**作为任何 ADR 评审的参考资料
> - ✅ 保留在 `docs/architecture/` 作为**事实背景**（不删除，仅标记）

---

## ⚠️ 时效性说明（2026-08-16）

本文档是 ADR-088 **早期方案**（55-57 ABI L1 API 级仿真，E.0-E.7 子阶段）的差距分析，**仅作历史参考**。当前已 Accepted 的设计以 [ADR-088](../00_adr/adr-088-dgpu-complete-simulation.md) 为准：**L3 寄存器级仿真 + CppTLM 仅 dGPU 板卡（23 ABI）+ 系统 IOMMU/CXL.mem 由 UsrLinuxEmu `src/system_hw/` 功能级仿真**。

---

## 1. Purpose

This document captures the **architectural gap** between:

- **Current state**: UsrLinuxEmu's internal `plugins/gpu_driver/sim/` module — handcrafted C++ state machines (HardwarePullerEmu FSM, GlobalScheduler, GpuQueueEmu ring buffer, semaphore manager, fence_id tracker, DMA-coherent pool, BAR simulator, page fault handler)
- **Target state**: `plugins/gpu_driver/hal/hal_user.cpp` in-place replaced by CppTLM-driven simulation via `libcpptlm_emulator.so`, providing a realistic APU SoC simulation (PCIe DF + GPU CP + Compute Cluster + Memory Controller + NoC + DMA + interrupt topology)

The end goal (per `roadmap.md` §终态蓝图): driver code developed in UsrLinuxEmu should behave **EXACTLY** like on real hardware — same MMIO ordering, same fence completion semantics, same MSI-X interrupt delivery, same DMA coherent vs streaming semantics.

---

## 2. Current Architecture Snapshot

### 2.1 UsrLinuxEmu HAL Surface

- `struct gpu_hal_ops` with 68 function pointers (`plugins/gpu_driver/hal/gpu_hal.h:29-700`)
- Single implementation: `hal_user.cpp` (880 lines, 72 ops-> call sites)
- `hal_user_context` is 23-field C++ struct holding registers, fences, sem_mgr, queues, pullers, interrupt handlers, iommu_mappings, events, etc.
- Append-only governance (ADR-023 §D4): no fn-ptr removed since ADR-023, all 65+3 = 68 stable

### 2.2 Sim Module Surface (kept for fall-back path)

| Module | Purpose | Lines | Coupling |
|--------|---------|------:|---------|
| `sim/vram_store.{h,cpp}` | BAR2 mmap backing | ~200 | Direct ctx member |
| `sim/semaphore_manager.{h,cpp}` | hal_sem_* impl | ~400 | Strong typed ctx member |
| `sim/fence_id.{h,cpp}` | fence_id_alloc/signal/check | ~150 | Direct ctx member |
| `sim/mem_pool.{h,cpp}` | 10 mem_pool_* impls | ~600 | Opaque handle space |
| `sim/graph.{h,cpp}` | 7 graph_* impls | ~500 | shared_ptr<Graph> |
| `sim/stream_capture.{h,cpp}` | 3 stream_capture_* | ~250 | shared_ptr<StreamCapture> |
| `sim/hardware/hardware_puller_emu.{h,cpp}` | puller_create/destroy + FSM | ~700 | shared_ptr<HardwarePullerEmu> |
| `sim/hardware/doorbell_emu.h` | doorbell_ring receiver | ~50 | HardwarePullerEmu owns |
| `sim/scheduler/global_scheduler.{h,cpp}` | Channel manager | ~800 | puller ctx arg |
| `sim/hardware/method_codec.h` | method_codec_encode | ~200 | Pure function |
| `sim/green_context.{h,cpp}` | hal_green_context_* | ~200 | shared_ptr<GreenContext> |
| `sim/pdl.{h,cpp}` | hal_pdl_* | ~150 | shared_ptr<PDL> |
| `sim/backdoor_preempt.{h,cpp}` | hal_preempt/hal_resume | ~150 | ChannelManager dep |
| `sim/dma_coherent_pool.{h,cpp}` | DMA coherent windows | ~300 | Per ADR-073 |
| `sim/page_fault_handler.{h,cpp}` | iommu page faults | ~250 | KFD process linkage |
| `sim/sim_event.{h,c}` | event_signal/wait/notify | ~150 | condvar + bool[256] |
| `sim/bar_sim.{h,cpp}` | BAR0/BAR5 register emulation | ~400 | PcieEmu descendant |

### 2.3 CppTLM Current State

- **`cpptlm_core`** = STATIC library at `CppTLM/src/CMakeLists.txt:53`
- **CP GPU modules shipped** (Phase 7.A done 2026-06-11):
  - `include/tlm/gpu/gpu_tlm.hh` (GPUTLM v0 black-box initiator, 207 lines)
  - `include/tlm/gpu/gpu_compute_unit_tlm.hh` (compute_unit_base + ComputeUnitTLM skeleton, Phase 7.B scope)
  - `include/tlm/gpu/gpu_soc_tlm.hh`, `gpu_mesh_noc_tlm.hh`, `async_completion_adapter.hh`
- **CP GPU modules planned (🟡)** with blueprint docs only:
  - `PCIBridgeTLM` (gpu-pcie_bridge.md — 0 code, only spec)
  - `PciHostTLM` + `PciDeviceTLM` (io-pci.md — 0 code, only spec)
  - `DmaDeviceTLM` (io-dma.md — 0 code, only spec)
  - `ComputeUnitTLM` full implementation (gpu-compute-unit.md — partial skeleton)
  - `TCC_TLM` (gpu-tcc.md — 0 code)
  - `GpuMeshNocTLM` full mesh NoC (gpu-noc-mesh.md — 0 code)
- **Existing C ABI surface** (`include/cudart/cpptlm_bridge.h`):
  - `cpptlm_attach_bridge(CppTLMBridge*)` (L162)
  - `cpptlm_detach_bridge()` (L169)
  - `cpptlm_set_driver(void*, PtxEmuDriverApi)` (L213) — PTX-EMU specific, NOT for UsrLinuxEmu HAL
- **Build output**:
  - `${CMAKE_BINARY_DIR}/bin/cpptlm_sim` (main CLI)
  - `${CMAKE_BINARY_DIR}/lib/libcpptlm_core.a` (static)

---

## 3. Architectural Gaps

### 3.1 Critical: 5 CppTLM Modules Need From-Scratch Implementation

**Gap**: Existing blueprints describe `PCIBridgeTLM`, `PciHostTLM`, `PciDeviceTLM`, `DmaDeviceTLM`, and full `ComputeUnitTLM`, but **no `.hh` or `.cc` files exist** for these in `CppTLM/include/tlm/io/` or `CppTLM/src/tlm/io/`.

**Impact**:
- Without `PciHostTLM` + `PciDeviceTLM`: no PCIe config space emulation, no BAR advertisement/sizing, no link training → driver `pci_enable_device()` fails
- Without `PCIBridgeTLM`: no BAR0/1/2/5 register path → register_read/write returns -ENOSYS
- Without `DmaDeviceTLM`: no DMA bounce / coherent semantics → mem_read/write hits null ptr
- Without `ComputeUnitTLM`: no GPFIFO fetch / dispatch / completion → fence never signals
- Without `MemoryTLM` HBM mode: no VRAM backing store → BAR2 mmap fails

**Resolution**: Treat these 5 modules as **NEW CppTLM subprojects**, to be tracked as UsrLinuxEmu change `cpptlm-pcie-bridge-emu` (proposed Q4 2026) feeding into CppTLM as PR.

### 3.2 Critical: SHARED Library Output Missing

**Gap**: `cpptlm_core` is STATIC; UsrLinuxEmu plugin needs `dlopen(libcpptlm_emulator.so)` per ADR-076 PTX-EMU pattern.

**Impact**: Cannot dlopen STATIC library (no runtime symbol resolution).

**Resolution**: Two-step — (1) `cpptlm_core` STATIC → SHARED, (2) introduce thin SHARED wrapper `libcpptlm_emulator.so` exposing only **43-56 C entry points**（v2 修订后：dGPU amdgpu mode = 43；nouveau mode = 56）+ internally linking `cpptlm_core`. Single-line CMake change to `add_library(cpptlm_core SHARED ...)` plus new `add_library(cpptlm_emulator SHARED src/c_emulator/c_emulator_api.cc)`.

### 3.3 Critical: C ABI Surface Is PTX-EMU Specific

**Gap**: Existing 3 C functions in `include/cudart/cpptlm_bridge.h` are all for PTX-EMU bridge (cpptlm_attach_bridge / cpptlm_detach_bridge / cpptlm_set_driver).

**Impact**: Zero of these serve UsrLinuxEmu HAL needs.

**Resolution**: Create new header `CppTLM/include/cpptlm_emulator.h` (independent file from cudart/) with **55-57 C functions**（v2 修订后总数：dGPU amdgpu mode 43 + nouveau mode 14，包含 11 sim→drv callback + 1 register）。Mirror this header to `UsrLinuxEmu/external/cpptlm_emulator/cpptlm_emulator.h` via vendoring or symlink.

### 3.4 Critical: 8 sim→drv Callback Functions Missing in Design

**Gap**: Original proposal only had 20 drv→sim forward fn-ptrs. CppTLM simulation must trigger events back into driver (interrupt delivery, fence signal, doorbell consume, page fault, power state change, error, reset complete, flush complete).

**Impact**: Without callbacks, CppTLM runs as black box; driver never sees fence completion (timeout), MSI-X interrupt (kernel panic), or FLR completion (stuck recovery).

**Resolution**: Add 8 callback function pointer typedefs in `cpptlm_emulator.h` + 1 register function `cpptlm_emulator_register_callbacks()`. `hal_user.cpp` registers all 8 callbacks pointing to local static functions, which marshal events to `kernel_workqueue` (per ADR-060) for async dispatch.

**v2 扩展**：nouveau path 新增 3 个 callback（fault_buffer_notify / replay_done / perflvl_changed）→ 总计 **11 callback**。

### 3.5 Major: BAR Backing Store Cross-Project Mmap Integration

**Gap**: User processes must `mmap(BAR2, VRAM size)` and read/write VRAM. In current sim mode, `sim/vram_store.cpp` allocates `void* vram_mmap` via host `mmap(0, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)`. In CppTLM mode, the VRAM lives inside CppTLM `MemoryTLM` process — must cross dlsym boundary.

**Impact**: User process cannot read VRAM contents if mmap returns a pointer that doesn't reflect CppTLM's internal state.

**Resolution**: Three options evaluated:
1. **In-process VRAM mirror** (recommended): CppTLM `MemoryTLM` uses `MAP_SHARED` mmap'd region; `cpptlm_emulator_vram_host_ptr()` returns the host pointer; UsrLinuxEmu's user-mode mmap returns the same pointer via BAR2 ioremap hook (per ADR-069 Stage 4.1 already ship)
2. fd-passing via SCM_RIGHTS: more complex, no win
3. Shared memory object: works but adds setup ceremony

Recommended = option 1 (in-process mirror, leveraging existing ADR-069 BAR backing pattern).

### 3.6 Major: MSI-X Interrupt Topology

**Gap**: Real driver paths use MSI-X for multiple interrupt vectors (per ADR-048). CppTLM PciDeviceTLM blueprint describes MSI capability but no MSI-X table or per-vector masking.

**Impact**: KFD path (which uses many vectors for events per process) cannot be tested.

**Resolution**: Implement MSI-X Capability Structure in PciDeviceTLM:
- MSI-X table at offset 0x40 in PCI config space (per PCIe 4.0 §7.7.2)
- Per-vector address/data pairs
- Per-vector mask bits
- Pending bits array (one bit per vector)
- Function-level callback routing to cpptlm_intr_deliver_cb(vector, user_data)

### 3.7 Major: MMIO Ordering Semantics

**Gap**: Real hardware has relaxed vs sequential ordering (ARMv8 + PCIe §2.4). Drivers using `writel_relaxed()` vs `writel()` rely on this distinction.

**Impact**: Drivers relying on relaxed ordering may appear to work in sim (no enforcement) but crash on real hardware due to subtle ordering.

**Resolution**: CppTLM `PciBridgeTLM` must model at least 3 ordering modes:
- **Device MMIO write**: relaxed (no barrier needed for completion)
- **Posted vs Completed**: PCIe Completion TLP required for read response
- **Fence semaphores**: explicit acquire/release semantics

Configurable per-BAR via JSON `mmio_ordering` parameter (default = sequential).

### 3.8 Major: Doorbell Wake-up Latency Injection

**Gap**: Real PCIe doorbell write takes 100-500 cycles for CP fetch. Current sim `doorbell_ring` is synchronous (set + return).

**Impact**: Driver test "submit doorbell + immediately fence_read" would pass in sim (always signaled) but fail on real hardware (fence not yet signaled).

**Resolution**: CppTLM `PCIBridgeTLM::ring_doorbell` posts a delayed callback via internal EventQueue, scheduled `T_doorbell_latency` cycles ahead. `fence_read` checks inflight count.

### 3.9 Minor: FLR / Reset / Hot-Reset

**Gap**: Driver path `pci_reset_function()` needs PCIe Function Level Reset to actually reset device state.

**Impact**: GPU hang recovery silently no-ops in sim.

**Resolution**: PciDeviceTLM implements FLR per PCI 3.0 §6.6.2: secondary bus reset + config space reload + all BAR backing zero'd + all MSI-X vectors masked. Callback `cpptlm_reset_complete_cb_t` fires when FLR done.

### 3.10 Minor: Power State (D0/D3)

**Gap**: Modern amdgpu has runtime PM; `pci_set_power_state()` puts device in D3 (sleep) where MMIO is inaccessible.

**Impact**: Power management paths silently no-op.

**Resolution**: PciDeviceTLM implements `cpptlm_emulator_power_state(D0|D3)`. D3 state = all MMIO returns -1; D0 = normal. Callback `cpptlm_power_cb_t(state)` fires on transition.

---

## 3.11-3.25: v2 调研新增 15 项 Gap（2026-08-14 Linux kernel 深度调研）

### 3.11 CRITICAL: SMU 邮箱 / SMC 消息（驱动 DPM/PG/reset 全部依赖）

**Gap**: amdgpu 驱动通过 SMU（RISC-V 协处理器）控制 DPM/GPU 时钟/PG/reset。两条独立路径：

1. 寄存器邮箱路径（`mmRLC_SMU_ARGUMENT_1..4` + `mmSMU_RLC_RESPONSE` 轮询）
2. SMC 消息路径（`smu_cmn_send_smc_msg_with_params` 0..N arg API）

**Impact**: `amdgpu_dpm_set_powergating_by_smu()` 每次发 SMC 消息；`baco_reset`/`mode2_reset` 内部都发 `SMU_MSG_GfxDeviceDriverReset`；50+ SMC 消息 ID（SetHardMinGfxClk、PowerUpVcn 等）。

**Resolution**: 新增 forward `cpptlm_emulator_smu_send_msg(msg_id, params[4], readback[4], timeout_ms)`。

### 3.12 CRITICAL: PSP Firmware Loading（早期启动必经路径）

**Gap**: `psp_init_sos_microcode / _asd_ / _ta_microcode` — PSP 是独立安全处理器，缺则 SMU 通信全废。

**Impact**: 早期启动阶段必须成功否则 `amdgpu_device_init` 失败。SOS 固件管理 SMC 接口，缺它 SMU 消息全废。

**Resolution**: 新增 forward `cpptlm_emulator_psp_load_fw(type, fw_addr, fw_size)` + `cpptlm_emulator_psp_query_status()`。

### 3.13 CRITICAL: IH-Based Retry Fault（AMD 不用 ATS/PRI）

**Gap**: AMD KFD/SVM 通过 GPU IH ring 上报页错误（不是 PCIe PRI）。`iommu_enable_prq` 仅 Intel IOMMU 使用。

**Impact**: 缺失 `vm_handle_fault()` fn-ptr → HMM/SVM 完全不可用 → ROCm/HIP/COMGR 100% 失败。

**Resolution**: 新增 forward `cpptlm_emulator_vm_handle_fault(pasid, vmid, node_id, addr, ts, write_fault)` + `cpptlm_emulator_tlb_flush_pasid/_vmid`。

### 3.14 HIGH: KFD Signal Page（用户态主通信通道）

**Gap**: `kfd_signal_event_interrupt(pasid, ctx_id, 32, false)` 写入 per-PASID signal page slot。

**Impact**: ROCm 用户态主通信通道；缺 → 用户态 event poll 永远超时。

**Resolution**: 新增 forward `cpptlm_emulator_kfd_signal_event(pasid, slot_index, event_data)` + `cpptlm_emulator_kfd_signal_page_alloc()`。

### 3.15 HIGH: HQD Context Save/Restore（抢占式调度必需）

**Gap**: `CP_HQD_CTX_SAVE_*` 寄存器 + `amdgpu_compute_mqd_save/load`。

**Impact**: 多 KFD 进程抢占式调度必需；缺 → KFD 多进程路径失效。

**Resolution**: 新增 forward `cpptlm_emulator_hqd_save/restore(ring_id, ctx_save_addr)`。

### 3.16 HIGH: mmu_interval_notifier（不是标准 mmu_notifier）

**Gap**: AMD 使用 `mmu_interval_notifier`（更细粒度锁定），`.invalidate` 返回 `bool`，需 `mmu_interval_set_seq()`。

**Impact**: KFD process eviction 路径全废。

**Resolution**: 新增 callback `cpptlm_emulator_mn_invalidate(mni_handle, start, end, cur_seq)` + `cpptlm_emulator_mn_set_seq()`。

### 3.17 HIGH: Golden Registers 批量加载（启动时静默错误）

**Gap**: `amdgpu_device_init_golden_registers()` — ~100 寄存器 per IP block，按 topology 顺序加载。

**Impact**: 错误时 GPU 启动异常但不报错。

**Resolution**: 新增 forward `cpptlm_emulator_apply_golden_registers(ip_id, inst_id)`（sim 内部查表写入）。

### 3.18 HIGH: HMM `devm_memremap_pages` + `migrate_to_ram` 双回调

**Gap**: SVM 依赖 HMM device-private memory，需要双向回调（`folio_free` + `migrate_to_ram`）。

**Impact**: SVM 路径不可用。

**Resolution**: 新增 3 个 forward `cpptlm_emulator_hmm_register / migrate_to_ram / folio_free`。

### 3.19 HIGH: Doorbell + MMIO + CWSR 三种 mmap 模式

**Gap**: KFD chardev 通过 `mmap` 暴露 3 类 IO 区域（doorbell/MMIO/CWSR），全部走 `io_remap_pfn_range`。

**Impact**: 缺 → KFD 用户态 mmap 失败。

**Resolution**: 新增 3 个 forward `cpptlm_emulator_mmap_doorbell / _mmio / _cwsr`。

### 3.20 MEDIUM: Per-VM PT Update（SDMA 路径）

**Gap**: `amdgpu_vm_update_range` + `amdgpu_vm_clear_freed`。

**Impact**: per-VM 页表更新失败 → KFD 进程无法访问分配内存。

**Resolution**: 新增 forward `cpptlm_emulator_vm_pt_update / _clear(vm, ...)`。

### 3.21 MEDIUM: Eviction Fence（BO 迁移跟踪）

**Gap**: `struct amdgpu_amdkfd_fence`（含 mm/svm_bo/context_id）— amdkfd BO 跟踪核心机制。

**Impact**: amdkfd BO evict 路径失效。

**Resolution**: 新增 forward `cpptlm_emulator_eviction_fence_create / signal`。

### 3.22 MEDIUM: PRAMIN 1MB 滑动窗口（nouveau 特有）

**Gap**: `BAR0 + 0x700000`，NV_PBUS_BAR0_WINDOW 寄存器控制窗口位置（64KB 对齐，1MB 窗口）。

**Impact**: nouveau 早期引导无法访问 VRAM 内 inst mem/Falcon ucode。

**Resolution**: 新增 3 个 forward `cpptlm_emulator_pramin_window_set / _read / _write`。

### 3.23 MEDIUM: BAR1 = 256MB GART/VRAM（nouveau 反转）

**Gap**: nouveau BAR1 = VRAM 小页 TTM；amdgpu BAR2 = 2MB Doorbell BAR（地址空间反转）。

**Impact**: nouveau 路径地址映射完全失效。

**Resolution**: 新增 forward `cpptlm_emulator_bar1_size / _map + _bar2_window_set`。

### 3.24 MEDIUM: Nouveau Fence 三代（NV06E → NV906F SEMAPHORE → NVC36F SEM+Non-stall）

**Gap**: c36f 必须发 `NON_STALL_INTERRUPT` 触发唤醒 + `MEM_OP` MEMBAR 保证顺序。

**Impact**: nouveau fence 不能跨 generation 工作。

**Resolution**: 扩展 `cpptlm_emulator_fence_emit32 / sync32` 加 generation enum 参数。

### 3.25 MEDIUM: MMU Fault Buffer（Hopper+ replayable faults）

**Gap**: `tu102_fault_buffer_notify` + NVKM_FAULT_BUFFER_EVENT_PENDING — Hopper+ 必备；与 amdgpu VM_L2_PROTECTION_FAULT_STATUS 机制完全不同。

**Impact**: Hopper+ GPU 故障无 replayable 路径。

**Resolution**: 新增 forward `cpptlm_emulator_fault_buffer_init / _query` + callback `cpptlm_fault_buffer_notify_cb_t`。

### 3.26 MEDIUM: PFIFO IB-mode Push（NV50+）+ USERD MMIO

**Gap**: IB entry 写入 + WRITE_PUT(userd, 0x8c) + 轮询 GET 指针。

**Impact**: nouveau push submit 数据竞争。

**Resolution**: 新增 forward `cpptlm_emulator_pfifo_push_ib / _push_wait / _userd_write`。

---

## 3.27 综合：v2 总 Gap 数（v1 + v2 调研）

| Gap 类别 | 数量 | 关键引用 |
|----------|----:|----------|
| v1 原始（§3.1-3.10）| 10 | PCIe modules + SHARED lib + C ABI + callbacks + BAR mmap + MSI-X + MMIO order + doorbell latency + FLR + power |
| v2 新增（§3.11-3.26）| **15** | SMU/PSP/retry fault/KFD Signal/HQD/mmu_interval_notifier/Golden/HMM/3×mmap/PT update/Eviction/PRAMIN/BAR1/Fence3/FaultBuf/IBPush |
| **总计** | **25** | — |

---

## 4. Architecture Decision

### 4.1 Chosen Pattern: In-place `hal_user.cpp` Replacement (NOT sibling `hal_emu.cpp`)

**Rejected pattern** (original proposal): Create `plugins/gpu_driver/hal/hal_emu.cpp` that forwards 20 fn-ptrs to CppTLM and inherits 48 from hal_user. **Oracle rejected this** because:
- 48 fn-ptrs internally access C++ class types (`shared_ptr<GpuQueueEmu>`, `SemaphoreManager`, `HardwarePullerEmu`) that don't exist in CppTLM
- `void* ctx` cast would segfault when CppTLM backend active
- Violates ADR-023 §D5 (HAL boundary rules)

**Chosen pattern**: Modify `hal_user.cpp` in-place. Each of the **34-44 fn-ptrs** (v2 修订后：dGPU amdgpu mode = 34, nouveau mode = 44) that need CppTLM gets a `if (ctx->use_cpptlm_backend) { cpptlm_emulator_*(...) } else { /* original sim/* path */ }` branch. No new HAL impl; one HAL impl with runtime backend switch. Backend selection via `cpptlm_emulator_get_version()` distinguishes dGPU amdgpu vs nouveau paths.

### 4.2 File Layout (Final)

```
UsrLinuxEmu/
├── plugins/gpu_driver/
│   ├── drv/                  (② — UNCHANGED, zero modification)
│   ├── hal/
│   │   ├── gpu_hal.h         (68 fn-ptrs — UNCHANGED per ADR-023 §D4 append-only)
│   │   ├── hal_user.h        (+ 2 fields: cpptlm_emulator_handle, use_cpptlm_backend)
│   │   ├── hal_user.cpp      (in-place: 28 fn-ptrs add CppTLM dispatch branch)
│   │   └── hal_mock.cpp      (UNCHANGED — unit test stub)
│   ├── sim/                  (UNCHANGED — mode A fall-back path uses it)
│   ├── shared/               (UNCHANGED)
│   └── plugin.cpp            (+ env var check: USR_LINUX_EMU_USE_CPPTLM)
│
├── external/cpptlm_emulator/
│   └── cpptlm_emulator.h     (vendored or symlinked from CppTLM)
│
├── docs/00_adr/
│   └── adr-088-dgpu-complete-simulation.md  (this ADR)
│
└── docs/architecture/
    └── cpptlm-emu-integration-gap-analysis.md  (this doc)
```

```
CppTLM/
├── include/
│   ├── cpptlm_emulator.h     (NEW — 28 C API declarations)
│   └── cudart/cpptlm_bridge.h (UNCHANGED — PTX-EMU ABI)
├── src/
│   ├── c_emulator/
│   │   ├── c_emulator_api.cc           (NEW — 28 C functions)
│   │   └── c_emulator_callbacks.cc     (NEW — callback bridge to EventQueue)
│   ├── tlm/
│   │   ├── io/
│   │   │   ├── pci_host_tlm.{hh,cc}    (NEW — Phase 8)
│   │   │   ├── pci_device_tlm.{hh,cc}  (NEW — Phase 8)
│   │   │   └── dma_device_tlm.{hh,cc}  (NEW — Phase 8)
│   │   └── gpu/
│   │       ├── pcie_bridge_tlm.{hh,cc} (NEW — Phase 8)
│   │       └── memory_tlm_hbm.{hh,cc}  (NEW — HBM mode extension)
│   └── CMakeLists.txt         (cpptlm_core STATIC→SHARED + new cpptlm_emulator target)
├── configs/
│   └── dgpu_v0.json           (NEW — dGPU topology for UsrLinuxEmu)
└── docs/soc_arch/adr/
    ├── ADR-SOC-06-dGPU-v0-for-UsrLinuxEmu.md (NEW)
    ├── ADR-SOC-07-c-emulator-api.md          (NEW)
    └── ADR-SOC-08-dgpu-v0-json-topology.md   (NEW)
```

### 4.3 HAL Function Distribution（**🚨 已重写 2026-08-16**：对齐 ADR-088 §D6.1）

> **🚨 本节 v3.0 之前内容（48 forward + 8 callback = 57 ABI；dGPU amdgpu/nouveau 双 mode）已全部作废**——ADR-088 范围收窄后，CppTLM 仅 23 ABI，单一模式（dGPU 参考设计，不分 vendor）。
>
> **重写依据**：[ADR-088 §D6.1 HAL fn-ptr vs 系统 IOMMU 仿真层级关系](../00_adr/adr-088-dgpu-complete-simulation.md) + [ADR-088 §D5 23 ABI 清单](../00_adr/adr-088-dgpu-complete-simulation.md)。

**HAL 68 fn-ptrs（UsrLinuxEmu 内部 HAL 接口）vs CppTLM 23 ABI（dlopen 边界）的映射**：

| HAL 类别 | HAL fn-ptr 数 | 走 CppTLM 23 ABI | 走 UsrLinuxEmu `src/system_hw/` | 留在 `sim/`（mode A fallback）|
|----------|--------------:|------------------:|--------------------------------|------------------------------:|
| **BAR MMIO 读/写** | 2 | ✅ 走 `cpptlm_emulator_mmio_read/write` | — | 2（mode A 走 `sim/bar_sim.cpp`）|
| **寄存器查询** | 1 | ✅ 走 `cpptlm_emulator_lookup_register` | — | 1（mode A 走 `sim/bar_sim.cpp`）|
| **多板卡枚举** | 0（HAL 不涉及）| ✅ 走 `cpptlm_emulator_get_device_count/info` + `create_by_id` | — | — |
| **PCIe Config Space** | 0（HAL 不涉及）| ✅ 走 `cpptlm_emulator_pcie_config_read/write` | — | — |
| **backdoor 调试** | 0（HAL 不涉及）| ✅ 走 `cpptlm_emulator_backdoor_read/write` + `register_backdoor_cb` | — | — |
| **MSI-X** | 0（HAL 不涉及）| ✅ 走 `cpptlm_emulator_msix_init/update_pending/clear_pending` | — | — |
| **DMA translate** | 0（HAL 不涉及）| ✅ 走 `cpptlm_emulator_register_dma_translate_cb` | — | — |
| **IOMMU 域操作** | 2（per ADR-061：`hal_iommu_map` + `hal_iommu_unmap`）| — | ✅ 走 `src/system_hw/iommu/` 内部 API（5 函数：domain_alloc / attach_dev / map / unmap / iova_to_phys）| — |
| **DMA / Memory ops** | 5 | — | — | 5（mode A 走 `sim/vram_store.cpp`）|
| **Fence / Sync** | 5 | — | — | 5（mode A 走 `sim/fence_id.cpp`）|
| **Interrupt / Event** | 6 | — | — | 6（mode A 走 `sim/sim_event.cpp` + ADR-060 `kernel_workqueue`）|
| **Semaphore / Green / PDL** | 9 | — | — | 9（mode A 走 `sim/semaphore_manager.cpp`）|
| **Graph / Stream Capture** | 10 | — | — | 10（mode A 走 `sim/graph.cpp`）|
| **Mem Pool** | 10 | — | — | 10（mode A 走 `sim/mem_pool.cpp`）|
| **Queue / Puller / Scheduling** | 13 | — | — | 13（mode A 走 `sim/hardware/hardware_puller_emu.cpp`）|
| **Method / Util** | 5 | — | — | 5（mode A 走 `sim/hardware/method_codec.cpp`）|
| **Kernel Module** | 3（per ADR-076 PTX-EMU）| — | — | 3（per ADR-076 已 ship，**与 CppTLM 共存不冲突**）|
| **Total** | **71**（行求和：2+1+2+5+5+6+9+10+10+13+5+3）| **3**（BAR MMIO 2 + 寄存器查询 1）| **2**（IOMMU 域操作 per ADR-061）| **69**（mode A fallback，含 3 个 CppTLM 替代路径）|

> **⚠️ 算术修正说明（Oracle 评审 v0.3 → v0.4）**：v0.3 表格 Total 行声称 "67 fn-ptrs + 1 helper = 68"，但行求和实为 71；"走 CppTLM" 列声称 6 但行求和实为 3；"留 sim/" 列声称 60 但行求和实为 69——三组数字均不可从表内推出。**本版本改用行求和作为权威：71 总 / 3 走 CppTLM / 2 走 src/system_hw/ / 69 留 sim/**。**HAL 68 计数可能在某些类别中重复计数**（如 DMA / Memory ops 5 + Fence / Sync 5 可能有概念重叠）；建议 v5.5+ 实施前对本表做完整 HAL 结构计数核对。

**关键结论**：

1. **HAL 68 fn-ptrs 保持不变**（per ADR-088 P4 + ADR-023 §D4 append-only 治理；本表 71 含类别重叠）
2. **HAL 3 个 fn-ptrs 走 CppTLM 23 ABI**（BAR MMIO 2 + 寄存器查询 1）+ **2 个 fn-ptrs 走 UsrLinuxEmu `src/system_hw/iommu/`**（per ADR-061）
3. **HAL 其余 69 个 fn-ptrs 留在 `sim/`**（mode A fallback path，含 3 个 CppTLM 替代路径）
4. **无 dGPU amdgpu / nouveau mode 区分**——ADR-088 是单一 dGPU 参考设计

**v3.0 → v4.0 修订偏差**（**已废弃**）：

- dGPU amdgpu mode forward: 48 → **0 走 CppTLM forward**（HAL 仅 6 个走 CppTLM ABI；其余走 sim/）
- HAL fn-ptr 走 CppTLM 的数量：48 → **6**（-42）
- 偏差原因：v3.0 把 HAL fn-ptrs 与 CppTLM forward ABI 直接混淆——HAL 是 UsrLinuxEmu 内部 C++ 类成员函数接口；CppTLM ABI 是 dlopen 边界 C 接口。两者**不一一对应**（HAL 6 fn-ptrs 通过 `hal_user.cpp` if/else 分支动态 dispatch 到 CppTLM 23 ABI）

### 4.4 Environment Variable Trigger（**🚨 已重写 2026-08-16**：对齐 ADR-088 §D6.1）

```bash
# Default (mode A) — 不变
./build/bin/cli
# → hal_user_init() loads hal_user_context + uses sim/* backend

# Mode B — CppTLM-driven simulation (单一 dGPU 参考设计模式)
USR_LINUX_EMU_USE_CPPTLM=1 \
USR_LINUX_EMU_CPPTLM_REGDB=/path/to/cpptlm_regs/dgpu_v0/registers.yaml \
./build/bin/cli
# → hal_user_init() detects env var, dlopen libcpptlm_emulator.so
# → cpptlm_emulator_get_version() returns "v1.0-dgpu-v0"（per ADR-088 §D6.2）
# → HAL 6 fn-ptrs dispatch 到 CppTLM 23 ABI（BAR MMIO + register lookup + 多板卡）
# → HAL 2 fn-ptrs 走 UsrLinuxEmu src/system_hw/iommu/（per ADR-061）
# → HAL 其余 60 fn-ptrs 继续走 sim/（mode A fallback）
```

> **🚨 移除**：v3.0 的 `USR_LINUX_EMU_CPPTLM_ARCH=amdgpu|nouveau` env var 已删除——ADR-088 是单一 dGPU 参考设计，无 vendor 区分。

---

## 5. Risk Assessment

| Risk | Probability | Impact | Mitigation |
|------|------------:|-------:|------------|
| `cpptlm_core` STATIC→SHARED breaks Phase 8.B (gpu-soc) build | Medium | High | Coordinate with CppTLM maintainer; isolate symbol exports via visibility preset `hidden` |
| BAR mmap cross-process boundary not solvable in-process | Low | Critical | All 3 CppTLM PCIe modules run in-process via dlopen; same address space |
| MSI-X table implementation drifts from real spec | Medium | Medium | Reference PCIe 4.0 §7.7.2; add Catch2 test against canonical sequences |
| 8 callback integration with `kernel_workqueue` (ADR-060) deadlocks | Medium | Medium | Async post only — never synchronous dispatch from CppTLM callback |
| ABI version mismatch between UsrLinuxEmu vendored header and CppTLM shipped header | Low | High | Static_assert ABI version + dlopen handshake returns version; abort on mismatch |
| Stage 8.B work-in-progress overlaps with our PCIe module additions | High | Medium | Coordinate change IDs; review CppTLM roadmap before committing |
| Existing 98 Catch2 tests fail in CppTLM backend mode | Medium | High | Run all tests in both modes (mode A baseline + mode B new); 0 regression target |

---

## 6. Migration Plan (24-32 weeks, v2 修订)

> 🚨 **本节内容已过时**（per 2026-08-16 重新定位）：本文档描述的 E.0-E.7 阶段 + 55-57 ABI + dGPU amdgpu/nouveau 双 mode + nouveau path（v2.0-nouveau）等均与 ADR-088 §D2 阶段 1-4 冲突。仅作历史参考；当前实施以 [ADR-088 §D2](../00_adr/adr-088-dgpu-complete-simulation.md) 阶段 1（3-4 周）+ 阶段 2a（2-3 周）+ 阶段 2b/3/4（UsrLinuxEmu 主导）为权威。

**总工期增长原因**：v2 新增 15 项 gap（含 SMU/PSP/SVM/HMM/mu_interval_notifier/3×mmap/PRAMIN/BAR1/Fence3/FaultBuf/IBPush 等），需要新增 **3 个 CppTLM 模块**（HMM/migrate_to_ram、PRAMIN/BAR1_window、MMU fault buffer）和** 13 个新 C ABI forward**（dGPU amdgpu mode），加上 nouveau path 的 10 forward + 3 callback 增量。

| Stage | Weeks | Owner | Deliverable |
|-------|------:|-------|-------------|
| **E.0** CppTLM infra | 2 | CppTLM team | (1) cpptlm_core STATIC→SHARED (2) libcpptlm_emulator.so target (3) cpptlm_emulator.h **55-57 C ABI** (4) ADR-SOC-06/07/08 |
| **E.1** P0 smoke test | 1 | UsrLinuxEmu | dlopen framework + 1 pure-compute fn-ptr (method_codec_encode) CppTLM dispatch verified |
| **E.2** CppTLM PCIe modules | 3-4 | CppTLM team | PciHostTLM + PciDeviceTLM + PCIBridgeTLM + BAR backing + 256B Config Space + MSI-X table |
| **E.3** CppTLM DMA + ComputeUnit | 3-4 | CppTLM team | DmaDeviceTLM + ComputeUnitTLM full impl + MemoryTLM HBM mode + dgpu_v0.json |
| **E.3.5** v2 CppTLM 新模块 | 4-5 | CppTLM team | SMU mailbox + PSP firmware + HMM/migrate_to_ram + SVM TLB flush + HQD save/restore + KFD Signal Page 6 项 CppTLM 新模块 |
| **E.4** UsrLinuxEmu 34 forward fn-ptrs (dGPU amdgpu mode) | 4-5 | UsrLinuxEmu | hal_user.cpp internal **34** fn-ptr dispatch to CppTLM (v1 20 + v2 14) |
| **E.5** UsrLinuxEmu 8 callback + 3 新 callback | 2-3 | UsrLinuxEmu | Register 8 v1 + 3 v2 (mn_invalidate/eviction_fence_signal/hmm_migrate_to_ram) callbacks + kernel_workqueue async dispatch |
| **E.6** Integration + docs | 2-3 | UsrLinuxEmu + CppTLM | TaskRunner smoke + ADR-088升档 Accepted + roadmap + SSOT sync |
| **E.7** nouveau path（已废弃，详见 §0 🚨 横幅） | 4-6 | 双方 | PRAMIN window + BAR1/BAR2 反转 + Nouveau Fence 三代 + MMU fault buffer + PFIFO IB-mode push；nouveau path **已废弃**（per ADR-088 §Open Questions：单一 dGPU 参考设计，不分 vendor）|
| **总计** | **24-32 周** | | dGPU amdgpu 必选 + nouveau 可选 |

---

## 7. Success Criteria

- [ ] `libcpptlm_emulator.so` produces with `nm -D` showing 28 exported symbols
- [ ] `dgpu_v0.json` loads + end-to-end run via `cpptlm_sim` (catch2 test passes)
- [ ] All 98 existing Catch2 tests pass with `USR_LINUX_EMU_USE_CPPTLM=1` (0 regression)
- [ ] New 5 Catch2 tests added for: (a) Config Space read (b) BAR2 mmap (c) MSI-X trigger (d) FLR reset (e) Power state transition
- [ ] ADR-088升档 Accepted after Oracle APPROVED-WITH-CONDITIONS
- [ ] `roadmap.md` updated with new Stage 5 sub-stage "CppTLM EMU integration"
- [ ] SSOT `post-refactor-architecture.md` §1.10 references ADR-088

---

## 8. References

- [ADR-088](../00_adr/adr-088-dgpu-complete-simulation.md) — Main ADR（当前权威设计）
- [ADR-076 PTX-EMU HAL Backend](../00_adr/adr-076-gpgpu-kernel-module-ioctl.md) — Reference pattern (dlopen + append-only HAL)；后续演进推迟已退出（ADR-088 已升 Accepted），详见 [ADR-076 §演进路线图](../00_adr/adr-076-gpgpu-kernel-module-ioctl.md)
- [ADR-036 3 区分](../00_adr/adr-036-three-way-separation.md) — drv/ zero modification principle
- [ADR-023 HAL 接口契约](../00_adr/adr-023-hal-interface.md) — Append-only fn-ptr governance
- [ADR-072 Portability Validation](../00_adr/adr-072-portability-validation.md) — L1/L2/L3 verification
- [ADR-069 BAR/ioremap](../00_adr/adr-069-bar-ioremap-emulation.md) — BAR backing pattern reuse
- [ADR-048 Interrupt Model](../00_adr/adr-048-interrupt-event-model.md) — MSI-X topology
- [ADR-061 HAL IOMMU ops](../00_adr/adr-061-hal-iommu-extension.md) — iommu_map/unmap
- [ADR-060 Message Notification Threading](../00_adr/adr-060-message-notification-threading.md) — kernel_workqueue for async dispatch
- [CppTLM APU SoC Design Spec](../../../../CppTLM/docs/soc_arch/specs/apu-soc-design.md) — Phase 7 architecture reference
- [CppTLM io-pci.md](../../../../CppTLM/docs/soc_arch/modules/io-pci.md) — PciHost/PciDevice blueprint
- [CppTLM gpu-pcie_bridge.md](../../../../CppTLM/docs/soc_arch/modules/gpu-pcie_bridge.md) — PCIBridgeTLM blueprint
- [CppTLM io-dma.md](../../../../CppTLM/docs/soc_arch/modules/io-dma.md) — DmaDeviceTLM blueprint

---

**Last verified**: 2026-08-14
**Auditor**: Oracle (session `ses_fffbcb1d5ffeZzJVDbbys5UXn2`) + Sisyphus (arch-side)
**Next step**: ~~Generate ADR-088 from this gap analysis~~（已完成；ADR-088 已 Accepted，当前设计见 ADR-088 正文）