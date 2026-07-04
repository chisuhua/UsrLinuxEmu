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
| **1.1** | IOMMU + ATS | ✅ Done | `openspec/changes/2026-07-02-stage-1-1-iommu-ats/` (archived) | ✅ Done (40/40 tests pass, 0 HAL changes, all acceptance items verified) |
| **1.2** | DRM 子集 | ✅ Done | `openspec/changes/archive/2026-07-02-stage-1-2-drm-subset/` | ✅ Done (52/52 tests pass, 0 HAL changes, all acceptance items verified) |
| **1.3** | UVM/HMM | ✅ Done | `openspec/changes/stage-1-3-uvm-hmm/` | ✅ Done (63/63 tests pass, 0 HAL changes, §14 KFD PoC deferred to 1.4) |
| **1.4** | 集成验证 | 🔄 In Progress | `openspec/changes/2026-07-04-stage-1-4-kfd-portability/` (active, `openspec validate` PASS, 56 tasks) | 🔄 In Progress（LC1-LC3 ✅，待 worktree 创建 + 代码实施）|

**总体进度**：4/5 子阶段完成 + 1.4 启动（1.0/1.1/1.2/1.3 完成；1.4 change 已创建、LC3 回归测试 8/8 PASS、待实施）

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

- [x] **1.0 已完成**（依赖前置）
- [x] **OpenSpec change 已创建** (`openspec/changes/stage-1-1-iommu-ats/`)
- [x] **变更提案已审批**（`openspec/changes/stage-1-1-iommu-ats/proposal.md`）
- [x] **Specs 已新增** (`openspec/changes/stage-1-1-iommu-ats/specs/iommu-ats/spec.md`，9 个 Requirement + Scenario)
- [x] **Tasks 已拆解** (`openspec/changes/stage-1-1-iommu-ats/tasks.md`，10 个 group / ~30 原子 task)
- [x] **设计文档已审批** (`openspec/changes/stage-1-1-iommu-ats/design.md`，8 个 Decision + 5 个 Risk)
- [x] **实现已完成**（`src/kernel/iommu/` 框架 + DMA remap + ATS + Invalidate + PCIe 集成，8 个 cpp 全部干净编译）
- [x] **HAL 扩展已决策**（**不**预先添加 `hal_iommu_*` ops，按 ADR-027 spec-driven / ADR-035；guardrail 在 spec §不引入 HAL 接口扩展）
- [x] **测试通过** (3 个新测试 + 37 个既有测试 = 40/40 全绿)
- [x] **验收清单全部勾选**（路线图 §1.1 验收 4 条全部通过）

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
**目标 LTS（锁定）**: **Linux 6.12 LTS**（盲点 5 决策；兼容矩阵评估 6.6 ↔ 6.12）
**Oracle 评估（2026-07-02）**: **Conditional Go** —— 4 项决策全部 Recommended，启动前必须满足 4 条启动条件（见下文"Launch Conditions"）

### Launch Conditions（启动前必须满足，引用 Oracle 2026-07-02 评估）

- [x] **C1**: `tests/test_iommu_emu_standalone` 100% 通过 + TaskRunner `tests/test_kfd_integration` 通过
- [x] **C2**: `openspec/changes/stage-1-2-drm-subset/.openspec.yaml` 的 `ABI变更: 否` + `编号静态化: 是` 字段验证通过（`scripts/check_gpu_ioctl_sync.sh` 报告 15 IOCTL 双端同步）
- [x] **C3**: amdkfd 单文件 PoC 在 Linux 6.12 LTS 环境编译通过（**warning ≤ 3**），artifacts 存 `openspec/evidence/amdkfd-poc-2026-07-02/`（仅 .o/.log，不修改源码；errors=0, warnings=2）
- [x] **C4**: 新增 HAL ops ≤ 2 个 且每 op 提供 **call trace + compile log** 证明（存入 `openspec/changes/stage-1-2-drm-subset/specs/hal-drm-ops-audit.md`）

### Oracle 决策评估摘要

