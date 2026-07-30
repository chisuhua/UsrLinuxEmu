## 1. MQD State Preempt/Resume Wiring

- [ ] 1.1 Implement `mqd_state_preempt()` body in `mqd_state.cpp`: ACTIVE → PREEMPTED, populate `PreemptContext { gpfifo_addr, current_index, total_entries, pending_fence_id }`
- [ ] 1.2 Implement `mqd_state_resume()` body in `mqd_state.cpp`: PREEMPTED → ACTIVE, restore gpfifo position
- [ ] 1.3 Implement edge case: IDLE channel preempt returns 0 no-op
- [ ] 1.4 Implement edge case: double-preempt on PREEMPTED returns 0 no-op
- [ ] 1.5 Implement edge case: resume on non-PREEMPTED returns -EINVAL
- [ ] 1.6 Verify mqd.h ABI unchanged (no new fields, no new exported symbols)

## 2. Per-Channel Pending Fence Table

- [ ] 2.1 Add `std::unordered_map<fence_id_t, SemHandle> pending_fences_` to `ChannelState` (drive-side only, not mqd.h)
- [ ] 2.2 Bind pending fence at batch submission time
- [ ] 2.3 Freeze pending fences on preempt: do not signal during preempt→resume gap
- [ ] 2.4 Rebind pending fences on resume: signal binds to resumed batch completion
- [ ] 2.5 Clean up pending fence entry after the fence signals on resumed batch completion (per 归档 spec: table SHALL have F1 correctly cleaned up)

## 3. Puller FSM Preempt Checkpoint Integration

- [ ] 3.1 Wire `mqd_state_preempt()` call into Puller FSM batch boundary checkpoint
- [ ] 3.2 Wire `mqd_state_resume()` call into Puller FSM post-high-priority-completion
- [ ] 3.3 Verify preempt checkpoint is skipped while `jump_stack_` non-empty (archived task 2.2 已实现行为): HIGH priority arrival during IB chain execution defers preempt until IB chain completes — jump_stack 不纳入 PreemptContext（抢占点 jump_stack 恒为空）
- [ ] 3.4 Verify resume after preempt at main-batch boundary (jump_stack empty) produces execution result byte-identical to a non-preempted control run
- [ ] 3.5 Save `ChannelSemaphoreState` (SEM_WAIT 挂起的 waiter/wait 条件) into channel context on preempt
- [ ] 3.6 Restore `ChannelSemaphoreState` on resume: semaphore wait state consistent (仍在同一 semaphore/value 上等待，signal 到达时正常唤醒)

## 4. Standalone Test: test_preemption_standalone

- [ ] 4.1 Write test for ACTIVE → PREEMPTED state transition
- [ ] 4.2 Write test for PREEMPTED → ACTIVE state transition
- [ ] 4.3 Write test for IDLE preempt no-op
- [ ] 4.4 Write test for double-preempt no-op
- [ ] 4.5 Write test for resume on non-PREEMPTED returns -EINVAL
- [ ] 4.6 Write test for fence NOT signaled during preempt→resume gap
- [ ] 4.7 Write test for fence signaled on resumed batch completion
- [ ] 4.8 Write test for IB safety: preempt deferred while `jump_stack_` non-empty; preempt at main-batch boundary resumes byte-identical to non-preempted control run
- [ ] 4.9 Write test for re-entrancy: preempt→resume→preempt again transitions correctly (ACTIVE→PREEMPTED→ACTIVE→PREEMPTED)
- [ ] 4.10 Write test for pending fence entry cleanup after fence signals on resumed batch completion
- [ ] 4.11 Write integration test (抢占+fence 组合): LOW channel preempted by HIGH; HIGH waits on LOW's fence; LOW resumes, completes, signals; HIGH continues
- [ ] 4.12 Write test for SEM_WAIT suspension: channel with SEM_WAIT-suspended entry is preempted; after resume, semaphore wait state consistent and wakes on signal

## 5. Sanitizer & Verification

- [ ] 5.1 Run `SANITIZER=asan-ubsan ./build.sh test` — all green
- [ ] 5.2 Run `SANITIZER=tsan ./build.sh test` — all green
- [ ] 5.3 Verify no new IOCTL numbers exposed (`grep GPU_IOCTL_*` before/after comparison)
- [ ] 5.4 Run `tools/docs-audit.sh --strict` — PASS

## 6. Documentation & ADR Sync

- [ ] 6.1 Update ADR-046 status: PROPOSED → Accepted
- [ ] 6.2 Add changelog entry to roadmap.md (Stage 4.5 preemption engine 完成)
- [ ] 6.3 Backfill ADR-045/047/050 status: PROPOSED → Accepted（三份 ADR 已由归档 change `stage4-4-gpu-cp-phase55` 完整实施，仅补登记状态），同步更新 `docs/00_adr/README.md` 索引表与状态分布表