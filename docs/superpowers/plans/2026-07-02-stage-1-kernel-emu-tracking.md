# Stage 1: Linux Kernel Environment Emulation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 补齐 Linux 内核环境模拟（PCIe + IOMMU + DRM + UVM/HMM），使真实 KFD 驱动代码**逻辑零修改**即可在 UsrLinuxEmu 中编译运行 5 个核心 ioctl。

**Architecture:**
- **5 个子阶段严格串行依赖**：1.0 PCIe → 1.1 IOMMU+ATS → 1.2 DRM → 1.3 UVM/HMM → 1.4 集成验证
- **3 区分架构原则**：① 内核环境模拟 + ② 可移植驱动 + ③ 硬件模拟（HAL 是 ②③ 之间的桥）
- **追踪型 plan**：本文档为高层路线追踪器，每个子阶段的详细 task 拆解到独立 OpenSpec change
- **演进驱动**：子阶段状态变更（`[ ]` → `[x]`）即触发对应 OpenSpec change 创建

**Tech Stack:**
- C++17（项目既有约束）
- CMake ≥ 3.14
- Catch2（vendored amalgamation，tests/catch_amalgamated.{hpp,cpp}）
- Linux 6.6/6.12 LTS 作为 API 对齐参考（不承诺 ABI 一致，按 ADR-027）
- OpenSpec CLI（change/spec/task 管理）
- ADR-035 治理流程（HAL/状态变更必须走 ADR/change）

**Reference Artifacts:**
- 路线图 SSOT: `docs/roadmap/stage-1-kernel-emu.md`（权威定义 5 子阶段范围与验收）
- 架构 SSOT: `docs/02_architecture/post-refactor-architecture.md §1.10`
- 3 区分原则: [ADR-036](../00_adr/adr-036-three-way-separation.md) ✅ Accepted
- 治理规则: [ADR-035](../00_adr/adr-035-governance-policy.md) ✅ Accepted
- Linux 兼容层策略: [ADR-027](../00_adr/adr-027-linux-compat-strategy.md) ✅ Accepted
- DRM/GEM/TTM 对齐: [ADR-019](../00_adr/adr-019-drm-gem-ttm-alignment.md) ✅ Accepted
- 驱动/仿真分离: [ADR-018](../00_adr/adr-018-driver-sim-separation.md) ✅ Accepted
- HAL 接口契约: [ADR-023](../00_adr/adr-023-hal-interface.md) ✅ Accepted
- UMQ（显式排除）: [ADR-024](../00_adr/adr-024-user-mode-queue-submission.md) ✅ Accepted

---

## Status Snapshot（总览）

| 子阶段 | 主题 | 计划状态 | OpenSpec Change | 当前进度 |
|--------|------|---------|----------------|---------|
| **1.0** | PCIe 设备模拟 | ✅ Done | `openspec/changes/stage-1-0-pcie-emu/` | ✅ Done |
| **1.1** | IOMMU + ATS | 📋 计划中 | `openspec/changes/stage-1-1-iommu-ats/`（待创建）| ⏸️ Not Started |
| **1.2** | DRM 子集 | 📋 计划中 | `openspec/changes/stage-1-2-drm-subset/`（待创建）| ⏸️ Not Started |
| **1.3** | UVM/HMM | 📋 计划中 | `openspec/changes/stage-1-3-uvm-hmm/`（待创建）| ⏸️ Not Started |
| **1.4** | 集成验证 | 📋 计划中 | `openspec/changes/stage-1-4-kfd-portability/`（待创建）| ⏸️ Not Started |

**总体进度**：0/5 子阶段完成（5 个 OpenSpec change 待创建）

---

## Implementation Pre-Flight

### Task 0: Verify Environment

**Files:**
- Read: `docs/roadmap/stage-1-kernel-emu.md`（必读 SSOT）
- Read: `docs/02_architecture/post-refactor-architecture.md §1.10`
- Read: `docs/00_adr/adr-035-governance-policy.md`（治理规则）

- [ ] **Step 1: Verify current branch is main**

```bash
cd /workspace/project/UsrLinuxEmu
git branch --show-current
# Expected: main
```

