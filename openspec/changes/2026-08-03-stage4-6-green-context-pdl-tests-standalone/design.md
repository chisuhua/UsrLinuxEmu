# Design: Stage 4.6 Green Context — Standalone Tests

## Scope of test file

`tests/test_green_context_standalone.cpp` is a **Catch2 standalone test binary** (linking only `gpu_sim` library, **no** plugin/drv dependency) covering 6 unit cases from 4.6 archive tasks.md §8.

**Layer coverage**:
- §1.6/1.7 (ContextType enum + immutability) — **NOT** in this file. Already covered by `tests/test_context_type_standalone.cpp`. Add only as cross-reference comments.
- §4.6 (HAL inline wrappers) — covered by test 8.7
- §调度交互 (ChannelManager + GlobalScheduler + ChannelState) — covered by tests 8.3-8.6

## Test scaffold pattern

参考 `tests/test_preemption_standalone.cpp` 的 helper pattern：

```cpp
#include <catch_amalgamated.hpp>
#include "shared/mqd.h"
#include "shared/gpu_types.h"
#include "sim/scheduler/channel_state.h"
#include "sim/hardware/channel_manager.h"

namespace {

MQD make_green_mqd(uint32_t entries = 64) { ... }
MQD make_brown_mqd(ChannelPriority prio, uint32_t entries = 64) { ... }

ChannelState make_active_channel(uint64_t channel_id, MQD mqd) { ... }

}  // namespace

TEST_CASE("...", "[green_context][sched]") { ... }
```

## Test cases — design specifics

### Test 8.2: `test_green_create_forces_low_priority`

**Setup**: 直接构造 MQD with `context_type = GREEN` and explicit `priority = HIGH`。
**Action**: 调用 (mock) `gpu_create_queue` 函数 / 直接构造 ChannelState。
**Assert**: `ChannelState::priority == LOW`，HIGH override 被忽略。

**注**：当前 4.6 archive 1.5 实施 `force LOW if context_type=GREEN`。Test 验证 runtime 行为。

### Test 8.3: `test_brown_preempts_running_green`

**Setup**:
- Channel G（id=1, context_type=GREEN, priority=LOW）运行中 → `ChannelState.state = ACTIVE`
- 提交 Channel B（id=2, context_type=BROWN, priority=NORMAL）→ pending 队列

**Action**: `GlobalScheduler::dispatch_next()`
**Assert**:
- G 转 `PREEMPTED`，`PreemptContext` 保存
- B 立即 dispatch 并 ACTIVE

### Test 8.4: `test_green_resumes_after_brown_completes`

**Setup**: 延续 8.3 — G PREEMPTED + B ACTIVE
**Action**: B.complete() → G.resume()
**Assert**:
- G state → ACTIVE
- `ChannelState.gpfifo_position` 连续（resume from saved PC）
- 与 B 完成前 G 的 entry_count 比较

### Test 8.5: `test_green_does_not_preempt_green`

**Setup**: 3 个 GREEN channels G1/G2/G3 全部 pending（FIFO submit order）
**Action**: 顺序 dispatch (`dispatch_next()` × 3)
**Assert**:
- G1 → G2 → G3 顺序（无 preempt 跳转）
- 没有 PREEMPTED state 出现
- G1→G2 不是 preempt 触发

### Test 8.6: `test_three_greens_fifo_order`

**Setup**: 与 8.5 类似但 3 个 GREEN channel 在 priority 相同下验证 FIFO
**Action + Assert**: 同 8.5 + size ordering checks

### Test 8.7: `test_hal_green_context_create_destroy`

**Setup**: `gpu_hal_ops *hal = &hal_mock_ops`（mock 已注册 GREEN fn-ptrs）
**Action**:
1. `rc = hal_green_context_create(hal, tsg_id, &handle)` → 0
2. `rc = hal_green_context_destroy(hal, handle)` → 0
3. **Negative**: `rc = hal_green_context_destroy(hal, handle)` 第二次 → -EINVAL（double-destroy）
4. **Inline wrapper**: 用本 change 实施的 inline wrapper（tasks.md §4.6 close-out），调用 fn-ptr 等价

**Assert**: rc values + handle uniqueness across iterations

## HAL mock setup

`hal_mock.cpp` 已包含 `hal_green_context_create/destroy` 实现（4.6 archive §4.3）。test file include 该 mock：

```cpp
extern "C" struct gpu_hal_ops *get_mock_hal_ops();
```

或 compile-time switch：

```cpp
static gpu_hal_ops kMockOps = []() {
  gpu_hal_ops ops{};
  // populate from hal_mock.cpp symbol
  // ...
  return ops;
}();
```

**Decided approach**: 直接 link hal_mock.cpp into test_green_context_standalone 二进制（与 plugin_gpu_driver build 类似）。use `hal_green_context_create(&kMockOps, ...)` 直接调用。

## File location

`tests/test_green_context_standalone.cpp`（与现有 30+ standalone test 同目录）

## CMakeLists.txt registration

```cmake
add_executable(test_green_context_standalone
    ${CMAKE_CURRENT_SOURCE_DIR}/test_green_context_standalone.cpp
)
target_link_libraries(test_green_context_standalone PRIVATE gpu_sim hal_mock)
target_include_directories(test_green_context_standalone PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/gpu_driver
    ${CMAKE_SOURCE_DIR}/include
)
add_test(NAME test_green_context_standalone COMMAND $<TARGET_FILE:test_green_context_standalone>)
set_tests_properties(test_green_context_standalone PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
```

（参考已有 `test_pdl_standalone` block 模式）
