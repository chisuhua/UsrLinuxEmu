# quickstart-verification

## ADDED Requirements

### Requirement: End-to-End Quickstart Validation

新用户从 `git clone` 到跑通第一个 GPU 示例的时间 MUST 测量并记录。

#### Scenario: Quickstart completes within 15 minutes

- **GIVEN** 基准环境条件已记录（OS, CPU，网络速度）
- **WHEN** 从零开始执行完整流程（git clone → build → 首个示例）
- **THEN** 3 次测量中位数 ≤ 15 分钟
- **AND** 验证报告写入 `docs/01-quickstart/quickstart-verification.md`

#### Scenario: Optimization if exceeding time budget

- **GIVEN** 中位数超 15 分钟
- **WHEN** 分析瓶颈步骤
- **THEN** 报告包含瓶颈分析和优化建议（并行编译、预编译依赖等）