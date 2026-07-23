# hal-event-signal: HAL Event Signal 扩展完整实现

## Why

ADR-062 已 Accepted，`gpu_hal_ops` 已扩展到 14 fn-ptrs，`hal_user.cpp` 和 `hal_mock.cpp` 已提交了 Event Signal 相关的 `gpu_hal_event_signal_ops` stub。当前 stub 全部返回 `-ENOSYS`，导致 ② 在调用 fence/event 相关路径时无法通过 HAL 桥接完成事件同步。

## What Changes

- 在 `hal_mock.cpp` 中实现 event signal/wait/notify 的 sim 版本（基于条件变量 + mutex）
- 在 `hal_user.cpp` 中实现 event signal/wait/notify 的真机版本（调用真实硬件 doorbell/event API）
- 与 Linux kernel `dma_fence` API 对齐错误码约定
- 添加 Catch2 测试覆盖 signal/wait 并发场景

## Capabilities

### New Capabilities
- `hal-event-signal`: Event Signal HAL 完整实现 (sim + 真机)
- `hal-event-wait`: Event Wait 并发同步

### Modified Capabilities
<!-- No existing specs modified -->

## Impact

- `plugins/gpu_driver/hal/hal_mock.cpp` — 实现 sim 端 event ops
- `plugins/gpu_driver/hal/hal_user.cpp` — 实现真机端 event ops
- `tests/` — 新增 HAL event 测试