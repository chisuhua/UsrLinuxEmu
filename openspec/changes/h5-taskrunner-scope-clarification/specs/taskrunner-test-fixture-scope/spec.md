## ADDED Requirements

### Requirement: Test-Fixture Scope 文档组织
TaskRunner 文档 SHALL 按范畴组织，所有 test-fixture 范畴的文档 SHALL 位于 `docs/test-fixture/` 子目录下，文档头部 MUST 包含 SCOPE: TEST-FIXTURE 元数据。

#### Scenario: 文档头部元数据完整
- **WHEN** 任意 test-fixture 范畴文档被创建或修改
- **THEN** 文档头部 MUST 包含 `SCOPE: TEST-FIXTURE` 字段
- **AND** MUST 包含 `STATUS` 字段（值域：`ACCEPTED`/`PROPOSED`/`DRAFT`/`DEPRECATED`）
- **AND** 涉及 TADR 重映射时 MUST 包含 `REPLACES: tadr-NNN` 字段

#### Scenario: 重映射后路径可访问
- **WHEN** 测试 CI 访问 `docs/test-fixture/adr/tadr-101-stub-tracker.md`
- **THEN** 文件 MUST 存在
- **AND** 内容 MUST 等价于原 `docs/adr/tadr-004-cuda-vulkan-runtime-stub-tracker.md`（保留 git history）

#### Scenario: 跨范畴引用规范
- **WHEN** test-fixture 范畴文档引用 umd-evolution 范畴内容
- **THEN** MUST 使用相对路径 `../umd-evolution/...`
- **AND** MUST 在引用处显式标注 `[UMD-EVOLUTION SCOPE]` 提示读者

### Requirement: Test-Fixture 范畴 TADR 编号
test-fixture 范畴的 TADR MUST 使用 1xx 编号段（即 tadr-100 ~ tadr-199），原 tadr-004~008 MUST 重映射为 tadr-101~105。

#### Scenario: TADR 编号段识别
- **WHEN** 任何 TADR 文件名匹配 `tadr-1[0-9][0-9]-*.md` 模式
- **THEN** 该 TADR MUST 属于 test-fixture 范畴
- **AND** MUST 位于 `docs/test-fixture/adr/` 目录下

#### Scenario: 原 TADR 重映射成功
- **WHEN** 重映射完成后 CI 验证 `docs/test-fixture/adr/` 目录
- **THEN** MUST 包含 tadr-101-stub-tracker.md、tadr-102-igpu-driver.md、tadr-103-h3-phase2.md、tadr-104-r2-mapping.md、tadr-105-h7-deferred.md 共 5 个文件
- **AND** MUST 包含对应 5 个 redirect 文件（tadr-004-redirect.md 等）

### Requirement: Test-Fixture 范畴代码归属
所有当前 main 已 shippable 的代码 MUST 保留在 `src/test_fixture/` 和 `include/test_fixture/` 目录下（CudaStub / CudaScheduler / GpuDriverClient / CLI）。

#### Scenario: 代码位置合规
- **WHEN** CI 验证 test-fixture 范畴代码位置
- **THEN** `src/test_fixture/` MUST 包含 `cuda_stub.cpp`、`cuda_scheduler.cpp`、`gpu_driver_client.cpp`、`cmd_cuda.cpp` 至少 4 个文件
- **AND** `include/test_fixture/` MUST 包含对应 4 个头文件
- **AND** 每个 .cpp/.hpp 文件头部 MUST 包含 `// SCOPE: TEST-FIXTURE` 注释

#### Scenario: 功能不回归
- **WHEN** 重构完成后运行 `test_cuda_scheduler` 和 `test_gpu_phase2`
- **THEN** 8 + 12 共 20 个 doctest 用例 MUST 全部通过
- **AND** 行为 MUST 与重构前完全一致（仅文件路径变更）