- [ ] **Step 2: Verify clean working tree**

```bash
cd /workspace/project/UsrLinuxEmu
git status -s
# Expected: clean（除可能存在的未追踪文件）
```

- [ ] **Step 3: Verify OpenSpec CLI is available**

```bash
cd /workspace/project/UsrLinuxEmu
openspec --version 2>&1 || echo "openspec CLI not installed"
# Expected: version output
# 如果缺失：npm install -g @opencode-ai/openspec 或参见 .opencode/skills/openspec-propose
```

- [ ] **Step 4: Verify baseline build succeeds**

```bash
cd /workspace/project/UsrLinuxEmu
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug .. 2>&1 | tail -5
make -j4 2>&1 | tail -10
ctest --output-on-failure 2>&1 | tail -10
# Expected: 所有现有测试通过（post-Phase 2 基线）
```

- [ ] **Step 5: Verify no in-flight stage-1 OpenSpec changes**

```bash
cd /workspace/project/UsrLinuxEmu
ls openspec/changes/ | grep -E "stage-1\.[0-4]" || echo "no in-flight stage-1 changes"
# Expected: "no in-flight stage-1 changes"（首次启动时）
```

- [ ] **Step 6: Update Stage 1 status badge**

在 `docs/roadmap/README.md` 阶段 1 行把状态从 "🔄 计划中" 改为 "🔄 启动中"，并在 `docs/roadmap/stage-1-kernel-emu.md` 顶部状态行追加"启动日期"。

---

## Sub-stage 1.0: PCIe 设备模拟

**范围引用**: [stage-1-kernel-emu.md §子阶段 1.0](../roadmap/stage-1-kernel-emu.md)
**预估工作量**: 2-3 周
**优先级**: P0（串行依赖的最前端）

### Status

- [x] **OpenSpec change 已创建** (`openspec/changes/stage-1-0-pcie-emu/`)
- [x] **变更提案已审批**（`openspec/proposal.md` 引用本 plan）
- [x] **Specs 已新增** (`openspec/specs/pcie-emu/spec.md`)
- [x] **Tasks 已拆解** (`openspec/changes/stage-1-0-pcie-emu/tasks.md`)
- [x] **实现已完成**（`src/kernel/pcie/pcie_emu.cpp` 完整实现）
- [x] **测试通过** (`tests/test_pcie_emu_standalone` 全绿)
- [x] **验收清单全部勾选**（见下方 Acceptance Checklist）

### Files to Create/Modify

**Create:**
- `src/kernel/pcie/pcie_emu.cpp`（基于 `include/kernel/pcie/pcie_emu.h` 接口实现）
- `src/kernel/pcie/config_space.cpp`（PCI 256B + PCIe 4KB config space）
- `src/kernel/pcie/msi_x.cpp`（MSI-X capability 结构 + PBA table + BIR 偏移）
- `src/kernel/pcie/capability_walk.cpp`（Capability 链遍历：Power Mgmt / PCIe / MSI / MSI-X / Vendor Specific）
- `include/linux_compat/pci/pci.h`（Linux 内核 pci_* API 子集，按 ADR-027 增量）
- `include/linux_compat/pci/msi.h`
- `tests/test_pcie_emu_standalone.cpp`
- `tests/test_config_space_standalone.cpp`
- `tests/test_msi_x_inject_standalone.cpp`

**Modify:**
- `include/kernel/pcie/pcie_emu.h`（扩展 MSI-X 接口 + config space 访问）
- `src/CMakeLists.txt`（添加 `src/kernel/pcie/` 到 kernel SHARED 库）
- `tests/CMakeLists.txt`（注册新 test 目标)
- `docs/02_architecture/post-refactor-architecture.md §1.10`（标注 PCIe 层就位状态）

### Acceptance Checklist

引用 [stage-1-kernel-emu.md §子阶段 1.0 验收](../roadmap/stage-1-kernel-emu.md)：

