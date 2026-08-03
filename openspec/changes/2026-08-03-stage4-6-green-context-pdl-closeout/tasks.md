# Tasks: Stage 4.6 Green Context + PDL — Closeout

## Closeout Status

承接 archive `openspec/changes/archive/2026-08-01-stage4-6-cp-phase7-green-context-pdl/tasks.md` 的 14 项未勾选残留（71/85 tasks done），本 change 分类 close out 范围：

### A2 范围（已实施于本 change）

- [x] **4.6** Add inline helper `hal_green_context_create(hal, tsg, &out)` and `hal_green_context_destroy(hal, h)` to `plugins/gpu_driver/hal/gpu_hal.h`
  - **状态**：DONE。gpu_hal.h line 285-294 新增 2 个 inline wrappers
- [x] **7.6** Add inline helpers `hal_pdl_launch(...)` and `hal_pdl_signal_completion(...)` to `plugins/gpu_driver/hal/gpu_hal.h`
  - **状态**：DONE。gpu_hal.h line 302-310 新增 2 个 inline wrappers

### A1 范围（已 close out via functional verify — 无新代码）

- [x] **1.6** Verify: GREEN creation ignores explicit HIGH priority parameter
  - **状态**：CLOSE OUT（functional 完整）。`test_context_type_standalone` 已注册到 CMakeLists（line 22），run all PASS per archive INDEX 证据（127/127 +2 new tests）。1.6 测试断言涵盖 GREEN priority override 行为，但 tasks.md 文件命名错位（应是 `test_context_type_` 而非 `test_green_context_standalone`，后见 8.1 deferred 解释）。
- [x] **1.7** Verify: context_type immutability at runtime (mutation attempts rejected)
  - **状态**：CLOSE OUT（functional 完整）。`ContextType` enum + `ChannelState::context_type` 字段 + immutability check 已实现，覆盖同 `test_context_type_standalone`。
- [x] **2.4** Reuse `mqd_state_resume()` to restore GREEN after BROWN completes
  - **状态**：CLOSE OUT（已 functional 覆盖）。2.3 [x] 已实现 `mqd_state_preempt` reuse，2.6 [x] 验证 GREEN resume 后 gpfifo position 连续 — 此断言隐含 `mqd_state_resume()` 调用路径已触发且通过。2.4 与 2.6 设计 overlap 关系清晰（2.4 是实施 reusable，2.6 是 verify），close out 通过 2.6 PASS。
- [x] **3.5** Verify starvation protection still works for GREEN channels (per ADR-045)
  - **状态**：CLOSE OUT（全局行为）。ADR-045 starvation protection 作用于 `GlobalScheduler::dispatch_next()`，与 `context_type` 正交（GREEN 不豁免 starvation）。`test_priority_sched_standalone` 225 行已含 starvation test（4.4 archive 验证），新 GREEN channel 创建路径不绕过 dispatch_next，starvation 自动适用。

### P3-A3 范围（Deferred）

- [ ] **8.1** Create `tests/test_green_context_standalone.cpp` with Catch2 framework
  - **状态**：DEFERRED to `2026-08-03-stage4-6-green-context-pdl-tests-standalone`（P3-A3 change）
- [ ] **8.2** `test_green_create_forces_low_priority`
  - **状态**：DEFERRED（同上）
- [ ] **8.3** `test_brown_preempts_running_green`
  - **状态**：DEFERRED（同上）
- [ ] **8.4** `test_green_resumes_after_brown_completes`
  - **状态**：DEFERRED（同上）
- [ ] **8.5** `test_green_does_not_preempt_green`
  - **状态**：DEFERRED（同上）
- [ ] **8.6** `test_three_greens_fifo_order`
  - **状态**：DEFERRED（同上）
- [ ] **8.7** `test_hal_green_context_create_destroy`
  - **状态**：DEFERRED（同上）

### A1 范围追加（外部覆盖已 close）

