## Why

阶段 1.2 子阶段需要补齐 DRM 子集，使 KFD 驱动代码能通过 `drm_ioctl` / `drm_gem` / `drm_prime` 标准接口调用，并把 UsrLinuxEmu 现有自创 GpgpuDevice 重构为 drm_device 风格。此 change 强依赖子阶段 1.1 的 IOMMU group 拓扑（用于 DMA remap + prime attach）——属于路线图 §6 强制串行位置（1.1 → 1.2）。

阶段 1（路线图 §1.2 验收第 1 条）的**最高门槛**是：真实 KFD 驱动 `.c` 文件拷贝到 UsrLinuxEmu 后，**逻辑零修改**（仅 `#include` 路径调整）即可编译。本 change 是达到该门槛的关键。

## What Changes

- **新增 capability `drm-subset`**：定义 UsrLinuxEmu 的 DRM/GEM 最小可用集（Linux 6.12 LTS 对齐 API 签名 + KFD 实际使用子集）
- **新增** `src/kernel/drm/` 框架：`drm_gem.cpp` / `drm_file.cpp` / `drm_prime.cpp` / `render_node.cpp`
- **补齐** `include/linux_compat/drm/`：在已有 `drm_driver.h` / `drm_gem.h` / `drm_ioctl.h` 基础上新增 `drm_prime.h` / `drm_file_operations.h` / `drm_mode_config.h`（基础结构占位）
- **重构** `plugins/gpu_driver/drv/gpgpu_device` 嵌入 `struct drm_device`（**保留** `FileOperations` 入口，按 Oracle D1）
- **扩展** `drm_ioctl_desc[]` 表从当前 7 个 IOCTL 扩展到 ≥15 个（覆盖 Stage 1.4 的 5 个 KFD ioctl + 现有系统 C 接口）
- **新增** render node `/dev/dri/renderD128` + primary node `/dev/dri/card0`（支持 KFD prime path）
- **新增** ADR-037：VFS Device 权限模型（render node 权限分离）
- **条件性 HAL 扩展**：`hal_drm_*` ops——**仅当 KFD 实际调用时**按 ADR-035 Rule 3 走 ADR 流程；本次实现**不预先**添加

> **本 change 不包含 UVM/HMM 完整实现**：mmu_notifier 完整集成留到 1.3；本 change 仅在 `drm_prime.h` 锁定 KFD prime 接口签名（基于 librarian 关键修正：`dma_buf_dynamic_attach/detach/map_attachment/unmap_attachment/pin/unpin` + `struct dma_buf_attach_ops`，**不**为 `dma_buf_attach`）。
> 
> **本 change 不预先实现 1.3 hmm_range / migrate**：仅在 `struct drm_device` 生命周期契约中保留 BO 释放顺序约束（G1-G4 边界契约），避免过早耦合。

## Capabilities

### New Capabilities

- `drm-subset`: DRM 子集模拟框架。提供 `drm_device`（设备上下文嵌入）、`drm_file`（文件描述符抽象）、`drm_gem`（buffer object 生命周期管理 init/handle_create/refcount/release）、`drm_prime`（跨设备 buffer 共享 —— `dma_buf_dynamic_attach/detach/map_attachment/unmap_attachment/pin/unpin`）、`render_node`（`/dev/dri/renderD128` 权限分离）；KFD 5 个 ioctl 编号已在 `gpu_ioctl.h` 预留（`0x44-0x47`）。

### Modified Capabilities

None — UsrLinuxEmu 阶段 1.2 子阶段在已有 `drm_driver/drm_gem/drm_ioctl` 头基础上扩展，不修改既有 capability。

## Impact

- **Code 规模**：新增 ~1500 行 C++ 实现 + ~600 行头文件 + ~600 行测试
  - `src/kernel/drm/` 新建目录（含 5 个 .cpp + 1 个公共头）
  - `include/linux_compat/drm/` 已有，扩展新增 3 个头文件
  - `plugins/gpu_driver/drv/` 修改（`gpu_drm_driver.cpp` 从 288 行扩展到 ≥15 IOCTL；`gpgpu_device.cpp/h` 嵌入 `struct drm_device`）
