# hal-iommu-full: 实施任务

## 1. 接口定义

- [x] 1.1 ✅ sim 后端已由 C-12 Phase B.3.3 完成（`sim_pm_*` 集成）
- [x] 1.2 ✅ hal_user.h: `iommu_mappings` unordered_map + `iommu_lock`

## 2. Sim 后端实现 (hal_mock.cpp)

- [x] 2.1 ✅ C-12 完成：`mock_iommu_map` → `sim_pm_migrate_to_device`
- [x] 2.2 ✅ C-12 完成：`mock_iommu_unmap` → `sim_pm_migrate_to_system`
- [x] 2.3 ⏩ 延后：`iommu_invalidate` 不在 gpu_hal_ops 中

## 3. 真机后端实现 (hal_user.cpp)

- [x] 3.1 ✅ `user_iommu_map`: VA→size 跟踪, EINVAL for size=0, EEXIST for double-map
- [x] 3.2 ✅ `user_iommu_unmap`: ENOENT for unmapped, EINVAL for size mismatch
- [x] 3.3 ⏩ 延后：`iommu_invalidate` 不在 gpu_hal_ops 中

## 4. 错误处理

- [x] 4.1 ✅ sim: -EINVAL/-ENOMEM (C-12); user: -EINVAL/-EEXIST/-ENOENT
- [x] 4.2 ✅ size=0→EINVAL, double-map→EEXIST, unmapped-unmap→ENOENT

## 5. 测试

- [x] 5.1 ✅ test_hal_iommu_standalone (6 tests, 10 assertions — map/unmap/cycle/double-map/zero-size/unmapped)
- [x] 5.2 ⏩ 延后：invalidate 不存在于当前 ops
- [x] 5.3 ✅ 错误路径已在 test_hal_iommu_standalone 中覆盖
