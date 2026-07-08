# Tasks: stage3-3-error-injection-tests

> **状态**: 📋 PROPOSED
> **目标**: 新建错误注入测试框架，覆盖关键路径 ≥ 80%

## 1. 框架设计（半天）

- [ ] 1.1 决定注入机制（env var / function pointer swap / mock layer）
- [ ] 1.2 选择最简 + 跨平台方式
- [ ] 1.3 起草 `tests/error_inject/README.md` 说明用法
- [ ] 1.4 框架头文件 `tests/error_inject/error_inject.h`

## 2. 框架实现（1 天）

- [ ] 2.1 `tests/error_inject/error_inject_main.cpp` — main + 工具
- [ ] 2.2 `tests/error_inject/inject_buddy_alloc_fail.cpp` — BO alloc 失败
- [ ] 2.3 `tests/error_inject/inject_va_space_fail.cpp` — VA space 失败
- [ ] 2.4 `tests/error_inject/inject_queue_submit_fail.cpp` — Queue 失败
- [ ] 2.5 `tests/error_inject/inject_fence_wait_timeout.cpp` — Fence 超时
- [ ] 2.6 `tests/CMakeLists.txt` 注册

## 3. 关键路径覆盖（1 天）

对以下 critical path 加注入测试：
- [ ] 3.1 BO alloc + map + free 完整链
- [ ] 3.2 VA Space + Queue 创建 + 提交 + 销毁
- [ ] 3.3 Fence submit + wait
- [ ] 3.4 Stream capture begin + end
- [ ] 3.5 Graph create + instantiate + launch + destroy
- [ ] 3.6 Pool create + alloc + alloc_async + free + destroy

## 4. 覆盖率验证（半天）

- [ ] 4.1 计算关键 path coverage（要求 ≥ 80%）
- [ ] 4.2 文档化哪些 path 已覆盖 / 待覆盖

## 5. CI 集成（半天）

- [ ] 5.1 注入测试作为 `bench` 类（非 required CI gating）
- [ ] 5.2 或作为定期全量测试

## 6. 验证 / commit（30 分钟）

- [ ] 6.1 ctest -R error_inject PASS
- [ ] 6.2 ctest 全量 PASS
- [ ] 6.3 文档化运行方式
- [ ] 6.4 commit：`test(error-inject): critical path failure injection coverage`
