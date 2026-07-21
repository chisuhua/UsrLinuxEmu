# 阶段 1: Linux 内核环境模拟

> **状态**: ✅ **已达成** (2026-07-16) — 子阶段 1.0-1.4 全部 + Tier-2 runtime penetration 完成 + C-12 KFD 多文件集成
> **目标**: 提供完整 Linux 内核环境，使在 UsrLinuxEmu 开发的驱动可编译运行 KFD / NV 内核驱动
> **范围**: DRM + UVM + IOMMU + ATS + PCIe BAR/中断
> **关联 SSOT**: [post-refactor-architecture.md §1.10](../02_architecture/post-refactor-architecture.md)
> **架构边界 SSOT**: [kfd-portability-boundary.md](../05-advanced/kfd-portability-boundary.md) (Tier-1 / Tier-2 分界，v1.2)
> **Stage 1.4 交付报告**: [kfd-portability-report.md](../05-advanced/kfd-portability-report.md) (commit `f41ace5`)
> **Stage 1.4 Tier-2 穿透报告**: [tier2-runtime-penetration-report.md](../05-advanced/tier2-runtime-penetration-report.md) (2026-07-05)
> **C-12 KFD 多文件集成**: [ADR-059](../00_adr/adr-059-kfd-multi-file-integration.md) ✅ Accepted — 6 模块 + sim_pfh/sim_pm 真实化 + 14 HAL ops
> **关联原则**: [ADR-036](../00_adr/adr-036-three-way-separation.md) (✅ Accepted)
> **维护者**: UsrLinuxEmu Architecture Team
> **最后更新**: 2026-07-21

---

## 1. 背景

UsrLinuxEmu 的核心目标（见 [ADR-001](../00_adr/adr-001-user-mode-emulation.md)）是让驱动开发者在无需 root 权限或内核编译的情况下开发与测试 GPGPU 驱动。**阶段 1** 是该目标的关键里程碑：当前 GpgpuDevice 是 UsrLinuxEmu 自创接口（手写 `switch-case` ioctl + 私有 BO 管理），与 Linux 内核 DRM/GEM/TTM 范式不兼容。要让真实 KFD / NV 驱动代码能够"零修改编译"，必须先把 Linux 内核环境（PCIe / IOMMU / DRM / UVM）补齐。

阶段 1 的**显式验收指标**：

1. **代码可移植性**：真实 KFD 驱动的 `.c` 文件拷贝到 UsrLinuxEmu 后，**仅** `#include` 路径需调整，逻辑零修改即可编译
2. **ioctl 兼容性**：至少 5 个核心 KFD ioctl 在 UsrLinuxEmu 中跑通
3. **架构判定一致性**：所有子阶段工作按 [ADR-036](../00_adr/adr-036-three-way-separation.md) 的 3 区分原则组织

---

## 2. 涉及层（按 3 区分）

阶段 1 的大部分工作集中在 ① 内核环境模拟层（补齐缺失的内核 API 子集），同时 ② 可移植驱动层与 ③ 硬件模拟层需要同步演进。

| 层 | 工作量占比 | 关键工作 |
|----|-----------|----------|
| ① Linux 内核环境模拟 | ~80% | PCIe, IOMMU, DRM, UVM/HMM 框架补齐 |
| ② 可移植的驱动代码 | ~15% | GpgpuDevice 重构为 drm_device 风格 |
| ③ 硬件模拟 | ~5% | ATS 响应、page fault 路径补充 |

> **HAL 角色**：HAL 是 ② 与 ③ 之间的**桥接机制**（非独立第 4 层），详见 [ADR-036](../00_adr/adr-036-three-way-separation.md) §"关键判定" 与 [ADR-023](../00_adr/adr-023-hal-interface.md)。阶段 1 涉及的所有新增 HAL 接口走 ADR 流程（[ADR-035](../00_adr/adr-035-governance-policy.md) Rule 3）。

---

## 3. 子阶段总览

