# Tasks: Stage 4.6 GPU CP Phase 7 — Green Context + PDL

## 1. Green Context Infrastructure

- [x] 1.1 Add `ContextType` enum to `plugins/gpu_driver/shared/gpu_types.h` (`BROWN=0`, `GREEN=1`)
- [x] 1.2 Add `context_type` field to `MQD` struct (default `BROWN`)
- [x] 1.3 Add `context_type` field to `ChannelState` (mirroring MQD for scheduler access)
- [ ] 1.4 Update `gpu_create_queue` API to accept `context_type` parameter (with default BROWN)
- [ ] 1.5 Implement GREEN priority override in queue creation (force LOW if context_type=GREEN)
- [ ] 1.6 Verify: GREEN creation ignores explicit HIGH priority parameter (`test_green_context_standalone`)
- [ ] 1.7 Verify: context_type immutability at runtime (mutation attempts rejected)

## 2. Dispatch-level Preemption Integration

- [ ] 2.1 Add preemption trigger in `GlobalScheduler::dispatch_next()` (BROWN pending + GREEN running → preempt GREEN)
- [ ] 2.2 Verify trigger fires only when BROWN priority >= GREEN priority + 1 tier (no preempt BROWN by same BROWN)
- [ ] 2.3 Reuse `mqd_state_preempt()` from ADR-046 (PreemptContext save/restore)
- [ ] 2.4 Reuse `mqd_state_resume()` to restore GREEN after BROWN completes
- [ ] 2.5 Verify: GREEN preempt saves correct gpfifo position (validated by test_green_context_standalone)
- [ ] 2.6 Verify: GREEN resume restores gpfifo position correctly (continues from saved index)

## 3. GREEN Channel Non-Preemption Rule

- [ ] 3.1 Add explicit check in `GlobalScheduler::dispatch_next()`: skip preempt if target is also GREEN
- [ ] 3.2 Add test scenario: 2 GREEN channels, second submission does NOT preempt first
- [ ] 3.3 Verify: GREEN channels follow normal priority/FIFO ordering
- [ ] 3.4 Add test scenario: 3 GREEN channels dispatched in FIFO order (no preemptions)
- [ ] 3.5 Verify starvation protection still works for GREEN channels (per ADR-045)

## 4. Green Context HAL Operations

- [ ] 4.1 Add `hal_green_context_create` fn-ptr to `struct gpu_hal_ops` (with `ctx`, `tsg_id`, `out_handle` parameters)
- [ ] 4.2 Add `hal_green_context_destroy` fn-ptr to `struct gpu_hal_ops` (with `ctx`, `handle` parameters)
- [ ] 4.3 Implement `hal_green_context_create` in `plugins/gpu_driver/hal/hal_mock.cpp` (mock GREEN context storage)
- [ ] 4.4 Implement `hal_green_context_create` in `plugins/gpu_driver/hal/hal_user.cpp` (real driver stub)
- [ ] 4.5 Implement `hal_green_context_destroy` in `hal_mock.cpp` + `hal_user.cpp`
- [ ] 4.6 Add inline helper `hal_green_context_create(hal, tsg, &out)` and `hal_green_context_destroy(hal, h)` to gpu_hal.h
- [ ] 4.7 Verify HAL boundary: `grep -rn '#include.*"sim/' plugins/gpu_driver/drv/` returns empty (ADR-023 enforce)

## 5. PDL Infrastructure

- [ ] 5.1 Add `GPU_OP_PDL_LAUNCH` enum value to `gpu_gpfifo_entry_type` in `plugins/gpu_driver/shared/gpu_types.h`
- [ ] 5.2 Add PDL payload struct: `{kernel_addr (u64), kernargs_gpu_va (u64), grid_x (u32), block_x (u32), signal_handle (u64), signal_value (u64)}`
- [ ] 5.3 Add `pdl_nest_counter_` field to `HardwarePullerEmu` (init 0, increment/decrement per launch)
- [ ] 5.4 Add `MAX_PDL_NEST=4` constant (mirroring `MAX_IB_NEST=4` from ADR-050)
- [ ] 5.5 Implement PDL guard: `sim_pdl_launch` returns `-E2BIG` if `pdl_nest_counter_ >= MAX_PDL_NEST`
- [ ] 5.6 Implement CPU-side rejection: `submitBatch` rejects direct `GPU_OP_PDL_LAUNCH` entries with `-EACCES` (PDL is internal only)

## 6. Puller PDL Dispatch

- [ ] 6.1 In `HardwarePullerEmu::fetchStage()`, recognize `GPU_OP_PDL_LAUNCH` entry
- [ ] 6.2 Construct child kernel dispatch entry from PDL payload (`kernel_addr`, `kernargs_va`, `grid/block`)
- [ ] 6.3 Construct `GPU_OP_SEM_RELEASE` entry using PDL `signal_handle` + `signal_value`
- [ ] 6.4 Append both entries to current batch tail (in-memory vector)
- [ ] 6.5 Increment `pdl_nest_counter_` after append
- [ ] 6.6 Decrement `pdl_nest_counter_` after child kernel completes (in `completeStage()`)
- [ ] 6.7 Verify: child kernel executes after PDL append (validated by test_pdl_standalone)
- [ ] 6.8 Verify: SEM_RELEASE fires after child completion (semaphore value incremented)

## 7. PDL HAL Operations

