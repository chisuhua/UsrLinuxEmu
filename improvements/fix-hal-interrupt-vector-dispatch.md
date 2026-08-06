# fix-hal-interrupt-vector-dispatch

**优先级**: P2 | **来源**: 项目健康报告 §3.A + HAL 深度分析 Group 1 #10
**阶段**: stage-4.3 | **分类**: core-impl
**类型**: bug-fix (vector 参数被丢弃)

## 架构依据

`plugins/gpu_driver/hal/hal_user.cpp:136-141` 当前实现：

```cpp
static void user_interrupt_raise(void *ctx, uint32_t vector) {
  auto *hc = static_cast<struct hal_user_context *>(ctx);
  hc->interrupt_count.fetch_add(1, std::memory_order_relaxed);
  // TODO(vector-dispatch): MSI-X vector routing for per-IH handlers
  (void)vector;
}
```

**问题**：`vector` 参数被显式丢弃（`(void)vector`）。counter 工作但实际不触发任何 handler。

参考 `mock_interrupt_raise` (`hal_mock.cpp:111-115`):

```cpp
static void mock_interrupt_raise(void *ctx, uint32_t vector) {
  auto *state = static_cast<struct hal_mock_state *>(ctx);
  state->interrupt_raise_count++;
  state->last_interrupt_vector = vector;
}
```

mock 正确记录 vector（用于测试验证）。user 完全不利用 vector。

**依赖前置**：此 change 依赖 `fix-hal-user-missing-interrupt-wiring` 完成（需先实现 `interrupt_register` 才能有 handler 存储）。

## 范围

- **In Scope**:
  - `plugins/gpu_driver/hal/hal_user.cpp` `user_interrupt_raise` 真实按 vector 路由到 handler
  - 利用 Proposal 1 中实现的 `interrupt_handlers[vector]` 数组
  - handler 调用模式：可同步或异步（thread spawn）— **建议同步**（与 mock 一致，更易测试）
  - 完整 ctest PASS（基线 130/130）
  - 新增 vector dispatch 测试
- **Out Scope**:
  - 内核 workqueue 异步 dispatch（独立 task）
  - MSI-X 硬件寄存器模拟
  - 嵌套中断处理

## 关键场景

- GIVEN drv/ 注册 handler `vector=2`: `hc->interrupt_handlers[2] = my_handler`
- AND drv/ 调用 `hal_interrupt_raise(vector=2, payload=0xDEADBEEF)`
- WHEN **修复前** THEN counter+1 但 my_handler 不被调用
- WHEN **修复后** THEN my_handler(0xDEADBEEF) 被触发调用
- GIVEN vector 未注册 handler WHEN drv/ 调用 raise THEN counter+1，无 handler 调用（不 crash）
- GIVEN vector ≥ 4（超过 mock 设定的 4-slot 数组）WHEN drv/ 调用 raise THEN 静默忽略（与 mock 行为一致：`if (vector >= 4) return`）

## 技术约束

- MUST 复用 Proposal 1 中实现的 `hc->interrupt_handlers[]` 数组
- MUST 与 mock 的 vector 限制保持一致（`vector < 4`）
- MUST 复用 `hc->interrupt_count` 原子计数器
- MUST NOT 修改 `struct gpu_hal_ops` 签名
- MUST NOT 修改 mock 实现
- SHOULD 同步触发 handler（与 mock 一致，简化测试）
- SHOULD 移除 `// TODO(vector-dispatch)` 注释（修复完成）
- SHOULD 处理 nullptr handler 防御（数组初始化为 nullptr）

## 验收标准

- `user_interrupt_raise` 真正按 vector 路由
- TODO 注释移除
- 新增单元测试 `test_interrupt_vector_dispatch`：
  - 注册 handler → 触发对应 vector → handler 被调用 + payload 正确
  - 未注册 vector → 触发 → 无副作用（不 crash）
  - 越界 vector（≥ 4）→ 触发 → 静默忽略
- `make -j4` 编译通过
- `ctest --output-on-failure` 全部 PASS
- 端到端：drv/ 注册 + 触发 vector → handler payload 正确传递
- Sanitizer run PASS
- `lsp_diagnostics` 无 error