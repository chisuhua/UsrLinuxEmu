## Why

阶段 1.1 子阶段需要在 UsrLinuxEmu 中补齐 IOMMU + ATS 模拟框架，使 KFD 驱动代码能通过标准 Linux `iommu_domain` / `iommu_group` 接口完成 DMA remapping 与 ATS 协议通信。此 change 强依赖子阶段 1.0 的 PCIe 设备枚举（用于确定 IOMMU group 拓扑），并被子阶段 1.2 的 DRM GEM prime 路径所依赖——属于路线图 §6 强制串行位置（1.0 → 1.1 → 1.2）。

## What Changes

- **新增 capability `iommu-ats`**：定义 IOMMU + ATS 在 UsrLinuxEmu 中的最小可用集（vtd + amd-iommu 行为子集）
- **新增** `src/kernel/iommu/` 框架（`iommu_domain` / `iommu_group` / `ioasid` / `dma_remap` / `ats_protocol` / `invalidate` 六个模块）
- **新增** `include/linux_compat/iommu/` 子集，按 ADR-027 spec-driven 增量原则
- **条件性 HAL 扩展**：`hal_iommu_*` ops——**仅当 KFD 实际调用时**按 ADR-035 Rule 3 走 ADR 流程；本次实现**不预先**添加
- **错误码语义对齐**：DMA remapping 失败错误码（`EREMOTEIO` 等）与 Linux 内核一致（路线图 §1.1 验收第 3 条）

> **本 change 不包含 mmu_notifier 完整实现**：mmu_notifier 在 1.3 UVM/HMM 子阶段实现；1.1 仅预留 `mmu_notifier` 集成点（接口定义 + 桩注册），完整集成放到 1.3，避免提前耦合。同步点：`src/kernel/iommu/invalidate.cpp` 提供 `iommu_invalidate_register_notifier()` 桩，1.3 子阶段填充 body。

## Capabilities

### New Capabilities

- `iommu-ats`: IOMMU + ATS 模拟框架。提供 `iommu_domain`（域抽象）、`iommu_group`（设备分组拓扑）、`ioasid`（IO 地址空间 ID）数据结构；DMA remapping 页表最小子集（vtd 行为单级页表 + amd-iommu 一致性）；ATS 协议（device-TLB 请求/完成消息，translation completion 路径）；device-IOTLB invalidate 与 mmu_notifier 集成点。

### Modified Capabilities

None — UsrLinuxEmu 阶段 1.1 子阶段从零创建，无既有 capability 被修改。

## Impact

- **Code 规模**：新增 ~2500 行 C++ 实现 + ~800 行头文件 + ~600 行测试
  - `src/kernel/iommu/` 新建目录（含 6 个 .cpp + 1 个公共头）
  - `include/linux_compat/iommu/` 新建目录（含 `iommu.h` / `iommu_domain.h` / `iommu_api.h`）
  - `tests/` 下 3 个 Catch2 standalone 测试
- **依赖关系**：
  - **上游前置**：[stage-1.0 PCIe 设备模拟](../../docs/roadmap/stage-1-kernel-emu.md)（已归档）—— 需其 PCIe route ID 拓扑才能构造 IOMMU group
  - **下游阻塞**：[stage-1.2 DRM 子集](../../docs/roadmap/stage-1-kernel-emu.md)—— DRM GEM prime `dma_buf_map_attachment` 路径依赖 IOMMU group isolation
- **系统 C ioctl 编号**：**无变化**——本 change 仅在内核环境模拟层（①）添加能力，不触及 System C ioctl（System C 在 1.2 / 1.4 子阶段调整）
- **HAL 接口契约**：**不预先**新增 `hal_iommu_*` ops。规则：仅当子阶段 1.4 集成真实 KFD 源码时，若发现 KFD 调用 `iommu_map()` / `iommu_unmap()` 等需要 HAL 桥接的 API 才走 ADR-035 流程
- **构建/测试**：
  - `src/CMakeLists.txt` 添加 `src/kernel/iommu/*.cpp` 到 kernel SHARED 库
  - `tests/CMakeLists.txt` 注册 3 个新 test 目标
  - 构建系统、依赖关系无破坏性变更
- **文档**：
  - 新增 `docs/05-advanced/iommu-error-semantics.md`（错误码对照表）
  - `docs/02_architecture/post-refactor-architecture.md §1.10` 标注 1.1 完成
  - `docs/00_adr/` 不新增 ADR（本 change 不引入新架构决策）
- **风险**（继承自路线图 §5）：
  - **概率中/影响中**：IOMMU 子系统理解偏差 → 缓解：参考 Linux 6.6/6.12 LTS 头文件，按 ADR-027 决策 3 不承诺 ABI 一致，只对齐 API 签名
  - **概率中/影响高**：DMA remapping 错误码语义偏差 → 缓解：新增错误码语义对照表，1.4 集成时验证