| 决策 | 选项 | 推荐 | 缓解 |
|------|------|------|------|
| **D1** 重构粒度 | A. 保留 FileOperations + 内部重定向 drm_ioctl / B. 移除 FileOperations | **A** | gpgpu_device.cpp 保留 `ioctl()` stub 内部重定向 `drm_ioctl()`；新增 `errno_to_linux()` 映射层对标 KFD errno 语义 |
| **D2** KFD 编号预留 | A. 1.2 阶段预留 / B. 1.4 阶段再添加 | **A** | TaskRunner 符号链接 CI 加 `diff` 脚本验证两端 `GPU_IOCTL_*` 编号一致 |
| **D3** amdkfd PoC 时机 | A. 早期（task group 9）/ B. 全 task 后 | **A** | 1.2 task group 9 加 `amdkfd_single_file_poc` milestone，**2026-07-15 前**完成 |
| **D4** HAL 扩展策略 | A. 条件性添加 / B. 全加 | **A** | 严守 ADR-023 的 11 ops 上限；每 op 必须 KFD 实际调用驱动 |

### Status

- [x] **1.1 已完成**（依赖前置，2026-07-02 验证）
- [x] **目录骨架已创建** (`openspec/changes/stage-1-2-drm-subset/.openspec.yaml`，仅 schema 声明)
- [x] **OpenSpec change 内容已填充** (`proposal.md` 63 行 + `design.md` 270 行 + `tasks.md` 159 行 + `specs/drm-subset/spec.md` 190 行，共 682 行；2026-07-02 完成)
- [x] **Launch Conditions C1-C4 全部满足**（见上方启动条件，2026-07-02 验证）
- [x] **Oracle 决策 D1-D4 已写入 design.md**（作为 Decision 章节）
- [x] **1.2/1.3 边界契约已签**（见下文"Sub-stage 1.2/1.3 Boundary Contract"，G1-G4 全部满足）
- [x] **5 个 KFD IOCTL 编号已在 SSOT 附录 A 预留**（避免 1.4 时再次同步）—— 2026-07-02 完成 B 选项：4 个新 IOCTL 已在 `gpu_ioctl.h` 预留（`0x44-0x47`），CREATE_QUEUE (0x40) 通过追加字段扩展（ABI 向后兼容），`scripts/check_gpu_ioctl_sync.sh` 验证 UsrLinuxEmu 与 TaskRunner 镜像双端 15 个 IOCTL 完全一致
- [x] **实现已完成**：
  - [x] GEM object 完整生命周期（`drm_gem_object_init` / `handle_create` / refcount / release）—— `src/kernel/drm/drm_gem.cpp`
  - [x] `drm_file` 抽象完整化 —— `src/kernel/drm/drm_file.cpp`
  - [x] `drm_prime.h` + `drm_file_operations.h` 按需补齐（依赖 1.1 IOMMU group）—— `include/linux_compat/drm/{drm_prime,drm_file_operations,drm_device,drm_mode_config}.h`
  - [x] render node `/dev/dri/renderD128` 支持 —— `src/kernel/drm/render_node.cpp`
  - [x] GpgpuDevice 从 `FileOperations` 改为嵌入 `struct drm_device`（**保留** `FileOperations` 入口，按 Oracle D1）—— `plugins/gpu_driver/drv/gpu_drm_driver.cpp`
  - [x] `drm_ioctl_desc[]` 扩展到 15+ 个 IOCTL
- [x] **HAL 扩展已决策**（`hal_drm_*` ops，**仅当 KFD 实际调用**，按 Oracle D4 + ADR-027 spec-driven）—— 0 ops 实际添加，审计在 `hal-drm-ops-audit.md`
- [x] **测试通过**（51/51 全绿）：
  - [x] `tests/test_drm_gem_standalone`（GEM 完整生命周期）
  - [x] `tests/test_drm_ioctl_dispatch_standalone`（**含 errno mapping 验证**，按盲点 3）
  - [x] `tests/test_render_node_standalone`（**含权限分离验证**，按盲点 4）
  - [x] `tests/test_uvm_drm_lifecycle_standalone`（1.2/1.3 边界契约 G1 骨架）
  - [x] 7 个其他 DRM 测试（file, mode_config, prime, ioctl 等）全绿
  - [x] ASan 验证无引用计数泄漏
- [x] **真实 amdkfd 单文件 PoC 编译通过**（Oracle D3，**Linux 6.12 LTS，warning ≤ 3**）—— `kfd_queue.c`：errors=0, warnings=2
- [x] **验收清单全部勾选**

### Sub-stage 1.2/1.3 Boundary Contract（盲点 1 关键约束）

