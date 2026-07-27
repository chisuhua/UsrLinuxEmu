# Proposal: Stage 4.3 Integration Wiring

## Summary

完成 Stage 4.3 的集成接线工作——将 5 个核心 sim 模块（method_codec、channel_manager、mqd_state、interrupt、timestamp_query）全部接入生产路径。

## Motivation

Stage 4.3 核心模块已通过 TDD 实现并验证（5 new + 4 baseline tests PASS），但以下集成路径尚未完成：

- ChannelManager 未接入 Puller FSM（CHANNEL_SWITCH 状态缺失）
- MQD/HQD 未接线到 BAR0 寄存器
- interrupt_register/raise_ex 未注册到 HAL mock
- interrupt_raise_ex 使用 std::thread 简易分发，未替换为 kernel_workqueue
- timestamp_query 未接入 Puller DISPATCH 阶段的 logical tick

## Scope

**In Scope** (~10 tasks):
1. Puller FSM: 添加 CHANNEL_SWITCH 状态，IDLE→CHANNEL_SWITCH→FETCH
2. ChannelManager: 接入 runLoop()，替代直接队列访问
3. BAR0 HQD: bar_sim.cpp 实现 HQD_ACTIVE/HQD_PREEMPT 寄存器
4. HAL mock: 注册 interrupt_register + interrupt_raise_ex 到 hal_mock.cpp
5. kernel_workqueue: 替换 interrupt.cpp 中的 std::thread 为 kernel_workqueue
6. Puller DISPATCH: 递增 g_sim_tick + 调用 timestamp_query_record
7. 全量回归: ctest + docs-audit + HAL 边界

**Out of Scope**:
- AQL/PM4 native encoding (ADR-052)
- Priority scheduling (ADR-045)
- Semaphore/Barrier (ADR-047)
- Preemption (ADR-046)

## Architecture Basis

All 5 ADRs already Accepted: ADR-042/044/048/054/057
Infrastructure already in place: ADR-069 (BAR/ioremap), ADR-060 (kernel_workqueue)

## Effort Estimate

~1 week for 10 integration tasks.