# hal-iommu-full: 实施任务

## 1. 接口定义

- [ ] 1.1 确认 `gpu_hal_iommu_ops` 函数签名与 `linux_compat/iommu/iommu_domain.h` IOMMU API 对齐
- [ ] 1.2 定义 IOMMU 映射内部数据结构（`IommuMapping` struct，含 gpu_va/user_va/size/flags）

## 2. Sim 后端实现 (hal_mock.cpp)

- [ ] 2.1 实现 `iommu_map_memory` sim 版本 — 基于 `UserEmuEnv` flat page table
- [ ] 2.2 实现 `iommu_unmap_memory` sim 版本 — 移除映射并释放
- [ ] 2.3 实现 `iommu_invalidate_range` sim 版本 — 遍历映射表失效

## 3. 真机后端实现 (hal_user.cpp)

- [ ] 3.1 实现 `iommu_map_memory` 真机版本 — 调用 `linux_compat` IOMMU API
- [ ] 3.2 实现 `iommu_unmap_memory` 真机版本 — 调用 linux IOMMU unmap
- [ ] 3.3 实现 `iommu_invalidate_range` 真机版本 — 调用 IOMMU domain flush

## 4. 错误处理

- [ ] 4.1 统一 sim/真机两端的错误码映射（`-EINVAL`, `-ENOMEM`, `-EFAULT`）
- [ ] 4.2 添加边缘情况处理（size=0, overflow, double-free）

## 5. 测试

- [ ] 5.1 编写 `test_hal_iommu_map` — 正常 map/unmap 流程
- [ ] 5.2 编写 `test_hal_iommu_invalidate` — invalidate 后 map 查询
- [ ] 5.3 编写 `test_hal_iommu_error` — 错误路径（无效参数、重复 map、重复 unmap）
