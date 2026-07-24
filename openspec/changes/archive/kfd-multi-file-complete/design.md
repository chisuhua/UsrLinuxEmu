# kfd-multi-file-complete: 技术设计

## Context
C-12 KFD 多文件集成 81% 完成，剩余 19% 是编译期问题。

## Goals
- 解决跨文件未解析符号
- 补齐 CMake target 链

## Decisions
- 使用 CMake `target_link_libraries` 显式声明依赖
- 逐个文件解决编译错误

## Risks
| Risk | Mitigation |
|------|------------|
| 引入新编译错误 | 每个文件修改后 verify build |