## Why

阶段 1.3 子阶段需要补齐 UVM (Unified Virtual Memory) + HMM (Heterogeneous Memory Management) 框架，使 KFD 真实驱动代码中的 SVM (Shared Virtual Memory) ioctl 路径可编译运行。本 change 强依赖子阶段 1.2 的 `struct drm_device` 完整化（uvm module 的父设备），是路线图 §1.3 → §1.4 串行链路上的关键节点（**风险最高**，Oracle 2026-07-02 评估）。

阶段 1（路线图 §1.3 验收）的核心门槛：KFD `kfd_process.c` + `kfd_svm.c` 等 SVM 相关源文件拷贝到 UsrLinuxEmu 后，**逻辑零修改**（仅 `#include` 路径调整）即可编译。1.2 已锁定 1.2/1.3 边界契约 G1-G4（design.md Decision 5），本 change 是在该契约上构建完整实现。

## What Changes

- **新增 capability `uvm-hmm`**：定义 UsrLinuxEmu 的 UVM/HMM 最小可用集（Linux 6.12 LTS 对齐 API 签名 + KFD 实际使用子集）
- **新增** `src/kernel/uvm/` 框架：`mmu_notifier` / `hmm_range` / `migrate` / `fault_inject` / `zone_device` / `page_state_machine`（5 个 .cpp + 1 个公共头）
- **新增** `include/linux_compat/{mmu_notifier,hmm}.h`：与 Linux 6.12 LTS ABI 对齐的 API 子集
  - `struct hmm_range` 完整字段（7 个：`notifier` / `notifier_seq` / `start` / `end` / `hmm_pfns` / `default_flags` / `pfn_flags_mask` / `dev_private_owner`）
  - `struct mmu_interval_notifier` + `mmu_interval_notifier_ops.invalidate` 回调（替代 Linux 6.x 已移除的 `struct hmm_mirror`）
  - 序列号协议：`mmu_interval_read_begin` / `mmu_interval_read_retry` / `mmu_interval_set_seq`
  - HMM PFN flags：`HMM_PFN_VALID` / `HMM_PFN_WRITE` / `HMM_PFN_ERROR` / `HMM_PFN_REQ_FAULT` / `HMM_PFN_REQ_WRITE`（64-bit 编码，`HMM_PFN_VALID = 1UL << 63`）
- **新增** `plugins/gpu_driver/sim/page_fault_handler.cpp` + `page_migration.cpp`：硬件模拟层 page fault 处理 + migrate 状态机
- **新增** `plugins/gpu_driver/uvm/svm_ioctl.cpp`（**仅当 KFD uvm 子模块需要时创建**，按路线图 §1.3 ②）
- **HAL 条件性扩展**：`hal_uvm_*` ops（mmap shared / fault 通知 / migrate 操作）—— **仅当 KFD 实际调用时**按 ADR-035 Rule 3 走 ADR 流程
- **承接 1.2/1.3 边界契约 G1-G4**：
  - G1: `drm_device` 生命周期 = `GpgpuDevice` 生命周期（已锁）
  - G2: `dma_buf_dynamic_attach` 等 API 签名（已锁）
  - G3: 4 项接口契约明确列出（已锁）
  - G4: 不预先实现 1.4 完整 migrate（仅留接口边界）

> **本 change 不包含 UMQ (User Mode Queue)**：UMQ 由 [ADR-024](../../docs/00_adr/adr-024-user-mode-queue-submission.md) 完整覆盖（用户态 Ring Buffer + mmap Doorbell 路径），与 UVM/HMM 在内核子系统层级上正交。
>
> **本 change 不包含完整 KFD 集成验证**：1.3 仅完成 UVM/HMM 框架 + PoC；1.4 集成验证才把 `kfd_process.c` / `kfd_svm.c` 等真实 KFD SVM 源文件拷贝进来编译运行。

## Capabilities

### New Capabilities

- `uvm-hmm`: UVM/HMM 模拟框架。提供 `mmu_notifier`（用户态 mmap 共享 → 内核 page invalidation 通知）、`hmm_range`（Heterogeneous Memory Management range fault 路径）、`migrate`（page migration between CPU/GPU domain）、`fault_inject`（user-space mmap 触发 page fault → mmu_notifier 通知 device driver）、`zone_device`（spm vma + page 状态机最简实现）、`page_state_machine`（`PAGE_STATE_CPU` / `PAGE_STATE_GPU` / `PAGE_STATE_MIGRATING` 三态机）。

### Modified Capabilities

None — UsrLinuxEmu 阶段 1.3 子阶段在 1.2 已锁定的 `drm-subset` capability 上扩展，不修改既有 capability 行为。

