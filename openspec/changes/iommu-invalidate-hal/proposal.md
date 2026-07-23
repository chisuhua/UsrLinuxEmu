# iommu-invalidate-hal: HAL IOMMU Invalidate 回调

## Why

hal-iommu-full 已成功在 `gpu_hal_ops` 中实现了 `iommu_map_memory` / `iommu_unmap_memory` / `iommu_invalidate_range` 三个回调，但当前 `gpu_hal_ops` 缺少独立的 `iommu_invalidate` fn-ptr，无法通过 HAL 接口触发细粒度的 IOMMU 页表失效通知。这在真机场景下会导致 GPU 在 IOMMU TLB 已失效的情况下仍然使用 stale 映射，造成数据损坏。

## What Changes

- 在 `gpu_hal_ops` 中新增第 15 个 fn-ptr `iommu_invalidate`（不影响现有 14 个签名）
- 在 `hal_mock.cpp` 中实现 sim 端 iommu_invalidate（基于 `UserEmuEnv` 的映射表批量失效）
- 在 `hal_user.cpp` 中实现真机端 iommu_invalidate（调用 Linux kernel `iommu_flush_iotlb_all`）
- 添加 Catch2 测试覆盖 invalidate 正常路径和错误路径

## Capabilities

### New Capabilities
- `hal-iommu-invalidate`: HAL IOMMU 独立的 invalidate fn-ptr 回调

### Modified Capabilities
<!-- 不修改现有 spec 级别行为 -->

## Impact

- `plugins/gpu_driver/hal/hal.h` / `hal_ops.h` — 新增 `iommu_invalidate` fn-ptr
- `plugins/gpu_driver/hal/hal_mock.cpp` — 实现 sim 端 invalidate
- `plugins/gpu_driver/hal/hal_user.cpp` — 实现真机端 invalidate
- `tests/` — 新增 IOMMU invalidate 独立测试