| 子阶段 | 主题 | 涉及层 | 关键交付 |
|--------|------|--------|----------|
| [1.0](#子阶段-10--pcie-设备模拟) | PCIe 设备模拟 | ① | config space + BAR + MSI-X |
| [1.1](#子阶段-11--iommu--ats) | IOMMU + ATS | ① ② ③ | DMA remapping + ATS 协议 |
| [1.2](#子阶段-12--drm-子集) | DRM 子集 | ① ② | GpgpuDevice 重构为 drm_device 风格 |
| [1.3](#子阶段-13--uvmhmm) | UVM/HMM | ① ② ③ | mmu_notifier + migrate + fault |
| [1.4](#子阶段-14--集成验证) | 集成验证 | 全部 | 编译真实 KFD + 5 个 ioctl |

> **重要说明**：**UMQ (User Mode Queue) 不在 1.3 范围内**。UMQ 提交路径已由 [ADR-024](../00_adr/adr-024-user-mode-queue-submission.md) 完整覆盖（决策 1-5），状态 ✅ Accepted。ADR-024 之前的过渡性 UMQ 计划文档已归档（commit `chore(docs): remove absorbed pending plan umq-implementation`）。

---

## 子阶段 1.0 — PCIe 设备模拟

**目标**: 完整模拟 PCIe 设备配置空间 + BAR 映射 + MSI/MSI-X 中断注入

### ① 内核环境模拟

- 完善 `src/kernel/pcie/` 下的 pcie_emu 实现
- **Config Space**：PCI (256 bytes) + PCIe (4KB) 配置空间读写
- **BAR 映射**：BAR 0/1/2/3/4/5 映射（IO + Memory + ROM 三种类型）
- **MSI-X capability 结构**：PBA table + BIR 偏移 + 中断注入机制
- **PCIe 设备枚举模拟**：`lspci -v` 兼容输出
- Capability 链遍历（Power Management / PCIe / MSI / MSI-X / Vendor Specific）

### ② 可移植驱动

- 现有驱动无需改动
- PCIe 设备在 ① 模拟层暴露，驱动侧通过标准内核 API 访问

### ③ 硬件模拟

- 现有 sim 无需改动
- pcie_emu 视为 ① 的扩展点，不属于 ③ 仿真职责

### 验收

- [x] `lspci -v` 在 UsrLinuxEmu 内可见模拟设备（config space / BAR / capability 全部正确）
- [x] 驱动能响应 config space read/write（通过标准内核 API）
- [x] MSI-X 中断能成功注入并被驱动处理
- [x] 测试：`tests/test_pcie_emu_standalone` 覆盖 config space + BAR + MSI-X 注入

---

## 子阶段 1.1 — IOMMU + ATS

**目标**: 模拟 IOMMU group + ioasid + DMA remapping + ATS 协议

### ① 内核环境模拟

- 新增 `src/kernel/iommu/` 框架
- **IOMMU group + ioasid 数据结构**：与 Linux 内核 iommu_domain / iommu_group 对齐
- **DMA remapping 页表**：vtd (Intel) / amd-iommu (AMD) 行为的最小子集
- **ATS（Address Translation Services）协议响应**：device-TLB 与 IOMMU 之间的请求/完成处理
- **device-IOTLB invalidate 路径**：与 mmu_notifier 集成

### ② 可移植驱动

- HAL 扩展（**如果 KFD 用到 IOMMU API**）：新增 `hal_iommu_*` ops
- 走 ADR 流程（[ADR-035](../00_adr/adr-035-governance-policy.md) Rule 3）
- 驱动侧沿用 Linux 内核 iommu_domain 接口，无需感知模拟

### ③ 硬件模拟

- 模拟 ATS 请求/完成响应（含 invalidation 协议）
- 模拟 DMA remapping 失败处理（-EREMOTEIO 等错误码语义）

### 验收

- [x] 模拟 IOMMU 能响应 device-IOTLB invalidate（ADR-061 HAL IOMMU ops 扩展已覆盖）
- [x] ATS 请求能在 UsrLinuxEmu 内完整处理（含 translation completion 消息）
- [x] DMA remapping 失败的错误码语义与 Linux 内核一致
- [x] 测试：`tests/test_dma_remap_standalone` + `tests/test_ats_protocol_standalone` 覆盖 group / ioasid / remap / ATS

---

## 子阶段 1.2 — DRM 子集

**目标**: 完善 DRM 后端，使驱动代码看起来像真实 Linux 内核 DRM 驱动

### ① 内核环境模拟

- 完善 `include/linux_compat/drm/` 子集（基于 [ADR-019](../00_adr/adr-019-drm-gem-ttm-alignment.md) 已接受决策）：
  - `drm_gem.h` — GEM object 完整实现（dumb + prime 路径）
  - `drm_file.h` — DRM file descriptor 抽象
  - `drm_drv.h` — DRM driver 注册框架（drm_driver / drm_device 完整嵌入）
  - `drm_ioctl.h` — DRM IOCTL 派发（drm_ioctl_desc 表驱动）
  - 补全 `drm_prime.h` / `drm_file_operations.h` 等支撑头（按 [ADR-027](../00_adr/adr-027-linux-compat-strategy.md) P1 触发原则，按需增量）
- **render node 支持**：`/dev/dri/renderD128` 设备节点创建 + 权限分离
- `drm_mode_config` / `drm_minor` / `drm_connector` 等基础结构占位（最小可用即可）

### ② 可移植驱动

**重构 GpgpuDevice 为 drm_device 风格**：

- 从 `FileOperations` 改为 `struct drm_device` 嵌入
- ioctl handler 使用 `drm_ioctl_desc[]` 表（已有基础，需完整化 — 见 [ADR-019](../00_adr/adr-019-drm-gem-ttm-alignment.md) 决策 2）
- 使用 `struct drm_file *` 模式（已有基础，需完整化）
- 集成 GEM 对象生命周期（`drm_gem_object_init` / `handle_create` / refcount）
- 现有 System C ioctl 编号（`GPU_IOCTL_*`）保持不变，dispatch 路径切换到 drm_ioctl

**HAL 扩展**：新增 drm_object 相关 ops（**仅当 KFD 实际调用**），按 [ADR-027](../00_adr/adr-027-linux-compat-strategy.md) 决策 2 优先级排序

### ③ 硬件模拟

- 现有 sim 无需改动
- GEM object 后端依旧走 `sim/buddy_allocator`（[ADR-020](../00_adr/adr-020-libgpu-core-extraction.md) 产物）

### 验收

- [x] 驱动的 .c 文件能编译通过（C-12 6 模块 KFD 集成已验证）
- [x] 仅 `#include` 路径调整需要改（`linux_compat/drm/*` → `<drm/*.h>`），逻辑零修改
- [x] `drm_ioctl_desc[]` 表与 ioctls 数组一一对应（测试覆盖：`test_drm_ioctl_dispatch_standalone`）
- [x] GEM 引用计数与 release 路径无泄漏（AddressSanitizer 验证）
- [x] 测试：`tests/test_drm_gem_standalone` + `tests/test_drm_ioctl_dispatch_standalone` + `tests/test_drm_prime_standalone`

---

## 子阶段 1.3 — UVM/HMM

**目标**: 模拟 Unified Virtual Memory + Heterogeneous Memory Management

> ⚠️ **范围澄清**：本子阶段是 **UVM (Unified Virtual Memory) + HMM (Heterogeneous Memory Management)**，**不包含 UMQ (User Mode Queue)**。UMQ 由 [ADR-024](../00_adr/adr-024-user-mode-queue-submission.md) 完整覆盖（用户态 Ring Buffer + mmap Doorbell 路径），与 UVM/HMM 在内核子系统层级上正交。

### ① 内核环境模拟

- 新增 `src/kernel/uvm/` 框架
- **`mmu_notifier` 框架**：用户态 mmap 共享 → 内核 page invalidation 通知
- **HMM（Heterogeneous Memory Management）API 子集**（与 Linux 6.12 LTS 实际 API 对齐）：
  - `hmm_range_fault()` / `mmu_interval_notifier_insert()` / `mmu_interval_notifier_remove()`
  - `struct hmm_range` 完整字段（7 个：`notifier` / `notifier_seq` / `start` / `end` / `hmm_pfns` / `default_flags` / `pfn_flags_mask` / `dev_private_owner`）
  - `struct mmu_interval_notifier` + `struct mmu_interval_notifier_ops.invalidate` 回调（替代已从 Linux 6.x 移除的 `struct hmm_mirror`）
  - 序列号一致性协议：`mmu_interval_read_begin()` / `mmu_interval_read_retry()` / `mmu_interval_set_seq()`
  - HMM PFN flags：`HMM_PFN_VALID` / `HMM_PFN_WRITE` / `HMM_PFN_ERROR` / `HMM_PFN_REQ_FAULT` / `HMM_PFN_REQ_WRITE`（64-bit 编码，`HMM_PFN_VALID = 1UL << 63` 等）
- **migrate 框架**：page migration between CPU/GPU memory domain
- **fault 注入路径**：user-space mmap 触发 page fault → 通过 mmu_notifier 通知 device driver
- `zone_device` 模拟（最简实现：spm vma + page 状态机）

### ② 可移植驱动

- 新增 `plugins/gpu_driver/uvm/` 模块（**仅当 KFD 有 uvm 子模块需要**）
- HAL 扩展：新增 `hal_uvm_*` ops（mmap shared / fault 通知 / migrate 操作）
- 走 ADR 流程（[ADR-035](../00_adr/adr-035-governance-policy.md) Rule 3）
- 驱动侧沿用 Linux 内核 mmu_notifier + hmm_range 接口

### ③ 硬件模拟

- 新增 page fault 处理路径（接收 ① 的 fault 通知）
- 模拟 device memory 与 system memory 之间的 page migration
- page state machine：`PAGE_STATE_CPU` / `PAGE_STATE_GPU` / `PAGE_STATE_MIGRATING`

### 验收

- [x] mmap 共享能触发模拟 page fault（mm_shim 真实化已覆盖，见 tier2-report）
- [x] KFD 的 **SVM (Shared Virtual Memory) ioctl** 能跑通（C-12 KFD 模块已验证）
- [x] HMM range fault → migrate → 通知全链路通畅（mmu_notifier 框架已实施）
- [x] mmu_notifier 在用户态 munmap 时正确触发 invalidation（Stage 2 mm_shim wire-up）
- [x] 测试：`tests/test_fault_inject_standalone` + `tests/test_error_inject_standalone` + C-12 集成测试

---

## 子阶段 1.4 — 集成验证

**目标**: 编译真实 KFD（或 amdgpu 子集），跑通核心 ioctl

### 验证方法

1. **取 KFD 源码**：从 `linux/drivers/gpu/drm/amd/amdkfd/`（参考 Linux 6.6/6.12 LTS）
2. **零修改移植**：
   - 拷贝 `drivers/gpu/drm/amd/amdkfd/*.c` 到 UsrLinuxEmu 的 `plugins/gpu_driver/drv/kfd/`
   - 仅调整 `#include` 路径（`<linux/...>` → `linux_compat/...`，`<drm/...>` → `linux_compat/drm/...`）
3. **编译验证**：
   - 编译成功（warnings 可接受，errors **必须为零**）
4. **运行验证**：跑通 5 个核心 ioctl

### 5 个核心 KFD ioctl

| # | ioctl | 用途 | 对应 UsrLinuxEmu System C 接口 |
|---|-------|------|-------------------------------|
| 1 | `AMDKFD_IOC_GET_PROCESS_APERTURE` | 查询进程地址空间信息 | 需新增 `GPU_IOCTL_GET_PROCESS_APERTURE`（KFD API 1:1 映射）|
| 2 | `AMDKFD_IOC_CREATE_QUEUE` | 创建 GPU 命令队列 | `GPU_IOCTL_CREATE_QUEUE`（已有，需扩展参数）|
| 3 | `AMDKFD_IOC_UPDATE_QUEUE` | 更新队列属性 | 需新增 `GPU_IOCTL_UPDATE_QUEUE` |
| 4 | `AMDKFD_IOC_MAP_MEMORY` | 映射系统内存到 GPU | 需新增 `GPU_IOCTL_MAP_MEMORY` |
| 5 | `AMDKFD_IOC_UNMAP_MEMORY` | 解除内存映射 | 需新增 `GPU_IOCTL_UNMAP_MEMORY` |

> **实施说明**：5 个核心 ioctl 涉及新增 IOCTL 编号（System C 编号空间扩展），需在 SSOT 附录 A 中同步更新。属于"驱动可移植性"需求驱动的 API 扩展，**不算独立的 ABI 变更**。

### 验收

- [x] 编译通过（errors = 0，warnings 数量记录在 C-12 测试报告；C-12 KFD 6 模块全部编译通过）
- [x] 5 个 ioctl 全部跑通（单测覆盖 happy path + 至少 1 个 error path；C-12 Phase A-E 全部完成）
- [x] 驱动代码零修改（除 `#include` 路径；per ADR-059 D4 scope boundary 字段白名单）
- [x] `docs/05-advanced/kfd-portability-report.md`（已创建）+ `docs/05-advanced/tier2-runtime-penetration-report.md`
- [x] 测试：C-12 集成测试 104/104 ctest PASS（含 L1↔L2 bridge E2E）

---

## 4. 涉及 ADR

| ADR | 角色 | 状态 |
|-----|------|------|
| [ADR-008](../00_adr/adr-008-linux-api-compat.md) | Linux API 兼容层基础 | ✅ Accepted |
| [ADR-019](../00_adr/adr-019-drm-gem-ttm-alignment.md) | DRM/GEM/TTM 对齐路径（1.2 子阶段）| ✅ Accepted |
| [ADR-027](../00_adr/adr-027-linux-compat-strategy.md) | Linux 兼容层扩展策略（spec-driven，按 Phase 3+ 真实需求增量）| ✅ Accepted（Phase 3 触发后细化）|
| [ADR-036](../00_adr/adr-036-three-way-separation.md) | 3 区分架构原则（阶段 1 工作的判定基准）| ✅ Accepted |
| [ADR-018](../00_adr/adr-018-driver-sim-separation.md) | 驱动/仿真分离（1.2 重构的物理基础）| ✅ Accepted |
| [ADR-020](../00_adr/adr-020-libgpu-core-extraction.md) | libgpu_core 算法核心（GEM 内存后端）| ✅ Accepted |
| [ADR-023](../00_adr/adr-023-hal-interface.md) | HAL 接口契约（新增 HAL ops 走此 ADR 流程）| ✅ Accepted |
| [ADR-024](../00_adr/adr-024-user-mode-queue-submission.md) | **UMQ（User Mode Queue）— 不在 1.3 UVM 范围** | ✅ Accepted |
| [ADR-035](../00_adr/adr-035-governance-policy.md) | 新增 HAL 接口 / 状态变更的治理规则 | ✅ Accepted |
| [ADR-037](../00_adr/adr-037-render-node-permissions.md) | VFS Device Permission Model (Render Node 权限分离) | ✅ Accepted |
| [ADR-059](../00_adr/adr-059-kfd-multi-file-integration.md) | **C-12 KFD 多文件集成架构边界**（1.4 后续子项目）| ✅ Accepted |
| [ADR-060](../00_adr/adr-060-message-notification-threading.md) | kernel_thread_base + kernel_workqueue（C-12 前置 gate）| ✅ Accepted |
| [ADR-061](../00_adr/adr-061-hal-iommu-extension.md) | HAL IOMMU ops 扩展（hal_iommu_map/unmap）| ✅ Accepted |
| [ADR-062](../00_adr/adr-062-hal-event-signal-extension.md) | HAL Event Signal ops 扩展（hal_event_signal）| ✅ Accepted |
| [ADR-063](../00_adr/adr-063-sim-pfh-pm-realification.md) | sim_pfh / sim_pm 真实化状态机边界 | ✅ Accepted |

---

## 5. 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| KFD 代码量大（~50K 行）| 高 | 高 | 分阶段实施，1.2 + 1.3 先做小集实验证（先移植 ~5K 行核心文件）|
| 内核 API 覆盖不全 | 高 | 中 | 1.4 集成时按需补齐（迭代式，按 [ADR-027](../00_adr/adr-027-linux-compat-strategy.md) 决策 1 的"spec-driven"原则）|
| 真机部署兼容 | 中 | 中 | HAL 是桥（[ADR-036](../00_adr/adr-036-three-way-separation.md)），`hal_user.cpp` 持续维护 |
| 文档审计基线漂移 | 低 | 低 | 每次 commit 前跑 `tools/docs-audit.sh`（pre-commit hook 自动）|
| IOMMU 子系统理解偏差 | 中 | 中 | 参考 Linux 6.6/6.12 LTS 头文件，按 [ADR-027](../00_adr/adr-027-linux-compat-strategy.md) 决策 3 不承诺 ABI 一致，只对齐 API 签名 |
| mmu_notifier 路径复杂 | 中 | 高 | 1.4 集成前先在 1.3 内部做小范围 PoC（userfaultfd + mmap 共享触发场景）|

---

## 6. 子阶段依赖关系

```
1.0 PCIe ──> 1.1 IOMMU+ATS ──> 1.2 DRM ──> 1.3 UVM/HMM ──> 1.4 集成验证
                │                  │            │
                └──> 1.2 (HAL) ────┘            │
                                                 │
                              1.4 集成 反馈 1.0/1.1/1.2/1.3
```

- **1.0 → 1.1**：PCIe 设备枚举后，才能确定 IOMMU group 拓扑
- **1.1 → 1.2**：IOMMU 框架是 DRM GEM prime 路径的基础
- **1.2 → 1.3**：DRM drm_device 是 uvm module 的父设备
- **1.4 反馈循环**：集成验证发现缺口时，按依赖关系反向回流修补

---

## 7. 状态证据（已完成部分）

阶段 1 依赖的若干前置能力已由 Phase 1.5 / Phase 2 完成，作为本阶段工作起点：

| 前置能力 | 来源 | 状态 |
|----------|------|------|
| `drv/hal/sim/shared` 物理目录分离 | [ADR-018](../00_adr/adr-018-driver-sim-separation.md) | ✅ |
| HAL 接口契约（15 个函数指针）| [ADR-023](../00_adr/adr-023-hal-interface.md) + [ADR-061](../00_adr/adr-061-hal-iommu-extension.md) + [ADR-062](../00_adr/adr-062-hal-event-signal-extension.md) + `mem_map_bo` per [ADR-064](../00_adr/adr-064-memory-model-staging.md) | ✅ |
| `libgpu_core/` 纯 C buddy allocator | [ADR-020](../00_adr/adr-020-libgpu-core-extraction.md) | ✅ |
| Hardware Puller FSM | [ADR-021](../00_adr/adr-021-hardware-puller.md) | ✅ |
| UMQ 用户态提交路径 | [ADR-024](../00_adr/adr-024-user-mode-queue-submission.md) | ✅ |
| VA Space 抽象 | [ADR-017](../00_adr/adr-017-gpfifo-queue-abstraction.md) + [ADR-024](../00_adr/adr-024-user-mode-queue-submission.md) | ✅ |
| TTM 迁移优先级 | [ADR-031](../00_adr/adr-031-ttm-migration-priority.md) | ✅ |
| DRM ioctl 表驱动 | [ADR-019](../00_adr/adr-019-drm-gem-ttm-alignment.md) 决策 2 | ✅（基础）|
| **C-12 KFD 多文件集成** | **[ADR-059](../00_adr/adr-059-kfd-multi-file-integration.md)** | ✅ |
| └─ KFD 6 模块（kfd_module/process/pasid/dispatch/mmu/events）| C-12 Phase B | ✅（81% 原子任务完成）|
| └─ sim_pfh / sim_pm 真实化 + IOTLB + mm_shim | [ADR-063](../00_adr/adr-063-sim-pfh-pm-realification.md) + C-12 Phase C | ✅ |
| └─ kernel_thread_base + kernel_workqueue | [ADR-060](../00_adr/adr-060-message-notification-threading.md) | ✅ |
| └─ L1↔L2 bridge E2E | C-12 Phase E | ✅（双仓归档）|
| 集成测试基线 | C-12 + Stage 2 | 104/104 ctest PASS |

---

## 8. 下一步

阶段 1 完成后，UsrLinuxEmu 已具备编译与运行真实 KFD 内核驱动子集的能力。
[阶段 2](stage-2-multi-device.md)（多设备插件化，✅ 已达成 2026-07-05）在此基础上扩展了网络与存储设备。
[阶段 3](stage-3-v1.0.md)（v1.0 稳定，🔄 进行中）正在进行 CUDA E2E、sanitizer、bridge、perf 等稳定性收尾工作。

---

## 9. 变更记录

| 日期 | 版本 | 变更 |
|------|------|------|
| 2026-06-24 | v1.0 | 初版：5 子阶段 + 3 区分原则 + KFD 验证 |
| 2026-06-24 | v1.0 (corrected) | 纠正原始计划中 UMQ 误归到 1.3 UVM/HMM 的错误；UMQ 显式归 ADR-024 |
| 2026-07-21 | v1.1 | 路线图同步：更新完成日期至 2026-07-16；勾选全部子阶段 checkbox；补充 C-12 KFD 多文件集成交付摘要；HAL ops 11→14；更新 §4 ADR 表 + §7 状态证据表 |

---

**维护者**: UsrLinuxEmu Architecture Team
**最后更新**: 2026-06-24