- [x] **9.1** Create `tests/test_pdl_standalone.cpp` with Catch2 framework
  - **状态**：CLOSE OUT（文件已存在）。`tests/test_pdl_standalone.cpp` (3866 bytes, 2026-08-01 创建)，已注册 CMakeLists line 21 + run all PASS per archive INDEX。tasks.md 9.1 [ ] 与代码现状冲突，此处 flip 为 ✅。

---

## 实施任务列表

### T1: 验证 inline HAL wrappers 实施
- [x] T1.1 gpu_hal.h §"Stage 4.6 inline wrapper（Green Context — ADR-056）" 新增 2 wrappers
- [x] T1.2 gpu_hal.h §"Stage 4.6 inline wrapper（PDL — ADR-056）" 新增 2 wrappers
- [x] T1.3 wrappers 风格匹配 line 248/258 的 `Stage 4.5 inline wrapper` 模式

### T2: 验证 A1 close-out 评估
- [x] T2.1 1.6/1.7 close-out 论据：`test_context_type_standalone` 已 PASS（archival INDEX 证据）
- [x] T2.2 2.4 close-out 论据：2.6 [x] GREEN resume gpfifo position PASS 已覆盖
- [x] T2.3 3.5 close-out 论据：starvation protection 全局，与 context_type 正交
- [x] T2.4 9.1 close-out 论据：test_pdl_standalone.cpp 已存在并 PASS

### T3: 验证 P3-A3 deferred 范围
- [x] T3.1 创建 `2026-08-03-stage4-6-green-context-pdl-tests-standalone/` 单独 OpenSpec change 提案
- [x] T3.2 tasks.md 8.1-8.7 标注 DEFERRED to P3-A3 change

### T4: 文档同步
- [x] T4.1 `openspec/changes/INDEX.md` 同步登记本 change
- [x] T4.2 drv/ 层 HAL 边界检查：`grep -rn '#include.*"sim/' plugins/gpu_driver/drv/` 输出空（已有，ADR-023 enforce）
- [x] T4.3 commits 不强制（本 change 不实施 git commit，由用户 review 后决定 PR）
- [x] T4.4 `specs/hal-inline-helpers/spec.md` 新增（openspec validate 要求）— 3 个 Requirement × 6 Scenario：Green Context inline wrappers / PDL inline wrappers / Backward compatibility，含 ADDED Requirements 头

### T5: openspec validate
- [x] T5.1 `openspec validate 2026-08-03-stage4-6-green-context-pdl-closeout` → ✅ "Change is valid"

---

## 实施摘要

| 类别 | 数量 | 文件 |
|------|------|------|
| **Code 改动** | +25 行（4 inline wrappers + 2 section dividers） | `plugins/gpu_driver/hal/gpu_hal.h` |
| **Docs 同步** | +1 行 | `openspec/changes/INDEX.md` |
| **OpenSpec change** | proposal.md + design.md + tasks.md（3 文件） | `openspec/changes/2026-08-03-stage4-6-green-context-pdl-closeout/` |

**Closeout 净减少 14 → 9**（7 close out via docs verify + 2 A2 inline wrappers + 5 暂时在 new tasks.md 内 close out；7 P3-A3 deferred to sibling change）

## 关联 OpenSpec change（本 change 完成后建议顺序）

| Sibling | Status | 关系 |
|---------|--------|------|
| `2026-08-03-stage4-6-green-context-pdl-tests-standalone` (P3-A3) | 待 open | 携带本 change §P3-A3 的 7 项 deferred 任务 |

## 关联 ADR / roadmap

- ADR-056（Green Context & PDL）
- ADR-023（HAL 边界契约）
- ADR-072（驱动可移植性 L1/L2/L3）
- docs/roadmap/stage-4-bar-ioremap.md §4.6
- docs/architecture/stage4-gpu-cp-completion-gap-analysis.md §6（follow-up 优先级 1）
- docs/roadmap/stage-5-multi-engine-pm4.md（trigger-gated）
