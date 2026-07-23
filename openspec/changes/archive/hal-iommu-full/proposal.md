# hal-iommu-full: HAL IOMMU 扩展完整实现

## Why

ADR-061 已 Accepted，`gpu_hal_ops` 已扩展到 14 fn-ptrs，`hal_user.cpp` 和 `hal_mock.cpp` 已提交了 IOMMU 相关的 3 个函数 stub（`iommu_map_memory`、`iommu_unmap_memory`、`iommu_invalidate_range`），但当前只返回 `-ENOSYS`。这导致 ②（可移植驱动代码）在调用 IOMMU 相关路径时实际上走的是 sim 的虚拟 VA 路径，真机环境下需要真实的 IOMMU 映射才能实现驱动代码零修改切换。

## What Changes

- 在 `hal_mock.cpp` 中实现 IOMMU map/unmap/invalidate 的完整 sim 版本（基于 UserEmuEnv VA 空间）
- 在 `hal_user.cpp` 中实现 IOMMU map/unmap/invalidate 的真机版本（调用真实硬件 IOMMU API）
- 扩展 `gpu_hal_iommu_ops` 错误码约定，与 Linux kernel IOMMU API 对齐
- 添加 Catch2 测试覆盖 map/unmap/invalidate 正常路径和错误路径

## Capabilities

### New Capabilities
- `hal-iommu-map`: IOMMU 地址映射完整实现（sim + 真机）
- `hal-iommu-invalidate`: IOMMU 页表失效完整实现

### Modified Capabilities
<!-- 不修改现有 spec 级别行为 -->

## Impact

- `plugins/gpu_driver/hal/hal_mock.cpp` — 实现 sim 端 IOMMU ops
- `plugins/gpu_driver/hal/hal_user.cpp` — 实现真机端 IOMMU ops
- `plugins/gpu_driver/hal/hal.h` / `hal_ops.h` — 可能需要补充错误码枚举
- `tests/` — 新增 IOMMU HAL 测试
