## Context

`strengthen-semantic-assertions-for-destroy-va-space-and-query-queue` 修复 `System C IOCTL 端到端测试完备性审计（2026-08-02）` 识别的能力缺口。

- **ADR-024（用户态队列提交）** — `QUERY_QUEUE` 是用户态查询 ring/queue 状态的核心入口；当前 `tests/test_va_space.cpp` 仅打印 `queue_type/queue_id/doorbell_offset`，无值断言，意味着回归（驱动返回错误数据）无法被 CI 拦截
- **ADR-017（GPFIFO 队列抽象）** — VA Space 是 Queue 的宿主（`gpu_va_space_args.page_size` + 队列 `va_space_handle`）；`DESTROY_VA_SPACE` 之后关联 queue 应失效，这是关键 invariant
- **现状（已审计验证）**：
  - `GPU_IOCTL_DESTROY_VA_SPACE` (0x31)：`tests/test_gpu_plugin.cpp` 清理路径中 `REQUIRE(result == 0)`；`tests/test_va_space.cpp` 同。**无任何"destroy 后失效"的下游断言**——如果 VA space 销毁是 no-op，CI 不会失败
  - `GPU_IOCTL_QUERY_QUEUE` (0x43)：`tests/test_va_space.cpp` 仅 `ret == 0` + `printf` 字段；`tests/test_stub_handlers_tier2_standalone.cpp` 走 driver 层直接调用，**无 plugin 路径的字段值断言**
- **rdd-workflow TDD 纪律** — 强语义断言是回归保护网；现有"ret==0"是必需非充分
- **AGENTS.md 风格** — Catch2 优先 `REQUIRE`（强约束）而非 `CHECK`（弱约束），用于 value assertion

## Goals / Non-Goals

**Goals:**

- `tests/test_va_space.cpp`：
- 新增/强化 `DESTROY_VA_SPACE` 语义断言：destroy 后对同一 handle 二次 destroy 返回 `< 0`；destroy 后用同一 handle 创建 queue 返回 `< 0`；destroy 重复调用不引起崩溃
- 不破坏既有 `CREATE_VA_SPACE` / `CREATE_QUEUE` / `DESTROY_QUEUE` 测试用例
- `tests/test_gpu_plugin.cpp`：
- 新增 `TEST_CASE "GPU_IOCTL_QUERY_QUEUE E2E semantic"`：
- 前置：`CREATE_VA_SPACE` + `CREATE_QUEUE` 携带特定 `queue_type`（如 `GPU_QUEUE_TYPE_COMPUTE=0`）和 `ring_buffer_size`
- `QUERY_QUEUE` 后断言：
- `ret == 0`
- `args.queue_type == 创建时设定值`
- `args.queue_id != 0`（非零 ID）
- `args.doorbell_offset ∈ [DOORBELL_ALLOC_BASE, ...)`（合理范围）
- `args.ring_buffer_size == 创建时设定值`
- 负路径：`QUERY_QUEUE` 携带不存在的 `queue_handle` → `< 0`

**Non-Goals:**

- - `handleDestroyVASpace` / `handleQueryQueue` 实现修改（除非测试暴露 bug；本提案不预设）
  - 既有 stub 测试 `test_stub_handlers_tier2_standalone.cpp` 改动
  - 跨进程 queue state 可见性（不在现状范围内）
  - 新增 `gpu_queue.h` 公共字段（仅断言已有结构体）

## Decisions

**架构决策**:
- **决策 1**: P1 优先级 — 修改活跃派发表而非 ABI 头（保持 ABI 兼容）
**技术约束（继承自 improvement）**:
- **MUST**:
  - DESTROY_VA_SPACE 不崩溃断言使用 Catch2 `REQUIRE_NOTHROW`（强约束）或显式 try/catch
  - 字段值断言使用 Catch2 `REQUIRE(...)`（不是 `CHECK`），失败立即终止测试
  - 字段范围断言（如 `doorbell_offset`）使用 `>= DOORBELL_ALLOC_BASE`（不强加上界，避免误报）
  - 既有 `tests/test_va_space.cpp` 中 `CREATE_VA_SPACE` / `CREATE_QUEUE` / `DESTROY_QUEUE` 用例不被修改（仅追加新 TEST_CASE）
  - 既有 `tests/test_gpu_plugin.cpp` 36 项测试用例不被修改（仅追加新 TEST_CASE）
  - 若测试暴露 `handleDestroyVASpace` 或 `handleQueryQueue` 行为 bug，本提案**不**就地修复——记录失败信息作为 follow-up 提案输入
  - 错误码采用 CTest 适配：若实现使用 `-ENOENT` 而非 `-EINVAL`，测试用 `REQUIRE(ret < 0)` 容错断言
- **MUST NOT**:
  - 不修改 `handleDestroyVASpace` 或 `handleQueryQueue` 实现
  - 不修改 `gpu_ioctl.h` / `gpu_queue.h` / `gpu_types.h` ABI 头
  - 不删除既有 ret==0 断言（仅追加新断言）
  - 不在测试中引入新 GpuQueueEmu / VASpace 公共 API
- **SHOULD**:
  - 字段值断言使用 named constant 而非 magic number（如 `GPU_QUEUE_TYPE_COMPUTE` 而非 `0`）
  - 负路径错误码先以实现现状匹配，并在 commit message 注明"实现当前返回 X，测试按 X 断言；若未来改为 Y，需更新测试"
  - 测试命名与既有 `TEST_CASE "GPU_IOCTL_*"` 模式一致

## Risks / Trade-offs

- [Risk] 活跃派发表扩展可能冲击既有 36 项 handler → [Mitigation] 既有 ctest 全 PASS + spot-check 测试独立验证
- [Risk] handler 实现桥接到 sim 层（违反分层 ADR-023）→ [Mitigation] 走 `kfd_sim_bridge` 公开 API，不直接 `#include "sim/"`
- [Risk] 测试覆盖深度不足 → [Mitigation] 验收标准明确列出 CTest 断言，包含 sim 侧回调计数与入参可观察状态

## Migration Plan

1. 修改活跃派发表 + handler 实现
2. 编写/强化测试（standalone binary）
3. CTest registered: `cd build && ctest --output-on-failure`
4. ABI 头 `git diff` 必须为空（兼容性回归保护）

## Open Questions

- 无 — 此 change 是增量修复，独立后续提案处理遗留问题（DRM 表 / render node）