- [x] `lspci -v` 在 UsrLinuxEmu 内可见模拟设备（config space / BAR / capability 全部正确）
- [x] 驱动能响应 config space read/write（通过标准内核 API `pci_read_config_byte/word/dword`）
- [x] MSI-X 中断能成功注入并被驱动处理（`request_irq` → 中断 handler 触发）
- [x] `tests/test_pcie_emu_standalone` 覆盖 config space + BAR + MSI-X 注入
- [x] Capability 链遍历正确（Power Mgmt / PCIe / MSI / MSI-X / Vendor Specific 全部识别）
- [x] 文档更新：`docs/02_architecture/post-refactor-architecture.md §1.10` 标注 1.0 完成

### Trigger Next Change

完成 1.0 全部 checkbox 后，**触发 1.1 的 OpenSpec change 创建**：

```bash
cd /workspace/project/UsrLinuxEmu
# 归档 1.0 change
openspec archive stage-1-0-pcie-emu
# 启动 1.1
openspec propose stage-1-1-iommu-ats \
    --template docs/superpowers/plans/2026-07-02-stage-1-kernel-emu-tracking.md \
    --reference docs/roadmap/stage-1-kernel-emu.md#子阶段-11--iommu--ats
```

---

## Sub-stage 1.1: IOMMU + ATS

**范围引用**: [stage-1-kernel-emu.md §子阶段 1.1](../roadmap/stage-1-kernel-emu.md)
**预估工作量**: 3-4 周
**优先级**: P0（DRM prime 路径的前置依赖）
**依赖**: 1.0 PCIe 设备枚举完成

### Status

- [ ] **1.0 已完成**（依赖前置）
- [ ] **OpenSpec change 已创建** (`openspec/changes/stage-1-1-iommu-ats/`)
- [ ] **变更提案已审批**
- [ ] **Specs 已新增** (`openspec/specs/iommu-ats/spec.md`)
- [ ] **Tasks 已拆解**
- [ ] **实现已完成**（`src/kernel/iommu/` 框架 + DMA remap + ATS）
- [ ] **HAL 扩展已决策**（`hal_iommu_*` ops 按 ADR-035 走 ADR 流程，**仅当 KFD 实际调用**）
- [ ] **测试通过** (`tests/test_iommu_emu_standalone`)
- [ ] **验收清单全部勾选**

### Files to Create/Modify

**Create:**
- `src/kernel/iommu/iommu_domain.cpp`（对齐 Linux 6.6/6.12 LTS `iommu_domain`）
- `src/kernel/iommu/iommu_group.cpp`（group 拓扑）
- `src/kernel/iommu/ioasid.cpp`
- `src/kernel/iommu/dma_remap.cpp`（vtd + amd-iommu 最小子集）
- `src/kernel/iommu/ats_protocol.cpp`（device-TLB 请求/完成处理）
- `src/kernel/iommu/invalidate.cpp`（device-IOTLB invalidate 与 mmu_notifier 集成）
- `include/linux_compat/iommu/iommu.h`
- `include/linux_compat/iommu/iommu_domain.h`
- `tests/test_iommu_emu_standalone.cpp`
- `tests/test_dma_remap_standalone.cpp`
- `tests/test_ats_protocol_standalone.cpp`

**Modify:**
- `include/linux_compat/drm/drm_gem.h`（prime 路径补齐，依赖 IOMMU group）
- `plugins/gpu_driver/hal/hal_user.cpp` + `hal_mock.cpp`（添加 `hal_iommu_*` ops，**条件性**）

### Acceptance Checklist

引用 [stage-1-kernel-emu.md §子阶段 1.1 验收](../roadmap/stage-1-kernel-emu.md)：

- [ ] 模拟 IOMMU 能响应 device-IOTLB invalidate
- [ ] ATS 请求能在 UsrLinuxEmu 内完整处理（含 translation completion 消息）
- [ ] DMA remapping 失败的错误码语义与 Linux 内核一致（`EREMOTEIO` 等）
- [ ] `tests/test_iommu_emu_standalone` 覆盖 group / ioasid / remap / ATS
- [ ] 错误码语义对照表已写入 ADR 或新增 `docs/05-advanced/iommu-error-semantics.md`

### Trigger Next Change

完成 1.1 全部 checkbox 后触发 1.2：

