# Tasks: Stage 4.3 - GPU CP Phase 5

> **Method Encoding + HyperQueue + Interrupt + MQD/HQD + Profiling**
>
> TDD 5-step structure per task group: Write failing test → Verify fail → Implement → Verify pass → Commit.
>
> **Architecture basis**: [proposal.md](proposal.md) | [design.md](design.md)

---

## Task Group 1: Method Codec (TDD)

> ADR-042: UsrNative method packet encode/decode, NV4-style packed bitfield, two-layer (packet -> entry).

- [ ] 1.1 Write failing test: `test_pm4_encode_decode_standalone` - encode round-trip (method_addr, engine, data_count, data[] consistency after encode->decode)
- [ ] 1.2 Verify test fails: build test, confirm RED phase (compile error or assertion failure expected - no `method_codec.h` yet)
- [ ] 1.3 Implement `sim/hardware/method_codec.h` - `gpu_method_packet` struct (FAM) + `GpuEngineType` enum + `encode()`/`decode()` API declarations
- [ ] 1.4 Implement `sim/hardware/method_codec.cpp` - UsrNative encode (entry <- packet) + decode (entry -> packet) logic, reserved bits = 0
- [ ] 1.5 Wire encode into `GpgpuDevice::handlePushbufferSubmitBatch` - convert user entries via method_codec before ChannelManager submission
- [ ] 1.6 Wire decode into `HardwarePullerEmu` DECODE stage - dispatch via `method_addr` lookup table (OP_LAUNCH_KERNEL=0x100, NOTIFY_INTR=0x200)
- [ ] 1.7 Verify test passes: `test_pm4_encode_decode_standalone` GREEN + existing ctest baseline maintained
- [ ] 1.8 Commit: `feat(gpu): add UsrNative method codec (ADR-042)`

**验收标准**:
- `test_pm4_encode_decode_standalone` PASS: encode->decode round-trip preserves all fields (method_addr, engine, data_count, data[])
- Reserved bits in `gpu_gpfifo_entry` remain 0 (NI/ACQUIRE/PREDICATED untouched)
- No regression in `test_gpu_ringbuffer_standalone` or `test_hardware_puller_emu_standalone`
- HAL boundary: `drv/` does not `#include "sim/hardware/method_codec.h"` (encode is sim-internal)

---

## Task Group 2: Channel Manager (TDD)

> ADR-044: Round-Robin ChannelManager, MAX_CHANNELS=32, CHANNEL_SWITCH FSM state, per-channel fence tracking.

- [ ] 2.1 Write failing test: `test_hyperqueue_multistream_standalone` - multi-channel Round-Robin scheduling, no fence cross-contamination
- [ ] 2.2 Verify test fails: confirm RED phase - no `channel_manager.h`, compile error expected
- [ ] 2.3 Implement `sim/hardware/channel_manager.h` - `ChannelState` struct (channel_id, queue, gpfifo_addr, current_index, total_entries, batch_in_flight, pending_fence_id) + `ChannelManager` class API (registerChannel, submitBatch, nextReadyChannel, yieldChannel)
- [ ] 2.4 Implement `sim/hardware/channel_manager.cpp` - Round-Robin `nextReadyChannel()` with `TIME_SLICE_ENTRIES=1024` + mutex (ioctl write vs Puller read, ADR-044 §D2.2)
- [ ] 2.5 Add `CHANNEL_SWITCH` state to `HardwarePullerEmu` FSM - transition IDLE->CHANNEL_SWITCH->FETCH, COMPLETE->CHANNEL_SWITCH
- [ ] 2.6 Wire `ChannelManager` into `runLoop()` entry point - Puller fetches from `nextReadyChannel()` instead of direct queue
- [ ] 2.7 Verify test passes: `test_hyperqueue_multistream_standalone` GREEN - 2+ channels scheduled without fence ID leak
- [ ] 2.8 Commit: `feat(gpu): add Round-Robin ChannelManager + CHANNEL_SWITCH FSM (ADR-044)`

