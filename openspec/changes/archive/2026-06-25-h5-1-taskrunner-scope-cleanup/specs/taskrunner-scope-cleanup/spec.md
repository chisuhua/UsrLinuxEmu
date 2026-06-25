## ADDED Requirements

### Requirement: H-5.1 范畴 cleanup 完整性

H-5.1 实施 MUST 补齐 H-5 漏掉的 3 类 spec 内容：TADR（tadr-108 + tadr-304）+ CMake 模块化 + SCOPE/STATUS 覆盖。

#### Scenario: TADR 补齐验证
- **WHEN** H-5.1 完成后检查 `docs/shared/adr/` 目录
- **THEN** MUST 包含 tadr-108-build-mode-selection.md（77 行 + YAML frontmatter）
- **AND** MUST 包含 tadr-304-error-handling-strategy.md（128 行 + YAML frontmatter）
- **AND** 两者 MUST 在 UsrLinuxEmu 端 `docs/00_adr/README.md` 的 TADR mirror 中出现

#### Scenario: CMake 模块化验证
- **WHEN** H-5.1 完成后检查 `cmake/` 目录
- **THEN** MUST 包含 `cmake/Shared.cmake` + `cmake/TestFixture.cmake` + `cmake/UMDEvolution.cmake`
- **AND** 根 `CMakeLists.txt` MUST < 50 行（原 139 行）
- **AND** MUST 定义 3 个 library target: `taskrunner_shared` + `taskrunner_test_fixture` + `taskrunner_umd_stub`

#### Scenario: SCOPE/STATUS 覆盖验证
- **WHEN** H-5.1 完成后 grep 所有 .hpp/.cpp/.h 文件
- **THEN** 100% 源文件 MUST 有 `// SCOPE:` 注释（实测 38/38）
- **AND** 100% 文档 MUST 有 YAML `SCOPE + STATUS` frontmatter（实测 44/44 验证或 19 修复后达标）
- **AND** 0 个文件 SCOPE 标签与所在目录不一致

### Requirement: H-5.1 Link-time 可用性

H-5.1 完成后两种 build 模式 MUST 实际编译 + 链接成功，并跑通所有 test。

#### Scenario: test-fixture 模式 6/6 targets build
- **WHEN** `cmake .. -DTASKRUNNER_BUILD_MODE=test-fixture && make -j4`
- **THEN** MUST 成功生成 6 个 targets: taskrunner_shared, taskrunner_test_fixture, taskrunner (CLI), test_cuda_scheduler, test_gpu_architecture, test_gpu_phase2
- **AND** MUST 跑通 31/31 test cases (8 + 11 + 12), 119/119 assertions, 0 failed

#### Scenario: umd-evolution 模式 3/3 targets build
- **WHEN** `cmake .. -DTASKRUNNER_BUILD_MODE=umd-evolution && make -j4`
- **THEN** MUST 成功生成 3 个 targets: taskrunner_shared, taskrunner_umd_stub, test_umd_skeleton
- **AND** MUST 跑通 3/3 test cases, 8/8 assertions, 0 failed

### Requirement: H-5.1 跨仓 4 步同步

H-5.1 MUST 按 ADR-035 §Rule 5.1 4 步流程同步到 UsrLinuxEmu 主仓。

#### Scenario: 4 步流程完整性
- **WHEN** H-5.1 完成后
- **THEN** Step 1 (TaskRunner 端 commit): MUST 落库到 TaskRunner main 分支
- **AND** Step 2 (UsrLinuxEmu 端 submodule bump): submodule pointer MUST 指向 H-5.1 最后 commit
- **AND** Step 3 (UsrLinuxEmu 端 mirror 更新): docs/00_adr/README.md MUST 含 tadr-108 + tadr-304
- **AND** Step 4 (UsrLinuxEmu 端 plan archive): docs/superpowers/plans/2026-06-24-*.md MUST 已 commit
- **AND** Step 5 (H-5.1 openspec archive): 本 spec MUST 存在
