## 1. Priority Scheduling Upgrade (ADR-045)

- [x] 1.1 Extend `ChannelManager` to maintain 3-level priority queues (`std::array<std::queue<ChannelHandle>, 3>`) with `GPU_CHAN_PRI_HIGH`, `GPU_CHAN_PRI_NORMAL`, `GPU_CHAN_PRI_LOW`
- [x] 1.2 Implement `selectNextChannel()` with HIGH→NORMAL→LOW priority selection order
- [x] 1.3 Implement starvation counter (increment each cycle, force dequeue 1 LOW entry at `kStarvationThreshold=10`, reset after forced dequeue)
- [x] 1.4 Add channel priority assignment at creation time (`GPU_CHAN_PRI_NORMAL` default)
- [x] 1.5 Write `test_priority_sched_standalone` — priority ordering, starvation threshold, counter reset (verified via existing test)
- [x] 1.6 Verify existing `test_priority_sched_standalone` STILL PASSES

## 2. Preemption Engine (ADR-046)

- [x] 2.1 Add `pending_preempt_` flag to scheduler: set on HIGH priority batch arrival, clear after context save
- [x] 2.2 Implement preemption check point in Puller FSM at batch boundary (after DISPATCH / before FETCH) — skip if `jump_stack_` non-empty
- [x] 2.3 Implement trigger/effect separation: trigger marks flag immediately, context save only at boundary
- [x] 2.4 Wire `mqd_state_preempt()` into preemption flow: ACTIVE → PREEMPTED, save gpfifo_addr/index/entries
- [x] 2.5 Wire `mqd_state_resume()` into resume flow: PREEMPTED → ACTIVE, restore gpfifo position
- [x] 2.6 Handle preempt on IDLE channel (no-op return 0), double-preempt on PREEMPTED (no-op), resume on non-PREEMPTED (-EINVAL)
- [x] 2.7 Implement per-channel pending fence table (`std::unordered_map<fence_id_t, SemHandle>`) — drive-side only, no `mqd.h` ABI change
- [x] 2.8 Ensure fence NOT signaled during preempt→resume gap; fence signal binds to resumed batch completion
- [x] 2.9 Write `test_preemption_standalone` — all state transitions, fence semantics, IB jump_stack safety

## 3. Timeline Semaphore (ADR-049, D1 Revised)

- [x] 3.1 Implement `SemaphoreManager` with `sem_create(initial)`: allocate sem, return handle, initial value set
- [x] 3.2 Implement `sem_signal(handle, value)`: strict monotonic check (`new > current`), reject equal or lower with `-EINVAL`
- [x] 3.3 Implement `sem_wait(handle, expected, callback)`: register waiter callback (non-blocking), FIFO queue ordering
- [x] 3.4 Implement `sem_query(handle)`: return current value with acquire semantics
- [x] 3.5 Implement `sem_destroy(handle)`: wake registered waiters with error, reject double-destroy with `-EINVAL`
- [x] 3.6 Implement `gpfifo_entry.timeline` field: `{handle, signal_value, wait_value}` — batch completion auto-calls `sem_signal`, pre-dispatch check `sem_wait` if `wait_value > 0`
- [x] 3.7 Implement `fence_create` → `sem_create(0)` and `fence_read` → `sem_query() > 0` wrappers
- [x] 3.8 Implement cross-thread safety: `std::atomic<uint64_t>` for value, release/acquire semantics, unlock-before-invoke for waiter callbacks
- [x] 3.9 Channel destructor cleanup: destroy all associated semaphores
- [x] 3.10 Write `test_timeline_semaphore_standalone` — create/signal/query/wait/destroy, FIFO ordering, monotonic enforcement, error paths

## 4. ADR-040 Migration

- [x] 4.1 Migrate `sim_fence_id_signal(pending_fence_id_)` call path to call `sem_signal` as trigger source
- [x] 4.2 Remove dual implementation: verify `grep sim_fence_id_signal plugins/gpu_driver/sim/` returns no source definitions (only thin call-through if any)
- [x] 4.3 Update Puller completion callback to trigger `sem_signal(fence_sem, 1)` instead of `sim_fence_id_signal`

## 5. HAL Ops Extension

- [x] 5.1 Add `hal_preempt` fn-ptr to `struct gpu_hal_ops`
- [x] 5.2 Add `hal_sem_create`, `hal_sem_signal`, `hal_sem_wait`, `hal_sem_query`, `hal_sem_destroy` fn-ptrs to `struct gpu_hal_ops`
- [x] 5.3 Implement corresponding functions in `hal_user.cpp` / `hal_mock.cpp`
- [x] 5.4 Verify HAL boundary: `grep -rn '#include.*"sim/' plugins/gpu_driver/drv/` outputs empty

## 6. Sim C-ABI Backdoor (Test Entry, ADR-057 D5)

- [x] 6.1 Add backdoor symbols in plugin `.so` for preemption/timeline-sem testing
- [x] 6.2 Verify: backdoor NOT called from `drv/` layer, NOT exposed via `GPU_IOCTL_*`
- [x] 6.3 Verify: `nm` shows backdoor symbols in built `plugin_gpu_driver.so`

## 7. Concurrency & Stress Testing

- [x] 7.1 Implement concurrent pressure test: N preempt/resume cycles × concurrent submit
- [x] 7.2 Verify no deadlocks, no fence loss, no state leaks under TSan
- [ ] 7.3 Run `SANITIZER=asan-ubsan ./build.sh test` — all green
- [ ] 7.4 Run `SANITIZER=tsan ./build.sh test` — all green

## 8. Documentation & ADR Sync

- [x] 8.1 Update ADR-049: revise D1 wait semantics from blocking to waiter callback; status PROPOSED → ACCEPTED
- [x] 8.2 Update ADR-040: migration note — `sim_fence_id_signal` → timeline sem signal trigger source
- [ ] 8.3 Run `tools/docs-audit.sh --strict` — PASS
- [x] 8.4 Verify `shared/gpu_ioctl.h` no new ioctl numbers (diff/grep)
