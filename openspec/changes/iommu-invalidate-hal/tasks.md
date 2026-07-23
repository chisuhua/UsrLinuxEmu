# iommu-invalidate-hal: 实施任务

## 1. 接口定义

- [ ] 1.1 在 `gpu_hal_ops` 末尾新增 `int (*iommu_invalidate)(gpu_va_t va, size_t size)` fn-ptr
- [ ] 1.2 定义返回值约定：0 成功、-ENOENT 未找到映射、-EINVAL 参数非法
- [ ] 1.3 确认 `gpu_va_t` 类型定义已可用（来自 `gpu_types.h`）

## 2. Sim 后端实现 (hal_mock.cpp)

- [ ] 2.1 实现 `hal_mock_iommu_invalidate` — 基于 `UserEmuEnv.iommu_mappings` 查找并移除
- [ ] 2.2 处理边界情况：va 不在映射表中 → `-ENOENT`，size=0 → `-EINVAL`
- [ ] 2.3 确认 `iommu_mappings` 数据结构（`std::unordered_map<gpu_va_t, IommuMapping>`）已存在
- [ ] 2.4 在 mock HAL 初始化中绑定 `iommu_invalidate` fn-ptr

## 3. 真机后端实现 (hal_user.cpp)

- [ ] 3.1 实现 `hal_user_iommu_invalidate` — 调用 `iommu_flush_iotlb_all()`（若可用）或逐页 flush
- [ ] 3.2 处理 VA 不在映射表中的情况 → `-ENOENT`
- [ ] 3.3 在 user HAL 初始化中绑定 `iommu_invalidate` fn-ptr

## 4. 调用点集成

- [ ] 4.1 检查 `drv/` 中是否需要通过 HAL 调用 `iommu_invalidate`
- [ ] 4.2 确认 `GpgpuDevice` 或 IOMMU 管理代码中有调用路径

## 5. 测试

- [ ] 5.1 编写 `test_hal_iommu_invalidate_valid` — 正常 map → invalidate → verify absent 流程
- [ ] 5.2 编写 `test_hal_iommu_invalidate_not_found` — 未映射 VA 返回 `-ENOENT`
- [ ] 5.3 编写 `test_hal_iommu_invalidate_zero_size` — size=0 返回 `-EINVAL`
- [ ] 5.4 编写 `test_hal_iommu_invalidate_remap` — invalidate 后可重新 map