# Tasks: Stage 4.6 Green Context — Standalone Tests

> 承接 archive `openspec/changes/archive/2026-08-01-stage4-6-cp-phase7-green-context-pdl/tasks.md` §8 的 7 项未勾选残留（8.1-8.7）。
> Parent change: `2026-08-03-stage4-6-green-context-pdl-closeout`（A2 inline wrappers + A1 docs verify 已完成；本 change 承接 8.1-8.7 deferred 部分）。

---

## 1. File scaffold

- [ ] 1.1 创建 `tests/test_green_context_standalone.cpp`
- [ ] 1.2 file header comment（Stage 4.6 / 4.6.6 Green Context close-out scope）
- [ ] 1.3 Catch2 framework `#include <catch_amalgamated.hpp>`
- [ ] 1.4 includes: `shared/mqd.h`, `shared/gpu_types.h`, `sim/scheduler/channel_state.h`, `sim/hardware/channel_manager.h`, `gpu_hal.h`
- [ ] 1.5 helper 命名空间 + `make_green_mqd()` / `make_brown_mqd()` / `make_active_channel()` factory functions（仿 `test_preemption_standalone.cpp` 模式）
- [ ] 1.6 Catch2 tag prefix: `[green_context]` for all cases

## 2. Test case 8.2: `test_green_create_forces_low_priority`

- [ ] 2.1 Create MQD with `context_type = GREEN` + explicit `priority = HIGH`
- [ ] 2.2 Construct ChannelState from this MQD (sim path that mirrors the gpu_create_queue close path)
- [ ] 2.3 REQUIRE: `ChannelState.priority == LOW` (HIGH override rejected)
- [ ] 2.4 Edge: priority = LOW with GREEN → stays LOW (idempotent)
- [ ] 2.5 Edge: context_type = BROWN with priority = HIGH → priority stays HIGH (BROWN respects override)

## 3. Test case 8.3: `test_brown_preempts_running_green`

- [ ] 3.1 Setup: G (id=1, GREEN, LOW, ACTIVE) + B (id=2, BROWN, NORMAL, pending)
- [ ] 3.2 Trigger: `GlobalScheduler::dispatch_next()` after B enqueue
- [ ] 3.3 REQUIRE: G transitioned to `PREEMPTED` (channel state)
- [ ] 3.4 REQUIRE: B is now ACTIVE
- [ ] 3.5 REQUIRE: G's `PreemptContext::saved_gpfifo_position` reflects mid-batch state

## 4. Test case 8.4: `test_green_resumes_after_brown_completes`

- [ ] 4.1 Continuation of 3 setup: G PREEMPTED, B ACTIVE
- [ ] 4.2 Trigger: B.complete() → `dispatch_next()` loop
- [ ] 4.3 REQUIRE: G state → ACTIVE (resumed)
- [ ] 4.4 REQUIRE: G's gpfifo position equals saved position (resume from saved PC)
- [ ] 4.5 REQUIRE: No fence loss (per ADR-046 starvation protection + fence migration invariants)

## 5. Test case 8.5: `test_green_does_not_preempt_green`

- [ ] 5.1 Setup: 3 GREEN channels G1/G2/G3 (priority=LOW) submitted in order
- [ ] 5.2 Initial: G1 ACTIVE, G2/G3 pending
- [ ] 5.3 Trigger: G1.complete() → dispatch_next()
- [ ] 5.4 REQUIRE: G2 active (NOT preempt branch from G1)
- [ ] 5.5 REQUIRE: G1 state stayed ACTIVE until complete (no PREEMPTED state)
- [ ] 5.6 Verify G2 → G3 follows same pattern

## 6. Test case 8.6: `test_three_greens_fifo_order`

- [ ] 6.1 Setup: 3 GREEN channels all same priority, submitted T0 < T1 < T2 order
- [ ] 6.2 Sequence of complete() + dispatch_next() × 2
- [ ] 6.3 REQUIRE: dispatch order = T0 first, then T1, then T2 (FIFO strictly)
- [ ] 6.4 Negative: assert NO preempt jumps (we're testing FIFO not preempt)

## 7. Test case 8.7: `test_hal_green_context_create_destroy`

- [ ] 7.1 Setup: `gpu_hal_ops *hal = &kMockOps` where `kMockOps.hal_green_context_create/destroy` are wired to `hal_mock.cpp` impls
- [ ] 7.2 REQUIRE create: `hal_green_context_create(hal, tsg_id, &handle)` returns 0 + non-zero handle
- [ ] 7.3 REQUIRE destroy: `hal_green_context_destroy(hal, handle)` returns 0
- [ ] 7.4 Negative: double-destroy returns -EINVAL
- [ ] 7.5 Verify inline wrappers from change `2026-08-03-stage4-6-green-context-pdl-closeout`: same calling convention as fn-ptr, fn-ptr registered in mock

## 8. CMakeLists.txt registration

- [ ] 8.1 Append `add_executable(test_green_context_standalone ...)` block in `tests/CMakeLists.txt`
- [ ] 8.2 `target_link_libraries` → `gpu_sim` + `hal_mock` (per design.md)
- [ ] 8.3 `add_test(NAME test_green_context_standalone ...)` ctest entry
- [ ] 8.4 `set_tests_properties` WORKING_DIRECTORY = PROJECT_SOURCE_DIR
- [ ] 8.5 Verify cmake configure succeeds

## 9. Build + test run

- [ ] 9.1 `cd build && cmake --build . --target test_green_context_standalone` — 0 errors
- [ ] 9.2 `./build/bin/test_green_context_standalone` — all 6 cases PASS
- [ ] 9.3 `cd build && ctest -R green_context --output-on-failure` — PASS
- [ ] 9.4 Cross-file: HAL boundary `grep -rn '#include.*"sim/' plugins/gpu_driver/drv/` 输出仍空
- [ ] 9.5 docs-audit clean (no broken xref to test name)

## 10. Task tracker sync

- [ ] 10.1 `archive/2026-08-01-stage4-6-cp-phase7-green-context-pdl/tasks.md` §8 全部 [ ] → 7/7 翻 [x]
- [ ] 10.2 `openspec/changes/INDEX.md` 同步登记本 change
- [ ] 10.3 `openspec archive` 本 change → move 进 archive 并 sync main specs (`green-context-tests` 新 spec 或扩展既有 `green-context` spec)

---

## 估计工作量

| Phase | Tasks | 估计 |
|-------|-------|------|
| File scaffold (T1) | 1.1-1.6 | 30 min |
| Per-test cases (T2-T7) | 8.2-8.7 | 6 cases × 30-90 min = 3-9 hrs |
| CMakeLists (T8) | 8.1-8.5 | 20 min |
| Build + test (T9) | 9.1-9.5 | 30 min |
| Doc sync (T10) | 10.1-10.3 | 20 min |
| **Total** | | **5-11 hrs session** |

**Priority**:
- P_high: T2 (8.2) + T7 (8.7) — pure API/HAL test, ≤2 hrs
- P_med: T3-T5 (8.3-8.6) — scheduler interaction tests, ≥3 hrs
- P_low: T6 (8.6) — extension of T5, 30 min if T5 done