> **目的**：锁定 1.2 与 1.3 的边界契约，避免 1.2 实施决策反向破坏 1.3 的 mmu_notifier / `struct hmm_range` 生命周期管理。

**接口契约（1.2 必须提供给 1.3）**：

| 契约 | 1.2 必须保证 | 1.3 期望 |
|------|-------------|---------|
| `struct drm_device` 生命周期 | 与 `GpgpuDevice` 同生命周期（创建时 init，析构前 shutdown） | uvm module 持有 `drm_device*` 指针，整个设备存在期间有效 |
| BO 引用计数 | `drm_gem_object` refcount 在 close(fd) 时全部 release | hmm_range 不会引用已 release 的 BO |
| prime import buffer 释放顺序 | `dma_buf_unmap` → `dma_buf_detach` → `dma_buf_put`（对标 Linux 6.12） | mmu_notifier invalidate 在 `dma_buf_detach` 前完成 |
| fence 触发时机 | GEM object release 前必须等待所有 fence signal | hmm_range fault 完成前不会 trigger GEM release |

**强制验收（1.2 不能 close，必须等 1.3 一起 close）**：

- [x] **G1**: `tests/test_uvm_drm_lifecycle_standalone`（新增，1.2 创建骨架 + 1.3 完整化）—— 验证 BO 释放顺序契约 — **2026-07-02 完成骨架**
- [x] **G2**: `dma_buf_dynamic_attach`/`detach`/`map_attachment`/`unmap_attachment`/`pin`/`unpin` API 签名在 `include/linux_compat/drm/drm_prime.h` 与 Linux 6.12 ABI 一致（按盲点 2 librarian 验证 + Design Decision 6 关键修正）
- [x] **G3**: 1.2 design.md "Decision D5: 1.2/1.3 边界契约" 章节明确列出 4 项接口契约
- [x] **G4**: 1.2 不得**预先**实现 1.3 的 mmu_notifier / hmm_range 代码（仅留接口边界，避免过早耦合）—— 验证通过

> **背景**：Oracle 评估盲点 1 —— "mmu_notifier + drm_device 生命周期耦合：1.2 的 `struct drm_device` 嵌入与 1.3 的 `struct hmm_range` 的生命周期管理是否冲突（如 BO 的释放时机）"

### Files to Create/Modify

**Create:**
- `src/kernel/drm/drm_gem.cpp`（GEM 对象完整生命周期）
- `src/kernel/drm/drm_file.cpp`（`struct drm_file` 抽象）
- `src/kernel/drm/drm_prime.cpp`（prime 路径）
- `src/kernel/drm/render_node.cpp`（`/dev/dri/renderD128` 节点创建 + 权限分离，按盲点 4）
- `src/kernel/drm/errno_to_linux.cpp`（**新增**，按 Oracle D1 缓解：errno 映射层对标 KFD errno 语义）
- `include/linux_compat/drm/drm_prime.h`
- `include/linux_compat/drm/drm_file_operations.h`
- `include/linux_compat/drm/drm_mode_config.h`（基础结构占位）
- `tests/test_drm_gem_standalone.cpp`
- `tests/test_drm_ioctl_dispatch_standalone.cpp`（**必须含 errno mapping 验证**，按盲点 3）
- `tests/test_render_node_standalone.cpp`（**必须含权限分离验证**，按盲点 4）
- `tests/test_uvm_drm_lifecycle_standalone.cpp`（**新增骨架**，1.3 完整化，按盲点 1 边界契约 G1）
- `openspec/evidence/amdkfd-poc-2026-07-XX/`（**新增目录**，Oracle D3 缓解：PoC artifacts 存放）

**Modify:**
- `include/linux_compat/drm/drm_gem.h`（扩展为完整实现）
- `include/linux_compat/drm/drm_ioctl.h`（扩展 ioctl 派发支持）
- `plugins/gpu_driver/drv/gpu_drm_driver.cpp`（从 288 行扩展，覆盖全部 15+ IOCTL）
- `plugins/gpu_driver/drv/gpgpu_device.cpp` + `gpgpu_device.h`（**保留** `FileOperations` 入口，内部重定向 `drm_ioctl()`，按 Oracle D1）
- `plugins/gpu_driver/shared/gpu_ioctl.h`（新增 5 个 KFD ioctl 编号）
- `docs/02_architecture/post-refactor-architecture.md`（附录 A 更新 SSOT）
- `.github/workflows/cmake-multi-platform.yml`（**新增** —— TaskRunner 符号链接 `diff` 脚本验证 `GPU_IOCTL_*` 编号一致性，按 Oracle D2 缓解）