```bash
cd /workspace/project/UsrLinuxEmu
openspec archive stage-1-1-iommu-ats
openspec propose stage-1-2-drm-subset \
    --template docs/superpowers/plans/2026-07-02-stage-1-kernel-emu-tracking.md \
    --reference docs/roadmap/stage-1-kernel-emu.md#子阶段-12--drm-子集
```

---

## Sub-stage 1.2: DRM 子集

**范围引用**: [stage-1-kernel-emu.md §子阶段 1.2](../roadmap/stage-1-kernel-emu.md)
**预估工作量**: 3-4 周（基础已就位，重点是增量）
**优先级**: P0（KFD 移植的基础）
**依赖**: 1.1 IOMMU group 拓扑完成

### Status

- [ ] **1.1 已完成**（依赖前置）
- [ ] **OpenSpec change 已创建** (`openspec/changes/stage-1-2-drm-subset/`)
- [ ] **变更提案已审批**
- [ ] **Specs 已新增** (`openspec/specs/drm-subset/spec.md`)
- [ ] **Tasks 已拆解**
- [ ] **5 个 KFD IOCTL 编号已在 SSOT 附录 A 预留**（避免 1.4 时再次同步）
- [ ] **实现已完成**：
  - [ ] GEM object 完整生命周期（`drm_gem_object_init` / `handle_create` / refcount / release）
  - [ ] `drm_file` 抽象完整化
  - [ ] `drm_prime.h` + `drm_file_operations.h` 按需补齐
  - [ ] render node `/dev/dri/renderD128` 支持
  - [ ] GpgpuDevice 从 `FileOperations` 改为嵌入 `struct drm_device`
  - [ ] `drm_ioctl_desc[]` 扩展到 15+ 个 IOCTL
- [ ] **HAL 扩展已决策**（`hal_drm_*` ops，**仅当 KFD 实际调用**）
- [ ] **测试通过**：
  - [ ] `tests/test_drm_gem_standalone`
  - [ ] `tests/test_drm_ioctl_dispatch_standalone`
  - [ ] ASan 验证无引用计数泄漏
- [ ] **真实 amdkfd 单文件 PoC 编译通过**（路线图 §1.2 验收第 1 条最高门槛）
- [ ] **验收清单全部勾选**

### Files to Create/Modify

**Create:**
- `src/kernel/drm/drm_gem.cpp`（GEM 对象完整生命周期）
- `src/kernel/drm/drm_file.cpp`（`struct drm_file` 抽象）
- `src/kernel/drm/drm_prime.cpp`（prime 路径）
- `src/kernel/drm/render_node.cpp`（`/dev/dri/renderD128` 节点创建 + 权限分离）
- `include/linux_compat/drm/drm_prime.h`
- `include/linux_compat/drm/drm_file_operations.h`
- `include/linux_compat/drm/drm_mode_config.h`（基础结构占位）
- `tests/test_drm_gem_standalone.cpp`
- `tests/test_drm_ioctl_dispatch_standalone.cpp`
- `tests/test_render_node_standalone.cpp`

**Modify:**
- `include/linux_compat/drm/drm_gem.h`（扩展为完整实现）
- `include/linux_compat/drm/drm_ioctl.h`（扩展 ioctl 派发支持）
- `plugins/gpu_driver/drv/gpu_drm_driver.cpp`（从 288 行扩展，覆盖全部 15+ IOCTL）
- `plugins/gpu_driver/drv/gpgpu_device.cpp` + `gpgpu_device.h`（重构嵌入 `struct drm_device`，保留 System C 编号）
- `plugins/gpu_driver/shared/gpu_ioctl.h`（新增 5 个 KFD ioctl 编号）
- `docs/02_architecture/post-refactor-architecture.md`（附录 A 更新 SSOT）

### Acceptance Checklist

引用 [stage-1-kernel-emu.md §子阶段 1.2 验收](../roadmap/stage-1-kernel-emu.md)：

