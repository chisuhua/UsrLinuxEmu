## 1. MQD State Preempt/Resume Audit (mqd_state.cpp 已实现)

> **重要**：`mqd_state_preempt/resume/activate/deactivate` 已完整实现于 `plugins/gpu_driver/sim/hardware/mqd_state.cpp:54-87`，按 ADR-054 §D4 工作。本 section 是**审计+对齐**，不是从零实现。
>
> **设计决策**：见 `design.md` §Decision 1（不扩展 mqd.h，复用 ADR-054 已有的 `saved_*` 字段）+ §Decision 4（边界处理与 ADR-054 D4 状态转移表对齐）+ §Decision 5（sim 端直接 struct 访问不违反 ADR-054 D3）+ §Decision 6（preempt checkpoint 先保存后切换）。

- [ ] 1.1 Audit `mqd_state_preempt()` (mqd_state.cpp:54-70)：验证 ACTIVE → PREEMPTED 时 `saved_gpfifo_addr/saved_index/saved_entries` 三字段正确填充（不引入 `pending_fence_id` 到 mqd.h，pending_fence 走 ChannelSemaphoreState，见 Decision 2 + Task 2.x）
- [ ] 1.2 Audit `mqd_state_resume()` (mqd_state.cpp:73-86)：验证 PREEMPTED → ACTIVE 时三字段正确恢复
- [ ] 1.3 Edge case: IDLE 通道 `mqd_state_preempt()` 返回 -EINVAL（per ADR-054 D4 "IDLE preempt = error"），test 4.3 改为断言 -EINVAL（非 0）
- [ ] 1.4 Edge case: PREEMPTED 通道 `mqd_state_preempt()` 返回 0（no-op, idempotent，per ADR-054 D4），test 4.4 保留断言 0
- [ ] 1.5 Edge case: non-PREEMPTED 通道 `mqd_state_resume()` 返回 -EINVAL（per ADR-054 D4），test 4.5 保留断言 -EINVAL
- [ ] 1.6 Verify mqd.h ABI unchanged（128-byte struct, packed, no new fields）— `git diff HEAD~1..HEAD -- plugins/gpu_driver/shared/mqd.h` 应为空

## 2. Per-Channel Pending Fence Table（归属 ChannelSemaphoreState）

> **设计决策**：见 `design.md` §Decision 2。**`pending_fences_` 归属 `ChannelSemaphoreState` 类**（`plugins/gpu_driver/sim/scheduler/channel_state.h`），不是调度侧 `struct ChannelState`，也不是 `mqd.h`。
>
> **理由**：fence 实现就是 timeline semaphore per ADR-049 D1（`fence_create → sem_create(0)`）；ChannelSemaphoreState 是 semaphore 状态视图。predication-aql §3.1 同样指向 `channel_state.{h,cpp}`，避免双向文件冲突。

- [ ] 2.1 Add `std::unordered_map<uint64_t /*fence_id*/, uint64_t /*sem_handle*>> pending_fences_` to `ChannelSemaphoreState` in `plugins/gpu_driver/sim/scheduler/channel_state.{h,cpp}`（不是调度侧 `struct ChannelState`，不是 `mqd.h`）
- [ ] 2.2 Bind pending fence at batch submission time：在 `submitBatch()` 中将 `(pending_fence_id, sem_handle)` 插入 `pending_fences_`（`sem_handle` 来自 `fence_create` 包装的 `sim_timeline_sem_create(0)` 返回值）
- [ ] 2.3 Freeze pending fences on preempt：在 `mqd_state_preempt()` 调用后，遍历 `pending_fences_` 标记 "frozen"（不删 entry，仅置位 flag）；Puller handleComplete 时检查 frozen flag 决定是否 signal
- [ ] 2.4 Rebind pending fences on resume：在 `mqd_state_resume()` 调用后，清除所有 frozen flag，sem_handle 与 Puller 的 `pending_fence_id_` rebind
- [ ] 2.5 Clean up pending fence entry after signal：在 handleComplete 调用 `sim_fence_id_signal(fence_id)` 成功后，从 `pending_fences_` 移除对应 entry（per 归档 spec "table SHALL have F1 correctly cleaned up"）

## 3. Puller FSM Preempt Checkpoint Integration（先保存后切换）

> **设计决策**：见 `design.md` §Decision 6。**现有 checkpoint 代码有缺陷**——`hardware_puller_emu.cpp:282-307` 只切换通道、不保存旧通道进度。本 section 补全 "save before switch" 逻辑。

