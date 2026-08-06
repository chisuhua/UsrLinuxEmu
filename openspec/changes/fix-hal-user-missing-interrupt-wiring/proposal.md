# fix-hal-user-missing-interrupt-wiring

## Why

`plugins/gpu_driver/hal/gpu_hal.h:90-96` 在 `struct gpu_hal_ops` 中声明两个 Stage 4.3 中断模型 fn-ptrs：

```c
int (*interrupt_register)(void *ctx, uint32_t vector,
                          void (*handler)(uint64_t user_data));
void (*interrupt_raise_ex)(void *ctx, uint32_t vector, uint64_t user_data);
```

`hal_mock.cpp:298-299` 在 `hal_mock_init` 中正确 wire：

```cpp
hal->interrupt_register = mock_interrupt_register;   // line 298
hal->interrupt_raise_ex = mock_interrupt_raise_ex;   // line 299
```

但 `hal_user.cpp`（609 行，`hal_user_init` 在 line 253-583）中**完全未 wire 这两个 fn-ptrs**。深度分析通过逐行搜索 `hal_user_init` 函数体确认无对应赋值行。

**当前后果**（active bug，非技术债务）：

- drv/ 调用 `hal->interrupt_register(...)` → deref nullptr fn-ptr → **SIGSEGV**
- 任何启用 MSI-X vector 注册的 GPU 驱动路径都会立即崩溃
- 测试套件中所有使用 mock HAL 的用例通过；**真实 HAL 路径未测试**

参考 mock 实现：
- `mock_interrupt_register` (`hal_mock.cpp:117-125`) — 简单存储 handler 数组
- `mock_interrupt_raise_ex` (`hal_mock.cpp:127-138`) — 调用 handler（spawn thread）

## What Changes

**In Scope**:

- `plugins/gpu_driver/hal/hal_user.cpp` — 在 `hal_user_init` (line 253-583) 中添加 2 个赋值行
- 新增 2 个 static 函数实现 `user_interrupt_register` + `user_interrupt_raise_ex`（参照 mock 实现）
- `struct hal_user_context` 可能需要新增 `interrupt_handlers` 数组字段（在 `hal_user.h` 中定义）
- 完整 ctest 130/130 PASS 验证

### 关键场景

- GIVEN `hal_user_init` 已执行 + drv/ 调用 `hal->interrupt_register(vector, handler)`
  - WHEN **修复前** THEN deref nullptr fn-ptr → SIGSEGV（segfault 路径）
  - WHEN **修复后** THEN handler 存储到 `hc->interrupt_handlers[vector]`，返回 0
- GIVEN drv/ 调用 `hal->interrupt_raise_ex(vector, payload)`
  - WHEN **修复前** THEN SIGSEGV
  - WHEN **修复后** THEN 找到对应 handler，触发调用（异步 thread 或 sync 模式待定）
- GIVEN 测试套件执行 WHEN 修复完成 THEN ctest 全部 PASS，无新增回归

**Out of Scope**:

- vector 路由逻辑的真实化（属于 `fix-hal-interrupt-vector-dispatch` proposal）
- 内核 workqueue 异步 dispatch（独立评估）
- MSI-X 硬件寄存器模拟

## Capabilities

- MUST 在 `hal_user_init` 中显式赋值 `hal->interrupt_register` 和 `hal->interrupt_raise_ex`
- MUST 参照 `mock_interrupt_register` 和 `mock_interrupt_raise_ex` 实现模式
- MUST NOT 修改 `struct gpu_hal_ops` 签名（HAL 接口不变）
- MUST NOT 修改 `hal_mock` 实现（保持测试行为不变）
- MUST 复用现有 `hc->interrupt_count` 原子计数器（如适用）
- SHOULD 用 4 个 vector slot（与 mock 一致：`if (vector >= 4) return -1`）
- SHOULD 对 `user_interrupt_raise_ex` 的 handler dispatch 简单同步即可（异步优化属独立 task）

## Impact

- MUST 在 `hal_user_init` 中显式赋值 `hal->interrupt_register` 和 `hal->interrupt_raise_ex`
- MUST 参照 `mock_interrupt_register` 和 `mock_interrupt_raise_ex` 实现模式
- MUST NOT 修改 `struct gpu_hal_ops` 签名（HAL 接口不变）
- MUST NOT 修改 `hal_mock` 实现（保持测试行为不变）
- MUST 复用现有 `hc->interrupt_count` 原子计数器（如适用）
- SHOULD 用 4 个 vector slot（与 mock 一致：`if (vector >= 4) return -1`）
- SHOULD 对 `user_interrupt_raise_ex` 的 handler dispatch 简单同步即可（异步优化属独立 task）

## Acceptance

- `hal_user.cpp` 中 `hal_user_init` 新增 2 个赋值行（搜索 `hal->interrupt_register` 和 `hal->interrupt_raise_ex` 应找到 1 个赋值 each）
- 新增 2 个 static 函数：`user_interrupt_register` + `user_interrupt_raise_ex`
- `hal_user_context` 结构可能新增字段（视实现选择）
- `make -j4` 编译通过，无 warning
- `ctest --output-on-failure` 全部 PASS（基线 130/130）
- 新增（或扩展现有）测试覆盖 user HAL 的 interrupt_register/raise_ex 路径
- 修改的代码行通过 `lsp_diagnostics` 检查，无 error/warning
- 端到端：drv/ 调用 `hal_interrupt_register()` 后调用 `hal_interrupt_raise_ex()`，handler 被触发