- [ ] 驱动的 .c 文件能直接拷贝到 `drivers/gpu/xxx/` 下编译通过（Linux 6.x LTS 环境）
- [ ] 仅 `#include` 路径调整需要改（`linux_compat/drm/*` → `<drm/*.h>`），逻辑零修改
- [ ] `drm_ioctl_desc[]` 表与 ioctls 数组一一对应
- [ ] GEM 引用计数与 release 路径无泄漏（AddressSanitizer 验证）
- [ ] `tests/test_drm_gem_standalone` + `tests/test_drm_ioctl_dispatch_standalone` 全绿
- [ ] render node 在 `/dev/dri/renderD128` 正确创建并可访问
- [ ] KFD 5 个 ioctl 编号已在 SSOT 附录 A 预留

### Trigger Next Change

完成 1.2 全部 checkbox 后触发 1.3：

```bash
cd /workspace/project/UsrLinuxEmu
openspec archive stage-1-2-drm-subset
openspec propose stage-1-3-uvm-hmm \
    --template docs/superpowers/plans/2026-07-02-stage-1-kernel-emu-tracking.md \
    --reference docs/roadmap/stage-1-kernel-emu.md#子阶段-13--uvmhmm
```

---

## Sub-stage 1.3: UVM/HMM

**范围引用**: [stage-1-kernel-emu.md §子阶段 1.3](../roadmap/stage-1-kernel-emu.md)
**预估工作量**: 3-4 周（含 PoC，风险最高）
**优先级**: P0
**依赖**: 1.2 `drm_device` 完整化（uvm module 的父设备）
**重要澄清**: **UMQ 不在本子阶段范围**（UMQ 由 ADR-024 完整覆盖）

### Status

- [ ] **1.2 已完成**（依赖前置）
- [ ] **内部 PoC 先完成**（userfaultfd + mmap 共享触发场景，避免一上来铺全套，路线图 §5 缓解）
- [ ] **OpenSpec change 已创建** (`openspec/changes/stage-1-3-uvm-hmm/`)
- [ ] **变更提案已审批**
- [ ] **Specs 已新增** (`openspec/specs/uvm-hmm/spec.md`)
- [ ] **Tasks 已拆解**
- [ ] **实现已完成**：
  - [ ] `mmu_notifier` 框架
  - [ ] HMM API 子集（`hmm_range_fault/register/unregister` + `struct hmm_range` + `struct hmm_mirror`）
  - [ ] migrate 框架（page migration between CPU/GPU）
  - [ ] fault 注入路径（user-space mmap → page fault → mmu_notifier）
  - [ ] zone_device 最小实现（spm vma + page state machine）
  - [ ] ③ sim 端 page fault 处理 + migrate 状态机
- [ ] **HAL 扩展已决策**（`hal_uvm_*` ops，按 ADR-035 走 ADR 流程）
- [ ] **测试通过**：
  - [ ] `tests/test_mmu_notifier_standalone`
  - [ ] `tests/test_hmm_range_standalone`
  - [ ] `tests/test_svm_ioctl_standalone`
- [ ] **验收清单全部勾选**

### Files to Create/Modify

**Create:**
- `src/kernel/uvm/mmu_notifier.cpp`
- `src/kernel/uvm/hmm_range.cpp`
- `src/kernel/uvm/migrate.cpp`
- `src/kernel/uvm/fault_inject.cpp`
- `src/kernel/uvm/zone_device.cpp`
- `src/kernel/uvm/page_state_machine.cpp`（`PAGE_STATE_CPU` / `PAGE_STATE_GPU` / `PAGE_STATE_MIGRATING`）
- `include/linux_compat/mmu_notifier.h`
- `include/linux_compat/hmm.h`
- `plugins/gpu_driver/sim/page_fault_handler.cpp`
- `plugins/gpu_driver/sim/page_migration.cpp`
- `plugins/gpu_driver/uvm/svm_ioctl.cpp`（**仅当 KFD 有 uvm 子模块需要**）
- `tests/test_mmu_notifier_standalone.cpp`
- `tests/test_hmm_range_standalone.cpp`
- `tests/test_svm_ioctl_standalone.cpp`

