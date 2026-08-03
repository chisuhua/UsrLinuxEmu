# Design: Stage 4.6 Green Context + PDL — Closeout

## Scope confirmation

本 design 文档针对 closeout change `2026-08-03-stage4-6-green-context-pdl-closeout`，**不重新打开** 4.6 实施阶段（已 archived）。仅捕获 closeout 范围内的 inline HAL wrappers + verify items 评估。

## A2 - inline HAL wrappers

### 实现模式

参考既有 inline wrapper 模式（`gpu_hal.h` line 250-283，Stage 4.5 wrappers）：

```c
static inline int hal_X_Y(struct gpu_hal_ops *hal, ...) {
  return hal->hal_X_Y(hal->ctx, ...);
}
```

命名约定：inline wrapper 名 = fn-pointer 字段名（同 name，靠 signature 区分 — fn-pointer 是 `hal->hal_X_Y(...)` 通过成员访问，inline wrapper 是 `hal_X_Y(hal_ptr, ...)` 通过函数调用）。

### 4 个 wrappers

| Wrapper | Fn-pointer 字段 | 参数 |
|---------|---------------|------|
| `hal_green_context_create` | `hal->hal_green_context_create` (line 142) | `(struct gpu_hal_ops *hal, uint64_t tsg_id, uint64_t *out_handle)` |
| `hal_green_context_destroy` | `hal->hal_green_context_destroy` (line 148) | `(struct gpu_hal_ops *hal, uint64_t handle)` |
| `hal_pdl_launch` | `hal->hal_pdl_launch` (line 157) | `(struct gpu_hal_ops *hal, uint64_t kernel_addr, uint64_t kernargs_va, uint32_t grid_x, uint32_t block_x, uint64_t *out_signal_handle)` |
| `hal_pdl_signal_completion` | `hal->hal_pdl_signal_completion` (line 166) | `(struct gpu_hal_ops *hal, uint64_t signal_handle, uint64_t value)` |

### Section divider

新增 2 个 section divider comments：

```
/* ── Stage 4.6 inline wrapper（Green Context — ADR-056） ──────── */
/* ── Stage 4.6 inline wrapper（PDL — ADR-056） ───────────────── */
```

风格匹配文件 line 248/258 的 `Stage 4.5 inline wrapper` dividers。

## A1 - verify items 评估

### 4.6 archive tasks.md 残留 14 项分类

| 任务 | 类别 | 评估结论 |
|------|------|---------|
| 1.6 Verify GREEN ignores HIGH priority | 验证测试 | **functional 完整**（create_queue 处理 `context_type=GREEN` 时已 force LOW）。tests `test_context_type_standalone.cpp` (1 of 2 "new tests") 已注册并 PASS — 8.10 ✅ 反映此。8.x 测试套件名字错位（应是 `test_context_type_` 而非 `test_green_context_`）|
| 1.7 Verify context_type immutability at runtime | 验证测试 | **functional 完整**（`ContextType` 是 enum + struct 字段 mutation 检查）。涵盖在 `test_context_type_standalone` 中。|
| 2.4 Reuse mqd_state_resume() to restore GREEN after BROWN | 代码复用 | **functional 完整**（2.3 [x] 已实现 `mqd_state_preempt` reuse，2.6 [x] 验证 GREEN resume 恢复 gpfifo position — 隐含 mqd_state_resume 已通过测试链调用）。2.4 与 2.6 任务存在轻微 overlap，2.6 ✅ 已覆盖 2.4 close-out 意图。|
| 3.5 Verify starvation protection works for GREEN channels | 验证测试 | **functional 完整**（ADR-045 starvation protection 是全局的 `GlobalScheduler::dispatch_next()` 行为，与 context_type 正交 — GREEN 不豁免）。`test_priority_sched_standalone.cpp` 225 行已含 starvation test（4.4 archive 验证），新 tasks.md §3.5 应 flip 为 ✅ — 见 tasks.md 修正。|
| 4.6 inline HAL helper `hal_green_context_create/destroy` | **A2 范围** | ✅ **DONE in this change**（gpu_hal.h line 285-294 实施）|
| 7.6 inline HAL helper `hal_pdl_launch/signal_completion` | **A2 范围** | ✅ **DONE in this change**（gpu_hal.h line 302-310 实施）|
| 8.1-8.7 `test_green_context_standalone` 6 test cases | **P3-A3 范围** | ❌ **Deferred** — 文件实际不存在，已注册的是 `test_context_type_standalone`（独立 ADR-056 §8 任务的子集）|
| 9.1 `test_pdl_standalone` framework (含 case 内容) | 全部完成 | ✅ **DONE** — `tests/test_pdl_standalone.cpp` 已存在并通过 127/127 ctest PASS（合并 archive INDEX 证据）。tasks.md 9.1 [ ] 与代码现状冲突，tasks.md 修正时 flip 为 ✅ |

### 结论

- A1 范围（1.6/1.7/2.4/3.5/4.6/7.6/9.1）：**7 项 close out via docs check**（功能或测试已存在，tasks.md 勾选状态需更新）
- A2 范围（4.6/7.6 HAL helpers）：**2 项 close out via impl done in this change**
- P3-A3 范围（8.1-8.7 `test_green_context_standalone`）：**7 项 deferred** to `2026-08-03-stage4-6-green-context-pdl-tests-standalone`

## Non-Goals (carry-forward)

P3-A3 的 7 项 `test_green_context_standalone.cpp` 测试创建工作单独走 OpenSpec change `2026-08-03-stage4-6-green-context-pdl-tests-standalone`，本 closeout change **不实施**。理由：

1. P3-A3 是 test-creation 工作，6 个 unit case 各自需要独立设计（test case contract / mock setup / assertion strategy）
2. 已有 `test_context_type_standalone` 提供基础覆盖（ContextType enum + immutability），新增 `test_green_context_standalone` 重点是 BROWN↔GREEN 抢占调度 — 需要独立的方案设计
3. 拆分避免单 change 跨度过大（设计回合 + 评审 + 实现 + 测试验证）
