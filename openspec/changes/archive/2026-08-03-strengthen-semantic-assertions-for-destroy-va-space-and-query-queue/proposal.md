## Why

System C IOCTL 端到端测试完备性审计（2026-08-02） 发现 `strengthen-semantic-assertions-for-destroy-va-space-and-query-queue` 的能力缺口 — 活跃派发表 + handler 与 ABI 头不同步，导致运行时 `ioctl()` 返回 `-EINVAL`。

**架构依据**:
- **ADR-024（用户态队列提交）** — `QUERY_QUEUE` 是用户态查询 ring/queue 状态的核心入口；当前 `tests/test_va_space.cpp` 仅打印 `queue_type/queue_id/doorbell_offset`，无值断言，意味着回归（驱动返回错误数据）无法被 CI 拦截
- **ADR-017（GPFIFO 队列抽象）** — VA Space 是 Queue 的宿主（`gpu_va_space_args.page_size` + 队列 `va_space_handle`）；`DESTROY_VA_SPACE` 之后关联 queue 应失效，这是关键 invariant
- **现状（已审计验证）**：
- `GPU_IOCTL_DESTROY_VA_SPACE` (0x31)：`tests/test_gpu_plugin.cpp` 清理路径中 `REQUIRE(result == 0)`；`tests/test_va_space.cpp` 同。**无任何"destroy 后失效"的下游断言**——如果 VA space 销毁是 no-op，CI 不会失败

**Why Now**: P1 优先级 — `strengthen-semantic-assertions-for-destroy-va-space-and-query-queue` 当前处于「声明存在但运行时不可达」的不一致状态，会直接挂起 KFD 集成 + E2E 测试扩展。修复该漂移是 IOCTL 测试完备性审计 (2026-08-02) 中识别的最高优先事项之一。

**现状摘录**:
- `GPU_IOCTL_DESTROY_VA_SPACE` (0x31)：`tests/test_gpu_plugin.cpp` 清理路径中 `REQUIRE(result == 0)`；`tests/test_va_space.cpp` 同。


## What Changes

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
- (Not in scope: `handleDestroyVASpace` / `handleQueryQueue` 实现修改（除非测试暴露 bug；本提案不预设）)
  - (Not in scope: 既有 stub 测试 `test_stub_handlers_tier2_standalone.cpp` 改动)
  - (Not in scope: 跨进程 queue state 可见性（不在现状范围内）)
  - (Not in scope: 新增 `gpu_queue.h` 公共字段（仅断言已有结构体）)

## Capabilities

### New Capabilities

- `ioctl-end-to-end-test-discipline`: 通过 `/dev/gpgpu0` 插件路径完成的端到端 ioctl 测试纪律

## Impact

- `tests/test_va_space.cpp`
- `tests/test_gpu_plugin.cpp`
