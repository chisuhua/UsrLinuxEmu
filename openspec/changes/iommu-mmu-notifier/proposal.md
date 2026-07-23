# iommu-mmu-notifier: mmu_notifier 回调完整实现

## Why

`include/linux_compat/iommu/iommu_domain.h:96` 标记 TODO(stage-1.3): full mmu_notifier callback。当前 `mmu_notifier` 仅定义了回调接口但未实现通知链路，导致用户态内存变化无法触达 GPU IOMMU 页表更新。

## What Changes

- 实现 `mmu_notifier` 的回调注册和通知机制
- 在 `iommu_invalidate_range` 完成后自动触发注册的回调
- 添加 Catch2 测试覆盖页失效通知场景

## Capabilities

### New Capabilities
- `mmu-notifier-callback`: mmu_notifier 回调通知机制

### Modified Capabilities
<!-- No existing specs modified -->

## Impact

- `include/linux_compat/iommu/iommu_domain.h` — 实现回调注册
- `src/kernel/iommu/` — 通知链路实现
- `tests/` — 新增 mmu_notifier 测试