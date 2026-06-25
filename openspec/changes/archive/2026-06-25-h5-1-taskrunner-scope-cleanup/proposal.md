# Proposal: H-5.1 TaskRunner Scope Cleanup

> **H-5.1** 是 [H-5 taskrunner-scope-clarification](../2026-06-24-h5-taskrunner-scope-clarification/) 的 cleanup follow-up。H-5 主 commit `3639d9f..b5d8036` (merged 2026-06-23) 已完成 3-scope 目录重组 + TADR 重映射 + SCOPE 注释，但实施后审计发现**3 类 P0 偏离 spec**：

1. **缺 2 个 TADR**：tadr-108 (build mode selection) + tadr-304 (Error Handling strategy layer)
2. **CMake 未模块化**：139 行单体 CMakeLists.txt 未拆为 `cmake/Shared.cmake` + `TestFixture.cmake` + `UMDEvolution.cmake`，无 `add_library()` 复用 target
3. **SCOPE 注释 + frontmatter 覆盖不完整**：19 个文档缺 YAML frontmatter、2 个 .cpp 标错 SCOPE、sample/main.cpp 缺 SCOPE

**H-5.1 范围**（已在另一会话中独立实施，6 commits `afae340..57b471a`）：
- 创建 tadr-108 + tadr-304
- 19 个文档补 frontmatter
- CMake 模块化重构（139 → 43 行 + 3 .cmake 模块 + 3 library target）
- SCOPE 注释修正（38/38 源文件 0 MISSING）
- docs-audit.sh 重写（适配 3-scope 结构，51 PASS / 0 FAIL）
- TaskRunner README + AGENTS 头部更新
- ~25 处跨文档死链修复
- tadr-301 描述更新（28→31 方法 via tadr-109）
- 4 个 research 文档 commit + .gitignore build_*/
- 跨仓 4 步同步（submodule pointer + mirror + plan archive）

**验证结果**（2026-06-25）：
- `docs-audit.sh`: PASS 51 / FAIL 0 / WARN 3（预期，跨仓同步相关）
- test-fixture 模式: 6/6 targets build OK，31/31 test cases passed，119/119 assertions
- umd-evolution 模式: 3/3 targets build OK，3/3 test cases passed，8/8 assertions
- UsrLinuxEmu 端 pre-commit docs-audit: 43/43 PASS / 0 FAIL

## 关联决策

- **ADR-035** (governance policy) — Rule 5.1 4 步同步协议
- **ADR-036** (3-way separation) — 范畴定义
- **tadr-108** (build mode selection) — `TASKRUNNER_BUILD_MODE` option 规范
- **tadr-304** (Error Handling strategy) — Linux errno 语义 + 传播规则

## 后续工作（已 deferred）

- Phase D umd-evolution PoC — 用户决策"维护优先，暂不启动新工作"
- H-3.5 后续工作（R2 mapping LOW32 溢出、ioctl path vs mmap path、attached_queues 弱校验）— 维护期内观察
- Stage 1.4 集成验证 — UsrLinuxEmu 端
