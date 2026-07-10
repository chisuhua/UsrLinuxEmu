# Tasks: stage3-3-errno-coverage-audit

> **状态**: 📋 PROPOSED
> **目标**: 20 个新 IOCTL handler errno 正确性审计 + 测试

## 1. 审计准备（30 分钟）

- [ ] 1.1 列出 20 个新 IOCTL handler 函数路径
- [ ] 1.2 每个 handler 识别 4 种 error 场景：
  - 参数无效
  - 资源耗尽
  - 不支持操作
  - 内部错误
- [ ] 1.3 建立 errno 对照表

## 2. 逐个 handler 审计 + 修复（每个 30 分钟，~10 小时 total）

对每个 handler（20 个）：
- [ ] 2.1 检查现状（grep handler impl）
- [ ] 2.2 对照标准 errno 评估
- [ ] 2.3 不当处修复（替换 -1 → -EINVAL 等）
- [ ] 2.4 加 test case 验证

逐项：
- [ ] stream_capture_begin / end / status
- [ ] graph_create / destroy / add_kernel_node / add_memcpy_node / instantiate / launch / destroy_exec
- [ ] mem_pool_create / destroy / alloc / alloc_async / free / free_async / set_attr / get_attr / trim
- [ ] mem_pool_export_shareable

## 3. 测试补充（4 小时）

- [ ] 3.1 `tests/test_sim_*_errno_standalone.cpp`（新文件，多 test case）
- [ ] 3.2 集成到 `tests/CMakeLists.txt`
- [ ] 3.3 运行 ctest 验证

## 4. 验证 / commit（30 分钟）

- [ ] 4.1 所有 20 个 handler 经过审计 + 测试
- [ ] 4.2 ctest 全绿
- [ ] 4.3 docs-audit 无新 warning
- [ ] 4.4 commit（多次 atomic）：`fix(gpu): errno coverage for Phase 3 IOCTLs`
