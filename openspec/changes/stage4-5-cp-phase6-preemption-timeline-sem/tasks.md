## 1. Timeline Semaphore: Core Implementation

- [ ] 1.1 Create `plugins/gpu_driver/sim/semaphore/` directory
- [ ] 1.2 Define `TimelineSemaphore` class with `atomic<uint64_t> value_` + `std::mutex waiters_mutex_` + `std::deque<Waiter> waiters_` (FIFO)
- [ ] 1.3 Implement `sem_create(uint64_t initial, uint64_t* handle_out)` returning monotonic value
- [ ] 1.4 Implement `sem_signal(uint64_t handle, uint64_t value)` with strict-greater validation (return -EINVAL if `value <= current`)
- [ ] 1.5 Implement `sem_wait(uint64_t handle, SemaphoreCallback cb)` registering callback (non-blocking)
- [ ] 1.6 Implement `sem_query(uint64_t handle)` returning current value (acquire semantics)
- [ ] 1.7 Implement `sem_destroy(uint64_t handle)` waking all waiters with -ECANCELED + removing from registry
- [ ] 1.8 Add handle registry (`unordered_map<handle, TimelineSemaphore*>`) with mutex protection
- [ ] 1.9 Validate handle on every op (return -EINVAL for invalid/destroyed handle)

## 2. Timeline Semaphore: gpfifo_entry.timeline Consumption

- [ ] 2.1 Extend `gpu_gpfifo_entry` with `timeline { uint64_t signal_handle, signal_value, wait_handle, wait_value }` field (already designed in ADR-049)
- [ ] 2.2 In `HardwarePullerEmu::tick()`, after DISPATCH: iterate entry.timeline.signal_* → call `sem_signal(signal_handle, signal_value)`
- [ ] 2.3 In `HardwarePullerEmu::tick()`, before DISPATCH: check entry.timeline.wait_* → if not satisfied, register waiter callback that re-tries DISPATCH when fired
- [ ] 2.4 Wire waiter callback into Puller FSM (callback reschedules the pending entry)
- [ ] 2.5 Verify: signal value > 0 with no matching sem_create returns -EINVAL (negative path)

## 3. HAL Ops Extension (ADR-023)

- [ ] 3.1 Add to `struct gpu_hal_ops`: `int (*sem_create)(uint64_t initial, uint64_t* handle_out)`
- [ ] 3.2 Add: `int (*sem_signal)(uint64_t handle, uint64_t value)`
- [ ] 3.3 Add: `int (*sem_destroy)(uint64_t handle)`
- [ ] 3.4 Add: `int (*preempt_channel)(uint32_t channel_id)` (sets preempt_pending_ flag, non-blocking)
- [ ] 3.5 Implement all 4 ops in `plugins/gpu_driver/hal/hal_user.cpp` (delegate to sim)
- [ ] 3.6 Implement all 4 ops in `plugins/gpu_driver/hal/hal_mock.cpp` (in-memory mock for unit tests)
- [ ] 3.7 Verify: link-time NULL detection catches missing implementations

## 4. Per-Channel Pending Fence Table (Driver Side)

- [ ] 4.1 Add to `GpgpuDevice` (or new `PerChannelFenceTracker` class): `unordered_map<uint64_t, sem_handle> pending_fences_` per-channel
- [ ] 4.2 Implement `register_fence(channel_id, fence_id, sem_handle)` (called on `fence_create`)
- [ ] 4.3 Implement `unregister_fence(channel_id, fence_id)` (called by Puller complete callback OR `fence_destroy`)
- [ ] 4.4 Use `std::shared_mutex` (read = GPU_IOCTL_WAIT_FENCE, write = register/unregister)
- [ ] 4.5 Channel destroy → call `unregister_fence_all(channel_id)` waking all waiters with error
- [ ] 4.6 Verify: fence_id monotonic unique per channel (no collision)

## 5. Preemption: MQD State Preempt/Resume API Completion

> **前置依赖**：ADR-054 已交付 `mqd_state.cpp` 提供 `mqd_state_preempt()` / `mqd_state_resume()` 基础 API，本节扩展：
> - ChannelSemaphoreState 保存/恢复（与 ADR-054 rebase 协调）
> - saved_gpfifo_addr / index / entries 正确保存
> - 与 Puller FSM 边界检查点对接

