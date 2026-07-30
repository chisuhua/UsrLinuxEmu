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

## 3. Puller FSM Preempt Checkpoint Integration

- [ ] 3.1 Wire `mqd_state_preempt()` call into Puller FSM batch boundary checkpoint
- [ ] 3.2 Wire `mqd_state_resume()` call into Puller FSM post-high-priority-completion
- [ ] 3.3 Save IB jump_stack state in PreemptContext on preempt
- [ ] 3.4 Restore IB jump_stack state from PreemptContext on resume

## 4. Standalone Test: test_preemption_standalone

- [ ] 4.1 Write test for ACTIVE → PREEMPTED state transition
- [ ] 4.2 Write test for PREEMPTED → ACTIVE state transition
- [ ] 4.3 Write test for IDLE preempt no-op
- [ ] 4.4 Write test for double-preempt no-op
- [ ] 4.5 Write test for resume on non-PREEMPTED returns -EINVAL
- [ ] 4.6 Write test for fence NOT signaled during preempt→resume gap
- [ ] 4.7 Write test for fence signaled on resumed batch completion
- [ ] 4.8 Write test for IB jump_stack preservation across preempt→resume

## 5. Sanitizer & Verification

- [ ] 5.1 Run `SANITIZER=asan-ubsan ./build.sh test` — all green
- [ ] 5.2 Run `SANITIZER=tsan ./build.sh test` — all green
- [ ] 5.3 Verify no new IOCTL numbers exposed (`grep GPU_IOCTL_*` before/after comparison)
- [ ] 5.4 Run `tools/docs-audit.sh --strict` — PASS

## 6. Documentation & ADR Sync

- [ ] 6.1 Update ADR-046 status: PROPOSED → Accepted
- [ ] 6.2 Add changelog entry to roadmap.md (Stage 4.5 preemption engine 完成)