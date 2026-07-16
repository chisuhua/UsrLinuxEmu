# Stage 1.4 Tier-2 Runtime Penetration Report

> **日期**: 2026-07-05
> **状态**: ✅ Tier-2 全部交付
> **OpenSpec change**: [`stage-1-4-tier2-kfd-integration`](../openspec/changes/archive/2026-07-05-stage-1-4-tier2-kfd-integration/)
> **Worktree**: `.worktrees/stage-1.4-tier2-kfd-integration` (基于 main@`b65acad`)
> **commits**: 13 commits (1 baseline fix + 9 STUB penetration + 1 duplicate fix + 2 kernel-side)

---

## 1. 执行摘要

Stage 1.4 Tier-1（commit `80f6a44`）已交付 handler dispatch + ABI 兼容 + sim 原语骨架，
但诚实标注了 **三类 Tier-2 遗留**：9 个 STUB_HANDLER（return 0）+ mmu_notifier callback
body（TODO）+ IOTLB flush（fprintf stub）。Tier-2 任务是把这些"占位"升级为"真实运行时行为"。

**结果**: 96 个任务全部完成（0/96 → 96/96），73/73 ctest PASS（从 68/68 起步，
+1 baseline fix + 1 duplicate fix + 5 Tier-2 新测试目标），0 regression。

---

## 2. 交付物清单

### 2.1 9 个 STUB_HANDLER 升级（boundary §3.1, design.md D1）

| Handler | commit | 桥接路径 | 测试 |
|---------|--------|---------|------|
| `gpu_ioctl_register_mmu_cb` | `c33d824` | `kfd_sim_register_mmu_cb` (sim bridge, 单 callback 约束) | `test_register_mmu_cb_runtime_standalone` (5 cases) |
| `gpu_ioctl_register_firmware_cb` | `8b6a33d` | `kfd_sim_register_firmware_cb` (占位，firmware load 延后 Stage 2) | `test_register_firmware_cb_runtime_standalone` (5 cases) |
| `gpu_ioctl_create_va_space` | `4261bb4` | `self->ioctl(GPU_IOCTL_CREATE_VA_SPACE)` → IoctlEntry → `handleCreateVASpace` | `test_stub_handlers_tier2_standalone` |
| `gpu_ioctl_destroy_va_space` | `4261bb4` | 同上 → `handleDestroyVASpace` | 同上 |
| `gpu_ioctl_register_gpu` | `4261bb4` | 同上 → `handleRegisterGPU` | 同上 |
| `gpu_ioctl_create_queue` | `4261bb4` | 同上 → `handleCreateQueue` | 同上 |
| `gpu_ioctl_destroy_queue` | `4261bb4` | 同上 → `handleDestroyQueue` | 同上 |
| `gpu_ioctl_map_queue_ring` | `4261bb4` | 同上 → `handleMapQueueRing` | 同上（happy path 因 pre-existing GpgpuDevice::handleMapQueueRing segfault 延后） |
| `gpu_ioctl_query_queue` | `4261bb4` | 同上 → `handleQueryQueue` | 同上 |

### 2.2 mmu_notifier callback body 完整化（boundary §3.3, design.md D2）

- commit `58777e5`:
  - `iommu_invalidate_register_notifier_internal` 调 `mmu_notifier_register`
  - `iommu_invalidate_unregister_notifier_internal` 调 `mmu_notifier_unregister`
  - 不引入新 sim 原语（沿用 1.3 暴露的 10 个 sim_pfh_* / sim_pm_*）
- 测试: `test_mmu_notifier_callback_runtime_standalone` (2 cases: Tier-2 路径 + framework 路径)

### 2.3 IOTLB flush 真实化（boundary §3.2, design.md D3）

- commit `62d2353`:
  - `default_flush_iotlb` 替换 fprintf stub 为真实 page-table walk
  - 遍历 `iommu_domain_state->iova_to_phys`，count [iova, iova+sz) 范围内 entries
  - 不修改 page table（`iommu_unmap` 已删 entries，本 hook 是 post-unmap 信号）
  - 纯用户态，**不依赖 host kernel**（无 vfio, 无 /dev/iommu）
- 测试: `test_iommu_invalidate_runtime_standalone` (3 cases: null, valid, range)

### 2.4 Pre-existing 修复（独立 hotfix commit，不混入 Tier-2）

- commit `2322429` (baseline): `sim/page_fault_handler.h` + `sim/page_migration.h` 缺失
  → 创建 C ABI headers，修复 e2e test null mm 参数问题。**纠正了 LC3 "8/8 PASS" 的不完整判定**。
- commit `c8f986c` (pre-Tier-2): 删除 `GpgpuDevice::ioctl/open/close` 在 `gpu_drm_driver.cpp` 的
  重复定义（pre-existing duplicate symbol bug，暴露于测试链接时）。保留 `gpgpu_device.cpp`
  中的 canonical IoctlEntry dispatch 版本。

---

## 3. 关键设计决策执行情况

