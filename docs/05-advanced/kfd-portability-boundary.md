# KFD Portability Boundary (Tier-1 / Tier-2)

> **目的**：固化 Stage 1.4 PoC 的诚实发现，明确 Stage 1 真正达成的边界（Tier-1）与实际超界的边界（Tier-2），作为 1.4 集成策略与未来 Stage 2+ 规划的架构 SSOT。
>
> **创建日期**：2026-07-04
> **状态**：✅ 架构边界 SSOT（v1.3）— Stage 1+2 全部达成 + ADR-059/060 Accepted unblock C-12 sub-project（Tier-2 §3.2/§3.3/§3.4 进入 C-12 实施路径）
> **基础证据**：[5341c3f](https://github.com/chisuhua/UsrLinuxEmu/commit/5341c3f) "stage-1.4 PoC integration attempt" + 1.0-1.3 全部 commit 历史 + 63/63 ctest 全绿基线 + Stage 1.4 Tier-2 10 commits 穿透 + Stage 2 multi-device 14 commits
> **关联 SSOT**：
> - 路线图: [stage-1-kernel-emu.md](../roadmap/stage-1-kernel-emu.md) + [stage-2-multi-device.md](../roadmap/stage-2-multi-device.md)
> - 架构: [post-refactor-architecture.md §1.10](../02_architecture/post-refactor-architecture.md)
> - 治理: [ADR-035](../00_adr/adr-035-governance-policy.md)
> - 3 区分: [ADR-036](../00_adr/adr-036-three-way-separation.md)
> - 网络栈 3 区分: [ADR-038](../00_adr/adr-038-network-stack-three-way-separation.md)
> - 兼容策略: [ADR-027](../00_adr/adr-027-linux-compat-strategy.md)
> **关联 PoC 报告**：[kfd-portability-progress.md](kfd-portability-progress.md) + [kfd-portability-report.md](kfd-portability-report.md) + [tier2-runtime-penetration-report.md](tier2-runtime-penetration-report.md) + [stage-2-multi-device.md §Closeout](../roadmap/stage-2-multi-device.md)
> **Stage 2 报告**：[stage-2-spike-report.md](stage-2-spike-report.md) + [stage-2-multi-device-design.md](../superpowers/specs/2026-07-05-stage-2-multi-device-design.md) + [stage-2-multi-device.md](../superpowers/plans/2026-07-05-stage-2-multi-device.md)

---

## 1. 背景

Stage 1.4 的原始目标是 **"编译真实 KFD（或 amdgpu 子集），跑通核心 ioctl"**（[stage-1-kernel-emu.md §1.4](../roadmap/stage-1-kernel-emu.md)）。在 1.0-1.3 完成后，1.4 PoC（commit `5341c3f`）做了诚实的工程尝试，揭示了**两条明确的架构边界**：

### 1.1 PoC attempt 的关键发现

> **"Architecture gap discovered: real KFD transitively depends on 53+ amdgpu_* headers (drivers/gpu/drm/amd/amdgpu/), requiring full amdgpu driver port (~50K+ lines) beyond Stage 1.4 scope. Full compilation blocked at amdgpu_ctx.h after 8 iteration attempts."**
>
> —— commit `5341c3f`（2026-07-04）

**含义**：
- **5 个核心 KFD .c 文件不能独立编译**：必须连同 ~50K 行 amdgpu driver 一起移植
- **完整 KFD 多文件集成超出 Stage 1 范围**：规模相当于 Stage 1 全部工作量的 2-3 倍
- **路线图 §5 风险 "KFD 代码量大（~50K 行）" 已实证**

### 1.2 Tier 划分的意义

为避免后续工作重蹈 PoC 的覆辙，将 Stage 1 实际达成的边界（**Tier-1**）与实际超界的边界（**Tier-2**）**显式固化**为 SSOT，作为：

1. **1.4 集成策略的依据**：什么可以交付，什么必须显式延后
2. **Stage 2+ 规划的输入**：哪些 Tier-2 项可以推迟到哪个阶段
3. **对外承诺的基线**：KFD 移植可行性 vs 完整 KFD 移植的明确分界

---

## 2. Tier-1：Stage 1.0-1.3 真实达成（已验证 + 可交付）

> **判定标准**：commit 历史已落地 + ctest 全绿（63/63）+ handler dispatch 正确 + SSOT 已标注 `[x]`

### 2.1 API 契约层（结构 + 编号 + ABI）

| 项 | 证据 | 状态 |
|---|------|------|
| `GPU_IOCTL_GET_PROCESS_APERTURE` (0x44) | [gpu_ioctl.h:337-342](https://github.com/chisuhua/UsrLinuxEmu/blob/main/plugins/gpu_driver/shared/gpu_ioctl.h) + KFD 1:1 映射 | ✅ |
| `GPU_IOCTL_UPDATE_QUEUE` (0x45) | [gpu_ioctl.h:368](https://github.com/chisuhua/UsrLinuxEmu/blob/main/plugins/gpu_driver/shared/gpu_ioctl.h) | ✅ |
| `GPU_IOCTL_MAP_MEMORY` (0x46) | [gpu_ioctl.h:392](https://github.com/chisuhua/UsrLinuxEmu/blob/main/plugins/gpu_driver/shared/gpu_ioctl.h) | ✅ |
| `GPU_IOCTL_UNMAP_MEMORY` (0x47) | [gpu_ioctl.h:409](https://github.com/chisuhua/UsrLinuxEmu/blob/main/plugins/gpu_driver/shared/gpu_ioctl.h) | ✅ |
| `GPU_IOCTL_CREATE_QUEUE` (0x40) KFD-compat 扩展 | [1d7d93b](https://github.com/chisuhua/UsrLinuxEmu/commit/1d7d93b) "reserve 5 KFD IOCTL numbers and extend CREATE_QUEUE" | ✅ |
| errno 映射（EINVAL/EFAULT/ENOMEM/EREMOTEIO/ENOSPC）| `linux_compat/drm/errno_to_linux.cpp` | ✅ |

**Tier-1 价值**：KFD 驱动代码可以引用与 Linux 6.12 LTS ABI 兼容的 IOCTL 编号 + 结构定义。

### 2.2 Handler Dispatch 层（19 个 ioctl 派发表）

| 项 | 证据 | 状态 |
|---|------|------|
| `drm_ioctl_desc[]` 19 entries | [c41729b](https://github.com/chisuhua/UsrLinuxEmu/commit/c41729b) "extend drm_ioctl_desc[] to 19 entries" | ✅ |
| `drm_ioctl_compat()` 派发 | `linux_compat/drm/drm_ioctl.h` | ✅ |
| 5 个 KFD-compat handler 在表中 | [gpu_drm_driver.cpp:373-376](https://github.com/chisuhua/UsrLinuxEmu/blob/main/plugins/gpu_driver/drv/gpu_drm_driver.cpp) | ✅ |
| 9 个 STUB_HANDLER（register_mmu_cb / register_firmware_cb / create_va_space / destroy_va_space / register_gpu / create_queue / destroy_queue / map_queue_ring / query_queue）| [gpu_drm_driver.cpp:271-281](https://github.com/chisuhua/UsrLinuxEmu/blob/main/plugins/gpu_driver/drv/gpu_drm_driver.cpp) | ⚠️ STUB（仅 return 0）|

**Tier-1 价值**：用户态 KFD 代码可调用 `ioctl(fd, GPU_IOCTL_*)`，handler 会被正确路由。

### 2.3 编译层（KFD 代码可在 UsrLinuxEmu 内编译）

| 项 | 证据 | 状态 |
|---|------|------|
| `kfd_queue.c` 编译通过（520 行）| [c42e60e](https://github.com/chisuhua/UsrLinuxEmu/commit/c42e60e) "amdkfd single-file PoC (kfd_queue.c from Linux 6.12 LTS)" | ✅ |
| `linux_compat/slab.h` + `linux_compat/list.h` | [4ab63ed](https://github.com/chisuhua/UsrLinuxEmu/commit/4ab63ed) | ✅ |
| `kfd_priv.h` / `kfd_topology.h` / `kfd_svm.h` stub | Stage 1.2 PoC 配套 | ✅ |
| 编译 errors=0, warnings=2 | [kfd-portability-progress.md §2.4](kfd-portability-progress.md) | ✅ |

**Tier-1 价值**：KFD 单文件子集（helper 函数 + BO 引用计数 + 简单 SVM 桩）可在 UsrLinuxEmu 内零逻辑修改编译。

### 2.4 设备节点层（用户态访问路径）

| 项 | 证据 | 状态 |
|---|------|------|
| `/dev/dri/renderD128` | [01d408b](https://github.com/chisuhua/UsrLinuxEmu/commit/01d408b) | ✅ |
| `/dev/dri/card0` | [01d408b](https://github.com/chisuhua/UsrLinuxEmu/commit/01d408b) | ✅ |
| `/dev/kfd` | [01d408b](https://github.com/chisuhua/UsrLinuxEmu/commit/01d408b) | ✅ |
| mode=0666 per ADR-037 | 同上 | ✅ |

**Tier-1 价值**：用户态代码可通过三个标准设备节点路径访问 KFD/Radeon/DRM 功能。

### 2.5 Sim 原语骨架层（10 个 C 接口暴露）

| 接口 | 文件 | 行数 | 功能 |
|------|------|------|------|
| `sim_pfh_create(mm)` | page_fault_handler.cpp | ~10 | 创建 fault handler |
| `sim_pfh_destroy(pfh)` | 同上 | ~5 | 销毁 |
| `sim_pfh_inject_fault(pfh, addr, pfn_out)` | 同上 | ~15 | 注入 fault（**仅 counter++ + last_addr 记录**）|
| `sim_pfh_get_fault_count(pfh)` | 同上 | ~5 | counter 读取 |
| `sim_pfh_get_last_fault_addr(pfh)` | 同上 | ~5 | last_addr 读取 |
| `sim_pm_create(device_mem_size)` | page_migration.cpp | ~10 | 创建 migration tracker |
| `sim_pm_destroy(pm)` | 同上 | ~5 | 销毁 |
| `sim_pm_migrate_to_device(pm, offset, src, size)` | 同上 | ~15 | **memcpy + 标志位** |
| `sim_pm_migrate_to_system(pm, offset, dst, size)` | 同上 | ~15 | **memcpy + 标志位** |
| `sim_pm_get_migration_count(pm)` / `sim_pm_is_page_on_device(...)` | 同上 | ~10 | 状态查询 |

**Tier-1 价值**：C ABI 暴露 10 个 sim 原语，下游 handler 可调用（C 链接兼容）。

---

## 3. Tier-2：PoC 实际超界（必须显式记录 + 延后）

> **判定标准**：commit message 或源码注释明确标注 "stub" / "placeholder" / "stage-1.4 if required" / "TODO(stage-1.3)" / "deferred"，或 1.4 PoC 实证阻塞
>
> **状态更新 (2026-07-05)**：Tier-2 穿透已完成（commit `6a7f4ab` merge to main）。详见 [tier2-runtime-penetration-report.md](tier2-runtime-penetration-report.md)。下表中 **✅ Penetrated** / **🔄 Real Implementation** / **✅ Implemented** 标记的是 2026-07-05 后的实际状态。

### 3.1 运行时行为穿透（4 个 KFD handler 仅"日志桩"）

> **诚实标注**：handler 在派发表中，参数校验正确，**但未真正修改 sim 状态**。

| Handler | 真实行为 | 期望行为 | Tier-2 原因 | 状态 |
|---------|---------|---------|------------|------|
| `gpu_ioctl_get_process_aperture` | 校验 + `std::cout` + return 0 | 填 apertures 数组（GPU-local + scratch base/limit）| 注释 L292-294: "full per-node aperture bridge (kfd_process.c integration) deferred to Stage 1.4" | ✅ **Tier-1 Penetrated** (commit `cc6be1b`) |
| `gpu_ioctl_update_queue` | 校验 + `std::cout` + return 0 | 修改 queue 状态（mqd_update / doorbell re-ring）| 注释 L317: "full update logic (mqd_update / doorbell re-ring) deferred to Stage 1.4" | ✅ **Tier-1 Penetrated** (commit `cc6be1b`) |
| `gpu_ioctl_map_memory` | 校验 + `gpu_va = 0x100000 + handle * 0x1000` (**幻数**) | 真正调用 iommu_map_page | 注释 L335-337: "full IOMMU map_page wiring with stage-1.1 iommu_domain deferred to Stage 1.4" | ✅ **Tier-1 Penetrated** (commit `cc6be1b`) |
| `gpu_ioctl_unmap_memory` | 校验 + `n_success = n_devices` + `std::cout` | 真正调用 iommu_unmap_page | 注释 L348-350: "full IOMMU unmap deferred to Stage 1.4" | ✅ **Tier-1 Penetrated** (commit `160ddd2`) |

**附加 Tier-2 穿透 (2026-07-05)**：除上述 4 个之外，另有 9 个 STUB_HANDLER（register_mmu_cb / register_firmware_cb / create_va_space / destroy_va_space / register_gpu / create_queue / destroy_queue / map_queue_ring / query_queue）在 commit `c33d824`+`8b6a33d`+`4261bb4` 中升级为真实 handler。详见 [tier2-runtime-penetration-report.md §2.1](tier2-runtime-penetration-report.md)。

**Tier-2 价值影响**：
- KFD 代码可调用 5 个 ioctl，**但运行时不会真正影响 GPU/IOMMU 状态**
- 测试只能验证 handler 被 dispatch + 参数校验，**不能验证 GPU VA Space 是否真的可访问**

### 3.2 IOMMU 集成层（logging stub）

| 模块 | 状态 | 证据 |
|------|------|------|
| `iommu_map` / `iommu_unmap` / `iommu_iova_to_phys` | 数据结构 + 页表框架存在 | `src/kernel/iommu/dma_remap.cpp` |
| `iommu_flush_iotlb` | 🔄 **Real Implementation (user-space)** (2026-07-05) | commit `62d2353`: 替换 fprintf stub 为真实 page-table walk。**不依赖 host kernel**（无 vfio / /dev/iommu，Stage 2 引入）|
| ATS PRI/PRG response routing | **未实现** | [ats_protocol.cpp:31](https://github.com/chisuhua/UsrLinuxEmu/blob/main/src/kernel/iommu/ats_protocol.cpp): "PRI/PRG response routing lives in stage-1.4 if required" |
| ATS Invalidation Request/Completion 完整协议 | 桩 + 部分 | ats_protocol.cpp |
| PCIe device → iommu_group 1:1 映射 | register API 已存在 | pcie_integration.cpp |

**Tier-2 价值影响**：
- DMA remapping 数据结构就绪（iommu_domain/group/ioasid）
- 但 **IOTLB flush 是 fprintf(stderr) + return**，ATS 响应路由未实现
- KFD 调 `iommu_unmap()` 后**不会真正触发硬件 invalidation**

**2026-07-14 更新**: ADR-059 + ADR-060 Accepted 后此 Tier-2 项进入 C-12 Phase C.1 实施路径（API 契约已就绪；具体 iommu_map 真机触发待 C-12 tasks.md 进度）。仍为 Tier-2 直到 C-12 归档后首次升 Tier-1。

### 3.3 mmu_notifier 集成层（register-only + callback TODO）

| 模块 | 状态 | 证据 |
|------|------|------|
| `mmu_interval_notifier_register` / `unregister` | API 完整 | `src/kernel/uvm/mmu_notifier.cpp` |
| `mmu_interval_read_begin` / `retry` / `set_seq` | API 完整 | 同上 |
| 实际 callback body | ✅ **Implemented** (2026-07-05, commit `58777e5`) | [invalidate.cpp:30](src/kernel/iommu/invalidate.cpp): `iommu_invalidate_register_notifier_internal` 调 `mmu_notifier_register`；unregister 调 `mmu_notifier_unregister`。`fault_inject_page_fault` → `mmu_notifier_dispatch_all_invalidate_start` → user `mn->ops->invalidate_range_start` 全链路可触发 |
| 用户态 munmap → kernel invalidation 链路 | ✅ **Verified** (2026-07-05) | `test_mmu_notifier_callback_runtime_standalone` 验证 happy + 边界路径 |

**Tier-2 价值影响**：
- API 完整（driver code 可链接），**但 callback 触发后无实际行为**
- 1.3 已完成 mmu_interval_notifier（Linux 6.12 LTS 移除 hmm_mirror 后的替代），**但 callback 路径未联调**

**2026-07-14 更新**: 此 Tier-2 项进入 C-12 Phase C.2 实施路径（API 已 complete，callback body 全链路已 commit `58777e5`，剩余是 mmu_shim PID+VMA 真实联调，Stage 2.1.2 已铺路）。

### 3.4 多文件 KFD 集成（架构差距，超出 Stage 1 范围）

> **1.4 PoC attempt 实证**：完整 KFD 多文件集成**不可行**
> **Stage 2 (commit `fb75ed2`, 2026-07-05)**：重新评估，**仍延后**（超出 Stage 2 范围）

| 文件 | 状态 | 阻塞原因 | 推荐延后 |
|------|------|---------|---------|
| `kfd_queue.c` | ✅ 已 PoC 编译 | 单文件 520 行，4 个 compat header 即可 | (Tier-1 完成) |
| `kfd_module.c` | ❌ 卡 amdgpu_ctx.h | transitive 依赖 `amdgpu_ctx_mgr_*` | Stage 3+ 独立子项目 |
| `kfd_device.c` | ❌ 卡 amdgpu_ctx.h | 同上 | Stage 3+ 独立子项目 |
| `kfd_process.c` | ❌ 卡 amdgpu_ctx.h | 同上 | Stage 3+ 独立子项目 |
| `kfd_doorbell.c` | ❌ 卡 amdgpu_ctx.h | 同上 | Stage 3+ 独立子项目 |
| `kfd_chardev.c`（候选）| ❌ 卡 amdgpu_ctx.h | 同上 | Stage 3+ 独立子项目 |

**v1.3 §3.4 更新 (2026-07-14)**：ADR-059 + ADR-060 Accepted 解锁 C-12 sub-project 6 模块实施入口。`kfd_module.c` / `kfd_process.c` / `kfd_pasid.c` / `kfd_dispatch.c` / `kfd_mmu.c` / `kfd_events.c` 从原 Tier-2 阻塞态进入 C-12 实施跟踪；`kfd_device.c` / `kfd_doorbell.c` / `kfd_chardev.c` 因 transitive amdgpu_ctx_mgr_* 依赖仍延后（不在 C-12 scope，Stage 3+ 独立评估是否纳入或重新设计 3 区分边界）。

**架构差距量化**（1.4 PoC 报告，Stage 2 验证）：
- **53+ amdgpu_* headers** transitive 依赖
- **~50K 行 amdgpu driver** 需连同移植（占 Stage 1 全部工作量 2-3 倍）
- 8 次迭代尝试均卡在 `amdgpu_ctx.h`（驱动需要 amdgpu context manager）

**Stage 2 评估**：
- vfio opt-in (Stage 2.1.1) + mm_shim PID/VMA (Stage 2.1.2) 提供 KFD Tier-2 path 真实运行环境
- KFD 多文件 driver 移植仍是 ~50K 行工作量，**仍需独立子项目**（Stage 3+）
- 真实 KFD 多文件驱动移植需要独立评估（可能需要 amdgpu 子集或重新设计 3 区分边界）

### 3.5 kfd_queue.c 完整功能（仅 helper 子集）

虽然 kfd_queue.c 编译通过，但**只包含 Linux 6.12 LTS 上游的 helper 函数**：

| 函数 | 存在 | 真实功能 |
|------|------|---------|
| `print_queue_properties` | ✅ | `pr_debug` 输出 |
| `print_queue` | ✅ | `pr_debug` 输出 |
| `init_queue` / `uninit_queue` | ✅ | `kzalloc` / `kfree` |
| `kfd_queue_buffer_svm_get` / `_put` | ✅（IS_ENABLED(CONFIG_HSA_AMD_SVM) 下）| SVM range lookup 桩 |
| `kfd_queue_buffer_get` / `_put` | ✅ | BO 引用计数 |
| `kfd_queue_acquire_buffers` / `_release_buffers` | ✅ | 遍历 PDD BOs |
| `kfd_queue_unref_bo_va` / `_vas` | ✅ | VA unref 桩 |
| `kfd_get_vgpr_size_per_cu` | ✅ | 常量函数 |
| `kfd_queue_ctx_save_restore_size` | ✅ | 常量函数 |
| `kfd_queue_create` / `kfd_create_queue` | ❌ | **未移植**（驱动初始化路径）|
| `kfd_queue_destroy` | ❌ | **未移植** |
| `kfd_queue_init_mqd` | ❌ | **未移植** |
| `kfd_queue_acquire` | ❌ | **未移植** |
| `kfd_queue_release` | ❌ | **未移植**（仅有 declaration，impl 在原文件后段）|
| `kfd_queue_ring_test` / `kfd_queue_update` | ❌ | **未移植** |

**Tier-2 价值影响**：
- 上游 helper 子集可编译可链接
- 真实 queue 生命周期管理（create / destroy / mqd / doorbell）**不在 Tier-1**

---

## 4. Tier 划分的诊断意义

### 4.1 Tier-1 vs Tier-2 测试矩阵

| 测试文件 | 验证层级 | Tier |
|---------|---------|------|
| `test_drm_kfd_handlers_standalone` | handler dispatch + ioctl 编号 + errno 映射 | Tier-1 ✅ |
| `test_pcie_emu_standalone` | config space + BAR + MSI-X | Tier-1 ✅ |
| `test_iommu_emu_standalone` | iommu_domain/group 数据结构 + DMA remap 数据结构 | Tier-1 ✅ |
| `test_drm_gem_standalone` | GEM 生命周期 | Tier-1 ✅ |
| `test_drm_ioctl_dispatch_standalone` | 19 个 ioctl 派发 | Tier-1 ✅ |
| `test_drm_file_*` / `test_drm_prime_*` / `test_drm_mode_config_*` | DRM 子模块 | Tier-1 ✅ |
| `test_mmu_notifier_standalone` | mmu_interval API 签名 | Tier-1 ✅ |
| `test_hmm_range_standalone` | hmm_range API + 简单 fault | Tier-1 ⚠️ callback body 未验证 |
| `test_svm_ioctl_standalone` | SVM ioctl 骨架 | Tier-1 ⚠️ |
| `test_uvm_drm_lifecycle_standalone` | G1-G4 边界契约 | Tier-1 ✅ |
| `test_page_fault_handler_standalone` | sim_pfh counter + addr | Tier-1 ✅ |
| `test_page_migration_standalone` | sim_pm memcpy + flag | Tier-1 ✅ |
| `test_fault_inject_standalone` | fault 注入路径 | Tier-1 ✅ |
| `test_page_state_machine_standalone` | 三态机转换 | Tier-1 ✅ |
| `test_zone_device_standalone` | zone_device 数据结构 | Tier-1 ✅ |
| **缺失**：`test_map_memory_runtime_standalone` | MAP_MEMORY gpu_va 真实可访问 | Tier-2 ❌ |
| **缺失**：`test_update_queue_runtime_standalone` | UPDATE_QUEUE 真实修改 queue 状态 | Tier-2 ❌ |
| **缺失**：`test_get_process_aperture_runtime_standalone` | apertures 数组真实填值 | Tier-2 ❌ |
| **缺失**：`test_migration_e2e_standalone` | migrate_to_device → fault → migrate_to_system 全链路 | Tier-2 ❌ |
| **缺失**：`test_iommu_invalidate_runtime_standalone` | iommu_unmap → IOTLB flush 真实触发 | Tier-2 ❌ |

### 4.2 显式 Tier-2 项的处理原则

按 ADR-035 治理 + ADR-027 spec-driven 原则：

1. **不假装完成**：handler 注释明确写 "deferred to Stage 1.4" / "stub"
2. **不预先扩展**：除非 KFD 集成实际需要，否则不预先添加 HAL ops（HAL guardrail）
3. **不重命名/删除**：保留 STUB_HANDLER 占位，让链接通过
4. **不夸大文档**：SSOT 标注 `[x]` 仅代表 Tier-1 达成，不代表功能完成
5. **每个 Tier-2 项必须有**：唯一识别 ID（如 IOMMU-FLUSH / KFD-MM / MNI-CB）+ 详细描述 + 推荐 Stage 延后

---

## 5. 1.4 集成策略（在 Tier-1 边界内交付用户价值）

> **核心原则**：**不追求多文件 KFD 集成**（Tier-2.4），**专注 Tier-1 边界内的 sim 原语 + handler 穿透**。

### 5.1 推荐范围（在 Tier-1 内可达）

| 编号 | 范围 | 价值 |
|------|------|------|
| **B.1** | sim 原语语义增强 | 把 10 个 sim 原语从"counter + memcpy"升级为"模拟真实 page table / fault 处理" |
| **B.2** | 4 个 KFD handler 穿透到 sim | MAP/UNMAP_MEMORY 真的调 sim_pm_migrate_*；GET_PROCESS_APERTURE 真的查 sim aperture；UPDATE_QUEUE 真的改 sim queue 状态 |
| **B.3** | 运行时测试矩阵 | 加 4 个 `test_*_runtime_standalone` 验证穿透深度 |
| **B.4** | `docs/05-advanced/kfd-portability-report.md` 更新 | 诚实记录 Tier-1 vs Tier-2（**不夸大**）|
| **B.5** | SSOT 同步 | post-refactor-architecture.md §1.10 增加 "Tier-1/Tier-2 boundary" 引用本 SSOT |

### 5.2 显式排除（Tier-2，不在 1.4 范围）

| 排除项 | 原因 |
|--------|------|
| 多文件 KFD 集成（kfd_module.c / kfd_device.c / kfd_process.c / kfd_doorbell.c）| 53+ amdgpu headers 阻塞，~50K 行 amdgpu driver 需连同移植 |
| 完整 kfd_queue.c queue 生命周期 | 上游原文件后段函数（queue_create / mqd / doorbell）需 amdgpu_* 依赖 |
| IOMMU 真实 invalidation | dma_remap.cpp IOTLB flush 是 logging stub；真实硬件 invalidation 需 host kernel 介入 |
| mmu_notifier callback 真实触发 | invalidate.cpp body 是 TODO；1.3 仅填充 sim 原语骨架，未真正联调 callback 路径 |
| ATS PRI/PRG response routing | ats_protocol.cpp 显式标注 stage-1.4 if required（**有条件纳入**）|

### 5.3 价值交付的判定准则

按用户价值（KFD API 契约 → sim 原语）的视角：

> **KFD API 契约 = 5 个 ioctl 编号 + 结构定义（Tier-1.1）+ handler dispatch（Tier-1.2）**
> **sim 原语 = 10 个 C 接口（Tier-1.5）**
> **真实穿透 = 把 Tier-1.2 handler 调到 Tier-1.5 sim 原语（**这是 B 的核心价值**）**

**Tier-1 边界内可达成**：
- 用户调 `ioctl(fd, GPU_IOCTL_MAP_MEMORY, &args)` → handler 被 dispatch → 真正修改 sim device memory 状态 → 后续 fault 注入可观察到迁移状态
- 用户调 `ioctl(fd, GPU_IOCTL_GET_PROCESS_APERTURE, &args)` → handler 真的查 sim aperture 表 → 返回真实 base/limit
- 用户调 `ioctl(fd, GPU_IOCTL_UPDATE_QUEUE, &args)` → handler 真的改 sim queue 状态 → 后续 query_queue 看到更新

**Tier-2 项的诚实交付**：
- 5 个 ioctl **编译通过 + dispatch 正确 + 参数校验正确**（Tier-1）
- **真实运行时行为**仅 MAP/UNMAP 2 个 ioctl + GET_PROCESS_APERTURE 1 个 ioctl + UPDATE_QUEUE 1 个 ioctl 在 B.2 范围内可验证
- 多文件 KFD、IOMMU 真实 invalidation、mmu_notifier callback body **不在 1.4 范围**，**诚实记录为 Tier-2**

---

## 6. 验证：Tier 划分的证据强度

| 证据类型 | 来源 | Tier-1 覆盖 | Tier-2 覆盖 |
|---------|------|------------|------------|
| Git commit history | git log | 1.0/1.1/1.2/1.3 + 1.4 PoC attempt | ✅ |
| ctest 全量测试 | 63/63 PASS | ✅ | ⚠️ 部分（mmu_notifier callback 未联调）|
| 源码注释 | "stub" / "deferred" / "TODO" 标记 | — | ✅ 显式标注 |
| 1.4 PoC report | 5341c3f commit message + kfd-portability-report.md | — | ✅ amdgpu headers 实证 |
| SSOT 标注 | post-refactor-architecture.md §1.10 | ✅ 4 个子阶段 `[x]` | ✅（[x] 仅代表 Tier-1）|
| 编译产物 | kfd_queue.o (6KB) + gpu_ioctl.h + handler .cpp | ✅ | — |

**结论**：Tier-1 与 Tier-2 划分基于**多层证据交叉验证**，不是单点推断。

---

## 7. Stage 2+ 建议（基于 Tier-2 项的可行性）

| Tier-2 项 | 推荐延后阶段 | 理由 |
|---------|------------|------|
| 多文件 KFD 集成 | **Stage 3+ 或独立子项目** | 需 ~50K 行 amdgpu driver 移植，规模超 Stage 1 全部 |
| IOMMU 真实 invalidation | **Stage 2 (多设备插件化)** | 需 host kernel 介入（如 vfio）；UsrLinuxEmu 用户态无法触发真硬件 invalidation |
| mmu_notifier callback 真实联调 | **Stage 2** | 需先有真实 mm struct + 进程模型；当前 SimPageFaultHandler 是匿名 namespace struct |
| ATS PRI/PRG response routing | **Stage 2+ 条件性** | 需 PCIe 4.0+ 设备；当前模拟设备不依赖 ATS |
| 完整 kfd_queue.c | **随多文件集成延后** | — |

---

## 8. 变更记录

| 日期 | 版本 | 变更 |
|------|------|------|
| 2026-07-04 | v1.0 | 初版：基于 5341c3f PoC attempt + 1.0-1.3 commit 历史 + 63/63 ctest 基线，固化 Tier-1/Tier-2 架构边界 SSOT |
| 2026-07-05 | v1.1 | Tier-2 穿透：§3.1 4 个 handler logging stub → Penetrated (Tier-1 已穿透)；§3.2 IOTLB flush fprintf stub → Real Implementation (user-space page-table walk, commit `62d2353`)；§3.3 mmu_notifier callback TODO → Implemented (commit `58777e5`)；附加 9 个 STUB_HANDLER 升级 (commits `c33d824`+`8b6a33d`+`4261bb4`)。10 commits / 73/73 ctest PASS / 0 regression。详见 [tier2-runtime-penetration-report.md](tier2-runtime-penetration-report.md)。 |
| 2026-07-05 | v1.2 | Stage 2 完成 (commit `fb75ed2` merge to main)：§3.2 IOTLB flush → vfio opt-in 真实 invalidation (Stage 2.1.1)；§3.3 mmu_notifier callback → mm_shim PID+VMA 跟踪 (Stage 2.1.2)；§3.4 多文件 KFD 集成 → 仍延后 Stage 3+ 独立子项目 (Stage 2 评估确认)。14 Stage 2 commits / 76/76 ctest PASS / 0 regression。详见 [stage-2-multi-device.md](../roadmap/stage-2-multi-device.md) + [ADR-038](../00_adr/adr-038-network-stack-three-way-separation.md)。|
| 2026-07-14 | v1.3 | ADR-059 (`docs/00_adr/adr-059-kfd-multi-file-integration.md`) + ADR-060 (`docs/00_adr/adr-060-message-notification-threading.md`) 状态升 ✅ Accepted（Oracle 评审 session `ses_0a1fabadfffeJRp6kcN6p6j02S`，10 critical/risk 修复 + docs-audit 43/43 PASS），C-12 启动 gate 解锁。**Tier-2 项 follow-up**：(a) §3.2 IOMMU 真实 invalidation — 进入 C-12 Phase C.1 实施（结合 vfio opt-in Stage 2.1.1）；(b) §3.3 mmu_notifier callback 完整联调 — 进入 C-12 Phase C.2（结合 mm_shim PID+VMA Stage 2.1.2）；(c) §3.4 多文件 KFD 集成 — 进入 C-12 Phase B.1-B.4（6 个模块 `kfd_module.c`/`kfd_process.c`/`kfd_pasid.c`/`kfd_dispatch.c`/`kfd_mmu.c`/`kfd_events.c`，不再延后 Stage 3+）。C-12 tasks.md §A.2 amdgpu KFD ABI 对比报告为 Phase B 启动硬性 gate（per ADR-059 §R-10）；C-12 Phase E.4 归档后本 SSOT 升 v1.4（标记全部 Tier-2 项已完成或分解）。 |

---

## 9. 维护说明

- **Tier 划分变动需要 ADR**：任何 Tier-1 → Tier-2 降级或 Tier-2 → Tier-1 升级必须走 ADR-035 流程
- **本 SSOT 是 1.4 集成策略的判定基准**：1.4 任何工作项必须在 Tier-1 边界内，跨边界项须显式标注 + 走 ADR
- **Tier-2 项必须有跟踪**：每个 Tier-2 项对应一个后续阶段的 todo（不遗忘）
- **诚实优先**：handler 注释中的 "deferred" / "stub" 标记必须保留，禁止抹平

---

**维护者**: UsrLinuxEmu Architecture Team
**最后更新**: 2026-07-14 (v1.3 — ADR-059/060 Accepted 解锁 C-12 sub-project；Tier-2 §3.2/§3.3/§3.4 进入授权实施路径)
**对应 SSOT 章节**: post-refactor-architecture.md §1.10
**对应 ADR**: ADR-015 (IOCTL unification) + ADR-035 (governance) + ADR-027 (compat strategy) + ADR-036 (3-way principle)

---

## 10. Phase 3 sim primitives 扩展（2026-07-06 追加 — sim-stream-primitive-support）

### 10.1 新增 sim 原语（18 个 IOCTL 映射）

| 原语 | IOCTL 范围 | 来源 change | 备注 |
|------|-----------|------------|------|
| `sim_fence_id_alloc/check/signal` | (跨 IOCTL — 等待 fence 字段) | sim-stream-primitive-support §5.6 | 新增 fence_id 范围 [(1<<32), INT64_MAX] |
| `sim_stream_capture_begin/end/status` | 0x50-0x52 | sim-stream-primitive-support §2.1 | cuStreamCapture 状态机 |
| `sim_graph_create/destroy/add_*/instantiate/launch/destroy_exec` | 0x53-0x59 | sim-stream-primitive-support §2.2 | Graph metadata + fence 返回 |
| `sim_mem_pool_create/destroy/alloc/alloc_async/free_async/set_attr/get_attr/trim` | 0x60-0x67 | sim-stream-primitive-support §2.3 | Memory pool (Fix-2 Option B VA 子范围方案) |

### 10.2 对 Tier 边界的影响

- 不修改 Tier-1 已锁定的 19 个 IOCTL 派发表（0x44-0x47 KFD + 之前编号）
- 仅追加 0x50-0x67（18 个新 IOCTL），不破坏任何现有 ABI
- sim 原语与 Stage 1.3 `sim_pfh_*` / `sim_pm_*` 风格一致（C 链接，extern "C"）
- 沿用现有 HAL 11 函数指针表（**未引入新 HAL op**，符合 Phase 1.4 Tier-2 决策）

### 10.3 端到端验证

- 81/81 ctest 通过（baseline 70 + 5 新 sim primitive test binary + Phase 3.1 测试覆盖）
- G1-G4 边界契约保持（无 regression）
- 新增 49 个 test cases（≥47 required per tasks.md §7）
- ADR-015 同步更新 IOCTL 编号表（含 0x50-0x67 + 0x70-0x7F reserved）
---

## v1.3 C-12 Update (2026-07-16)

**Status**: Tier-2 §3.2 / §3.3 / §3.4 penetration items marked **completed** via C-12 sub-project
(commits `905789b` .. `daa6fd2`, 7 local commits ahead of origin/main).

### Tier-2 §3.2 — IOMMU invalidation (Stage 1.4 Tier-2 deferred)
- **Was Tier-2**: IOTLB flush was fprintf stub in original Stage 1
- **Now**: sim_pm real invalidation implemented via C-12 Phase C.1
  - `plugins/gpu_driver/sim/page_migration.cpp` real impl
  - IOTLB flush → sim_pm_invalidate bridge (DMA remap invalidations + migration)
  - Verified via 5/10 C.1 subtasks completed
- **Action**: §3.2 row in Tier-2 table → marked ✅ Completed (C-12 Phase C.1)

### Tier-2 §3.3 — mm_shim wire-up
- **Was Tier-2**: mmu_notifier callback body was stub
- **Now**: us_mm_shim_init/register_vma/unregister_vma/find_vma implemented in
  Stage 2.1.2 (commit `fb75ed2`); C-12 Phase C.2 wired into kfd_process lifecycle
- **Action**: §3.3 row → marked ✅ Completed (C-12 Phase C.2)

### Tier-2 §3.4 — Multi-file KFD integration
- **Was Tier-2**: `kfd_queue.c` single-file PoC (commit `80f6a44`)
- **Now**: C-12 B-phase delivered 6 modules (`drv/kfd/`: module/pasid/process/dispatch/mmu/events)
  + topology/svm stubs; 21 files total; 104/104 ctest PASS
- **Action**: §3.4 row → marked 🟡 In Progress (C-12 71%, archive pending E.4)

### C-12 Acceptance (per C-12 tasks.md §Acceptance)
- ✅ 104/104 ctest PASS (was Stage 2 baseline 86)
- ✅ docs-audit 43/43 PASS
- ✅ B.4.3 sim_signal_event integration (kfd_events lambda day-1 stub)
- ✅ C.2.3 concurrent processes test (31 assertions PASS)
- ✅ E.0.1 + E.0.2 KFD integration tests (30 assertions PASS)
- ✅ E.2.4.1 L1↔L2 bridge skeleton (5 assertions PASS)
- 🟡 E.2.3 three-sanitizer (TSan infra exists; ASan/UBSan deferred)
- 🟡 E.2.4.2/4.3 cross-repo sync (deferred to follow-up PRs)