- [ ] 7.1 Add `hal_pdl_launch` fn-ptr to `struct gpu_hal_ops` (kernel_addr, kernargs_va, grid_x, block_x, out_signal_handle)
- [ ] 7.2 Add `hal_pdl_signal_completion` fn-ptr (signal_handle, value)
- [ ] 7.3 Implement `hal_pdl_launch` in `hal_mock.cpp` (delegate to sim_pdl_launch, increment nest counter)
- [ ] 7.4 Implement `hal_pdl_signal_completion` in `hal_mock.cpp` (delegate to sim timeline semaphore)
- [ ] 7.5 Implement both in `hal_user.cpp` (real driver stubs)
- [ ] 7.6 Add inline helpers `hal_pdl_launch(...)` and `hal_pdl_signal_completion(...)` to gpu_hal.h
- [ ] 7.7 Verify HAL boundary (ADR-023): no `sim/` includes in `drv/`

## 8. Test: test_green_context_standalone

- [ ] 8.1 Create `tests/test_green_context_standalone.cpp` with Catch2 framework
- [ ] 8.2 Implement `test_green_create_forces_low_priority`: GREEN context creation ignores HIGH priority override
- [ ] 8.3 Implement `test_brown_preempts_running_green`: BROWN pending → GREEN preempted → PreemptContext saved
- [ ] 8.4 Implement `test_green_resumes_after_brown_completes`: BROWN done → GREEN resumed from saved PC
- [ ] 8.5 Implement `test_green_does_not_preempt_green`: 2 GREEN channels, second does NOT preempt first
- [ ] 8.6 Implement `test_three_greens_fifo_order`: 3 GREEN channels dispatched in submission order
- [ ] 8.7 Implement `test_hal_green_context_create_destroy`: HAL ops round-trip + double-destroy error
- [ ] 8.8 Register in `tests/CMakeLists.txt` as standalone test
- [ ] 8.9 Verify build: `cmake --build build --target test_green_context_standalone`
- [ ] 8.10 Run: `./build/bin/test_green_context_standalone` — all PASS

## 9. Test: test_pdl_standalone

- [ ] 9.1 Create `tests/test_pdl_standalone.cpp` with Catch2 framework
- [ ] 9.2 Implement `test_pdl_basic_launch`: parent K launches child K via PDL → child executes → semaphore signaled
- [ ] 9.3 Implement `test_pdl_nested_chain_4`: K0→K1→K2→K3→K4 all execute in order
- [ ] 9.4 Implement `test_pdl_nest_overflow_returns_e2big`: 5th level PDL launch returns -E2BIG
- [ ] 9.5 Implement `test_pdl_invalid_kernel_addr`: unmapped kernel_addr returns -EFAULT
- [ ] 9.6 Implement `test_cpu_rejected_pdl_entry`: submitBatch rejects direct PDL entry with -EACCES
- [ ] 9.7 Implement `test_hal_pdl_launch_signal_completion`: HAL ops round-trip + signal_value verification
- [ ] 9.8 Implement `test_pdl_nest_counter_balanced`: 4 nested launches + 4 completions → nest back to 0
- [ ] 9.9 Register in `tests/CMakeLists.txt` as standalone test
- [ ] 9.10 Verify build: `cmake --build build --target test_pdl_standalone`
- [ ] 9.11 Run: `./build/bin/test_pdl_standalone` — all PASS

## 10. ADR and Roadmap Updates

- [ ] 10.1 Update `docs/00_adr/adr-056-green-context-pdl.md` status: 📋 PROPOSED → ✅ Accepted (with date)
- [ ] 10.2 Update `docs/roadmap/stage-4-bar-ioremap.md` § 4.6: ❌ 未开始 → ✅ 已归档
- [ ] 10.3 Update stage-4-bar-ioremap.md §4.6 关键交付 + 验收 (checkbox + archive records)
- [ ] 10.4 Update ADR status table: ADR-056 PROPOSED → Accepted (2026-08-XX)
- [ ] 10.5 Update "已归档 Changes 汇总" 表格: 4.6 行添加 stage4-6-cp-phase7-green-context-pdl
- [ ] 10.6 Update 变更记录 v2.3 (2026-08-XX)
- [ ] 10.7 Update risk table: Phase 4 全部完成, "Stage 4 整体验收" 进入可执行态
- [ ] 10.8 Update dependency graph (optional): 4.6 节点加 ✅

## 11. Integration and Validation

- [ ] 11.1 Run full test suite: `ctest --output-on-failure` — all 125+2=127 tests PASS (no regression)
- [ ] 11.2 Run sanitizer validation: `SANITIZER=asan-ubsan ./build.sh test` — green
- [ ] 11.3 Run TSan: `CC=clang CXX=clang++ SANITIZER=tsan ./build.sh test` — green
- [ ] 11.4 Verify HAL boundary enforce: `grep -rn '#include.*"sim/' plugins/gpu_driver/drv/` empty
- [ ] 11.5 Verify HAL fn-ptr count: `grep -cE '^\s*int \(\*' plugins/gpu_driver/hal/gpu_hal.h` shows 31 (was 29 + 2)
- [ ] 11.6 Update `docs/02_architecture/post-refactor-architecture.md` if HAL fn-ptr list is documented (29 → 31)
- [ ] 11.7 Run `tools/docs-audit.sh --strict` — 53/53 PASS (no new failures)
- [ ] 11.8 Update `openspec/changes/INDEX.md` total count after archive (was 23 completed, +1 = 24)
- [ ] 11.9 Sync `timeline-semaphore` spec if any changes affect it (verify via openspec validate)
- [ ] 11.10 Sync `preemption-engine-finish` spec if any changes affect MQD/context_type (verify via openspec validate)
