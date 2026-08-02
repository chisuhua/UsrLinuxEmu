# strengthen-semantic-assertions-for-destroy-va-space-and-query-queue

**优先级**: P1 | **来源**: System C IOCTL 端到端测试完备性审计（2026-08-02）
**阶段**: default | **分类**: core-test
**类型**: functional

## 架构依据

- **ADR-024（用户态队列提交）** — `QUERY_QUEUE` 是用户态查询 ring/queue 状态的核心入口；当前 `tests/test_va_space.cpp` 仅打印 `queue_type/queue_id/doorbell_offset`，无值断言，意味着回归（驱动返回错误数据）无法被 CI 拦截
- **ADR-017（GPFIFO 队列抽象）** — VA Space 是 Queue 的宿主（`gpu_va_space_args.page_size` + 队列 `va_space_handle`）；`DESTROY_VA_SPACE` 之后关联 queue 应失效，这是关键 invariant
- **现状（已审计验证）**：
  - `GPU_IOCTL_DESTROY_VA_SPACE` (0x31)：`tests/test_gpu_plugin.cpp` 清理路径中 `REQUIRE(result == 0)`；`tests/test_va_space.cpp` 同。**无任何"destroy 后失效"的下游断言**——如果 VA space 销毁是 no-op，CI 不会失败
  - `GPU_IOCTL_QUERY_QUEUE` (0x43)：`tests/test_va_space.cpp` 仅 `ret == 0` + `printf` 字段；`tests/test_stub_handlers_tier2_standalone.cpp` 走 driver 层直接调用，**无 plugin 路径的字段值断言**
- **rdd-workflow TDD 纪律** — 强语义断言是回归保护网；现有"ret==0"是必需非充分
- **AGENTS.md 风格** — Catch2 优先 `REQUIRE`（强约束）而非 `CHECK`（弱约束），用于 value assertion

## 范围

- **In Scope**:
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
- **Out Scope**:
  - `handleDestroyVASpace` / `handleQueryQueue` 实现修改（除非测试暴露 bug；本提案不预设）
  - 既有 stub 测试 `test_stub_handlers_tier2_standalone.cpp` 改动
  - 跨进程 queue state 可见性（不在现状范围内）
  - 新增 `gpu_queue.h` 公共字段（仅断言已有结构体）

## 关键场景

- **GIVEN** `CREATE_VA_SPACE` 成功获得 handle H，`CREATE_QUEUE` 关联 H 成功
  **WHEN** `DESTROY_VA_SPACE(H)` 返回 0
  **THEN** 后续对同一 H 再次 `DESTROY_VA_SPACE(H)` 返回 `< 0`（实现当前返回 `-ENOENT` 或 `-EINVAL`，由 CTest 适配）

- **GIVEN** `CREATE_VA_SPACE` 成功获得 H，`CREATE_QUEUE` 关联 H 成功
  **WHEN** `DESTROY_VA_SPACE(H)` 返回 0
  **THEN** 后续 `CREATE_QUEUE` 携带已销毁 H 返回 `< 0`（VA space 失效关键 invariant）

- **GIVEN** `DESTROY_VA_SPACE` 已被销毁的无效 H
  **WHEN** 重复 destroy 或后续操作
  **THEN** 不发生崩溃（DEFINITE-NO-CRASH 强约束，CI 必检）

- **GIVEN** plugin 已加载，`CREATE_VA_SPACE` + `CREATE_QUEUE` 携带 `queue_type=COMPUTE, ring_buffer_size=4096`
  **WHEN** `QUERY_QUEUE(0x43, &info)`
  **THEN** `ret == 0`；`info.queue_type == COMPUTE`；`info.queue_id != 0`；`info.doorbell_offset >= DOORBELL_ALLOC_BASE`；`info.ring_buffer_size == 4096`

- **GIVEN** 任意已注册 queue
  **WHEN** `QUERY_QUEUE` 携带不存在的 `queue_handle`
  **THEN** 返回 `< 0`

- **GIVEN** `CREATE_QUEUE` 后 + `DESTROY_QUEUE` 之前
  **WHEN** 多次 `QUERY_QUEUE` 调用
  **THEN** 返回的 `queue_id` / `doorbell_offset` / `ring_buffer_size` 保持稳定（同 queue handle 下确定）

## 技术约束

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

## 验收标准

- [ ] `tests/test_va_space.cpp` 新增 DESTROY_VA_SPACE 语义 TEST_CASE PASS：
  - destroy 同一 handle 第二次返回 `< 0`（实现当前返回 `-ENOENT` 或 `-EINVAL`）
  - destroy 后用同一 handle 创建 queue 返回 `< 0`
  - destroy 重复调用不崩溃
- [ ] `tests/test_gpu_plugin.cpp` 新增 `GPU_IOCTL_QUERY_QUEUE E2E semantic` TEST_CASE PASS：
  - `info.queue_type == 创建时设定值`
  - `info.queue_id != 0`
  - `info.doorbell_offset >= DOORBELL_ALLOC_BASE`
  - `info.ring_buffer_size == 创建时设定值`
  - 不存在的 queue_handle → `< 0`
  - 同一 queue 多次 QUERY 字段值稳定
- [ ] 既有 `tests/test_va_space.cpp` 全部既有 TEST_CASE 仍 PASS（不修改既有断言）
- [ ] 既有 `tests/test_gpu_plugin.cpp` 36 项既有 TEST_CASE 仍 PASS
- [ ] 既有 `test_stub_handlers_tier2_standalone` / `test_kfd_l1_l2_bridge_standalone` / `test_gpu_mempool_export_standalone` / `test_register_cb_ioctl_standalone`（若 P0 已合并）全 PASS，0 regression
- [ ] `handleDestroyVASpace` / `handleQueryQueue` 实现未变
- [ ] `plugins/gpu_driver/shared/gpu_ioctl.h` / `gpu_queue.h` ABI 头未变
- [ ] `docs/00_adr/` 中无新增 ADR
- [ ] 若测试暴露实现 bug：记录到 `docs/roadmap/` follow-up，不在本提案修复
