# complete-msi-x-vector-routing

## Why

[ADR-062](docs/00_adr/adr-062-hal-event-signal-extension.md) 定义 HAL event signal ops，包含：
- `interrupt_register(ctx, vector, handler)` — 注册 vector handler
- `interrupt_raise_ex(ctx, vector, user_data)` — 触发指定 vector

`plugins/gpu_driver/hal/gpu_hal.h` 中 `struct gpu_hal_ops` 声明这两个 fn-ptrs。

`hal_user.cpp:135-138` 实现 `user_interrupt_raise`：

```cpp
static void user_interrupt_raise(void *ctx, uint32_t vector) {
  auto *hc = static_cast<struct hal_user_context *>(ctx);
  hc->interrupt_count.fetch_add(1, std::memory_order_relaxed);
  // TODO(vector-dispatch): MSI-X vector routing for per-IH handlers
  (void)vector;
}
```

**当前后果**：
- `vector` 参数被忽略，所有中断都增加同一个 `interrupt_count`
- `hc->interrupt_handlers[vector]` 已经存储在 `user_interrupt_register`，但 `user_interrupt_raise` 未调用
- 真实 GPU 中每个 IH (Interrupt Handler) 有独立 MSI-X vector，独立 dispatch
- 当前实现：所有 vector 共用一个 handler（仅 `interrupt_handlers[0]` 被触发）

## What Changes

**In Scope**:

- `user_interrupt_raise()` 中添加 vector 查找 + 调用 `hc->interrupt_handlers[vector]`
- 添加 vector 边界检查（与 `user_interrupt_register` 一致：`if (vector >= 4) return -EINVAL`）
- 验证 `hc->interrupt_handlers[vector]` 是否非空（NULL handler 应跳过）
- 扩展 `test_hal_event_signal_standalone` 覆盖 4 个 vector 各自的 trigger

### 关键场景

- GIVEN `user_interrupt_register(vector=0, handler_a)` + `user_interrupt_register(vector=1, handler_b)`
  - WHEN `user_interrupt_raise(vector=0)` 触发
  - THEN handler_a 被调用，handler_b 不被调用
- GIVEN `user_interrupt_register(vector=0, handler_a)` only
  - WHEN `user_interrupt_raise(vector=2)` 触发
  - THEN handler_a 不被调用（vector=2 未注册），`interrupt_count` 增加
- GIVEN `user_interrupt_raise(vector=4)` 触发
  - WHEN vector=4 >= 4
  - THEN 早返回（`return -EINVAL`），与 `user_interrupt_register` 行为一致
- GIVEN 测试套件执行 WHEN 修复完成 THEN `test_hal_event_signal_standalone` 全部 PASS，新增 4 个 vector 各自的 test case

**Out of Scope**:

- MSI-X 硬件寄存器模拟（per-IH PCI 配置空间）
- 异步 dispatch（当前同步即可，异步属独立 task）
- Multi-IH thread pool（独立 task）

## Capabilities

- MUST 保持 `struct gpu_hal_ops` 签名不变
- MUST 参照 `mock_interrupt_raise_ex` (`hal_mock.cpp:127-138`) 的实现模式
- MUST NOT 修改 `struct hal_user_context` 字段（已有 `interrupt_handlers[vector]`）
- SHOULD 同步触发 handler（与 mock 一致）
- SHOULD 增加 `Logger::debug("interrupt_raise vector=%u handler=%p")` 日志

## Impact

- MUST 保持 `struct gpu_hal_ops` 签名不变
- MUST 参照 `mock_interrupt_raise_ex` (`hal_mock.cpp:127-138`) 的实现模式
- MUST NOT 修改 `struct hal_user_context` 字段（已有 `interrupt_handlers[vector]`）
- SHOULD 同步触发 handler（与 mock 一致）
- SHOULD 增加 `Logger::debug("interrupt_raise vector=%u handler=%p")` 日志

## Acceptance

- `hal_user.cpp:user_interrupt_raise()` 不再 `(void)vector`，实际调用 `hc->interrupt_handlers[vector]`
- vector 边界检查 `if (vector >= 4) return -EINVAL` 在 raise 路径存在
- `test_hal_event_signal_standalone` 扩展至少 4 个 test case 覆盖 vector 0/1/2/3
- `make -j4` 编译通过，无 warning
- `ctest --output-on-failure` 全部 PASS
- 修改的代码行通过 `lsp_diagnostics` 检查

