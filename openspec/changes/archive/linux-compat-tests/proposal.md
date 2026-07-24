# linux-compat-tests: linux_compat 层测试覆盖

## Why

`include/linux_compat/` 目录有 9 个 header 文件，但 0 个 Catch2 测试。linux_compat 层是 UsrLinuxEmu 与 Linux kernel API 兼容的关键桥梁，缺少测试覆盖会影响内核 API 兼容性的可靠性验证。

## What Changes

- 为 linux_compat 层关键模块添加 Catch2 测试：
  - `types.h` 类型定义测试
  - `iommu/iommu_domain.h` IOMMU 接口测试
  - `drm/drm_device.h` DRM 接口测试
  - `pci/pci_device.h` PCIe 接口测试
- 覆盖正常路径和错误路径

## Capabilities

### New Capabilities
- `linux-compat-test-coverage`: linux_compat 层测试覆盖

### Modified Capabilities
<!-- No existing specs modified -->

## Impact

- `tests/` — 新增 5+ Catch2 测试文件
- `CMakeLists.txt` — 新增测试 target