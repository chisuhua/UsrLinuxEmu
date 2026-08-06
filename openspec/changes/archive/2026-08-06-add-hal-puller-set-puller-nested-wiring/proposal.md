# add-hal-puller-set-puller-nested-wiring

## Why

`plugins/gpu_driver/hal/hal_user.cpp:548-560` 当前实现：

```cpp
hal->puller_set_puller = [](void* ctx, hal_puller_handle_t puller,
                            uint64_t sim_puller_handle) -> int {
  (void)sim_puller_handle;
  if (puller == 0) return -EINVAL;
  auto* hc = static_cast<struct hal_user_context*>(ctx);
  std::lock_guard<std::mutex> lock(hc->puller_lock);
  auto it = hc->pullers.find(puller);
  if (it == hc->pullers.end()) return -EINVAL;
  // HardwarePullerEmu does not expose a setPuller method; this fn-ptr
  // is reserved for future nested-puller wiring and is currently a no-op.
  (void)it;
  return 0;
};
```

**明确文档化的债务**：注释说明这是 "reserved for future nested-puller wiring"。

**当前后果**：drv/ 调用 `hal_puller_set_puller` 永远返回 0 但无效果。嵌套 puller 场景（如一个 HardwarePullerEmu 监听多个 sim_puller 实例）无法实施。

**修复路径**：

1. 在 `plugins/gpu_driver/sim/hardware/hardware_puller_emu.h` 中**最小化扩展** `HardwarePullerEmu` 类，新增 `setSimPuller(uint64_t sim_puller_handle)` 方法
2. 实施：内部存储 `sim_puller_handle_` 字段（`std::atomic<uint64_t>` 保持线程安全）
3. HAL lambda 委托：`it->second->setSimPuller(sim_puller_handle)`

注意：`HardwarePullerEmu` 是 C++ 类，HAL 端通过 `hal_puller_handle_t`（uint64_t opaque）持有 `shared_ptr<HardwarePullerEmu>`，转换无类型问题。

## What Changes

**In Scope**:

- `plugins/gpu_driver/sim/hardware/hardware_puller_emu.h` 新增 `setSimPuller(uint64_t)` 方法声明
- `plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp` 实现该方法（存储字段 + 可选 callback）
- `plugins/gpu_driver/hal/hal_user.cpp` `puller_set_puller` lambda 从 no-op 升级为真实委托
- 完整 ctest PASS（基线 130/130）
- 新增嵌套 puller 单元测试（最小：set 后 verify 字段被设置）

### 关键场景

- GIVEN drv/ 调用 `hal_puller_set_puller(puller=valid_handle, sim_puller_handle=42)`
  - WHEN **修复前** THEN 立即返回 0，无内部状态变化
  - WHEN **修复后** THEN `HardwarePullerEmu` 实例存储 `sim_puller_handle=42`，返回 0
- GIVEN drv/ 调用无效 `puller=0` 或不存在 handle
  - WHEN THEN 返回 -EINVAL（已正确）
- GIVEN drv/ 在 set 之后查询（需要新增 query 方法 — out of scope）

**Out of Scope**:

- 嵌套 puller 实际使用场景（仅 API 完备化，不实现多 sim_puller 监听）
- `HardwarePullerEmu` 类其他方法扩展
- 性能优化（最小化扩展原则）

## Capabilities

- MUST 最小化扩展 `HardwarePullerEmu`（仅 1 个 setter 方法 + 1 个存储字段）
- MUST 使用线程安全存储（`std::atomic<uint64_t>` 或 mutex 保护）
- MUST NOT 修改 `struct gpu_hal_ops` 签名
- MUST NOT 修改 mock 实现（mock 仍 no-op）
- MUST 保持现有 `HardwarePullerEmu` 行为不变（不破坏 puller_create/destroy/register_queue 等）
- SHOULD 记录 `setSimPuller` 调用日志（如有 logger）
- SHOULD 在 change 描述中明确"仅 API 完备化，不承诺嵌套 puller 实际工作场景"

## Impact

- MUST 最小化扩展 `HardwarePullerEmu`（仅 1 个 setter 方法 + 1 个存储字段）
- MUST 使用线程安全存储（`std::atomic<uint64_t>` 或 mutex 保护）
- MUST NOT 修改 `struct gpu_hal_ops` 签名
- MUST NOT 修改 mock 实现（mock 仍 no-op）
- MUST 保持现有 `HardwarePullerEmu` 行为不变（不破坏 puller_create/destroy/register_queue 等）
- SHOULD 记录 `setSimPuller` 调用日志（如有 logger）
- SHOULD 在 change 描述中明确"仅 API 完备化，不承诺嵌套 puller 实际工作场景"

## Acceptance

- `hardware_puller_emu.h` 新增 `setSimPuller(uint64_t)` 方法
- `hardware_puller_emu.cpp` 实现该方法（最小：1 个原子字段赋值）
- `hal_user.cpp` 中 `puller_set_puller` lambda 真实委托（移除注释 "currently a no-op"）
- 新增单元测试 `test_puller_set_puller`：
  - 成功路径：set 后 verify 内部字段被设置（可通过新增 getter 或保留内部状态可见性测试）
  - 错误路径：puller=0 → -EINVAL
  - 错误路径：不存在的 puller handle → -EINVAL
- `make -j4` 编译通过
- `ctest --output-on-failure` 全部 PASS（基线 130/130）
- Sanitizer run (TSan) 验证原子操作无 race
- `lsp_diagnostics` 无 error
- 不破坏现有 puller 相关测试

