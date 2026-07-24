# hal-event-signal: 技术设计

## Context
HAL Event Signal 是 ADR-062 定义的 ②→③ 桥接接口，当前 stub 返回 `-ENOSYS`。

## Goals
- Sim 端用 `std::condition_variable` + `std::mutex` 实现
- 真机端调用硬件 doorbell API
- 对 Linux kernel `dma_fence` 语义对齐

## Non-Goals
- 不实现硬件中断控制器模拟
- 不修改 fn-ptr 签名

## Decisions
- Sim 端: `std::condition_variable` + `std::mutex` (零依赖)
- 真机端: 通过 `linux_compat` 调用 real kernel API

## Risks
| Risk | Mitigation |
|------|------------|
| signal/wait 竞态 | test 覆盖并发场景 |