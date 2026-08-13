# implement-hal-preempt-resume-semaphore

**优先级**: P1 | **来源**: 项目健康报告 §3.A + HAL 深度分析 Group 5+6
**阶段**: stage-4.5 | **分类**: core-impl
**类型**: functional (HAL 接口真实化)

## 架构依据

ADR-046 (Preemption) 和 ADR-049 (Timeline Semaphore) 规定了 7 个 HAL fn-ptrs：

- `hal_preempt(ctx, channel_id)` — 抢占当前执行 channel
- `hal_resume(ctx, channel_id)` — 恢复被抢占 channel
- `hal_sem_create(ctx, initial, *out_handle)` — 创建 timeline semaphore
- `hal_sem_signal(ctx, handle, value)` — 信号递增
- `hal_sem_wait(ctx, handle, expected, callback, user_data)` — 注册 waiter callback
- `hal_sem_query(ctx, handle, *out_val)` — 查询当前值
- `hal_sem_destroy(ctx, handle)` — 销毁 semaphore

**关键观察**：

`hal_user.cpp:14` 已经 `#include "../sim/semaphore_manager.h"`，表明 SemaphoreManager 已存在：

```cpp
#include "../sim/semaphore_manager.h"  // Stage 4.5: fence→sem migration
```

但当前 7 个 fn-ptrs 实现状态：

| fn-ptr | user 实现 (line) | mock 实现 (line) |
|--------|------------------|------------------|
| `hal_preempt` | no-op 返回 0 (293) | no-op 返回 0 (307) |
| `hal_resume` | no-op 返回 0 (294) | no-op 返回 0 (308) |
| `hal_sem_create` | 分配递增 handle 不存储 (295-299) | 分配递增 handle 不存储 (309-313) |
| `hal_sem_signal` | no-op 返回 0 (300) | no-op 返回 0 (314) |
| `hal_sem_wait` | no-op 返回 0 (301-302) | no-op 返回 0 (315-316) |
| `hal_sem_query` | 返回 0 永远 (303-306) | 返回 0 永远 (317-320) |
| `hal_sem_destroy` | no-op 返回 0 (307) | no-op 返回 0 (321) |

**完成度状态**：50% — `SemaphoreManager` 已存在但 HAL 接口未对接。`fence_create` (line 90) 已经 use SemaphoreManager 作为示范（参考实现）。

## 范围

- **In Scope**:
  - `plugins/gpu_driver/hal/hal_user.cpp` — 替换 7 个 lambda 为真实委托：
    - `hal_preempt` / `hal_resume`：调用 sim 调度器（如 `GlobalScheduler::preempt(channel)` + `resume(channel)`）
    - `hal_sem_create`：`SemaphoreManager::create(initial)` 真实存储
    - `hal_sem_signal`：调用 `SemaphoreManager::signal(handle, value)`
    - `hal_sem_wait`：调用 `SemaphoreManager::wait(handle, expected, callback, user_data)`
    - `hal_sem_query`：调用 `SemaphoreManager::query(handle)`
    - `hal_sem_destroy`：调用 `SemaphoreManager::destroy(handle)`
  - `struct hal_user_context` 可能需要新增 semaphore handle → instance 映射（与 fence 类似）
  - 完整 ctest 验证（基线 130/130）
  - 新增 semaphore/preemption 单元测试覆盖真实路径
- **Out Scope**:
  - Green Context / PDL HAL fn-ptrs（独立 proposal: `implement-hal-green-context-and-pdl`）
  - SemaphoreManager 自身的功能扩展（假设已完整 — 需在 change 实施前验证）
  - kernel workqueue 集成（独立 task）

## 关键场景

- GIVEN drv/ 调用 `hal_sem_create(0, &handle)`
  - WHEN **修复前** THEN 返回递增计数器（不存储），handle 后续操作无效
  - WHEN **修复后** THEN 返回真实 `SemaphoreManager` handle，存储在 `hc->sem_handles` 中
- GIVEN drv/ 注册 `hal_sem_wait(handle, expected, cb, ud)` 后 signal `handle` 至 ≥ expected
  - WHEN **修复前** THEN callback 永不触发
  - WHEN **修复后** THEN callback 异步触发，user_data 正确传递
- GIVEN drv/ 调用 `hal_preempt(channel_id)` 在 channel 执行中
  - WHEN **修复前** THEN 立即返回 0，无实际效果
  - WHEN **修复后** THEN 调度器标记 channel 为 PREEMPTED，返回 0
- GIVEN `hal_resume` 被调用
  - WHEN **修复前** THEN 立即返回 0
  - WHEN **修复后** THEN 调度器恢复 channel 执行

## 技术约束

- MUST 复用 `sim::SemaphoreManager`（已存在，per `hal_user.cpp:14` include）
- MUST 复用 sim 调度器的 preempt/resume API（需先验证存在性）
- MUST NOT 修改 `struct gpu_hal_ops` 签名
- MUST NOT 修改 `hal_mock` 实现（保持测试行为）
- MUST 保持 `hal_user_context` 线程安全（参考 fence_lock 模式）
- MUST 处理 handle 不存在场景（返回 `-EINVAL`）
- SHOULD 复用 `fence_create` 的 SemaphoreManager path 作为参考实现（`hal_user.cpp:90-109`）

## 验收标准

- `hal_user.cpp` 中 7 个 lambda 替换为真实实现
- `struct hal_user_context` 新增 semaphore 相关字段（如需要）
- 新增单元测试：
  - `test_sem_create_signal_query_destroy`
  - `test_sem_wait_callback_triggered`
  - `test_preempt_resume_basic`
- `make -j4` 编译通过，无 warning
- `ctest --output-on-failure` 全部 PASS（基线 130/130）
- 端到端验证：drv/ 完整 semaphore 生命周期创建→signal→wait→destroy 行为正确
- `lsp_diagnostics` 无 error
- Sanitizer run (ASan/UBSan) 全部 PASS