**Modify:**
- `plugins/gpu_driver/hal/hal_user.cpp` + `hal_mock.cpp`（添加 `hal_uvm_*` ops）
- `plugins/gpu_driver/sim/CMakeLists.txt`（包含 page fault + migration）

### Acceptance Checklist

引用 [stage-1-kernel-emu.md §子阶段 1.3 验收](../roadmap/stage-1-kernel-emu.md)：

- [ ] mmap 共享能触发模拟 page fault
- [ ] KFD 的 **SVM (Shared Virtual Memory) ioctl** 能跑通
- [ ] HMM range fault → migrate → 通知全链路通畅
- [ ] mmu_notifier 在用户态 munmap 时正确触发 invalidation
- [ ] page state machine 状态转换正确（无死锁、无丢失 invalidation）
- [ ] 3 个 standalone 测试全绿

### Trigger Next Change

完成 1.3 全部 checkbox 后触发 1.4（最终集成验证）：

```bash
cd /workspace/project/UsrLinuxEmu
openspec archive stage-1-3-uvm-hmm
openspec propose stage-1-4-kfd-portability \
    --template docs/superpowers/plans/2026-07-02-stage-1-kernel-emu-tracking.md \
    --reference docs/roadmap/stage-1-kernel-emu.md#子阶段-14--集成验证
```

---

## Sub-stage 1.4: 集成验证

**范围引用**: [stage-1-kernel-emu.md §子阶段 1.4](../roadmap/stage-1-kernel-emu.md)
**预估工作量**: 2-3 周
**优先级**: P0（最终验收）
**依赖**: 1.0/1.1/1.2/1.3 全部完成

### Status

- [ ] **1.0/1.1/1.2/1.3 全部完成**（依赖前置）
- [ ] **OpenSpec change 已创建** (`openspec/changes/stage-1-4-kfd-portability/`)
- [ ] **变更提案已审批**
- [ ] **Specs 已新增** (`openspec/specs/kfd-portability/spec.md`)
- [ ] **Tasks 已拆解**
- [ ] **KFD 源码已拷贝**（Linux 6.6/6.12 LTS `drivers/gpu/drm/amd/amdkfd/` 5 个核心 .c）
- [ ] **移植完成**（拷贝到 `plugins/gpu_driver/drv/kfd/`，仅 `#include` 路径调整）
- [ ] **编译通过**（errors = 0）
- [ ] **5 个核心 ioctl 跑通**：
  - [ ] `AMDKFD_IOC_GET_PROCESS_APERTURE`
  - [ ] `AMDKFD_IOC_CREATE_QUEUE`（扩展参数）
  - [ ] `AMDKFD_IOC_UPDATE_QUEUE`
  - [ ] `AMDKFD_IOC_MAP_MEMORY`
  - [ ] `AMDKFD_IOC_UNMAP_MEMORY`
- [ ] **移植报告已写** (`docs/05-advanced/kfd-portability-report.md`)
- [ ] **端到端测试通过** (`tests/test_kfd_portability_standalone`)
- [ ] **Stage 1 全部 checkbox 完成**

### Files to Create/Modify

**Create:**
- `plugins/gpu_driver/drv/kfd/kfd_module.c`（来自 Linux LTS）
- `plugins/gpu_driver/drv/kfd/kfd_device.c`（来自 Linux LTS）
- `plugins/gpu_driver/drv/kfd/kfd_process.c`（来自 Linux LTS）
- `plugins/gpu_driver/drv/kfd/kfd_queue.c`（来自 Linux LTS）
- `plugins/gpu_driver/drv/kfd/kfd_doorbell.c`（来自 Linux LTS）
- `tests/test_kfd_portability_standalone.cpp`（端到端集成测试）
- `docs/05-advanced/kfd-portability-report.md`（移植报告）

**Modify:**
- `plugins/gpu_driver/CMakeLists.txt`（添加 kfd 子目录）
- `plugins/gpu_driver/shared/gpu_ioctl.h`（如果 1.2 阶段未预留，**此时必须**新增 5 个 IOCTL 编号）
- `docs/02_architecture/post-refactor-architecture.md §1.10`（标注 Stage 1 完成）

### Acceptance Checklist