- [ ] 3.1 Add `ChannelManager::getMqdForChannel(uint32_t channel_id)` 返回该 channel 的 MQD 指针（实现：从 `sim_bar0_readl()` 缓存的 `std::array<MQD*, MAX_CHANNELS>` 查表；ChannelManager 注册通道时缓存）
- [ ] 3.2 Modify preempt checkpoint in `hardware_puller_emu.cpp:282-307`：**先**调用 `mqd_state_preempt(channel_mgr_->getMqdForChannel(current_channel_id_))`，**然后**切换到新通道（替换 line 295-301 的直接覆盖逻辑）
- [ ] 3.3 Modify CHANNEL_SWITCH phase：当 `nextReadyChannel()` 返回 PREEMPTED 通道时，调用 `mqd_state_resume(channel_mgr_->getMqdForChannel(ch->channel_id))`，恢复 `current_gpfifo_addr_/current_index_/total_entries_`
- [ ] 3.4 Verify preempt checkpoint is skipped while `jump_stack_` non-empty (archived task 2.2 已实现行为)：HIGH priority arrival during IB chain execution defers preempt until IB chain completes — jump_stack 不纳入 PreemptContext（抢占点 jump_stack 恒为空）
- [ ] 3.5 Verify resume after preempt at main-batch boundary (jump_stack empty) produces execution result byte-identical to a non-preempted control run
- [ ] 3.6 Save `ChannelSemaphoreState` (SEM_WAIT 挂起的 waiter/wait 条件) into channel context on preempt：deep-copy `pending_entries_` + `barriers_` map（保留 waiting_entries vectors）到 backup 实例
- [ ] 3.7 Restore `ChannelSemaphoreState` on resume：`std::swap` 当前实例与 backup（仍在同一 semaphore/value 上等待，signal 到达时正常唤醒）

## 4. Standalone Test: test_preemption_standalone

- [ ] 4.1 Write test for ACTIVE → PREEMPTED state transition
- [ ] 4.2 Write test for PREEMPTED → ACTIVE state transition
- [ ] 4.3 Write test for IDLE preempt no-op
- [ ] 4.4 Write test for double-preempt no-op
- [ ] 4.5 Write test for resume on non-PREEMPTED returns -EINVAL
- [ ] 4.6 Write test for fence NOT signaled during preempt→resume gap
- [ ] 4.7 Write test for fence signaled on resumed batch completion
- [ ] 4.8 Write test for IB safety: preempt deferred while `jump_stack_` non-empty; preempt at main-batch boundary resumes byte-identical to non-preempted control run（添加 `compute_memory_diff(snapshot_before, snapshot_after, &out_diff_bytes)` helper 到 test framework）
- [ ] 4.9 Write test for re-entrancy: preempt→resume→preempt again transitions correctly (ACTIVE→PREEMPTED→ACTIVE→PREEMPTED)
- [ ] 4.10 Write test for pending fence entry cleanup after fence signals on resumed batch completion
- [ ] 4.11 Write integration test (抢占+fence 组合, backdoor scope): LOW channel preempted by HIGH; HIGH 的下一 batch 通过 `sim_backdoor_set_sem_handle(channel_id, low_completion_sem)` 引用 LOW 完成时的 sem handle；LOW resumes, completes, signals 该 sem；HIGH 继续 dispatch. **实现细节**：测试用 backdoor 直接绑定 HIGH 的 `waiting_semaphore_handle_` 到 LOW 的 completion sem，避免在本 change 引入 production 跨 channel 同步机制（多引擎同步另立项，见 design.md §Non-Goals）
- [ ] 4.12 Write test for SEM_WAIT suspension: channel with SEM_WAIT-suspended entry is preempted; after resume, semaphore wait state consistent and wakes on signal
- [ ] 4.13 Negative test: NULL MQD pointer in preempt/resume returns -EINVAL（覆盖 mqd_state_preempt/resume 各 1 个 case）
- [ ] 4.14 Negative test: Triple-preempt no-op (PREEMPTED → preempt×3 → still PREEMPTED with original saved_*)
- [ ] 4.15 Negative test: destroy(PREEMPTED) returns -EBUSY per ADR-054 D4（per channel_manager destroy 路径）
- [ ] 4.16 TSan stress: 100× preempt/resume cycles × concurrent submitBatch（替代归档 7.1，并发场景）
- [ ] 4.17 Negative test: resume with corrupted saved_index=0xFFFFFFFF returns -EINVAL (not ACTIVE with garbage)

## 5. Sanitizer & Verification

- [ ] 5.1 Run `SANITIZER=asan-ubsan ./build.sh test` — all green
- [ ] 5.2 Run `SANITIZER=tsan ./build.sh test` — all green
- [ ] 5.3 Verify no new IOCTL numbers exposed (`grep GPU_IOCTL_*` before/after comparison)
- [ ] 5.4 Run `tools/docs-audit.sh --strict` — PASS

## 6. Documentation & ADR Sync

- [ ] 6.1 Update ADR-046 status: PROPOSED → Accepted
- [ ] 6.2 Add changelog entry to roadmap.md (Stage 4.5 preemption engine 完成)
- [ ] 6.3 Backfill ADR-045/047/050 status: PROPOSED → Accepted（三份 ADR 已由归档 change `stage4-4-gpu-cp-phase55` 完整实施，仅补登记状态），同步更新 `docs/00_adr/README.md` 索引表与状态分布表