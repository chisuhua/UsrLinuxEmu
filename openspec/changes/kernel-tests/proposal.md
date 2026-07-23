# kernel-tests: kernel 层测试覆盖

## Why

`include/kernel/` 目录有 15 个 header 文件，但 0 个 Catch2 测试。kernel 层是 UsrLinuxEmu 的核心模拟环境，VFS、scheduler、IOMMU、device model 等关键组件缺少测试覆盖，影响整体可靠性。

## What Changes

- 为 kernel 层核心组件添加 Catch2 测试：
  - `vfs.h` VFS 文件系统操作测试
  - `device/device.h` 设备模型测试
  - `scheduler.h` 调度器测试
  - `module_loader.h` 模块加载测试
- 覆盖正常路径和关键错误路径

## Capabilities

### New Capabilities
- `kernel-test-coverage`: kernel 层核心组件测试覆盖

### Modified Capabilities
<!-- No existing specs modified -->

## Impact

- `tests/` — 新增 8+ Catch2 测试文件
- `CMakeLists.txt` — 新增测试 target