引用 [stage-1-kernel-emu.md §子阶段 1.4 验收](../roadmap/stage-1-kernel-emu.md)：

- [ ] 编译通过（errors = 0，warnings 数量记录在移植报告）
- [ ] 5 个 ioctl 全部跑通（单测覆盖 happy path + 至少 1 个 error path）
- [ ] 驱动代码零修改（除 `#include` 路径）
- [ ] `docs/05-advanced/kfd-portability-report.md` 记录移植报告
- [ ] `tests/test_kfd_portability_standalone`（端到端集成测试）全绿
- [ ] **Stage 1 状态全部从 🔄 改为 ✅**

### Stage 1 Completion Trigger

完成 1.4 全部 checkbox 后，**归档所有 5 个 OpenSpec change 并更新 SSOT**：

```bash
cd /workspace/project/UsrLinuxEmu
for change in stage-1-0-pcie-emu stage-1-1-iommu-ats stage-1-2-drm-subset stage-1-3-uvm-hmm stage-1-4-kfd-portability; do
    openspec archive $change
done

# 更新路线图状态
# docs/roadmap/stage-1-kernel-emu.md 顶部状态从 🔄 改为 ✅
# docs/roadmap/README.md 阶段 1 行状态从 🔄 计划中 改为 ✅ 已达成
# README.md 顶部 badges 同步更新
```

---

## Cross-Stage Dependencies

```
1.0 PCIe ──> 1.1 IOMMU+ATS ──> 1.2 DRM ──> 1.3 UVM/HMM ──> 1.4 集成验证
                │                  │            │
                └──> 1.2 (HAL) ────┘            │
                                                  │
                               1.4 集成 反馈 1.0/1.1/1.2/1.3
```

| 上游 | 下游 | 依赖内容 |
|------|------|---------|
| 1.0 PCIe | 1.1 IOMMU | PCIe 设备枚举后才能确定 IOMMU group 拓扑 |
| 1.1 IOMMU | 1.2 DRM | IOMMU 框架是 DRM GEM prime 路径的基础 |
| 1.2 DRM | 1.3 UVM | DRM `drm_device` 是 uvm module 的父设备 |
| 1.0/1.1/1.2/1.3 | 1.4 KFD | KFD 移植需要完整内核环境模拟 |

**并行机会**（已在路线图 §6 标注）：
- 1.0 与 1.1 可在独立 worktree 并行（route ID 拓扑对接点明确）
- 1.2 与 1.3 **不可**并行（1.3 依赖 1.2 drm_device）

---

## Pre-Staged OpenSpec Change Templates

为加速后续 change 创建，预先提供模板：

### Change 命名约定

```
openspec/changes/stage-1.<N>-<short-name>/
├── proposal.md       # 变更提案（why + what + scope）
├── design.md         # 详细设计（接口/数据结构/算法）
├── tasks.md          # task 拆解（每个 task 一个 PR）
└── specs/
    └── <capability>/
        └── spec.md   # 新增 capability spec
```

### Proposal 模板（每个子阶段复用）

每个子阶段 proposal.md 应包含：

```markdown
# Stage 1.<N>: <子阶段主题> 提案

## Why
[引用 stage-1-kernel-emu.md §子阶段 1.<N> + ADR-035 治理规则]

## What
[范围 / 涉及层（①②③）/ 关键交付]

## Scope Boundary
[明确 in-scope / out-of-scope / 依赖前置子阶段]

## Layer Classification (ADR-036)
- ① [具体模块清单]
- ② [具体模块清单]
- ③ [具体模块清单]
- HAL [新增 ops 列表，**条件性**]

## ADR Compliance
- ADR-027 (Linux compat strategy): [说明]
- ADR-023 (HAL interface): [新增 ops 走 ADR 流程]
- ADR-035 (governance): [提案已审批]

## Reference
- docs/roadmap/stage-1-kernel-emu.md §子阶段 1.<N>
- docs/superpowers/plans/2026-07-02-stage-1-kernel-emu-tracking.md
```

---

## Pre-Mortem：风险与缓解（继承自路线图 §5）

