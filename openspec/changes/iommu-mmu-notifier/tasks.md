# iommu-mmu-notifier: 实施任务

## 1. 接口定义
- [ ] 1.1 在 `iommu_domain.h` 中完成 `mmu_notifier_ops` 回调类型定义
- [ ] 1.2 在 `src/kernel/iommu/` 中实现回调注册/通知

## 2. 集成
- [ ] 2.1 在 `iommu_invalidate_range` 完成后触发回调
- [ ] 2.2 确保回调链按注册顺序执行

## 3. 测试
- [ ] 3.1 编写 `test_mmu_notifier_register` — 注册回调
- [ ] 3.2 编写 `test_mmu_notifier_invalidate` — invalidate 触发通知