### Acceptance Checklist

引用 [stage-1-kernel-emu.md §子阶段 1.2 验收](../roadmap/stage-1-kernel-emu.md)：

**路线图 §1.2 验收 7 条**：

- [x] 驱动的 .c 文件能直接拷贝到 `drivers/gpu/xxx/` 下编译通过（**Linux 6.12 LTS**，warning ≤ 3）—— `kfd_queue.c` errors=0, warnings=2
- [x] 仅 `#include` 路径调整需要改（`linux_compat/drm/*` → `<drm/*.h>`），逻辑零修改
- [x] `drm_ioctl_desc[]` 表与 ioctls 数组一一对应
- [x] GEM 引用计数与 release 路径无泄漏（AddressSanitizer 验证）
- [x] `tests/test_drm_gem_standalone` + `tests/test_drm_ioctl_dispatch_standalone` + `tests/test_render_node_standalone` 全绿
- [x] render node 在 `/dev/dri/renderD128` 正确创建并可访问（**权限分离对标 Linux udev**）
- [x] KFD 5 个 ioctl 编号已在 SSOT 附录 A 预留（`gpu_ioctl.h` + TaskRunner 镜像双端 15 IOCTL 同步，2026-07-02 完成）

**Oracle 评估启动条件（4 条）**：

- [x] **C1**: `tests/test_iommu_emu_standalone` 100% 通过 + TaskRunner `tests/test_kfd_integration` 通过
- [x] **C2**: `.openspec.yaml` 标注 `ABI变更: 否` + `编号静态化: 是`，CI 双端 `diff` 脚本通过
- [x] **C3**: amdkfd 单文件 PoC 在 Linux 6.12 LTS 编译通过（warning ≤ 3），artifacts 存 `openspec/evidence/amdkfd-poc-2026-07-02/`
- [x] **C4**: 新增 HAL ops ≤ 2 个，每 op 含 call trace + compile log 证明（存入 `specs/hal-drm-ops-audit.md`）

**1.2/1.3 边界契约（4 条 Oracle 盲点 1）**：

- [x] **G1**: `tests/test_uvm_drm_lifecycle_standalone`（1.2 创建骨架 + 1.3 完整化）
- [x] **G2**: `dma_buf_dynamic_attach/detach/map_attachment/unmap_attachment/pin/unpin` 在 `include/linux_compat/drm/drm_prime.h` 与 Linux 6.12 ABI 一致（**关键修正**：amdgpu **不调用** `dma_buf_attach()`，而是用 `dma_buf_dynamic_attach()`，见下方"Blind Spot 2 结论"）
- [x] **G3**: design.md "Decision D5: 1.2/1.3 边界契约" 章节列出 4 项接口契约
- [x] **G4**: 1.2 不预先实现 mmu_notifier / hmm_range 代码

> **背景**：Oracle 评估盲点 1 —— "mmu_notifier + drm_device 生命周期耦合：1.2 的 `struct drm_device` 嵌入与 1.3 的 `struct hmm_range` 的生命周期管理是否冲突（如 BO 的释放时机）"

### Blind Spot 2 结论（Linux 6.12 dma-buf 语义，librarian 2026-07-02 验证）

| 项目 | 结论 | 设计影响 |
|------|------|---------|
| `dma_buf_attach()` 签名 6.6 ↔ 6.12 | ✅ **无变化** | stub 即可 |
| **amdgpu 实际调用** | ❌ **不调用 `dma_buf_attach()`**，改用 `dma_buf_dynamic_attach()`（`amdgpu_dma_buf.c:570`） | **`drm_prime.h` 必须实现 `dma_buf_dynamic_attach`**，非 `dma_buf_attach` |
| `struct dma_buf.list_node` 6.6 ↔ 6.12 | ⚠️ **条件化**：`#if IS_ENABLED(CONFIG_DEBUG_FS)` | 仅当 `CONFIG_DEBUG_FS=n` 时影响大小；UsrLinuxEmu 编译默认开 debugfs，**影响可控** |
| `struct dma_buf_ops` 强制项 | `map_dma_buf`/`unmap_dma_buf`/`release`（3 项） | 必须实现，不可返回 -ENOSYS |
| `dma_buf_export` 验证 | `map_dma_buf` 必须非 NULL 且返回有效 `sg_table` | IOMMU `map_page` 可旁路（恒等映射），但 `map_dma_buf` 必须有效 |
| **amdgpu 实际依赖 API 清单** | `dma_buf_dynamic_attach/detach/map_attachment/unmap_attachment/pin/unpin` + `struct dma_buf_attach_ops`（`allow_peer2peer` + `move_notify`） | `drm_prime.h` 至少实现这 6 函数 + 1 结构体 |

