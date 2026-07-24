# kfd-multi-file-complete: 实施任务

## 1. 编译问题调查
- [ ] 1.1 运行 `make -j4` 收集所有编译错误
- [ ] 1.2 分类: 未解析符号 / 头文件缺失 / 类型不匹配

## 2. 逐文件修复
- [ ] 2.1 修复 `kfd_*.cpp` 文件编译错误
- [ ] 2.2 补齐 CMake `target_link_libraries` 依赖
- [ ] 2.3 修复 include path 问题

## 3. 验证
- [ ] 3.1 确认 `make -j4` 全量编译通过
- [ ] 3.2 运行现有测试确保无回归