| 决策 | 设计要求 | 实际执行 | 一致性 |
|------|---------|---------|--------|
| **D1** STUB 升级策略 | 桥接到既有 GpgpuDevice 实现 | `self->ioctl(...)` → IoctlEntry → `handleXxx` | ✅ |
| **D2** mmu_notifier callback | 最小可行 callback，不引入新 sim 原语 | 沿用 1.3 暴露的 10 个 sim 接口 | ✅ |
| **D3** IOTLB flush | 用户态 page table invalidation | unordered_map iteration + count + log | ✅ |
| **D4** 每个 STUB 独立测试 | 9 个 STUB → 9 个 test_*.cpp | 合并为 2 个 (mmu_cb + firmware_cb 独立；va_space/queue 共享 1 个 stub_handlers_tier2) | ⚠️ 偏离 |
| **D5** 诚实标记替换 | handler 注释显式 `// Tier-2 penetrated: [date]` | 全部 9 个 STUB + 2 kernel 实现均已替换 | ✅ |

**D4 偏离说明**: 5 个 test 文件（mmu_cb, firmware_cb, va_space+queue 合并 1 个, mmu_notifier,
iommu_invalidate）+ 1 个 stub_handlers 合并测试。设计原本要求 9 个独立 test_*.cpp，但
va_space/queue 等 7 个 STUB 共享 GpgpuDevice 公共 dispatch 路径，独立测试无独立价值，
合并为 1 个文件覆盖 12 cases 更有效。

---

## 4. 关键约束遵守情况

### 4.1 G1-G4 边界契约（**不破坏**）

| 边界契约 | 测试 | 结果 |
|---------|------|------|
| G1: `drm_device` 生命周期 = `GpgpuDevice` 生命周期 | `test_uvm_drm_lifecycle_standalone` | ✅ PASS |
| G2: BO 引用计数 | `test_drm_gem_standalone` | ✅ PASS |
| G3: prime 释放顺序 | `test_drm_prime_standalone` + `test_drm_prime_lifecycle_standalone` | ✅ PASS |
| G4: fence 触发时机 | `test_drm_ioctl_dispatch_standalone` | ✅ PASS |

**结论**: 5/5 边界契约测试全绿，Tier-2 升级未破坏任何 Stage 1.2 锁定的契约。

### 4.2 HAL 接口契约（**不预先扩展**）

- 9 个 STUB 升级**不**通过 `struct gpu_hal_ops` (11 个函数指针) 注入
- 全部通过 Tier-1 桥接层 `kfd_sim_bridge.{h,cpp}` 或 `self->ioctl()` → IoctlEntry
- per ADR-027 + ADR-035 治理

### 4.3 显式排除项（boundary §5.2）

按 `kfd-portability-boundary.md` Tier-2 显式排除项：

| 排除项 | 推荐延后 | 实际处理 |
|--------|---------|---------|
| 多文件 KFD 集成 | Stage 3+ | 未触碰（kfd_sim_bridge 仅为单文件 PoC 桥接） |
| 完整 kfd_queue.c queue 生命周期 | Stage 3+ | 未触碰（仅 handleCreateQueue/Destroy 已 Tier-1 实现） |
| IOMMU 真实硬件 invalidation | Stage 2 (vfio) | **诚实记录为 Tier-3 延后**（D3 用户态实现仅 count + log） |
| ATS PRI/PRG response routing | Stage 2+ 条件性 | 未触碰（atlas_protocol.cpp 仍标注 "stage-1.4 if required"） |
| mmu_notifier 真实进程模型 | Stage 2 | 未触碰（SimPageFaultHandler 仍为匿名 struct） |

---

## 5. 已知限制与未解决问题

### 5.1 MAP_QUEUE_RING happy path 测试 segfault

- 现象: `dev.ioctl(0, GPU_IOCTL_MAP_QUEUE_RING, &args)` 在 valid queue 情况下 segfault
- 定位: `GpgpuDevice::handleMapQueueRing` Phase 2.5 shared-memory 绑定路径
- 状态: **pre-existing Tier-1 bug, 非 Tier-2 范围**
- 处理: 测试仅保留"未知 queue_handle 返回 -ENOENT"路径（验证 STUB 已被替换为真实 handler）
- 后续: Stage 2 Phase 2.5 修复（独立任务）

### 5.2 IOTLB flush 仅 count + log

- 现象: `default_flush_iotlb` 遍历 page table 但不修改（已由 iommu_unmap 删除）
- 实际行为: 提供"有多少 entries 被 flushed"的可见性，不触发 sim fault
- 后续: Stage 2 可加 `sim_pfh_inject_fault_with_cause(IOTLB_FLUSH)` 调用

### 5.3 Tier-2 callbacks (mmu_cb, firmware_cb) 注册后不主动调用

- 设计决策: callback 注册到 sim bridge，**调用**依赖 future KFD 驱动
- 当前状态: 仅注册成功可查（`kfd_sim_mmu_cb_is_registered` / `kfd_sim_firmware_cb_is_registered`）
- 后续: Stage 2+ KFD 集成时实际触发

---

## 6. 与 boundary §3 状态映射

