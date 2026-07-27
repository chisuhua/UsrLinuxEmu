# Tasks: Stage 4.3 Integration Wiring

> TDD 5-step: Write failing test → Verify fail → Implement → Verify pass → Commit

---

## Task 1: CHANNEL_SWITCH FSM

- [x] 1.1 Write test: Puller FSM traverses CHANNEL_SWITCH state correctly
- [x] 1.2 Add CHANNEL_SWITCH to Puller FSM enum + state transitions
- [x] 1.3 Implement CHANNEL_SWITCH handler in runLoop()
- [x] 1.4 Verify test passes + baseline Puller tests still green

## Task 2: ChannelManager Integration

- [x] 2.1 Replace direct queue access in Puller with ChannelManager
- [x] 2.2 Wire submitBatch() through ChannelManager per-channel tracking
- [x] 2.3 Verify hyperqueue_multistream test passes after wiring

## Task 3: BAR0 HQD Registers

- [x] 3.1 Add HQD register handlers in bar_sim.cpp (offset 0x4000+)
- [x] 3.2 Wire writel(HQD_CTL_ACTIVE) → mqd_state_activate, readl(HQD_STATUS) → MQD.state
- [x] 3.3 Verify mqd_state_standalone test passes after wiring

## Task 4: HAL Mock Interrupt Ops

- [x] 4.1 Register interrupt_register + interrupt_raise_ex in hal_mock.cpp
- [x] 4.2 Wire to sim/hardware/interrupt.cpp implementation
- [x] 4.3 Verify cp_interrupt_standalone test passes via HAL path

## Task 5: kernel_workqueue Dispatch

- [x] 5.1 Replace std::thread::detach() in interrupt.cpp with kernel_workqueue::enqueue()
- [x] 5.2 Verify cp_interrupt_standalone test passes with workqueue dispatch

## Task 6: Puller DISPATCH Tick + Timestamp

- [x] 6.1 Add g_sim_tick increment in Puller DISPATCH stage
- [x] 6.2 Call timestamp_query_record for entries with non-zero ts_query handle
- [x] 6.3 Verify timestamp_query_standalone test passes in Puller integration

## Task 7: Integration & Regression

- [x] 7.1 Run full ctest regression — verify 0 new failures
- [x] 7.2 Run docs-audit — verify 0 new warnings
- [x] 7.3 Verify all 9 Stage 4.3 tests (5 new + 4 baseline) still PASS
- [x] 7.4 Commit final integration