- **依赖关系**：
  - **上游前置**：[stage-1.1 IOMMU + ATS](../archive/2026-07-02-stage-1-1-iommu-ats/)（已归档）—— prime import path `dma_buf_dynamic_attach` 需要 IOMMU group isolation
  - **下游阻塞**：[stage-1.3 UVM/HMM](../docs/roadmap/stage-1-kernel-emu.md) —— uvm module 是 `drm_device` 的子模块；SVM ioctl 依赖 GEM object 释放顺序契约
- **系统 C ioctl 编号变更**：`gpu_ioctl.h` 新增 4 个 IOCTL（`0x44-0x47`）+ CREATE_QUEUE (0x40) 字段扩展（**已由选项 B 完成，2026-07-02**）。`scripts/check_gpu_ioctl_sync.sh` 验证 UsrLinuxEmu ↔ TaskRunner 双端 15 IOCTL 同步
- **HAL 接口契约**：**不预先**新增 `hal_drm_*` ops（按 ADR-023 + ADR-035，仅当 1.4 集成 KFD 时按需走 ADR 流程）。
- **ADR-035 合规**：新增 ADR-037（VFS Device 权限模型），已走 ADR 流程。
- **构建/测试**：
  - `src/CMakeLists.txt` 添加 `src/kernel/drm/*.cpp` 到 kernel SHARED 库（保持 kernel SHARED，遵守 Issue #11）
  - `plugins/gpu_driver/CMakeLists.txt` 添加 `plugins/gpu_driver/drv/kfd/` 子目录
  - `tests/CMakeLists.txt` 注册 4 个新测试
- **文档**：
  - `docs/05-advanced/drm-compat-matrix.md`（**新增**，Linux 6.6 ↔ 6.12 兼容矩阵，按盲点 5）
  - `docs/02_architecture/post-refactor-architecture.md §1.10` 标注 1.2 完成
  - `docs/00_adr/adr-037-render-node-permissions.md`（**已创建**，2026-07-02）
- **风险**（继承自路线图 §5 + Oracle 评估 + 5 盲点）：
  - **概率中/影响高**：mmu_notifier + drm_device 生命周期耦合 → 缓解：1.2/1.3 边界契约 G1-G4（`test_uvm_drm_lifecycle_standalone` 1.2 骨架 + 1.3 完整化）
  - **概率高/影响中**：KFD 代码量大（~50K 行）→ 缓解：分阶段实施，先用 `kfd_queue.c` PoC 验证框架（**已由选项 C 完成，2026-07-02，errors=0, warnings=2**）
  - **概率低/影响高**：migrate 接口签名 6.6 ↔ 6.12 断崖 → 缓解：锁目标 LTS = 6.12，1.4 集成时再补 6.6 兼容矩阵（盲点 5 决策）

## Launch Conditions（Oracle 2026-07-02 评估）

本 change 进入正式实施前必须满足 4 条启动条件：

- **C1**：`tests/test_iommu_emu_standalone` 100% 通过 + TaskRunner `tests/test_kfd_integration` 通过
- **C2**：`openspec/changes/stage-1-2-drm-subset/.openspec.yaml` 标注 `ABI变更: 否` + `编号静态化: 是`，`scripts/check_gpu_ioctl_sync.sh` 通过
- **C3**：amdkfd 单文件 PoC 在 Linux 6.12 LTS 编译通过（warning ≤ 3）—— **2026-07-02 已达成**（evidence 在 `openspec/evidence/amdkfd-poc-2026-07-02/`）
- **C4**：新增 HAL ops ≤ 2 个，每 op 含 call trace + compile log 证明（存入 `specs/hal-drm-ops-audit.md`）