| 风险 | 概率 | 影响 | 缓解 | 触发时机 |
|------|------|------|------|---------|
| KFD 代码量大（~50K 行）| 高 | 高 | 分阶段实施，1.2 + 1.3 先做小集实验证（先移植 ~5K 行核心文件）| 1.4 启动时 |
| 内核 API 覆盖不全 | 高 | 中 | 1.4 集成时按需补齐（ADR-027 spec-driven）| 每个子阶段 |
| 真机部署兼容 | 中 | 中 | HAL 是桥（ADR-036），`hal_user.cpp` 持续维护 | 1.4 验收后 |
| 文档审计基线漂移 | 低 | 低 | 每次 commit 前跑 `tools/docs-audit.sh`（pre-commit hook 自动）| 持续 |
| IOMMU 子系统理解偏差 | 中 | 中 | 参考 Linux 6.6/6.12 LTS 头文件，按 ADR-027 决策 3 不承诺 ABI 一致 | 1.1 实施 |
| mmu_notifier 路径复杂 | 中 | 高 | 1.4 集成前先在 1.3 内部做小范围 PoC | 1.3 启动时 |

---

## File Structure Map（阶段 1 涉及的所有文件）

### 新建目录（首次创建）

```
src/kernel/pcie/          # 1.0
src/kernel/iommu/         # 1.1
src/kernel/drm/           # 1.2 (linux_compat/drm 已有，实现 src 端)
src/kernel/uvm/           # 1.3
plugins/gpu_driver/drv/kfd/  # 1.4
include/linux_compat/pci/   # 1.0
include/linux_compat/iommu/ # 1.1
include/linux_compat/mmu_notifier.h  # 1.3
include/linux_compat/hmm.h  # 1.3
```

### 关键修改文件

| 文件 | 修改阶段 | 修改内容 |
|------|---------|---------|
| `include/kernel/pcie/pcie_emu.h` | 1.0 | 扩展 MSI-X + config space 接口 |
| `plugins/gpu_driver/drv/gpu_drm_driver.cpp` | 1.2 | 288 行 → 15+ IOCTL 覆盖 |
| `plugins/gpu_driver/drv/gpgpu_device.cpp` | 1.2 | 嵌入 `struct drm_device` |
| `plugins/gpu_driver/shared/gpu_ioctl.h` | 1.2 / 1.4 | 新增 5 个 KFD IOCTL 编号 |
| `docs/02_architecture/post-refactor-architecture.md` §1.10 | 各阶段完成时 | 更新层就位状态 |

---

## Change Log

| 日期 | 版本 | 变更 |
|------|------|------|
| 2026-07-02 | v1.0 | 初版：5 子阶段追踪 plan + checkbox 状态 + OpenSpec change 触发机制 |

---

## Maintenance Notes

### 本文档的更新规则

- **每次子阶段完成时**：勾选对应 checkbox + 在 Change Log 追加一行
- **每次 OpenSpec change 创建时**：在 Status Snapshot 表格填入 change 路径
- **每次路线图修订时**：同步更新本 plan 的 Reference Artifacts
- **不绑定** OpenSpec change 编号：本文档是高层追踪器，change 编号由实际创建决定

### 与 writing-plans skill 的关系

本文档采用追踪型 plan 模式（高层路线 + checkbox 状态 + OpenSpec change 触发点），**不是**一次性详细执行手册。每个子阶段的详细 task 拆解遵循各自 OpenSpec change 的 `tasks.md`（每个 task 一个独立 PR 的粒度）。

### ADR-035 治理对齐

任何子阶段涉及的 HAL 接口扩展、新增 capability spec、SSOT 变更均走 ADR 流程（ADR-035 Rule 3）。本文档本身不创建 ADR，但每个子阶段的 OpenSpec proposal 必须引用对应 ADR。

---

**维护者**: UsrLinuxEmu Architecture Team
**创建日期**: 2026-07-02
**最后更新**: 2026-07-02
**对应 SSOT**: [stage-1-kernel-emu.md](../roadmap/stage-1-kernel-emu.md)
**对应 ADR**: ADR-035 (governance) + ADR-036 (3-way principle) + ADR-027 (compat strategy)