# Proposal: Stage 4.6 Green Context — Standalone Tests

> **OpenSpec change**: 2026-08-03-stage4-6-green-context-pdl-tests-standalone
> **Trigger**: stage4-6-green-context-pdl-closeout `P3-A3 范围 8.1-8.7 deferred to sibling`
> **Owner**: UsrLinuxEmu Architecture Team
> **关联 ADR**: ADR-056（Green Context & PDL）
> **关联 archive**: openspec/changes/archive/2026-08-01-stage4-6-cp-phase7-green-context-pdl/tasks.md §8 (8.1-8.7)
> **关联 change**: openspec/changes/2026-08-03-stage4-6-green-context-pdl-closeout/

---

## Why

Stage 4.6 (`stage4-6-cp-phase7-green-context-pdl`) 实施了 Green Context 与 PDL 逻辑，但其 tasks.md §8 列出的 6 个 unit test 实际**未实施**（`tests/test_green_context_standalone.cpp` 文件不存在；已存在的 `test_context_type_standalone.cpp` 涵盖 §1 的 enum/MQD 字段层验证，但**不**涵盖调度层 interaction）。

Closeout change（`2026-08-03-stage4-6-green-context-pdl-closeout`）的 §P3-A3 deferred 段确认 7 项 8.1-8.7 未在本 session 内实施。本 change 实施该 7 项。

## What Changes

| 范围 | 内容 | 范围 |
|------|------|------|
| `tests/test_green_context_standalone.cpp` | 新文件 | Catch2 framework + 6 test cases（8.2-8.7）+ CMakeLists.txt 注册 |
| `tests/CMakeLists.txt` | +1 `add_executable` block | 编译 + ctest 注册 |
| `tasks.md` | 7/7 全部 [x] | 测试 PASS 验证后 flip |

**Non-scope**（不在本 change）：
- ❌ 修改 GPU sim/drv HAL 实现（功能已实现，change 是 test-only）
- ❌ 修改 `test_context_type_standalone.cpp`（已 PASS，文件命名错位历史遗留）
- ❌ 修改 4.6 archive 实施（已归档不可重写）

## Test Cases

对应 tasks.md §8 7 项：

| Task | Test case | Coverage type |
|------|-----------|---------------|
| 8.1 | (file creation) | Catch2 framework scaffold |
| 8.2 | `test_green_create_forces_low_priority` | API contract：context_type=GREEN 强制 priority=LOW |
| 8.3 | `test_brown_preempts_running_green` | Scheduler interaction：BROWN 抢断 GREEN |
| 8.4 | `test_green_resumes_after_brown_completes` | State machine：BROWN 完成后 GREEN resume |
| 8.5 | `test_green_does_not_preempt_green` | Scheduler rule：GREEN↛GREEN 抢占 |
| 8.6 | `test_three_greens_fifo_order` | Ordering：3 GREEN channels 按提交顺序 dispatch |
| 8.7 | `test_hal_green_context_create_destroy` | HAL round-trip：create/destroy + double-destroy error |

## Acceptance Criteria

- [ ] `tests/test_green_context_standalone.cpp` 创建，Catch2 framework 完整
- [ ] 6 个 test case（8.2-8.7）实现，可独立执行
- [ ] `tests/CMakeLists.txt` 注册 `test_green_context_standalone` executable + ctest entry
- [ ] `./build/bin/test_green_context_standalone` 运行：全部 PASS
- [ ] `cd build && ctest -R green_context` PASS
- [ ] drv/ 层 HAL 边界 `grep -rn '#include.*"sim/' plugins/gpu_driver/drv/` 输出仍空（test file 不破坏 HAL 边界）
- [ ] `tasks.md` 8.1-8.7 全部 flip 为 [x]
- [ ] `openspec validate 2026-08-03-stage4-6-green-context-pdl-tests-standalone` ✅

## Risks

| 风险 | 概率 | 缓解 |
|------|------|------|
| Test 8.3/8.4 依赖 `ChannelManager::dispatch_next()` 与真实 scheduler 协作，引入 fragile mock setup | 中 | 复用 `tests/test_preemption_standalone.cpp` 的 MQD + ChannelState helper pattern；如必要可拆分为 2 个 sub-binary（test_green_context_sched + test_green_context_hal）|
| Test 8.7 (HAL round-trip) 需 `hal_mock.cpp` 或 `hal_user.cpp` mock 配合，已有 stage4-6 archive 验证 | 低 | 复用 4.6 archive 已实施 fn-ptr 实现，本 test 仅验证 wrapper 调用 + null-handle error path |
| CMakeLists.txt 注册与其他 test_*standalone 同步 | 极低 | 已有 ≥30 个 `add_executable(test_*standalone ...)` block 可直接参照 |

## Linked ADRs / docs

- ADR-056（Green Context & PDL — ✅ Accepted 2026-08-01）
- ADR-046（Preemption — mqd_state_preempt/resume reference）
- ADR-045（Priority Scheduling — priority field reference）
- docs/roadmap/stage-4-bar-ioremap.md §4.6 验收段
- docs/architecture/stage4-gpu-cp-completion-gap-analysis.md §"4.6 closeout 残留"
- tests/test_preemption_standalone.cpp（reference scheduler test pattern）
- tests/test_context_type_standalone.cpp（reference MQD + ContextType test pattern）

## Sibling change references

- Parent: openspec/changes/2026-08-03-stage4-6-green-context-pdl-closeout/（A2 + A1 完成；本 change 承担 P3-A3 deferred 部分）

## Status

Pending — proposal.md + design.md + tasks.md + specs/green-context-tests/spec.md 即将补全。Implementation deferred to next session.
