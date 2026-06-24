## ADDED Requirements

### Requirement: TASKRUNNER_BUILD_MODE CMake Option
TaskRunner MUST 提供 `TASKRUNNER_BUILD_MODE` CMake option，值域为 `test-fixture`（默认）| `umd-evolution`。

#### Scenario: 默认 build 模式
- **WHEN** 用户执行 `cmake ..` 不指定 `TASKRUNNER_BUILD_MODE`
- **THEN** `TASKRUNNER_BUILD_MODE` MUST 默认为 `test-fixture`
- **AND** MUST 加载 `cmake/Shared.cmake` + `cmake/TestFixture.cmake`
- **AND** MUST NOT 加载 `cmake/UMDEvolution.cmake`

#### Scenario: UMD-Evolution 模式切换
- **WHEN** 用户执行 `cmake .. -DTASKRUNNER_BUILD_MODE=umd-evolution`
- **THEN** MUST 加载 `cmake/Shared.cmake` + `cmake/UMDEvolution.cmake`
- **AND** MUST NOT 加载 `cmake/TestFixture.cmake`
- **AND** MUST 输出明确警告"WARNING: UMD-EVOLUTION is experimental, not for production use"

#### Scenario: 无效值拒绝
- **WHEN** 用户指定 `TASKRUNNER_BUILD_MODE=invalid`
- **THEN** CMake MUST 报告 `FATAL_ERROR`
- **AND** 错误信息 MUST 列出有效值域

### Requirement: CMake 模块化拆分
CMake 配置 MUST 拆分为 3 个独立模块文件：Shared.cmake / TestFixture.cmake / UMDEvolution.cmake。

#### Scenario: 模块文件存在
- **WHEN** 重构完成后检查 `cmake/` 目录
- **THEN** MUST 包含 `cmake/Shared.cmake`、`cmake/TestFixture.cmake`、`cmake/UMDEvolution.cmake` 三个文件
- **AND** 每个模块 MUST 仅定义其范畴相关的 target、依赖、include 路径
- **AND** 模块之间 MUST NOT 相互 include（避免循环依赖）

#### Scenario: 顶层 CMakeLists.txt 简化
- **WHEN** 顶层 `CMakeLists.txt` 被检查
- **THEN** MUST 仅包含：项目声明、`TASKRUNNER_BUILD_MODE` option 定义、Shared 子目录总是构建
- **AND** MUST 条件 include TestFixture 或 UMDEvolution 子目录
- **AND** 文件总行数 MUST < 50 行

### Requirement: Target 命名空间隔离
不同范畴的 target MUST 使用命名空间隔离的命名，避免冲突。

#### Scenario: Target 命名规范
- **WHEN** 任何 target 被声明
- **THEN** shared 范畴 MUST 使用 `taskrunner_shared` 名称
- **AND** test-fixture 范畴 MUST 使用 `taskrunner_test_fixture` 名称（static lib）+ `taskrunner` 名称（CLI exe）
- **AND** umd-evolution 范畴 MUST 使用 `taskrunner_umd_stub` 名称（shared lib）

#### Scenario: 跨范畴 target 不可见
- **WHEN** test-fixture 模式构建
- **THEN** `taskrunner_umd_stub` target MUST NOT 被声明
- **AND** `src/umd/` 下的源文件 MUST NOT 被编译

- **WHEN** umd-evolution 模式构建
- **THEN** `taskrunner_test_fixture` target MUST NOT 被声明
- **AND** `src/test_fixture/` 下的源文件 MUST NOT 被编译

### Requirement: Build 文档强制
每次模式切换 MUST 更新 `docs/build-instructions.md`，避免遗忘当前 build 模式。

#### Scenario: 文档同步
- **WHEN** `TASKRUNNER_BUILD_MODE` 默认值变更
- **THEN** `docs/build-instructions.md` MUST 在同一次提交中更新
- **AND** MUST 列出两种模式各自的 build 命令示例