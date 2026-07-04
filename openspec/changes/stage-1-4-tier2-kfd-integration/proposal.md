## Why

Stage 1.4 KFD portability 在 Tier-1 边界内已交付（[kfd-portability-boundary.md v1.0](../../docs/05-advanced/kfd-portability-boundary.md)，commit `80f6a44` Merge）：5 个 ioctl 编号预留、19 个 ioctl 派发表、kfd_queue.c 单文件 PoC 编译通过、3 个设备节点创建、10 个 sim C 接口暴露。但 Tier-1 PoC 同时**诚实验证**了多条边界超界（Tier-2），用户实际使用 KFD API 时会立刻遇到：

1. **9 个 STUB_HANDLER 仅 return 0**：`register_mmu_cb` / `register_firmware_cb` / `create_va_space` / `destroy_va_space` / `register_gpu` / `create_queue` / `destroy_queue` / `map_queue_ring` / `query_queue` —— KFD 真实驱动调用这些 ioctl 后**没有运行时行为**（boundary §3.1）。
2. **mmu_notifier callback body 是 TODO**：`src/kernel/iommu/invalidate.cpp:27` 标注 `TODO(stage-1.3): implement mmu_notifier callback body`，用户态 munmap 后**无实际 invalidation**（boundary §3.3）。
3. **IOMMU IOTLB flush 是 fprintf stub**：`src/kernel/iommu/dma_remap.cpp:145` 标注 logging stub，KFD 调 `iommu_unmap()` 后**不会真触发硬件 invalidation**（boundary §3.2）。

**现状**：Stage 1.0-1.3 + 1.4 Tier-1 已全部交付（SSOT §1.10 全 `[x]` + ctest 70+/70+ PASS）。Tier-2 项阻塞 KFD 真实驱动的**运行时使用**，必须作为 follow-up change 显式推进，否则 Stage 1 价值仅限于"编译通过 + handler dispatch 正确"（Tier-1），未达"运行时真实行为"（Tier-2）。

## What Changes

- **新增 capability `kfd-tier2-runtime-penetration`**：把 9 个 STUB_HANDLER + mmu_notifier callback body + IOMMU IOTLB flush 升级为真实运行时行为（基于现有 sim 原语，**不引入 amdgpu driver 移植**）
- **承接 1.4 Tier-1 边界契约**：所有 Tier-2 工作在 Tier-1 已锁定的 10 个 sim C 接口 + 5 个 IOCTL 编号基础上扩展（**不重新设计接口**，仅替换 stub 实现）
- **新增 capability `mmu-notifier-callback-body`**：完整化 `mmu_notifier` callback 调用链（用户态 munmap → kernel invalidation → mmu_notifier callback → sim 原语）
- **新增 capability `iommu-iotlb-flush-real`**：把 IOTLB flush 从 fprintf stub 升级为真实 page table invalidation（**仅在用户态模拟范围内**，不依赖 host kernel）
- **承接 G1-G4 边界契约**：1.2/1.3 已锁定的 4 项接口契约（`drm_device` 生命周期 / BO 引用计数 / prime 释放顺序 / fence 触发时机）在 Tier-2 实施时**不破坏**
- **承接 1.4 决策 2 严格性**：任何 KFD 实际调用的 HAL op 走 ADR 流程，**不预先添加**
- **承接诚实优先**：handler 注释中 "deferred to Stage 1.4" / "stub" 标记在 Tier-2 完成后**显式替换为** "Tier-2 penetrated" 或 "Tier-2 verified"

## Capabilities

### New Capabilities

- `kfd-tier2-runtime-penetration`: 9 个 STUB_HANDLER 的运行时穿透。定义每个 STUB 的最小可行实现（基于现有 sim 原语）、验收准则（runtime test 覆盖 happy path + 1 个 error path）、与 Tier-1 边界契约 G1-G4 的兼容性。
- `mmu-notifier-callback-body`: 完整化 mmu_notifier callback 链路。定义从用户态 munmap 到 sim 原语 `sim_pfh_inject_fault` / `sim_pm_migrate_to_system` 的完整调用图，覆盖 fault 注入、invalid 请求、page table invalidation。
- `iommu-iotlb-flush-real`: 把 `iommu_flush_iotlb` 从 logging stub 升级为真实 page table invalidation。**仅在 UsrLinuxEmu 用户态范围内**，通过 `iommu_domain->ops->flush_iotlb` 调用链 + 用户态 page table invalidation 实现，**不依赖 host kernel**。

### Modified Capabilities

- `kfd-portability`: 1.4 Tier-1 capability 增加 Tier-2 runtime penetration 验收。新增 Requirement："9 个 STUB_HANDLER 在 Tier-2 完成后 MUST 真正修改 sim 状态" + Scenario 覆盖每个 handler 的 happy path + 至少 1 个 error path。
- `drm-subset`: 1.2 capability 增加 Tier-2 边界约束。新增 Requirement："STUB_HANDLER 仅在 Tier-2 完成时才能升级为 real handler，**不预先实现**"（守恒现有 G1-G4 契约）。

## Impact

- **Code 规模**：
  - `plugins/gpu_driver/drv/gpu_drm_driver.cpp`：9 个 STUB_HANDLER 替换为 real handler（基于现有 sim 原语，预计 +200 行）
  - `src/kernel/iommu/invalidate.cpp`：mmu_notifier callback body 完整化（预计 +80 行）
  - `src/kernel/iommu/dma_remap.cpp`：`iommu_flush_iotlb` 升级（预计 +50 行）
  - 新增 `tests/test_*_runtime_standalone.cpp`（5+ 个，覆盖 9 个 STUB_HANDLER + mmu_notifier callback + IOTLB flush）
  - 新增 `docs/05-advanced/tier2-runtime-penetration-report.md`（诚实记录每个 handler 的 Tier-1 → Tier-2 演进）
