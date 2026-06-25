# Design: H-5.1 TaskRunner Scope Cleanup

> **H-5.1** 是 [H-5 taskrunner-scope-clarification](../2026-06-24-h5-taskrunner-scope-clarification/) 的 cleanup follow-up。完整范畴规范、双轨分类原则、TADR 编号段见 H-5 design.md。

## H-5.1 偏离分析（H-5 完成后审计发现）

H-5 主 commit（`3639d9f..b5d8036`, merged 2026-06-23）实施时**漏掉了 3 类 spec 内容**：

| 类别 | 漏掉内容 | H-5.1 修复 |
|------|----------|-----------|
| TADR 缺失 | tadr-108 (build mode) + tadr-304 (Error Handling strategy) | 2 个新 TADR 创建（commit `0f4ef07`）|
| CMake 未模块化 | 139 行单体 CMakeLists.txt | 模块化为 `cmake/Shared.cmake` + `TestFixture.cmake` + `UMDEvolution.cmake`（commit `b064fb7`）|
| SCOPE/STATUS 覆盖不完整 | 19 个文档缺 frontmatter + 2 个 .cpp 标错 SCOPE + sample/main.cpp 缺 SCOPE | 补齐 19 docs + 修正 2 cpp + 加 1 cpp（commit `afae340` + `0f4ef07`）|

H-5.1 实施时**额外发现 3 个 spec 漏掉的 link-time 必要项**（Subagent 2 透明披露后补齐）：

| 类别 | 漏掉内容 | H-5.1 修复 |
|------|----------|-----------|
| UsrLinuxEmu 符号链接 include path | 3 个 .cmake 文件 `target_include_directories` 缺 `${CMAKE_SOURCE_DIR}` | 在 3 个 .cmake 文件加（commit `57b471a`）|
| CLI main 来源 | `taskrunner` executable 缺 `cli_main.cpp` + `cmd_buffer_v2.cpp` | 补 2 个 source（commit `57b471a`）|
| 3 test main 冲突 | 3 个 test 文件都用 `DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN`，合并 1 executable 链接冲突 | 拆为 3 个独立 executable（commit `57b471a`）|

## 跨仓同步（ADR-035 §Rule 5.1 4 步）

| Step | 端 | 内容 | Commit |
|------|----|------|--------|
| 1 | TaskRunner | 6 commits `afae340..57b471a` | （本地，未 push）|
| 2 | UsrLinuxEmu | `git add external/TaskRunner` 更新 submodule pointer 到 `57b471a` | `89113ec` |
| 3 | UsrLinuxEmu | mirror 更新（加 tadr-108 + tadr-304，更新 tadr-301 描述，更新"最后更新"日期）| `89113ec` |
| 4 | UsrLinuxEmu | plan archive commit | `69c9c0f` |

## 验证结果（2026-06-25）

### TaskRunner 端
- `docs-audit.sh`: **PASS 51 / FAIL 0 / WARN 3**（WARN 是预期的 mirror 同步相关）
- test-fixture 模式: 6/6 targets build OK，**31/31 test cases** (8+11+12)，**119/119 assertions** 0 failed
- umd-evolution 模式: 3/3 targets build OK，**3/3 test cases**，**8/8 assertions** 0 failed
- 38 个源文件全部有 `// SCOPE:` 注释，0 MISSING

### UsrLinuxEmu 端
- `pre-commit docs-audit.sh`: **43 PASS / 0 FAIL / 0 WARN** ✓

## 关联决策

- **ADR-035** (governance policy) — Rule 5.1 4 步同步协议
- **ADR-036** (3-way separation) — 范畴定义
- **tadr-108** (build mode selection) — `TASKRUNNER_BUILD_MODE` option 规范（H-5.1 新增）
- **tadr-304** (Error Handling strategy) — Linux errno 语义 + 传播规则（H-5.1 新增，扩展 tadr-303）
- **tadr-301** (IGpuDriver contract) — 28 → 31 方法扩展（via tadr-109）