| boundary §3.x | Tier-1 状态 | Tier-2 状态（本文） |
|--------------|------------|---------------------|
| §3.1 4 handler logging stubs | 已穿透（MAP/UNMAP/APERTURE/UPDATE_QUEUE，commit `cc6be1b`+`160ddd2`） | N/A |
| §3.1 9 STUB_HANDLERs | STUB | **Penetrated** (commits `c33d824`+`8b6a33d`+`4261bb4`) |
| §3.2 IOTLB flush | fprintf stub | **Real Implementation (user-space)** (commit `62d2353`) |
| §3.2 ATS PRI/PRG | 未实现 | **延后 Stage 2+** (未触碰) |
| §3.3 mmu_notifier callback | TODO stage-1.3 | **Implemented** (commit `58777e5`) |
| §3.4 多文件 KFD | 阻塞 (53+ amdgpu headers) | **延后 Stage 3+** (未触碰) |
| §3.5 完整 kfd_queue.c | 仅 helper 子集 | **延后 Stage 3+** (未触碰) |

---

## 7. 后续工作

- **Stage 2**: 多设备插件化 — 可顺势引入 IOMMU 真实硬件 invalidation (vfio)
- **Stage 3+**: 真实 KFD 多文件集成（需独立子项目 ~50K 行 amdgpu driver 移植）
- **Phase 2.5 修复**: `GpgpuDevice::handleMapQueueRing` segfault (独立任务)
- **TaskRunner 集成**: 在 TaskRunner/UsrLinuxEmu 端跑 KFD 测试验证 Tier-2 runtime

---

## 8. ctest 演进时间线

| 阶段 | PASS | FAIL | 总数 |
|------|------|------|------|
| main@`b65acad` (baseline 修复前) | 67 | 1 | 68 (build break on test_migration_e2e) |
| commit `2322429` (baseline fix) | 68 | 0 | 68 |
| commit `c33d824` (§3.1 mmu_cb) | 69 | 0 | 69 |
| commit `8b6a33d` (§3.2 firmware_cb) | 70 | 0 | 70 |
| commit `4261bb4` (§3.3-§3.7 STUBs) | 71 | 0 | 71 |
| commit `58777e5` (§4 mmu_notifier) | 72 | 0 | 72 |
| commit `62d2353` (§5 IOTLB) | 73 | 0 | 73 |

**净增**: +5 个新测试目标, +15+ 个新 test cases, 0 regression.

---

**维护者**: UsrLinuxEmu Architecture Team
**最后更新**: 2026-07-05
**对应 commit**: `62d2353` (Tier-2 closeout head)
**关联 SSOT**: [post-refactor-architecture.md §1.10](../02_architecture/post-refactor-architecture.md), [kfd-portability-boundary.md v1.1](kfd-portability-boundary.md)
---

## v1.1 C-12 Phase C Implementation Records (2026-07-16)

**Status**: C-12 Phase C (Tier-2 deferred realification) 10/18 subtasks completed.

### C.1 IOMMU Invalidation (5/10 subtasks)

Per C-12 tasks.md §C.1, sim_pfh (page fault handler) + sim_pm (page migration)
realified to perform actual state transitions:

- **C.1.1** ✅ `plugins/gpu_driver/sim/page_fault_handler.cpp` real impl
  - callback body invokes kfd_events_signal() when cause == WRITE
  - verified via test_kfd_fault_handling_standalone (8 assertions, 2 cases)
- **C.1.2** ✅ `plugins/gpu_driver/sim/page_migration.cpp` real impl
  - real sim_pm_migrate_to_device + sim_pm_migrate_to_system
  - 16MB device memory lazy init
- **C.1.3** 🟡 IOTLB flush → sim_pm invalidation bridge (partially complete)
  - C.1.3a (DMA remap bridge) ✅ done
  - C.1.3b (test_iommu_invalidate_runtime additional TEST_CASE) deferred

### C.2 mm_shim Wire-Up (4/4 subtasks)

- **C.2.1** ✅ us_mm_shim wired into kfd_process lifecycle
  - kfd_priv.h appends `void *mm_shim` field (opaque)
  - kfd_process_create → us_mm_shim_init + assign
  - kfd_process_destroy → free mm_shim
  - KFD MAP_MEMORY → register_vma; UNMAP_MEMORY → unregister_vma
- **C.2.2** ✅ unit test_mm_shim_standalone (117 assertions / 7 cases)
  - already in commit e93f26f; covers init/register/unregister/find/foreach/capacity
- **C.2.3** ✅ integration test_kfd_concurrent_processes_standalone
  - 31 assertions / 2 cases; multi-thread single-process PID isolation
- (C.2.1.3 `iommu_domain_attach_mm_shim` deferred to Phase E — required C ABI change)

### Acceptance

- Phase C contracted deliverables: spec (phase-c-realification-contract.md) ✅
- ADR-063 Accepted (sim_pfh_pm realification state machine boundary) ✅
- 4 handlers (MAP_MEMORY / UNMAP_MEMORY / GET_PROCESS_APERTURE / UPDATE_QUEUE)
  + bridge audit + LEGACY/CLEAN tags per B.3.5
