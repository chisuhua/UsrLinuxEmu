# kfd-multi-file-complete: KFD 多文件集成剩余 19%

## Why

ADR-059 已 Accepted，C-12 KFD 多文件集成已完成 81%。剩余 19% 涉及跨文件链接、CMake target 依赖、头文件包含路径等编译期集成问题，需要完成以达成 100% 集成目标。

## What Changes

- 完成 KFD 多文件集成剩余 19% 的编译期依赖解决
- 修复跨文件链接错误（未解析符号）
- 补齐 CMake target 依赖链
- 修复头文件包含路径问题
- 确保所有现有 Catch2 测试通过

## Capabilities

### New Capabilities
- `kfd-multi-file-link`: KFD 多文件链接完整性

### Modified Capabilities
<!-- No existing specs modified -->

## Impact

- `plugins/gpu_driver/drv/` — KFD 多文件集成
- `CMakeLists.txt` 相关文件 — target 依赖链条