**验收标准**:
- `test_hyperqueue_multistream_standalone` PASS: 2 channels with interleaved batches, each fence completes on correct channel
- No fence cross-contamination: channel A fence_id != channel B fence_id at completion
- `MAX_CHANNELS=32` enforced (registerChannel beyond 32 returns -ENOSPC)
- Thread safety: mutex held during `nextReadyChannel()` snapshot (Issue #21 pattern)
- Existing `test_hardware_puller_emu_standalone` still PASS (FSM backward compat)

---

## Task Group 3: MQD/HQD State Management (TDD)

> ADR-054: MQD in shared/mqd.h (128 bytes packed), HQD BAR0 registers at offset 0x4000+channel*64, state transitions IDLE/ACTIVE/PREEMPTED.

- [ ] 3.1 Write failing test: `test_mqd_state_standalone` - MQD state transitions (IDLE->ACTIVE->PREEMPTED->ACTIVE->IDLE) + BAR0 HQD register read/write
- [ ] 3.2 Verify test fails: confirm RED phase - no `shared/mqd.h`, no `mqd_state.h`
- [ ] 3.3 Create `shared/mqd.h` - `MQD` struct (ring state, batch state, scheduling, preempt context, perf hooks) + `static_assert(sizeof(MQD)==128)` + `static_assert(sizeof(MQD)%8==0)` + `__attribute__((packed))`
- [ ] 3.4 Implement `sim/hardware/mqd_state.h` - `MqdState` class API: activate/deactivate/preempt/destroy + getMqd(channel_id) + state query
- [ ] 3.5 Implement `sim/hardware/mqd_state.cpp` - state machine per ADR-054 D4 table (IDLE/ACTIVE/PREEMPTED x activate/deactivate/preempt/destroy transitions) + MQD backing store (per-channel MQD array)
- [ ] 3.6 Wire BAR0 HQD registers in `sim/bar_sim.cpp` - offset 0x4000+channel*64: HQD_ACTIVE (writel triggers activate), HQD_PREEMPT (writel triggers preempt), HQD_WPTR (driver writes), HQD_RPTR (hardware writes, readl)
- [ ] 3.7 Wire MQD allocation via DMA coherent pool (ADR-073) - `dma_alloc_coherent` for MQD backing, VA returned to driver
- [ ] 3.8 Verify test passes: `test_mqd_state_standalone` GREEN - all state transitions valid, BAR0 read/write consistent
- [ ] 3.9 Commit: `feat(gpu): add MQD/HQD state management + BAR0 registers (ADR-054)`

**验收标准**:
- `test_mqd_state_standalone` PASS: IDLE->ACTIVE (activate), ACTIVE->PREEMPTED (preempt), PREEMPTED->ACTIVE (resume), ACTIVE->IDLE (deactivate)
- `static_assert(sizeof(MQD) == 128)` holds (packed layout)
- BAR0 HQD register access: `writel(0x4000+ch*64, 1)` -> state==ACTIVE; `readl(0x4000+ch*64+8)` returns rptr
- Invalid transition returns `-EINVAL` (e.g., IDLE->PREEMPTED without activate)
- MQD backing via `dma_alloc_coherent` (ADR-073), not raw malloc

---

## Task Group 4: Interrupt Model (TDD)

> ADR-048: InterruptVector enum, NOTIFY_INTR entry, async dispatch via kernel_workqueue (ADR-060), WaitQueue wake integration.

- [ ] 4.1 Write failing test: `test_cp_interrupt_standalone` - interrupt handler invocation via `interrupt_raise_ex`, handler receives correct vector + user_data
- [ ] 4.2 Verify test fails: confirm RED phase - no `interrupt.h`, no `interrupt_register`/`interrupt_raise_ex` in HAL
- [ ] 4.3 Add `interrupt_register` + `interrupt_raise_ex` to `gpu_hal.h` (ADR-023 append-don't-modify: add new ops, keep old `interrupt_raise` deprecated) + `InterruptVector` enum (FENCE_SIGNALED=0, NOTIFY_INTR=1, GPU_FAULT=2, ENGINE_HANG=3)
- [ ] 4.4 Implement `sim/hardware/interrupt.h` - `InterruptVector` + handler table (per-vector callback array) + `interrupt_handler_t` typedef
- [ ] 4.5 Implement `sim/hardware/interrupt.cpp` - `interrupt_raise_ex()` enqueues to `kernel_workqueue` (ADR-060), workqueue thread dispatches to registered handler
- [ ] 4.6 Wire NOTIFY_INTR handling in Puller DECODE - when `method_addr==NOTIFY_INTR`, call `interrupt_raise_ex(NOTIFY_INTR, user_data)`
- [ ] 4.7 Wire FENCE_SIGNALED via `interrupt_raise_ex` in `handleComplete()` - pass `fence_id` as `user_data`
- [ ] 4.8 Implement WaitQueue wake integration in ① kernel env - interrupt handler calls `WaitQueue::wake()` to unblock `GPU_IOCTL_WAIT_FENCE`
- [ ] 4.9 Verify test passes: `test_cp_interrupt_standalone` GREEN - handler invoked async, correct vector + user_data
- [ ] 4.10 Commit: `feat(gpu): add interrupt model with workqueue dispatch (ADR-048)`

**验收标准**:
- `test_cp_interrupt_standalone` PASS: `interrupt_register` + `interrupt_raise_ex(FENCE_SIGNALED, fence_id)` -> handler called with matching args
- Dispatch is async (via `kernel_workqueue`, not inline) - handler runs on workqueue thread
- NOTIFY_INTR in pushbuffer triggers `interrupt_raise_ex(NOTIFY_INTR, user_data)` during DECODE
- FENCE_SIGNALED raised in `handleComplete()` with `fence_id` as user_data
- `GPU_IOCTL_WAIT_FENCE` unblocks when corresponding FENCE_SIGNALED interrupt fires
- HAL ops: old `interrupt_raise` kept (deprecated), new `interrupt_register` + `interrupt_raise_ex` appended (ADR-023)
- `hal_mock.cpp` and `hal_user.cpp` updated with new op stubs/implementations

---

## Task Group 5: Profiling Hooks (TDD)

> ADR-057: logical tick counter + sim C-ABI timestamp query (create/record/resolve/destroy), test backdoor exposure, ioctl deferred to Phase 5.5.

- [ ] 5.1 Write failing test: `test_timestamp_query_standalone` - query create -> record (entry_index, tick) -> resolve -> destroy lifecycle
- [ ] 5.2 Verify test fails: confirm RED phase - no `timestamp_query.h`, no `g_sim_tick`
- [ ] 5.3 Implement `sim/hardware/timestamp_query.h` - `SimTimestampQuery` opaque handle + C-ABI API: `sim_timestamp_query_create/record/resolve/destroy` + `extern std::atomic<uint64_t> g_sim_tick`
- [ ] 5.4 Implement `sim/hardware/timestamp_query.cpp` - handle table (create returns handle), `record()` stores (entry_index, tick), `resolve()` returns recorded tick or -EAGAIN if not yet recorded, `destroy()` frees handle
- [ ] 5.5 Wire tick increment + query record in Puller DISPATCH stage - `g_sim_tick++` per DISPATCH; if `entry.ts_query != 0`, call `sim_timestamp_query_record(query, entry_index, g_sim_tick)`
- [ ] 5.6 Verify test passes: `test_timestamp_query_standalone` GREEN - create returns valid handle, record stores tick, resolve returns tick, destroy frees
- [ ] 5.7 Commit: `feat(gpu): add profiling hooks with logical tick + timestamp query (ADR-057)`

**验收标准**:
- `test_timestamp_query_standalone` PASS: create -> record(entry=5, tick=42) -> resolve returns 42 -> destroy
- `g_sim_tick` increments once per DISPATCH stage (atomic, thread-safe)
- `resolve()` before `record()` returns `-EAGAIN`
- `destroy()` on invalid handle returns `-EINVAL`
- C-ABI exposed (no C++ symbols in public API), test backdoor works
- No ioctl exposure (deferred to Phase 5.5 per ADR-057)

---

## Task Group 6: Integration & Regression

> Full regression + boundary verification + handoff metadata.

- [ ] 6.0 Register all 5 new tests in `tests/CMakeLists.txt` (add `test_pm4_encode_decode_standalone`, `test_hyperqueue_multistream_standalone`, `test_cp_interrupt_standalone`, `test_mqd_state_standalone`, `test_timestamp_query_standalone`)
- [ ] 6.1 Run full `ctest` regression - verify 110+ PASS (105 baseline + 5 new)
- [ ] 6.1a Explicitly verify existing baseline tests survive FSM extension:
  - `test_hardware_puller_emu_standalone` — Puller FSM (IDLE→FETCH→DECODE→...), must work after adding CHANNEL_SWITCH state
  - `test_gpu_ringbuffer_standalone` — ring buffer push/pop, must work after ChannelManager integration
  - `test_fence_id_lifecycle_standalone` — fence create/signal lifecycle, must work after per-channel fence tracking
  - `test_gpfifo_translator_standalone` — GPFIFO→LaunchParams, must work after method codec interposes DECODE
- [ ] 6.2 Run `tools/docs-audit.sh --strict` - verify 0 new warnings (pre-existing warnings OK, no new ones introduced)
- [ ] 6.3 Verify HAL boundary: `drv/` contains no `#include "sim/"` includes (grep check: `grep -r '#include.*sim/' plugins/gpu_driver/drv/` returns empty)
- [ ] 6.4 Verify 3 区分 boundary: `shared/` headers have no `sim/` or `drv/` includes; `sim/` has no `drv/` includes
- [ ] 6.5 Update `iteration.json` + `.plan-handoff.json` with Stage 4.3 completion status (all task groups done, tests green)
- [ ] 6.6 Final commit: `feat(gpu): complete Stage 4.3 CP Phase 5 (method+hyperqueue+interrupt+mqd+profiling)`

**验收标准**:
- `ctest` 110+ PASS (105 baseline + 5 new: pm4_encode_decode, hyperqueue_multistream, mqd_state, cp_interrupt, timestamp_query)
- All 4 baseline tests explicitly verified GREEN (hardware_puller_emu, gpu_ringbuffer, fence_id_lifecycle, gpfifo_translator)
- 0 new docs-audit warnings
- HAL boundary clean: `drv/` has zero `sim/` includes
- 3 区分 boundary clean: `shared/` independent, `sim/` -> `drv/` forbidden
- `iteration.json` updated: Stage 4.3 status = "completed"
- `.plan-handoff.json` updated with task completion summary

---

## Known Debt

> Pre-existing issues NOT caused by Stage 4.3 work. Listed here for visibility - do NOT fix in this change.

### 1. ADR-050 (Indirect Buffer) Scope Ambiguity

- **Issue**: Roadmap places Indirect Buffer (IB) chaining in Stage 4.4, but ADR-050 text says "Phase 5+".
- **4.3 Assumption**: IB chaining NOT needed for Phase 5 method encoding. Method codec handles flat pushbuffer only (no control flow).
- **Revisit Trigger**: If method encoding requires IB chaining (e.g., long kernels exceeding single GPFIFO entry), escalate to Stage 4.4 and implement ADR-050.
- **Risk**: Low - Phase 5 scope explicitly excludes control flow (design.md §1: "No control flow deferred to ADR-050").

### 2. Pre-existing `test_gpu_fence_return` Failure

- **Issue**: `test_gpu_fence_return` fails before Stage 4.3 work begins (pre-existing failure, not caused by this change).
- **Root Cause**: Suspected fence ID reuse race in `GlobalScheduler` (pre-4.3 codebase).
- **Action**: Do NOT fix in 4.3. Track separately. Stage 4.3 adds per-channel fence tracking (ChannelState::pending_fence_id) which may incidentally resolve, but fix is out of scope.
- **Verification**: Baseline `ctest` before 4.3 should show this failure; 4.3 must not introduce NEW failures.

### 3. HAL Ops Count Mismatch with Documentation

- **Issue**: AGENTS.md states `gpu_hal_ops` has "11 function pointers", but actual count in `gpu_hal.h` was 17 before Stage 4.3 (pre-existing drift from ADR-061, ADR-062, Stage 4.1 additions).
- **4.3 Impact**: ADR-023 "append-don't-modify" — we add `interrupt_register` + `interrupt_raise_ex` (2 new ops). Count becomes 17→19.
- **Fix Separately**: Documentation count sync should be done in a dedicated docs PR, not in 4.3.
- **Risk**: Low — append-only does not break existing HAL consumers.

---

## Summary

| Task Group | Tasks | Tests Added | ADRs |
|-----------|-------|-------------|------|
| 1. Method Codec | 8 | test_pm4_encode_decode_standalone | ADR-042 |
| 2. Channel Manager | 8 | test_hyperqueue_multistream_standalone | ADR-044 |
| 3. MQD/HQD State | 9 | test_mqd_state_standalone | ADR-054, ADR-073 |
| 4. Interrupt Model | 10 | test_cp_interrupt_standalone | ADR-048, ADR-060, ADR-023 |
| 5. Profiling Hooks | 7 | test_timestamp_query_standalone | ADR-057 |
| 6. Integration | 9 | (regression only) | - |
| **Total** | **51** | **5 new** | **6 ADRs** |

**Effort**: 2-3 weeks | **Split**: ① 15% | ② 25% | ③ 60%