- [ ] 5.1 Verify `mqd_state_preempt(channel_id, *saved_state)` already exists (ADR-054); extend to save `ChannelSemaphoreState` if missing
- [ ] 5.2 Verify `mqd_state_resume(channel_id, *saved_state)` already exists; extend to restore `ChannelSemaphoreState` if missing
- [ ] 5.3 Add `ChannelSemaphoreState` struct: `{ set<sem_handle> attached; vector<Waiter> pending_waits; }` to `channel_state.h`
- [ ] 5.4 Implement save/restore for `ChannelSemaphoreState` in `mqd_state.cpp`
- [ ] 5.5 Verify: PREEMPTED state cannot be preempted again (return 0, no-op)
- [ ] 5.6 Verify: IDLE state preempt returns 0 (no-op, no side effect)
- [ ] 5.7 Verify: state transitions ACTIVE → PREEMPTED → ACTIVE on resume (no leak)

## 6. Preemption: FSM Boundary Checkpoints

- [ ] 6.1 Add `std::atomic<bool> preempt_pending_[channel_id]` to `GlobalScheduler` (per-channel flag)
- [ ] 6.2 In `HardwarePullerEmu::tick()` BEFORE FETCH: check `preempt_pending_[current_channel]` → if set, call `mqd_state_preempt()` and switch channel to next ready
- [ ] 6.3 In `HardwarePullerEmu::tick()` AFTER DISPATCH: same check (allows preemption at end of entry)
- [ ] 6.4 Add `std::atomic<bool> jump_stack_active_[channel_id]` to detect IB chain state → block preempt when true
- [ ] 6.5 Implement `set_jump_stack(channel_id, bool)` called when entering/leaving IB nested mode
- [ ] 6.6 Verify: trigger and effect separation — `preempt_pending_` set immediately on HIGH batch submit, but save only at next boundary
- [ ] 6.7 Verify: IB jump_stack safety — preempt ignored while jump_stack active, executed after pop

## 7. Preemption: Fence Completion Binding (Critical Correctness)

- [ ] 7.1 Modify Puller complete callback: when batch completes, look up `pending_fences_[channel_id][fence_id]` → call `sem_signal(sem_handle, signal_value)` AND unregister
- [ ] 7.2 Verify: pre-empted batch's fence is NOT signaled before resume
- [ ] 7.3 Verify: after resume + batch completes, fence value == restored batch's committed signal_value
- [ ] 7.4 Verify: `pending_fences_[channel_id]` entry is cleared on unregister
- [ ] 7.5 Add test backdoor `bd_fence_read(channel_id, fence_id)` → returns current sem value (for assertions)
- [ ] 7.6 Verify three-step invariant: pre-preempt fence unchanged → resume completes → fence == new signal_value

## 8. Priority Scheduling Regression Validation

- [ ] 8.1 Run existing `test_priority_sched_standalone` — PASS (no regression)
- [ ] 8.2 Verify `kStarvationThreshold = 10` constant unchanged (else update test expectations)
- [ ] 8.3 Verify HIGH priority channel selected before LOW when both ready
- [ ] 8.4 Verify starvation protection: every 10 cycles, lowest priority entry forced through

## 9. ADR-040 Migration (Eliminate Dual Implementation)

> **⛔ 任务顺序约束**：
> - 必须先完成本节 9.1（识别测试调用点）+ 9.2（迁移测试）才能执行 9.3（删除旧路径）
> - 9.3 后既有 fence 测试应全部走 drv 路径通过 GPU_IOCTL_WAIT_FENCE 间接验证

- [ ] 9.1 `grep -rn sim_fence_id_signal tests/ plugins/gpu_driver/` — identify all callers
- [ ] 9.2 Migrate any test that calls `sim_fence_id_signal` directly → use drv `GPU_IOCTL_CREATE_FENCE` + `GPU_IOCTL_WAIT_FENCE` instead
- [ ] 9.3 Delete `plugins/gpu_driver/sim/fence_id_signal.cpp` (or rename to deprecated shim that forwards to sem_signal)
- [ ] 9.4 `grep sim/fence_id.* sim_fence_id_signal` — must return empty (verify no dual implementation)
- [ ] 9.5 Verify: existing `test_fence_*` tests pass via drv path
- [ ] 9.6 Add ADR-040 migration note: `sim_fence_id_signal` → timeline sem signal trigger source

## 10. Sim C-ABI Backdoor (ADR-057 D5)

- [ ] 10.1 Create `plugins/gpu_driver/sim/backdoor/` directory
- [ ] 10.2 Add `bd_preempt(uint32_t channel_id)` — sets preempt_pending_ flag (calls preempt_channel HAL op directly)
- [ ] 10.3 Add `bd_sem_create/signal/wait/query/destroy` — direct sem ops (bypass drv HAL chain)
- [ ] 10.4 Add `bd_fence_read(uint32_t channel_id, uint64_t fence_id)` — reads current fence value for assertions
- [ ] 10.5 Verify: `nm -D plugin_*.so | grep bd_` lists all backdoor symbols
- [ ] 10.6 Verify: `grep -rn backdoor plugins/gpu_driver/drv/` returns empty (drv layer MUST NOT call)
- [ ] 10.7 Verify: no `GPU_IOCTL_*` exposed for backdoor (backdoor not registered as ioctl handler)

