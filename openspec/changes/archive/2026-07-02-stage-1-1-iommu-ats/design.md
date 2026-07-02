## Context

### 背景

UsrLinuxEmu 现有架构（[post-refactor-architecture.md §1.10](../../docs/02_architecture/post-refactor-architecture.md)）在 `src/kernel/` 下提供 VFS、ModuleLoader、Logger、WaitQueue 等基础服务，在 `plugins/gpu_driver/` 下提供 GpgpuDevice 驱动的 ② / ③ 层实现，但**完全没有 IOMMU 模拟层**。

阶段 1.1 子阶段（[stage-1-kernel-emu.md §子阶段 1.1](../../docs/roadmap/stage-1-kernel-emu.md)）是阶段 1（Linux 内核环境模拟）中补齐 Linux 内核 API 子集的第二个子阶段，强依赖 1.0 的 PCIe 设备枚举（已归档）以确定 group 拓扑，并被子阶段 1.2 DRM GEM prime 路径所依赖。

### 当前状态

- **已存在**：
  - `include/linux_compat/drm/`（drm_driver / drm_gem / drm_ioctl 头）—— 1.2 子阶段基础
  - `src/kernel/pcie/`（pcie_emu / config_space / capability_walk / msi_x）—— 1.0 子阶段已归档
  - `include/linux_compat/pci/` —— 1.0 子阶段已有
  - `plugins/gpu_driver/drv/gpu_drm_driver.cpp`（288 行 DRY 风格 ioctl 表）—— 1.2 子阶段起点
- **完全不存在**：
  - `src/kernel/iommu/` 框架
  - `include/linux_compat/iommu/` 子集
  - `src/kernel/uvm/mmu_notifier.cpp`（1.3 才建；1.1 仅做集成点）

### 约束

- **ADR-027**：Linux 兼容层扩展走 spec-driven 增量，禁止凭想象预先添加
- **ADR-035**：HAL 接口扩展必须走 ADR 流程
- **ADR-036**：工作分层必须在 ① / ② / ③ 三区中明确归属
- **路线图 §1.1 验收**：
  - 模拟 IOMMU 能响应 device-IOTLB invalidate
  - ATS 请求能在 UsrLinuxEmu 内完整处理（含 translation completion 消息）
  - DMA remapping 失败的错误码语义与 Linux 内核一致
  - `tests/test_iommu_emu_standalone` 覆盖 group / ioasid / remap / ATS

## Goals / Non-Goals

### Goals