- **依赖关系**：
  - **上游前置**：[stage-1.4 Tier-1 portability (archived)](../archive/2026-07-04-stage-1-4-kfd-portability/) + [kfd-portability-boundary.md](../../docs/05-advanced/kfd-portability-boundary.md) Tier-1/Tier-2 划分 SSOT
  - **下游阻塞**：Stage 2（多设备插件化）—— Stage 2 启动时 KFD Tier-2 runtime 是已具备能力之一
- **System C ioctl 编号变更**：**无**（Tier-1 已预留的 5 个 IOCTL 编号 + 19 个 ioctl 派发表保持不变）
- **HAL 接口契约变更**：**不预先**新增 `hal_iommu_*` / `hal_uvm_*` ops（按 ADR-027 spec-driven + ADR-035 治理；用户决策 2：**仅当 KFD 集成实际需要时走独立 ADR 流程**）
- **ADR-035 合规**：所有 HAL / 状态变更走 change 流程；本次 Tier-2 工作的 mmu_notifier callback body 与 IOTLB flush 不涉及新 HAL op（基于现有 sim 原语）
- **构建/测试**：
  - `plugins/gpu_driver/CMakeLists.txt`：9 个 STUB_HANDLER 替换为 real handler 后无需新增 target（仍在 `gpu_drm_driver` 静态库内）
  - `tests/CMakeLists.txt`：注册 5+ 个新 runtime test 目标
  - `src/CMakeLists.txt`：kernel 库增量更新（mmu_notifier + iommu callback）
- **文档**：
  - `docs/05-advanced/tier2-runtime-penetration-report.md`（**新增**，每个 handler 演进记录）
  - `docs/05-advanced/kfd-portability-boundary.md`（**修订**：Tier-2 状态从 "Stub / Logging / TODO" 改为 "Penetrated / Implemented"，附 Tier-2 完成时间戳）
  - `docs/02_architecture/post-refactor-architecture.md §1.10`（标注 Tier-2 完成）
  - `docs/roadmap/stage-1-kernel-emu.md` 顶部状态保留 `🔄 计划中`（Stage 1 整体未达 `✅`，因 Tier-2-D 多文件 KFD 仍延后）
- **风险**（继承自 boundary §3 + 路线图 §5）：
  - **概率中/影响中**：9 个 STUB_HANDLER 升级可能暴露 Tier-1 隐藏的设计假设 → 缓解：每个 handler 升级前先写 runtime test（红）→ 改 handler（绿）→ commit
  - **概率中/影响中**：mmu_notifier callback 与现有 G1-G4 边界契约冲突 → 缓解：每个 callback 升级前跑 `tests/test_uvm_drm_lifecycle_standalone` G1-G4 全验证
  - **概率低/影响高**：IOTLB flush 真实实现触发未预期的 IOMMU 域边界 → 缓解：先在 `tests/test_iommu_invalidate_runtime_standalone` 单测验证，**不立即进入集成测试**

## Launch Conditions

本 change 进入正式实施前必须满足 4 条启动条件：

- **LC1**：1.4 Tier-1 KFD portability 已 merge（`80f6a44`）—— **2026-07-04 已达成**
- **LC2**：`docs/05-advanced/kfd-portability-boundary.md` Tier-1/Tier-2 划分 SSOT 已发布 —— **2026-07-04 已达成**（v1.0）
- **LC3**：Tier-1 回归测试无 regression（LC3 决策 3，8/8 PASS：`test_drm_kfd_handlers_standalone` + `test_uvm_drm_lifecycle_standalone` G1-G4 + 5 个 stage-1 核心测试）—— **2026-07-04 已验证**
- **LC4**：worktree 创建完成（用户决策 1，**实施 Tier-2 代码时创建**，不在 change 启动阶段）

> LC4 故意延后到实施阶段创建：本 change 启动阶段（写 proposal / design / tasks / specs + 验证 OpenSpec）可在 main 上完成；实际代码实施在独立 worktree（`stage-1.4-tier2-kfd-integration`）进行，遵守 using-git-worktrees skill 约定。

## Out of Scope（显式排除）

按 [kfd-portability-boundary.md §5.2](../../docs/05-advanced/kfd-portability-boundary.md) Tier-2 显式排除项：

| 排除项 | 原因 | 推荐延后阶段 |
|--------|------|-------------|
| **多文件 KFD 集成**（kfd_module.c / kfd_device.c / kfd_process.c / kfd_doorbell.c） | 53+ amdgpu headers 阻塞，~50K 行 amdgpu driver 需连同移植（boundary §3.4） | Stage 3+ 或独立子项目 |
| **完整 kfd_queue.c queue 生命周期**（queue_create / destroy / mqd / doorbell） | 上游原文件后段函数需 amdgpu_* 依赖（boundary §3.5） | 随多文件集成延后 |
| **IOMMU 真实硬件 invalidation**（需 host kernel 介入，如 vfio） | UsrLinuxEmu 用户态无法触发真硬件 invalidation（boundary §3.2） | Stage 2 |
| **ATS PRI/PRG response routing** | 需 PCIe 4.0+ 设备；当前模拟设备不依赖 ATS（boundary §3.2） | Stage 2+ 条件性 |
| **mmu_notifier 真实进程模型**（需真实 mm struct） | 当前 SimPageFaultHandler 是匿名 namespace struct；1.3 仅填充 sim 原语骨架 | Stage 2 |