## Impact

- **Code 规模**：新增 ~2000 行 C++ 实现 + ~800 行头文件 + ~800 行测试
  - `src/kernel/uvm/` 新建目录（含 6 个 .cpp + 1 个公共头）
  - `include/linux_compat/{mmu_notifier,hmm}.h` 新建
  - `plugins/gpu_driver/sim/` 新增 2 个文件
  - `plugins/gpu_driver/uvm/` 新建目录（**仅当 KFD uvm 子模块需要**）
  - `plugins/gpu_driver/hal/` 条件性扩展 `hal_uvm_*` ops（按 ADR-035 流程）
- **依赖关系**：
  - **上游前置**：[stage-1.2 DRM 子集](../archive/2026-07-02-stage-1-2-drm-subset/)（已归档，2026-07-02）—— `drm_device` 嵌入 + G1-G4 边界契约
  - **下游阻塞**：[stage-1.4 KFD 集成验证](../docs/roadmap/stage-1-kernel-emu.md) —— `kfd_process.c` / `kfd_svm.c` 等 SVM 源文件拷贝编译
- **系统 C ioctl 编号变更**：无（1.2 阶段已预留 5 个 KFD ioctl 编号 0x44-0x47；1.3 阶段不新增 ioctl）
- **HAL 接口契约**：**不预先**新增 `hal_uvm_*` ops（按 ADR-035，仅当 1.4 集成 KFD 时按需走 ADR 流程；Oracle 评估盲点 1 严格约束）
- **ADR-035 合规**：所有 HAL / 状态变更走 change 流程；本次创建 `openspec/changes/stage-1-3-uvm-hmm/specs/hal-uvm-ops-audit.md` 记录 0 ops 决策
- **构建/测试**：
  - `src/CMakeLists.txt` 添加 `src/kernel/uvm/*.cpp` 到 kernel SHARED 库（保持 kernel SHARED，遵守 Issue #11）
  - `plugins/gpu_driver/sim/CMakeLists.txt` 添加 `page_fault_handler.cpp` + `page_migration.cpp`
  - `plugins/gpu_driver/CMakeLists.txt` 添加 `uvm/` 子目录（**仅当 KFD uvm 子模块需要**）
  - `tests/CMakeLists.txt` 注册 3 个新测试（mmu_notifier / hmm_range / svm_ioctl）
- **文档**：
  - `docs/05-advanced/uvm-error-semantics.md`（**新增**，errno 对照表，类比 1.1/1.2）
  - `docs/05-advanced/hmm-compat-matrix.md`（**新增**，Linux 6.6 ↔ 6.12 LTS HMM 差异矩阵，类比 1.2 drm-compat-matrix.md）
  - `docs/02_architecture/post-refactor-architecture.md §1.10` 标注 1.3 完成
  - 在 SSOT 附录 A 同步 `linux_compat/{mmu_notifier,hmm}.h` 头文件清单
- **风险**（继承自路线图 §5 + Oracle 评估 + 1.2/1.3 边界契约盲点 1）：
  - **概率高/影响高**：`mmu_notifier` + `drm_device` 生命周期耦合 → 缓解：1.2/1.3 边界契约 G1-G4 已锁（`tests/test_uvm_drm_lifecycle_standalone` 1.2 骨架 + 1.3 完整化）
  - **概率中/影响高**：`struct hmm_mirror` 在 Linux 6.x 已移除（用 `mmu_interval_notifier` 替代）→ 缓解：API 签名锁定 6.12 LTS + librarian 验证 amdkpu/amdgpu 实际调用
  - **概率中/影响中**：HMM range fault 性能与一致性 → 缓解：先 PoC（userfaultfd + mmap 共享触发场景），避免一上来铺全套（路线图 §5 缓解）
  - **概率低/影响高**：migrate 接口签名 6.6 ↔ 6.12 断崖 → 缓解：锁目标 LTS = 6.12，1.4 集成时再补 6.6 兼容矩阵

## Launch Conditions

本 change 进入正式实施前必须满足 3 条启动条件：

- **LC1**：1.2/1.3 边界契约 G1-G4 已锁 + `tests/test_uvm_drm_lifecycle_standalone` 1.2 骨架存在 —— **2026-07-02 已达成**（在 stage-1-2-drm-subset/tasks.md §5）
- **LC2**：内部 PoC 先完成（userfaultfd + mmap 共享触发场景，验证 ① 内核环境模拟层 + ③ 硬件模拟层 page fault 链路通畅）
- **LC3**：1.2 测试全绿（52/52）—— **2026-07-02 已达成**（`ctest --output-on-failure`）