1. **支持 KFD 驱动的 IOMMU API 子集**：使 1.4 子阶段拷贝的 `drivers/gpu/drm/amd/amdkfd/*.c` 文件能通过 `iommu_domain_map()` 等标准接口完成 DMA remapping
2. **ATS 协议最小可用集**：device-TLB 请求/完成消息 + invalidation 协议 + 与 mmu_notifier 集成点
3. **错误码语义对齐 Linux 内核**：路由到 [-EREMOTEIO](file:///workspace/project/UsrLinuxEmu/include/linux_compat/errno.h) 等标准错误码
4. **PCIe 拓扑自动推导**：从子阶段 1.0 的 PCIe device 枚举（PCIe route ID）推导 iommu_group
5. **3 区分架构遵守**：本次工作全部在 ① 内核环境模拟层

### Non-Goals

1. **mmu_notifier 完整实现**：完整 `mmu_subsystem` 在 1.3 UVM/HMM 子阶段；1.1 仅预留集成点（`iommu_invalidate_register_notifier()` 桩注册）
2. **多级 IOMMU 页表**：仅实现单级 4KB 页表。真实 KFD 通常不需要多级；如需要走 1.3 + 1.4 集成按需扩展
3. **ATS cache hierarchy 深度**：仅支持 device-TLB ↔ IOMMU 双向通道；**不**模拟 page walk / ATS cache hint / PASID（PASID 是 PCIe 扩展，AMD KFD 用得上但 1.1 不强求）
4. **HAL `hal_iommu_*` ops**：本次**不**预先添加，按 ADR-027 + ADR-035 仅当 1.4 集成 KFD 时按需走 ADR 流程
5. **生产性能**：阶段 1.1 仅要求"最小可用集"，不要求接近真机性能

## Decisions

### Decision 1: 数据结构对齐 Linux 6.6/6.12 LTS

**选择**：`iommu_domain` / `iommu_group` / `ioasid` 数据结构对齐 Linux 6.6/6.12 LTS（最新 LTS）头文件 API 签名与字段语义，按 ADR-027 决策 3 **不承诺 ABI 一致**（例如字段顺序可调整）。

**理由**：
- 真实 KFD 驱动需要看到 `iommu_ops` 完整字段才能编译
- 用户态模拟与内核态实现的 ABI 不必完全相同，但**接口签名必须兼容**

**备选**：
- ❌ 自创一套简化数据结构：会使 KFD 拷贝路径大量 `#ifdef`，违背阶段 1 验收第 1 条"逻辑零修改"
- ✅ 对齐 LTS 头文件：增加 10-15% 实现体积但保持兼容

### Decision 2: 单级页表（4KB）

**选择**：DMA remapping 页表仅支持单级 4KB；不预先实现两级/多级。

**理由**：
- 阶段 1.1 最小可用集；KFD 通常不需要 >4KB 大页 DMA remapping（VRAM 大块走 GEM import 而非 IOMMU map）
- 单级页表实现复杂度约为多级的 1/3，加快 1.1 推进
- 1.3 UVM/HMM 涉及 HMM range fault，可能触发多级需求——届时按需扩展

**备选**：
- 多级页表：复杂度高，开发周期 +1-2 周，对 1.4 验收贡献有限

### Decision 3: ATS 协议最小子集

**支持**：
- Translation Request（device → IOMMU）
- Translation Completion（IOMMU → device）
- Invalidation Request（device → IOMMU）
- Invalidation Completion（IOMMU → device）

**不支持**（明确列出）：
- Page Walk 协议
- ATS cache hint
- PASID（Process Address Space ID，PCIe 扩展）
- ATS Invalidate Page Request for selective page

**理由**：4 个核心消息覆盖 KFD 的需求；其余通过 1.4 集成时按需添加（ADR-027 spec-driven）

### Decision 4: mmu_notifier 集成点（桩而非实现）

**选择**：1.1 子阶段在 `src/kernel/iommu/invalidate.cpp` 提供 `iommu_invalidate_register_notifier()` 桩函数：
- **签名与 Linux 内核一致**
- **body 为空**（仅 logging）
- **TODO 注释明确指向 1.3 子阶段**

**理由**：
- 避免 1.1 与 1.3 工作耦合（mmu_notifier 完整逻辑属于 1.3）
- 1.1 路线图验收第 1 条"模拟 IOMMU 能响应 device-IOTLB invalidate"可通过桩实现（响应 = 调用桩 + logging）
- 1.3 子阶段可平滑填充桩 body

**备选**：
- ❌ 在 1.1 完整实现 mmu_notifier：会导致 1.1 工作量增加 50%，并与 1.3 任务重叠
- ❌ 不留桩：会使 1.3 不得不修改 1.1 完成的代码（破坏串行）

### Decision 5: PCIe device → iommu_group 拓扑策略

**选择**：每个 PCIe device 自动映射到独立的 iommu_group（一对一隔离）。

**理由**：
- 单一 GPU 设备的场景下，1 device = 1 group 简化拓扑管理
- 真实 KFD 场景下，单 GPU 不涉及多 device 共享 DMA 域，不需要 group 含多 device
- 与子阶段 1.0 的 PCIe 枚举一一对应，避免拓扑推导逻辑

**备选**：
- 多 device 共享一个 group：会增加 group member 管理逻辑，1.1 不必要
- 多 device 共享一个 domain 但多个 group：复杂度高，1.4 验收前不必

### Decision 6: ATS 请求处理策略

**选择**：in-process 内存模拟（不依赖网络 / 物理硬件），通过函数调用模拟 device-TLB ↔ IOMMU 通信。

**理由**：
- UsrLinuxEmu 是用户态环境，所有"设备 ↔ IOMMU"通信都在同一进程内
- 函数调用比真 PCIe 请求完成延迟低 3-4 个数量级，便于测试

### Decision 7: 错误码语义对照表

**选择**：新增 `docs/05-advanced/iommu-error-semantics.md`（路线图 §1.1 验收第 3 条专门要求）。

**内容**：
- DMA remap 失败 → `-EREMOTEIO`（Linux 内核一致）
- ATS 请求格式错误 → `-EINVAL`
- IOTLB invalidate 超时 → `-ETIMEDOUT`
- ioasid 分配失败 → `-ENOMEM`

### Decision 8: 测试架构

**选择**：3 个 Catch2 standalone 测试（[路线图 §1.1 验收第 4 条](../../docs/roadmap/stage-1-kernel-emu.md)）。

| 测试 | 范围 |
|------|------|
| `test_iommu_emu_standalone` | group 创建 / ioasid 分配 / basic map/unmap |
| `test_dma_remap_standalone` | DMA remapping 页表正确性 + 错误码语义 |
| `test_ats_protocol_standalone` | Translation Request → Completion 全链路 |

**理由**：3 个测试覆盖 3 个核心验收点（[路线图 §1.1 验收](../../docs/roadmap/stage-1-kernel-emu.md) 1-3 条），与第 4 条 test_iommu_emu_standalone 一致。

## Risks / Trade-offs

### Risk 1: IOMMU 子系统理解偏差

- **概率**：中
- **影响**：中
- **缓解**：
  - 参考 Linux 6.6/6.12 LTS 头文件作为唯一事实来源
  - 按 ADR-027 决策 3 不承诺 ABI 一致，只对齐 API 签名 → 减少实现偏差
  - 1.4 集成真实 KFD 时代码会自动暴露理解偏差

### Risk 2: DMA remap 错误码与 Linux 不一致

- **概率**：中
- **影响**：高（路线图 §5 标注）
- **缓解**：
  - 新增 `docs/05-advanced/iommu-error-semantics.md` 错误码对照表
  - 1.4 集成时 KFD 调用 `iommu_map()` 返回值会被验证
  - 与 Linux kernel 头文件 `errno-base.h` / `errno.h` 对齐

### Risk 3: 与子阶段 1.0 的 PCIe 拓扑对接失败

- **概率**：低
- **影响**：高（破坏串行依赖）
- **缓解**：
  - 1.0 子阶段归档前已确认 PCIe device enum 暴露 route ID（`pcie_device.h` 提供）
  - 1.1 顶部加集成测试 `test_pcie_to_iommu_topology_standalone` 验证对接

### Risk 4: ATS 协议实现过于简化，KFD 无法使用

- **概率**：中
- **影响**：中
- **缓解**：
  - Decision 3 明确列出 1.1 不实现的 4 类 ATS 协议
  - 1.4 集成时按 KFD 实际调用按需添加（ADR-027 spec-driven）
  - 测试覆盖 KFD 实际使用的 4 个核心消息

### Risk 5: mmu_notifier 桩实现误导后续开发者

- **概率**：低
- **影响**：中
- **缓解**：
  - 桩函数显式 `// TODO(stage-1.3): implement mmu_notifier callback` 注释
  - `docs/superpowers/plans/2026-07-02-stage-1-kernel-emu-tracking.md §Sub-stage 1.3` 列出填充任务

## Migration Plan

### Rollout

1. **Phase A**（代码提交）：新增 src/kernel/iommu/ + include/linux_compat/iommu/ + 3 测试，**不开新 ABI**
2. **Phase B**（CI 验证）：所有现有 30+ 测试 + 3 个新测试全绿
3. **Phase C**（触发下一子阶段）：归档本 change 后启动 `stage-1-2-drm-subset`

### Rollback

由于 1.1 不修改 System C ioctl 编号、不引入 HAL ops、不修改既有 capability，回滚策略为：

```bash
git revert <stage-1.1 commit>
```

回滚后 UsrLinuxEmu 退回子阶段 1.0 状态，无任何兼容性副作用。

## Open Questions

1. **Q：1.1 是否需要支持 AMD `amd_iommu` 与 Intel `vtd` 的差异？**
   A：v0 用 vtd 单实现 + 接口预留 `domain->ops` 抽象层；后续按 1.4 集成反馈按需添加 amd_iommu 分支（ADR-027 决策 1 增量）

2. **Q：ATS protocol 测试是否需要 fuzz？**
   A：本子阶段不需要——3 个 unit test 足以覆盖 4 个核心消息；fuzz 留给 1.4 / 阶段 3 性能优化阶段

3. **Q：是否在 1.1 阶段就预留 PCIe SR-IOV 给 iommu_group？**
   A：**不预留**。SR-IOV 在 1.x 路线图中无对应子阶段，留给未来 ADR-026 等新决策评估
