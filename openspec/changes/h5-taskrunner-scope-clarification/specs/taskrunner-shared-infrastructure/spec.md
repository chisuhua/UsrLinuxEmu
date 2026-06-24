## ADDED Requirements

### Requirement: Shared Infrastructure 范畴边界
shared 范畴 MUST 包含可被 test-fixture 和 umd-evolution 两个范畴共同使用的基础设施代码与文档，**不**包含任何范畴专属业务逻辑。

#### Scenario: shared 范畴包含范围
- **WHEN** 任意代码或文档被分类到 shared 范畴
- **THEN** MUST 满足以下条件之一：
  - 是 test-fixture 和 umd-evolution 都需要的接口契约（IGpuDriver）
  - 是 test-fixture 和 umd-evolution 都需要的同步原语（fence / barrier）
  - 是 test-fixture 和 umd-evolution 都需要的错误处理抽象
  - 是 test-fixture 和 umd-evolution 都需要的共享工具函数

#### Scenario: shared 范畴不包含业务逻辑
- **WHEN** CI 验证 shared 范畴代码
- **THEN** MUST NOT 包含 CUDA Driver API 业务逻辑（cuda_stub 专属）
- **AND** MUST NOT 包含 CudaScheduler 调度逻辑
- **AND** MUST NOT 包含 GPU IOCTL 包装（gpu_driver_client 专属）
- **AND** MUST NOT 包含 UMD 业务逻辑（cuda_api.cpp 专属）

### Requirement: IGpuDriver 共享契约
IGpuDriver 抽象 MUST 位于 `include/shared/igpu_driver.hpp`，包含 28 个方法签名（当前 main 已定义）。

#### Scenario: IGpuDriver 文件位置
- **WHEN** 重构完成后访问 IGpuDriver 头文件
- **THEN** MUST 位于 `include/shared/igpu_driver.hpp`
- **AND** MUST NOT 位于 `include/test_fixture/` 或 `include/umd/`
- **AND** `namespace async_task::gpu` 命名空间 MUST 保留

#### Scenario: 28 方法签名稳定
- **WHEN** 任何代码引用 IGpuDriver 方法
- **THEN** 28 个方法签名 MUST 与 H-2.5 完成时完全一致
- **AND** 5 个 H-3 Phase 2 方法（create_va_space/destroy_va_space/register_gpu/create_queue/destroy_queue）签名 MUST 不变
- **AND** MUST NOT 在本次重构中变更方法签名

### Requirement: Sync Primitives 共享抽象
同步原语（MPSC Queue、Atomic Counter、Mutex wrappers）MUST 位于 `include/shared/sync_primitives.hpp`。

#### Scenario: Sync Primitives 文件位置
- **WHEN** 重构完成后访问同步原语头文件
- **THEN** MUST 位于 `include/shared/sync_primitives.hpp`
- **AND** MUST NOT 位于范畴专属目录

#### Scenario: 双向引用合法
- **WHEN** test-fixture 代码 `#include "shared/sync_primitives.hpp"`
- **THEN** 编译 MUST 成功
- **AND** umd-evolution 代码 `#include "shared/sync_primitives.hpp"` 也 MUST 编译成功

### Requirement: Error Handling 共享抽象
错误处理抽象（Result<T>、ErrorCode enum）MUST 位于 `include/shared/error_handling.hpp`。

#### Scenario: Error Handling 文件位置
- **WHEN** 重构完成后访问错误处理头文件
- **THEN** MUST 位于 `include/shared/error_handling.hpp`
- **AND** MUST NOT 位于范畴专属目录

### Requirement: 共享区 Review 严格化
任何 shared 范畴代码变更 MUST 由 test-fixture 和 umd-evolution 两个范畴的代表共同 review，避免单方破坏。

#### Scenario: PR 审查要求
- **WHEN** shared 范畴代码变更 PR 被提交
- **THEN** MUST 至少包含 1 个 test-fixture 范畴 maintainer 的 review approval
- **AND** MUST 至少包含 1 个 umd-evolution 范畴 maintainer 的 review approval（即使 umd-evolution 尚未活跃）
- **AND** PR 描述 MUST 显式说明"此变更对两个范畴的影响"