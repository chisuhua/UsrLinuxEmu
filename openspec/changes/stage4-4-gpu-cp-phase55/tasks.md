# Tasks: Stage 4.4 GPU CP Phase 5.5

## 1. Semaphore/Barrier — Puller FSM 扩展

- [ ] 1.1 Add `SEM_WAIT` and `SEM_RELEASE` entry types to GPFIFO entry enum in `gpu_queue.h`
- [ ] 1.2 Implement semaphore WAIT in Puller FETCH phase: `mem_read(va) >= value` check, move to pending queue
- [ ] 1.3 Implement pending queue in `ChannelState` (`std::deque<pending_entry>`)
- [ ] 1.4 Implement pending queue re-check loop in Puller's main dispatch cycle
- [ ] 1.5 Implement semaphore RELEASE in Puller COMPLETE phase: `mem_write(va, value)`
- [ ] 1.6 Implement `BARRIER_AND` entry: multi-stream counter, all-streams signal → release
- [ ] 1.7 Implement `BARRIER_OR` entry: first-stream signal → immediate release, ignore subsequent
- [ ] 1.8 Write `test_semaphore_barrier_standalone`: WAIT/RELEASE sequence + AND barrier + OR barrier
- [ ] 1.9 Verify: pending entry never blocks other channels; infinite WAIT doesn't crash

## 2. Priority Scheduling — GlobalScheduler

- [ ] 2.1 Add `ChannelPriority` enum (IDLE=0/LOW=1/NORMAL=2/HIGH=3/REALTIME=4) to `gpu_types.h`
- [ ] 2.2 Add `priority` field to `ChannelState` with default=NORMAL, set at queue creation
- [ ] 2.3 Refactor `GlobalScheduler` dispatch queue from `std::deque` to `std::multiset` sorted by `(priority, sequence_id)`
- [ ] 2.4 Implement starvation protection: 10-cycle counter forces at least 1 LOW priority dispatch
- [ ] 2.5 Implement priority inheritance: when REALTIME entry blocks on semaphore signalled by LOW, boost LOW to HIGH
- [ ] 2.6 Write `test_priority_sched_standalone`: 3 queues at different priorities, verify order
- [ ] 2.7 Write starvation test: sustained HIGH submission verifies LOW not indefinitely postponed

## 3. Indirect Buffer — Puller JUMP

- [ ] 3.1 Add `IB_JUMP` entry type to GPFIFO entry enum in `gpu_queue.h`
- [ ] 3.2 Add `gpu_ib_ref` struct (gpu_va, size, flags) for IB reference management
- [ ] 3.3 Add optional `ib_refs` field to `submitBatch` parameters
- [ ] 3.4 Implement Puller JUMP behavior in FETCH phase: save current PC, switch to target_gpu_va
- [ ] 3.5 Implement `continue_flag` support: chained JUMP returns to saved PC after completion
- [ ] 3.6 Implement IB reference lifecycle: auto-release on batch completion; validate target VA is mapped
- [ ] 3.7 Implement nested depth limit (`MAX_IB_NEST=4`), return `-E2BIG` on overflow
- [ ] 3.8 Write `test_indirect_buffer_standalone`: single JUMP + chained JUMP + illegal target + nest overflow
- [ ] 3.9 Verify: no memory leaks from IB reference allocation

## 4. Integration

- [ ] 4.1 Verify all 3 new tests pass in isolation
- [ ] 4.2 Run full test suite: `ctest --output-on-failure` — 0 regression
- [ ] 4.3 Verify ASan build: `SANITIZER=asan ./build.sh test` — 0 failures
