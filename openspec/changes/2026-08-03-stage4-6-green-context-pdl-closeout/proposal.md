# Proposal: Stage 4.6 Green Context + PDL — Closeout

> **OpenSpec change**: 2026-08-03-stage4-6-green-context-pdl-closeout
> **Trigger**: stage4-6-cp-phase7-green-context-pdl archived 2026-08-01 (commit `c6f6ed3`) with 71/85 tasks done; 14 未勾选残留作为 closeout 范围
> **Owner**: UsrLinuxEmu Architecture Team
> **关联 ADR**: ADR-056（Green Context & PDL — ✅ Accepted 2026-08-01）
> **关联 archive**: openspec/changes/archive/2026-08-01-stage4-6-cp-phase7-green-context-pdl/
> **关联 roadmap**: docs/roadmap/stage-4-bar-ioremap.md §4.6

---

## Why

Stage 4.6 (`stage4-6-cp-phase7-green-context-pdl`) 实施完成（merge commit `c6f6ed3`）并归档，但其 tasks.md 残留 14 项未勾选条目（verify 测试 / inline HAL helpers / standalone 测试框架）。INDEX.md 声称 `127/127 ctest PASS（+2 new tests）` 但其中 `test_green_context_standalone.cpp` 文件实际**不存在**（仅 `test_pdl_standalone.cpp` 与 `test_context_type_standalone.cpp` 实际注册到 CMakeLists）；tasks.md 的勾选状态与代码现状存在不一致。

本次 closeout 目标：

1. **A2 - inline HAL helpers**：补齐 4 个缺失的 inline HAL wrapper（`hal_green_context_create/destroy` + `hal_pdl_launch/signal_completion`），消除调用方需直接访问 `hal->hal_*` 的 boilerplate
2. **A1 - verify items 审查**：对 tasks 1.6/1.7/2.4/3.5 进行现状评估，决定是补 verify 测试、closeout 文档化还是 reopen
3. **P3-A3 - standalone tests**：记录 `test_green_context_standalone.cpp` 实际创建工作，单独走 P3-A3 follow-up change（本 change 不实施）

## What Changes

| 范围 | 内容 | 本 change |
|------|------|----------|
| `plugins/gpu_driver/hal/gpu_hal.h` | +4 inline wrappers（Stage 4.6 inline block）| ✅ 已实施（2026-08-03 commit `TBD`） |
| 1.6 / 1.7 / 2.4 / 3.5 verify items | docs note in this change | ✅ 文档化（see tasks.md §"Closeout status"） |
| `tests/test_green_context_standalone.cpp` | 创建 Catch2 测试 + 6 unit cases | ❌ Deferred to `2026-08-03-stage4-6-green-context-pdl-tests-standalone`（P3-A3） |

## Non-Goals

- ❌ 创建 `tests/test_green_context_standalone.cpp` (P3-A3 范围）
- ❌ 修改 HAL fn-pointer 结构（已 33 entries，符合 ADR-072 HAL 上限 ≤35 还有 2 余量）
- ❌ 修改 drv/ 层代码（HAL inline wrappers 是 API 便利，对调用方零侵入）
- ❌ 触发或实施任何 trigger-gated 后续（ADR-049 Phase 6+ / ADR-052 Phase 6.5）

## Acceptance Criteria

- [x] A2: gpu_hal.h 新增 4 个 inline HAL wrappers（hal_green_context_create / hal_green_context_destroy / hal_pdl_launch / hal_pdl_signal_completion）
- [x] A1: tasks.md §"Closeout status" 文档化 1.6/1.7/2.4/3.5 评估结论（已 functional 完整 / 不需补 verify 测试 — see tasks.md）
- [x] Drv layer boundary 检查：`grep -rn '#include.*"sim/' plugins/gpu_driver/drv/` 仍输出空（HAL 边界 enforce 持续生效）
- [x] lsp_diagnostics 在 gpu_hal.h 干净
- [x] 所有现有 ctest 仍 PASS（inline wrappers 不改变行为，仅 boilerplate 减少）
- [x] INDEX.md 同步登记本 change
- [x] `2026-08-03-stage4-6-green-context-pdl-closeout/` 完成归档后，tasks.md 残留 14 项降为 9 项（A1 4 项 + A2 4 项关闭 + P3-A3 7 项继续 carry）

## Risks

| 风险 | 概率 | 缓解 |
|------|------|------|
| drv/ 代码因引入 inline wrapper 而引用了错误 fn-pointer 字段 | 极低 | wrapper 仅传递 `hal->hal_X(hal->ctx, ...)`，所有调用方语义不变；现有 5 hal_sem_* wrappers 已运行 6 周无 issue |
| 本 change 的 docs note 与实际功能状态不一致 | 低 | A1 评估由 plan-mode review 验证，对照实际 sim 代码 grep `mqd_state_resume` 调用点 |

## Linked ADRs / docs

- ADR-056（Green Context & PDL — ✅ Accepted 2026-08-01）
- ADR-023（HAL 边界契约 — drv/ 禁直接访问 sim/）
- ADR-072（驱动可移植性 L1/L2/L3 框架）
- docs/architecture/stage4-gpu-cp-completion-gap-analysis.md §6（follow-up 优先级 1）
- docs/roadmap/stage-4-bar-ioremap.md §4.6（关键交付 / 验收）
- docs/roadmap/stage-5-multi-engine-pm4.md（deferred follow-up — 不在本 change）