## 11. Standalone Tests

- [ ] 11.1 Write `test_preemption_standalone`: HIGH preempt LOW → context save/restore round-trip
- [ ] 11.2 Write test: IDLE.preempt → no-op(0)
- [ ] 11.3 Write test: PREEMPTED.preempt → no-op(0)
- [ ] 11.4 Write test: trigger/effect separation — HIGH arrival marks immediately, save only at boundary
- [ ] 11.5 Write test: fence binding — pre-preempt fence unchanged, resume completes, fence == new value
- [ ] 11.6 Write test: SEM_WAIT suspended state survives preempt/resume (ChannelSemaphoreState)
- [ ] 11.7 Write test: IB jump_stack — preempt ignored during jump_stack, executed after pop
- [ ] 11.8 Write `test_timeline_semaphore_standalone`: sem_create → signal → query returns monotonic value
- [ ] 11.9 Write test: sem_signal with value <= current returns -EINVAL
- [ ] 11.10 Write test: sem_wait callback fires FIFO when condition met
- [ ] 11.11 Write test: sem_destroy with pending waiters wakes all with -ECANCELED
- [ ] 11.12 Write test: destroyed/invalid handle on signal/wait/query/double-destroy returns -EINVAL
- [ ] 11.13 Write test: gpfifo_entry.timeline signal consumed on batch completion
- [ ] 11.14 Write test: gpfifo_entry.timeline wait_value not met → entry suspended
- [ ] 11.15 Write test: fence_create/read thin wrapper (sem_create(0)/sem_query()>0)

## 12. Concurrent Stress Test

- [ ] 12.1 Write `test_concurrent_preempt`: N=100 preempt/resume cycles × concurrent submit threads
- [ ] 12.2 Verify: no deadlock (test completes within timeout)
- [ ] 12.3 Verify: no fence lost (every submitted fence eventually signals or is correctly canceled)
- [ ] 12.4 Verify: no state leak (channel destroy → all sem destroyed, no dangling pointers)

## 13. HAL Boundary Static Checks

- [ ] 13.1 `grep -rn '#include.*"sim/' plugins/gpu_driver/drv/` — must return empty
- [ ] 13.2 `grep -rn '#include.*"drv/' plugins/gpu_driver/sim/` — must return empty (boundary clean both directions)
- [ ] 13.3 `grep -rn backdoor plugins/gpu_driver/drv/` — must return empty
- [ ] 13.4 Verify `shared/gpu_ioctl.h` no new ioctl numbers (diff against main)

## 14. Sanitizer & Verification

- [ ] 14.1 Run `SANITIZER=asan-ubsan ./build.sh test` — all green
- [ ] 14.2 Run `SANITIZER=tsan ./build.sh test` — all green (concurrent code path critical)
- [ ] 14.3 Run full ctest — 0 failures (no regression vs baseline)
- [ ] 14.4 Performance baseline — no >10% regression on existing benchmarks

## 15. Documentation & ADR Sync

- [ ] 15.1 Update ADR-045 status: PROPOSED → Accepted (priority scheduling + starvation)
- [ ] 15.2 Update ADR-046 status: PROPOSED → Accepted (preemption + context switch)
- [ ] 15.3 Update ADR-047 status: PROPOSED → Accepted (semaphore acquire/release primitives)
- [ ] 15.4 Update ADR-049 status: PROPOSED → Accepted — **D1 修订**: wait blocking → waiter callback
- [ ] 15.5 Update ADR-040 migration note: sim_fence_id_signal → timeline sem trigger
- [ ] 15.6 Run `tools/docs-audit.sh --strict` — PASS
- [ ] 15.7 Add changelog entry to `roadmap.md` (Stage 4.5 Phase 6 + Timeline Sem 完成)
- [ ] 15.8 Add changelog entry to `docs/02_architecture/post-refactor-architecture.md` if needed

## 16. Plan-Done Gate

- [ ] 16.1 `git add openspec/changes/stage4-5-cp-phase6-preemption-timeline-sem/{proposal,design,tasks}.md specs/`
- [ ] 16.2 `git commit -m "feat(stage4.5): add preemption-timeline-sem change (proposal + design + tasks + specs)"`
- [ ] 16.3 Run `openspec validate stage4-5-cp-phase6-preemption-timeline-sem --strict` — pass
- [ ] 16.4 Update `.rddf/state/.plan-handoff.json` to add this change to `active_changes`