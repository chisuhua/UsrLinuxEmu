## Why

阶段 1.4 子阶段是路线图 §1（[stage-1-kernel-emu.md](../../docs/roadmap/stage-1-kernel-emu.md)）的**最终集成验证里程碑**：把真实 KFD（AMD Kernel Fusion Driver，来自 Linux 6.6/6.12 LTS `drivers/gpu/drm/amd/amdkfd/`）的 5 个核心 .c 文件拷贝到 UsrLinuxEmu 后，**逻辑零修改**（仅 `#include` 路径调整）即可编译通过并跑通 5 个核心 ioctl。

阶段 1 的**显式验收指标**（路线图 §1）：

1. **代码可移植性**：真实 KFD 驱动的 `.c` 文件拷贝到 UsrLinuxEmu 后，**仅** `#include` 路径需调整，逻辑零修改即可编译
2. **ioctl 兼容性**：至少 5 个核心 KFD ioctl 在 UsrLinuxEmu 中跑通

阶段 1.0/1.1/1.2/1.3 四个子阶段已全部完成（commit 历史 2026-07-02 ~ 07-04，详见各自 archive），本 change 是它们的**集成验证**，依赖全部前置：

- **1.0 PCIe**（已归档 `stage-1-0-pcie-emu/`）—— KFD 通过 `pci_*` / `pci_msix_*` API 访问硬件
- **1.1 IOMMU + ATS**（已归档 `2026-07-02-stage-1-1-iommu-ats/`）—— KFD `kfd_process.c` 通过 `iommu_*` API 管理 DMA remapping
- **1.2 DRM 子集**（已归档 `2026-07-02-stage-1-2-drm-subset/`）—— KFD `/dev/kfd` 设备节点 + 5 个 KFD-compat ioctl 编号（0x40 扩展 + 0x44-0x47）+ KFD 单文件 PoC `kfd_queue.c` 编译通过
- **1.3 UVM/HMM**（已归档 `2026-07-04-stage-1-3-uvm-hmm/`）—— KFD `kfd_process.c` + `kfd_svm.c`（如涉及）通过 `mmu_notifier` + `hmm_range` 接口实现 SVM

> **本 change 不包含 UMQ (User Mode Queue)**：UMQ 由 [ADR-024](../../docs/00_adr/adr-024-user-mode-queue-submission.md) 完整覆盖（用户态 Ring Buffer + mmap Doorbell 路径），与 1.4 集成验证在内核子系统层级上正交。
>
> **本 change 不包含 HMM/UVM 实际 SVM ioctl 跑通**（路线图 §1.3 验收第 2 条）：SVM ioctl 路径深度依赖 `kfd_svm.c` 等源文件，规模超出 1.4 ~5K 行 PoC 范围。1.4 仅做 5 个核心 ioctl（路线图 §1.4 验收），SVM 完整路径作为 follow-up。

## What Changes

- **新增 capability `kfd-portability`**：定义 UsrLinuxEmu 对真实 KFD 驱动的可移植性契约（仅 `#include` 路径调整，逻辑零修改）
- **新增** `plugins/gpu_driver/drv/kfd/` 子目录：存放从 Linux LTS 拷贝的 KFD .c 文件（5 个）
  - `kfd_module.c`（~500 行，模块入口 / 设备探测）
  - `kfd_device.c`（~1500 行，设备生命周期 / topology）
  - `kfd_process.c`（~2000 行，进程级 KFD 上下文 / DMA 管理）
  - `kfd_queue.c`（~800 行，队列创建 / 提交，**1.2 阶段单文件 PoC 基础**）
  - `kfd_doorbell.c`（~500 行，Doorbell MMIO 映射）
- **新增** `plugins/gpu_driver/drv/kfd/CMakeLists.txt`：作为 `gpu_kfd` C 库（沿用 1.2 阶段 `add_subdirectory(kfd)` 模式）
- **新增** `tests/test_kfd_portability_standalone.cpp`：端到端集成测试（5 个 ioctl happy path + 至少 1 个 error path）
- **新增** `docs/05-advanced/kfd-portability-report.md`：移植报告（warnings 数量 / 错误码一致性 / 决策点 / 后续改进建议）
- **HAL 条件性扩展**：`hal_iommu_*` 或 `hal_uvm_*` ops —— **仅当 KFD 实际调用**按 ADR-035 Rule 3 走独立 ADR 流程
- **承接 1.2/1.3 边界契约 G1-G4**：
  - G1: `drm_device` 生命周期 = `GpgpuDevice` 生命周期（1.2 已锁）
  - G2: `dma_buf_dynamic_attach` 等 API 签名（1.2 已锁）
  - G3: 4 项接口契约（1.2 已锁）
  - G4: migrate 接口边界（1.3 已实现）
- **承接 1.2 阶段预留的 5 个 KFD ioctl 编号**：
  - 0x40 `GPU_IOCTL_CREATE_QUEUE`（已扩展 KFD-compat 字段）
  - 0x44 `GPU_IOCTL_GET_PROCESS_APERTURE` ← `AMDKFD_IOC_GET_PROCESS_APERTURES_NEW` (0x14)
  - 0x45 `GPU_IOCTL_UPDATE_QUEUE` ← `AMDKFD_IOC_UPDATE_QUEUE` (0x07)
  - 0x46 `GPU_IOCTL_MAP_MEMORY` ← `AMDKFD_IOC_MAP_MEMORY_TO_GPU` (0x16)
  - 0x47 `GPU_IOCTL_UNMAP_MEMORY` ← `AMDKFD_IOC_UNMAP_MEMORY_FROM_GPU` (0x18)