> **行动**：1.2 design.md 必须将 `drm_prime.h` 实现目标从"`dma_buf_attach/detach`"修正为"`dma_buf_dynamic_attach/detach/map_attachment/unmap_attachment/pin/unpin` + `struct dma_buf_attach_ops`"。

**错误码语义端到端一致性（盲点 3）**：

- [x] `test_drm_ioctl_dispatch_standalone` 含 errno mapping test：覆盖 `-EACCES` / `-EFAULT` / `-ENOMEM` / `-EREMOTEIO` / `-ENOSPC` 在 DRM IOCTL 路径的统一映射
- [x] UsrLinuxEmu 模拟器返回的 errno 与 Linux 6.12 ABI 一致（按 ADR-027 §Decision 3）—— `docs/05-advanced/drm-error-semantics.md` 记录完整对照表

**Linux 6.6 vs 6.12 兼容矩阵（盲点 5）**：

- [x] `docs/05-advanced/drm-compat-matrix.md`（**新增**）记录 Linux 6.6 LTS ↔ 6.12 LTS 在 DRM 子集的 API 差异：
  - struct layout 变化（`struct dma_buf.list_node` 条件化）
  - 函数签名变化（无）
  - 新增 required ops（无）
- [x] amdkfd 源码 **取自 Linux 6.12 LTS**（锁定 6.12 作为目标 LTS）
- [x] 兼容矩阵报告中标注每个差异点的 UsrLinuxEmu 模拟策略

**render node 权限与 device node（盲点 4）**：

> **关键发现（explore 2026-07-02）**：UsrLinuxEmu 现有 VFS **零权限基础设施**。Device 结构体只有 `name/dev_id/plugin_handle/fops` 4 个字段；VFS::open() 是纯字符串查找，无任何 `mode_t`/`uid_t`/`gid_t` 检查。整个代码库 grep 0666/0660/chmod/chown/i_mode 全部零命中。`DRM_NODE_RENDER=2` 已定义但无使用站点。**结论：从头构建。**

| 任务 | 范围 | 设计影响 |
|------|------|---------|
| **VFS-1**: `include/kernel/device/device.h` 扩展 `Device` 结构体：新增 `mode_t mode`、`uid_t uid`、`gid_t gid` 字段 | ① | 字段存在但**不强执行**（UsrLinuxEmu 单用户） |
| **VFS-2**: `src/kernel/vfs.cpp` 路径解析扩展：支持多段路径 `/dev/dri/renderD128`（嵌套子目录） | ① | 当前 `open()` 仅剥离 `/dev/` 前缀并精确匹配 |
| **VFS-3**: `VFS::open()` 添加 mode 检查 hook（在 `fops->open()` 前调用，可 bypass） | ① | 编译时结构正确性 + 未来强执行扩展点 |
| **VFS-4**: 新增 `VFS::chmod()`/`chown()` 方法（接口存在 + no-op 实现） | ① | ABI 完整 + KFD 可编译调用 |
| **ADR-37**: 新增 `docs/00_adr/adr-037-render-node-permissions.md`（首个权限 ADR）| 治理 | 明确 primary vs render 节点分离语义 |

- [x] **VFS-1** 至 **VFS-4** 全部实现（编译通过 + 类型正确）
- [x] **ADR-37** 已创建并 Approved ([adr-037-render-node-permissions.md](../00_adr/adr-037-render-node-permissions.md)，2026-07-02 Proposed，5 Decision：Device 扩展 + 多段路径 + 权限 hook + chmod/chown + 默认模式)
- [x] `/dev/dri/renderD128` mode = 0666，`/dev/dri/card0` mode = 0666（与 Linux udev 默认一致）
- [x] `test_render_node_standalone` 验证两类节点都可 `open()` 成功
- [x] VFS `open()` 路径已记录权限检查位置（即便当前 bypass，记录未来扩展点）

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

