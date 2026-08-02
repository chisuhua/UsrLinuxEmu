## ADDED Requirements

### Requirement: End-to-end ioctl test discipline via /dev/gpgpu0 + plugin path

The `strengthen-semantic-assertions-for-destroy-va-space-and-query-queue` change MUST ensure the contract in proposal.md is observable from `/dev/gpgpu0` through the plugin path. It applies to functional changes in category `core-test`.

Architectural references:
- **ADR-024（用户态队列提交）** — `QUERY_QUEUE` 是用户态查询 ring/queue 状态的核心入口；当前 `tests/test_va_space.cpp` 仅打印 `queue_type/queue_id/doorbell_offset`，无值断言，意味着回归（驱动返回错误数据）无法被 CI 拦截
- **ADR-017（GPFIFO 队列抽象）** — VA Space 是 Queue 的宿主（`gpu_va_space_args.page_size` + 队列 `va_space_handle`）；`DESTROY_VA_SPACE` 之后关联 queue 应失效，这是关键 invariant


#### Scenario: 1

- **GIVEN** `CREATE_VA_SPACE` 成功获得 handle H，`CREATE_QUEUE` 关联 H 成功
- **WHEN** `DESTROY_VA_SPACE(H)` 返回 0
- **THEN** 后续对同一 H 再次 `DESTROY_VA_SPACE(H)` 返回 `< 0`（实现当前返回 `-ENOENT` 或 `-EINVAL`，由 CTest 适配）.

#### Scenario: 2

- **GIVEN** `CREATE_VA_SPACE` 成功获得 H，`CREATE_QUEUE` 关联 H 成功
- **WHEN** `DESTROY_VA_SPACE(H)` 返回 0
- **THEN** 后续 `CREATE_QUEUE` 携带已销毁 H 返回 `< 0`（VA space 失效关键 invariant）.

#### Scenario: 3

- **GIVEN** `DESTROY_VA_SPACE` 已被销毁的无效 H
- **WHEN** 重复 destroy 或后续操作
- **THEN** 不发生崩溃（DEFINITE-NO-CRASH 强约束，CI 必检）.

#### Scenario: 4

- **GIVEN** plugin 已加载，`CREATE_VA_SPACE` + `CREATE_QUEUE` 携带 `queue_type=COMPUTE, ring_buffer_size=4096`
- **WHEN** `QUERY_QUEUE(0x43, &info)`
- **THEN** `ret == 0`；`info.queue_type == COMPUTE`；`info.queue_id != 0`；`info.doorbell_offset >= DOORBELL_ALLOC_BASE`；`info.ring_buffer_size == 4096`.

#### Scenario: 5

- **GIVEN** 任意已注册 queue
- **WHEN** `QUERY_QUEUE` 携带不存在的 `queue_handle`
- **THEN** 返回 `< 0`.
