## ADDED Requirements

### Requirement: UMD-Evolution Scope 文档组织
TaskRunner umd-evolution 范畴的文档 SHALL 位于 `docs/umd-evolution/` 子目录下，文档头部 MUST 包含 SCOPE: UMD-EVOLUTION 元数据，STATUS MUST 为 PROPOSED 或 DRAFT（**不接受** ACCEPTED，因为代码骨架尚未实现）。

#### Scenario: 文档头部元数据严格
- **WHEN** 任意 umd-evolution 范畴文档被创建
- **THEN** 文档头部 MUST 包含 `SCOPE: UMD-EVOLUTION` 字段
- **AND** `STATUS` 字段值 MUST 仅为 `PROPOSED` 或 `DRAFT`（不可为 `ACCEPTED`）
- **AND** MUST 包含 `IMPLEMENTED: NO` 字段以提示代码未实施

#### Scenario: Vision 文档存在
- **WHEN** umd-evolution 范畴目录创建完成
- **THEN** `docs/umd-evolution/vision.md` MUST 存在
- **AND** MUST 包含从原 `plan.md` v0.1 提取的 UMD 完整愿景内容
- **AND** MUST 明确标注"以下内容为设计愿景，当前未实施"

#### Scenario: Gap Analysis 文档存在
- **WHEN** umd-evolution 范畴目录创建完成
- **THEN** `docs/umd-evolution/gap-analysis.md` MUST 存在
- **AND** MUST 包含与 AMD ROCm UMD 的职责对比（来自 rocm-systems 调研）
- **AND** MUST 包含与 NVIDIA CUDA UMD 的职责对比（来自 open-gpu-kernel-modules 调研）
- **AND** MUST 明确列出 TaskRunner 当前缺失的关键能力（CUmodule 加载、Stream/Context 模型、doorbell mmap、PTX JIT 等）

### Requirement: UMD-Evolution 范畴 TADR 编号
umd-evolution 范畴的 TADR MUST 使用 2xx 编号段，原 tadr-001~003 MUST 重映射为 tadr-201~203。

#### Scenario: TADR 编号段识别
- **WHEN** 任何 TADR 文件名匹配 `tadr-2[0-9][0-9]-*.md` 模式
- **THEN** 该 TADR MUST 属于 umd-evolution 范畴
- **AND** MUST 位于 `docs/umd-evolution/adr/` 目录下

#### Scenario: UMD 范畴 TADR 明确标 vision
- **WHEN** umd-evolution 范畴 TADR 被阅读
- **THEN** MUST 在文档开头明确标注"本文档为设计愿景，当前未实施"
- **AND** MUST 不被作为 issue tracker 或任务来源
- **AND** MUST 在 `## Status` 部分标注 `DRAFT` 或 `PROPOSED`

### Requirement: UMD-Evolution 代码骨架
umd-evolution 范畴的代码 MUST 仅包含骨架（占位头文件 + 空 cpp），**不**包含具体实现逻辑。

#### Scenario: 代码骨架完整
- **WHEN** `TASKRUNNER_BUILD_MODE=umd-evolution` 时执行 `cmake --build build`
- **THEN** MUST 成功编译 `libumd_stub.so` target
- **AND** `src/umd/` MUST 包含 `cuda_api.cpp`、`module_loader.cpp`、`ring_buffer.cpp` 至少 3 个文件
- **AND** 每个文件 MUST 仅包含占位实现（返回 `nullptr` / `0` / 抛 `not_implemented` 异常）
- **AND** `include/umd/` MUST 包含对应 3 个头文件 + 类声明

#### Scenario: 默认 build 模式隔离
- **WHEN** 默认 `TASKRUNNER_BUILD_MODE=test-fixture` 时执行 `cmake --build build`
- **THEN** MUST NOT 编译 `libumd_stub.so`
- **AND** MUST NOT 引用 `src/umd/` 任何源文件
- **AND** 现有 test-fixture 范畴测试 MUST 全部通过

#### Scenario: 切换到 UMD 模式
- **WHEN** 用户执行 `cmake .. -DTASKRUNNER_BUILD_MODE=umd-evolution`
- **THEN** CMake MUST 切换到 `cmake/UMDEvolution.cmake` 模块
- **AND** MUST 仅构建 `taskrunner_shared` + `taskrunner_umd_stub`（不构建 test-fixture target）
- **AND** MUST NOT 引用 `src/test_fixture/` 任何源文件