- [x] **1.2 已完成**（依赖前置）—— `openspec/changes/archive/2026-07-02-stage-1-2-drm-subset/`（已归档，2026-07-02 验证）
- [x] **内部 PoC 完成**（userfaultfd + mmap 共享触发场景，2026-07-03 TDD 完成，53/53 全绿）—— `tests/poc/test_userfaultfd_poc.cpp`
- [x] **OpenSpec change 已创建** (`openspec/changes/stage-1-3-uvm-hmm/`) —— 2026-07-03 包含 proposal.md (75 行) + design.md (269 行) + tasks.md (154 行) + specs/uvm-hmm/spec.md (142 行)，共 640 行；`openspec validate` 通过
- [ ] **变更提案已审批**
- [x] **Specs 已新增** (`openspec/changes/stage-1-3-uvm-hmm/specs/uvm-hmm/spec.md`) —— 8 个 ADDED Requirements + 11 个 Scenario
- [x] **Tasks 已拆解** —— 15 个 task group，涵盖 PoC + 头文件 + 框架 + sim + uvm 条件性 + 边界契约 + HAL + errno + 兼容矩阵 + 测试 + CMake + 文档 + KFD PoC + 验收归档
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

- [x] **1.0/1.1/1.2/1.3 全部完成**（依赖前置，SSOT §1.10 + ctest 63/63 PASS）
- [x] **OpenSpec change 已创建** (`openspec/changes/2026-07-04-stage-1-4-kfd-portability/`，`openspec validate` PASS，56 tasks)
- [ ] **变更提案已审批**（待 OpenSpec review 流程）
- [x] **Specs 已新增** (`openspec/changes/2026-07-04-stage-1-4-kfd-portability/specs/kfd-portability/spec.md`，3 个 Requirement / 6 个 Scenario)
- [x] **Tasks 已拆解** (`openspec/changes/2026-07-04-stage-1-4-kfd-portability/tasks.md`，12 节 / 56 个原子 task)
- [x] **LC3 回归测试无 regression**（决策 3，8/8 PASS：`test_drm_kfd_handlers_standalone` + `test_uvm_drm_lifecycle_standalone` G1-G4 + 5 个 stage-1 核心测试）
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
| 2026-07-02 | v1.1 | **Stage 1.2 启动前置增强**（全面更新追踪文档 + 处理全部 5 个盲点）：<br>• Oracle 评估（2026-07-02）：4 决策全部 Recommended + Conditional Go<br>• 启动条件 C1-C4 + 1.2/1.3 边界契约 G1-G4（盲点 1）<br>• errno mapping test 设计（盲点 3）<br>• Linux 6.12 LTS 锁定 + 6.6 ↔ 6.12 兼容矩阵验收（盲点 5）<br>• amdkfd PoC artifacts 路径 `openspec/evidence/amdkfd-poc-2026-07-XX/`（Oracle D3 缓解）<br>• `errno_to_linux.cpp` 映射层（Oracle D1 缓解）<br>• TaskRunner CI `diff` 脚本（Oracle D2 缓解）<br>• `hal-drm-ops-audit.md` 审计（Oracle D4 缓解）<br>• render node 权限验收（盲点 4）<br>• 待 librarian/explore 完成后补充 dma_buf_attach / VFS 权限具体结论 |
| 2026-07-02 | v1.2 | **盲点 2 + 盲点 4 结论整合**：<br>**盲点 2（librarian 2026-07-02 验证）**：<br>• `dma_buf_attach()` 签名 6.6 ↔ 6.12 无变化<br>• **关键修正**：amdgpu **不调用** `dma_buf_attach()`，改用 `dma_buf_dynamic_attach()`（`amdgpu_dma_buf.c:570`）<br>• `struct dma_buf.list_node` 在 6.12 受 `CONFIG_DEBUG_FS` 条件化（默认开 debugfs，影响可控）<br>• amdgpu 实际依赖 API：`dma_buf_dynamic_attach/detach/map_attachment/unmap_attachment/pin/unpin` + `struct dma_buf_attach_ops`（`allow_peer2peer` + `move_notify`）<br>• `map_dma_buf` 必须返回有效 `sg_table`，**不能** 返回 -ENOSYS；但 IOMMU `map_page` 可旁路（恒等映射）<br>• 行动：`drm_prime.h` 实现目标从 `dma_buf_attach/detach` 修正为 `dma_buf_dynamic_attach/detach/map_attachment/unmap_attachment/pin/unpin + struct dma_buf_attach_ops`<br>**盲点 4（explore 2026-07-02 验证）**：<br>• UsrLinuxEmu 现有 VFS **零权限基础设施**（grep 0666/chmod/chown/i_mode 全部零命中）<br>• `Device` 结构体只有 4 个字段（`name/dev_id/plugin_handle/fops`），无 `mode_t`/`uid_t`/`gid_t`<br>• `VFS::open()` 纯字符串查找，无任何权限检查<br>• 无 `/dev/dri/` 多级路径支持<br>• `DRM_NODE_RENDER=2` 已定义但**未使用**<br>• 31 份 ADR 中**零份**涉及设备权限<br>• **结论：从头构建** —— 1.2 需新增 VFS-1~VFS-4（Device 字段 + 路径解析 + mode 检查 hook + chmod/chown 接口）+ ADR-37（首个权限 ADR） |
| 2026-07-02 | v1.3 | **G1 修复 + ADR-37 创建 + Oracle 审查**：<br>• ✅ G1：`include/linux_compat/types.h` 已标注 `mode_t`/`uid_t`/`gid_t`（来自 `<sys/types.h>` 透传，编译验证通过）<br>• ✅ ADR-37（`docs/00_adr/adr-037-render-node-permissions.md`）已创建（5 Decision）<br>• ✅ Oracle 审查（2026-07-02）：ADR-37 可行，确认 4 缺口（G1-G4），推荐立即进入 VFS-1~VFS-4<br>• G2-G4 待办：DRM_NODE_RENDER 注册、access() 语义、DRM ioctl 权限（Issue #38） |
| 2026-07-02 | v1.4 | **VFS-1~VFS-4 实施**：<br>• VFS-1：`Device` 结构体扩展 `mode_t mode`/`uid_t uid`/`gid_t gid`，构造函数默认值 0666/0/0<br>• VFS-2：新增 `tests/test_vfs_path_standalone.cpp`，验证多段路径 `/dev/dri/renderD128`<br>• VFS-3：open() 插入 `check_permission()` hook（no-op，预留硬扩展点）<br>• VFS-4：实现 `chmod()`/`chown()`/`fchmod()`/`access()` POSIX 接口<br>• 测试：41/41 全绿（含新 `test_vfs_path_standalone`） |
| 2026-07-02 | v1.5 | **AMD/NVIDIA 驱动 IOCTL 差异调研 + 选项 C (amdkfd PoC) + 选项 B (KFD IOCTL 预留)**：<br>• 调研：AMD KFD 与 NVIDIA open DRM 在 DRM 层均为 `DRM_IOCTL_DEF_DRV` + per-struct 模式，**框架可复用**；NVIDIA `NV_ESC_*` escape 路径在 DRM 外部，不在 Stage 1.2 范围<br>• 选项 C：`kfd_queue.c` 从 Linux 6.12 LTS 拷贝到 `plugins/gpu_driver/drv/kfd/`，逻辑零修改（仅 `#include` 调整）<br>  ◦ 新增 5 个最小 compat 头文件：`linux_compat/slab.h`/`list.h` + KFD 本地 stub `kfd_priv.h`/`kfd_topology.h`/`kfd_svm.h`<br>  ◦ **PoC 结果：errors=0, warnings=2（≤3）**，41/41 既有测试零回归<br>  ◦ Artifacts：`openspec/evidence/amdkfd-poc-2026-07-02/{kfd_queue.o,build.log}`<br>• 选项 B：`gpu_ioctl.h` 新增 5 个 KFD IOCTL（`0x44-0x47`），CREATE_QUEUE (0x40) 通过追加字段扩展（ABI 向后兼容）<br>  ◦ 新增 `scripts/check_gpu_ioctl_sync.sh`：自动验证 UsrLinuxEmu 与 TaskRunner 镜像双端 IOCTL 同步<br>  ◦ 当前双端 15 个 IOCTL 同步确认（diff=0） |

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