# Tasks: Stage 4.3 Integration Wiring

> TDD 5-step: Write failing test → Verify fail → Implement → Verify pass → Commit

---

## Task 1: CHANNEL_SWITCH FSM

- [ ] 1.1 Write test: Puller FSM traverses CHANNEL_SWITCH state correctly
- [ ] 1.2 Add CHANNEL_SWITCH to Puller FSM enum + state transitions
- [ ] 1.3 Implement CHANNEL_SWITCH handler in runLoop()
- [ ] 1.4 Verify test passes + baseline Puller tests still green

## Task 2: ChannelManager Integration

- [ ] 2.1 Replace direct queue access in Puller with ChannelManager
- [ ] 2.2 Wire submitBatch() through ChannelManager per-channel tracking
- [ ] 2.3 Verify hyperqueue_multistream test passes after wiring

## Task 3: BAR0 HQD Registers

- [ ] 3.1 Add HQD register handlers in bar_sim.cpp (offset 0x4000+)
- [ ] 3.2 Wire writel(HQD_CTL_ACTIVE) → mqd_state_activate, readl(HQD_STATUS) → MQD.state
- [ ] 3.3 Verify mqd_state_standalone test passes after wiring

## Task 4: HAL Mock Interrupt Ops

- [ ] 4.1 Register interrupt_register + interrupt_raise_ex in hal_mock.cpp
- [ ] 4.2 Wire to sim/hardware/interrupt.cpp implementation
- [ ] 4.3 Verify cp_interrupt_standalone test passes via HAL path

## Task 5: kernel_workqueue Dispatch

- [ ] 5.1 Replace std::thread::detach() in interrupt.cpp with kernel_workqueue::enqueue()
- [ ] 5.2 Verify cp_interrupt_standalone test passes with workqueue dispatch

## Task 6: Puller DISPATCH Tick + Timestamp

- [ ] 6.1 Add g_sim_tick increment in Puller DISPATCH stage
- [ ] 6.2 Call timestamp_query_record for entries with non-zero ts_query handle
- [ ] 6.3 Verify timestamp_query_standalone test passes in Puller integration

## Task 7: Integration & Regression

- [ ] 7.1 Run full ctest regression — verify 0 new failures
- [ ] 7.2 Run docs-audit — verify 0 new warnings
- [ ] 7.3 Verify all 9 Stage 4.3 tests (5 new + 4 baseline) still PASS
- [ ] 7.4 Commit final integration