## Capabilities

### New Capabilities

- `kfd-portability`: KFD 真实驱动可移植性契约。定义 UsrLinuxEmu 集成验证的方法（拷贝 → `#include` 调整 → 编译 → ioctl 跑通）、边界（仅 `#include` 路径调整，逻辑零修改）、决策（worktree 隔离 + HAL ops 走 ADR + 回归测试先行）。

### Modified Capabilities

None — UsrLinuxEmu 阶段 1.4 子阶段在 1.0/1.1/1.2/1.3 已锁定的 capability 上做集成验证，不修改既有 capability 行为。

## Impact

- **Code 规模**：~5K 行 KFD 移植（按路线图 §5 风险缓解"先移植 ~5K 行核心文件"）
  - `plugins/gpu_driver/drv/kfd/` 新建目录（5 个 .c + CMakeLists.txt）
  - `tests/test_kfd_portability_standalone.cpp` 新建（~300 行）
  - `docs/05-advanced/kfd-portability-report.md` 新建（~150 行）
- **依赖关系**：
  - **上游前置**：
    - [stage-1.0 PCIe Emulation](../archive/stage-1-0-pcie-emu/)（已归档，2026-07-02）
    - [stage-1.1 IOMMU+ATS](../archive/2026-07-02-stage-1-1-iommu-ats/)（已归档，2026-07-02）
    - [stage-1.2 DRM Subset](../archive/2026-07-02-stage-1-2-drm-subset/)（已归档，2026-07-02）
    - [stage-1.3 UVM/HMM](../archive/2026-07-04-stage-1-3-uvm-hmm/)（已归档，2026-07-04）
  - **下游阻塞**：阶段 2（多设备插件化）—— 阶段 1 完成后 UsrLinuxEmu 具备编译运行真实 KFD/NV 内核驱动的能力
- **System C ioctl 编号变更**：无（1.2 阶段已预留 5 个 KFD ioctl 编号 0x40 扩展 + 0x44-0x47）
- **HAL 接口契约**：**不预先**新增 `hal_iommu_*` / `hal_uvm_*` ops（按 ADR-027 spec-driven + ADR-035 治理；用户决策 2：**仅当 1.4 集成 KFD 时按需走独立 ADR 流程**）
- **ADR-035 合规**：所有 HAL / 状态变更走 change 流程；本次如发现 KFD 实际调用 `iommu_*` 或 `mmu_notifier_*` API，需创建独立 `hal-ops-extension-kfd` change 走 ADR
- **构建/测试**：
  - `plugins/gpu_driver/CMakeLists.txt` 已含 `add_subdirectory(kfd)`（1.2 阶段连入，commit `ca75fc6`）
  - `tests/CMakeLists.txt` 注册新测试 `test_kfd_portability_standalone`
- **文档**：
  - `docs/05-advanced/kfd-portability-report.md`（**新增**，移植报告，含 warnings 数量 / 错误码一致性 / 决策点）
  - `docs/02_architecture/post-refactor-architecture.md §1.10` 标注 Stage 1 完成
  - `docs/roadmap/stage-1-kernel-emu.md` 顶部状态从 🔄 改为 ✅
  - `docs/roadmap/README.md` 阶段 1 行状态从 🔄 计划中 改为 ✅ 已达成
  - `README.md` 顶部 badges 同步更新
- **风险**（继承自路线图 §5 + 用户决策 + Oracle 评估）：
  - **概率高/影响高**：KFD 代码量大（~50K 行完整 KFD）→ 缓解：按路线图 §5，1.4 仅移植 ~5K 行核心文件（5 个 .c），不做完整 KFD 移植
  - **概率中/影响高**：HAL ops 临时添加 → 缓解：用户决策 2 严格走 ADR 流程（每个新 HAL op → 一个独立 ADR → 一个独立 commit）
  - **概率中/影响中**：worktree 与 main 分支同步冲突 → 缓解：决策 1 用 worktree 隔离，合并前 rebase main
  - **概率低/影响高**：1.2/1.3 G1-G4 边界契约 regression → 缓解：决策 3 跑回归测试（kfd_queue.c + G1-G4 + KFD handler dispatch）

## Launch Conditions

本 change 进入正式实施前必须满足 4 条启动条件：

- **LC1**：1.0/1.1/1.2/1.3 四个子阶段全部完成（SSOT §1.10 标注 `[x]`）—— **2026-07-04 已达成**（git log + ctest 63/63 PASS）
- **LC2**：1.2 阶段 KFD 单文件 PoC（`kfd_queue.c`）编译通过 —— **2026-07-02 已达成**（commit `c42e60e`，errors=0, warnings=2, 零逻辑修改）
- **LC3**：回归测试无 regression（用户决策 3）—— **2026-07-04 验证中**（`test_drm_kfd_handlers_standalone` + `test_uvm_drm_lifecycle_standalone` G1-G4）
- **LC4**：worktree 创建完成（用户决策 1，**实施 1.4 代码时创建**，不在 change 启动阶段）

> LC4 故意延后到实施阶段创建：本 change 启动阶段（写 proposal / design / tasks / specs + 验证 OpenSpec）可在 main 上完成；实际代码移植在独立 worktree（`stage-1.4-kfd-portability`）进行，遵守 using-git-worktrees skill 约定。