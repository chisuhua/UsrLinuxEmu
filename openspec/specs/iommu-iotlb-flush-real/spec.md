# iommu-iotlb-flush-real Specification

## Purpose
TBD - created by archiving change stage-1-4-tier2-kfd-integration. Update Purpose after archive.
## Requirements
### Requirement: iommu_flush_iotlb 升级为真实 page table invalidation

The system SHALL 升级 `iommu_flush_iotlb` 从 fprintf logging stub 为真实 page table invalidation（**仅在 UsrLinuxEmu 用户态范围内**，不依赖 host kernel），通过 `iommu_domain->ops->flush_iotlb` 调用链 + 用户态 page table invalidation + sim 原语触发实现。

#### Scenario: iommu_unmap 触发 IOTLB flush

- **WHEN** KFD 驱动调用 `iommu_unmap(domain, iova, size)` 后
- **THEN** system MUST 调 `iommu_domain->ops->flush_iotlb(domain, iova, size, IOMMU_CACHE_INVALIDATE)`
- **AND** flush_iotlb 实现 MUST 遍历 `iommu_domain->page_table` 标记对应 iova 范围为 invalid
- **AND** flush_iotlb 实现 MUST 触发 `sim_pfh_inject_fault_with_cause(addr, cause=IOTLB_FLUSH)` （commit `32e012d` 增强的 cause register）
- **AND** 测试 `tests/test_iommu_invalidate_runtime_standalone` MUST 覆盖 happy path（unmap → flush → page table invalid）+ error path（domain 不存在返回 `-EINVAL`）

#### Scenario: 用户态 page table invalidation 正确性

- **WHEN** flush_iotlb 标记 iova 范围为 invalid 后
- **THEN** 后续访问该 iova 范围 MUST 触发 page fault
- **AND** page fault handler MUST 识别 cause=IOTLB_FLUSH
- **AND** 测试 MUST 验证 fault cause 类型正确

#### Scenario: 不依赖 host kernel

- **WHEN** IOTLB flush 实现完成
- **THEN** system MUST NOT 调用 `/dev/iommu` / vfio / 任何 host kernel 接口
- **AND** system MUST 完全在用户态模拟范围内实现 invalidation
- **AND** 测试 MUST 在用户态环境运行通过（不需 root 权限）

#### Scenario: 多 iommu_domain 跨边界处理

- **WHEN** 系统中存在多个 iommu_domain，flush_iotlb 跨 domain 调用
- **THEN** system MUST 限制递归深度（防御性编程，避免无限循环）
- **AND** 跨 domain IOTLB flush MUST 正确处理（不漏 flush / 不重复 flush）
- **AND** 测试 MUST 覆盖多 domain 场景

