## Why

v1 实施 (`archive/2026-07-29-stage4-5-cp-phase6-preemption-timeline-sem/`) 已完成核心能力交付并归档（22/22 tasks），但 2026-07-30 spec audit + 2026-07-31 v1 gap analysis 识别出 **5 项遗留 gap**，需本 change 闭合：

| Gap | 严重度 | 性质 |
|-----|--------|------|
| 缺失 `test_concurrent_preempt`（源码、CMake、二进制均无） | **P0** | 验收标准未达成 |
| Sanitizer 验证（asan-ubsan + tsan）从未运行 | **P0** | 并发强制要求 |
| `tools/docs-audit.sh --strict` FAIL（3 个 warnings） | **P1** | tasks.md §8.3 验收未通过 |
| Spec/实现不一致（IB jump_stack save/restore vs defer） | **P2** | 文档准确性（不修改 archive spec，新增独立 spec 澄清） |
| Archived tasks.md checkbox 失同步（6 项 `[ ]` 但实施已完成） | **P2** | 归档 hygiene |

**P0 直接影响生产可用性**：
- 并发抢占是核心场景，无并发测试 = 抢占引擎在并发场景下的正确性无法证明
- Sanitizer 是抢占引擎（涉及状态机 + 多 channel 并发 + fence 跟踪）的强制验证手段

**P1/P2 影响文档可信度**：
- docs-audit FAIL 阻塞 CI gate（`.github/workflows/cmake-multi-platform.yml` 跑 `--strict`）
- Spec 不一致会让新人阅读 archive 时产生误解（IMPLEMENTATION_NOTES.md 已记录但 archive spec 未修正 — 按 ADR 政策不修改 archive）

## What Changes

### 1. Concurrent Preempt Test（§concurrent-preempt-test）

- 新增 `tests/test_concurrent_preempt.cpp` — 默认 N=100 preempt/resume 循环 × M 并发 submit 线程（TSan 下 N=20 缩减以控制 CI 时长）
- 在 `tests/CMakeLists.txt` 注册为 `test_concurrent_preempt_standalone` 独立 Catch2 二进制
- 验证：无死锁（60s timeout 内完成）+ 无 fence 丢失（每 fence 最终 signal 或 cancel）+ 无 state 泄漏（channel 销毁后无 dangling）+ cancel ratio < 1%（容忍 TSan 下 channel-destroy race）
- 失败自动 retry 3 次（用于 flaky benign race 容忍）
- 复用 v1 已交付的 backdoor symbols (`bd_preempt`, `bd_sem_*`, `bd_fence_read`)

### 2. Sanitizer Validation（§sanitizer-validation）

- 运行 `./build.sh test` 在 3 个 sanitizer 配置下：
  - `SANITIZER=asan-ubsan ./build.sh test` (AddressSanitizer + UndefinedBehaviorSanitizer)
  - `SANITIZER=tsan ./build.sh test` (ThreadSanitizer — **抢占涉及并发，强制要求**)
  - 默认配置（无 sanitizer）回归基线
- 修复任何 sanitizer 报告的 bug（潜在 race condition、use-after-free、未初始化读取等）
- 建立 sanitizer-clean 基线，确保后续 change 不会引入回归

### 3. Docs-Audit Cleanup（§docs-audit-cleanup）

修复 `tools/docs-audit.sh --strict` 的 3 个 warnings：

| Warning | 根因 | 修复方式 |
|---------|------|----------|
| `src/kernel has 46 cpp files (baseline 44)` | v1 / 后续 change 新增 kernel 模块未更新 baseline | 在 audit script 或 baseline 配置中更新文件数阈值 |
| `gpu_hal.h has 22 fn-ptrs (doc claims 14)` | v1 新增 8 个 HAL fn-ptrs（`hal_preempt`, `hal_resume`, `hal_sem_create`, `hal_sem_signal`, `hal_sem_wait`, `hal_sem_query`, `hal_sem_destroy`, `interrupt_register`），post-refactor-architecture.md §附录 A 描述过期 | 更新 post-refactor-architecture.md 附录 A 的 fn-ptr 数量（14→22）与列表 |
| `Doxygen not installed` | CI runner 缺少 doxygen | 在 pre-commit / CI 安装 doxygen，或显式声明 doxygen 为可选 |

### 4. Preemption Spec Correction（§preemption-spec-correction）

按 archive IMPLEMENTATION_NOTES.md 政策"归档 spec 不修改"，新增独立 spec 作为现有 canonical 的 **ADDENDUM** 澄清 IB jump_stack 语义：

- **Canonical 来源**：`openspec/changes/archive/2026-07-30-stage4-5-cp-phase6-preemption-engine-finish/specs/preemption-engine-finish/spec.md` §"Preemption deferred during IB execution"（已存在，禁止修改）
- 本 change 的 `specs/preemption-spec-correction/spec.md` 作为 **ADDENDUM**：
  - 顶部声明 CANONICAL REFERENCE（指向 preemption-engine-finish）
  - 补充 3 个增量场景：saved state field constraints + resume trigger conditions + defer guard mechanism
  - 不重复 canonical 已有的 "Preempt deferred while in IB chain" / "Resume after boundary preempt" 两个场景
- 读者引导：`docs/02_architecture/post-refactor-architecture.md` 或 `roadmap.md` 添加 link 指向本 addendum（而非 archive spec）

### 5. Archived Tasks.md Checkbox Sync（§archive-tasks-sync）

修正 `openspec/changes/archive/2026-07-29-stage4-5-cp-phase6-preemption-timeline-sem/tasks.md` 中 6 项 checkbox 状态：

- 2.4 `mqd_state_preempt` wiring → `[x]` (commit `d1f569b`)
- 2.5 `mqd_state_resume` wiring → `[x]` (commit `de620b5`)
- 2.6 IDLE/double-preempt → `[x]` (commit `d9728e8`)
- 2.7 pending fence table → `[x]` (commit `91b1fbf`)
- 2.8 fence freeze → `[x]` (commit `d1f569b`)
- 2.9 `test_preemption_standalone` → `[x]` (commit `cbe5bf7`, **PASS 477 assertions**)

**注**：此变更仅修改 archive 目录中的 tasks.md（不修改 spec，不修改代码），符合 IMPLEMENTATION_NOTES.md "归档 spec 不修改" 政策的延伸（tasks.md 是实施记录，应反映实际状态）。

## Capabilities

### New Capabilities

- `concurrent-preempt-test`: 100+ preempt/resume 循环 × 并发 submit 的压力测试，验证死锁/fence 丢失/state 泄漏
- `sanitizer-validation`: asan-ubsan + tsan 全部 green，建立 sanitizer-clean 基线
- `docs-audit-cleanup`: docs-audit `--strict` PASS，消除 3 个 warnings
- `preemption-spec-correction`: 新 spec 作为 canonical-spec addendum 澄清 IB jump_stack 的"defer"语义（独立 spec，明确指向 preemption-engine-finish 为 canonical）
- `archive-tasks-sync`: archive tasks.md 6 项 checkbox 状态同步为实施实际状态

### Modified Capabilities

（无现有 spec-level 行为变更；本 change 是 v1 实施的 gap 闭合，不修改 v1 的功能契约）

## Impact

### 代码

- `tests/test_concurrent_preempt.cpp` (new, ~150-200 行 Catch2)
- `tests/CMakeLists.txt` — 注册新 standalone test
- `tools/docs-audit.sh` 或 baseline config — 更新 kernel 文件数
- `docs/02_architecture/post-refactor-architecture.md` 附录 A — 更新 HAL fn-ptr 数量与列表
- `.github/workflows/cmake-multi-platform.yml` 或 `scripts/install-hooks.sh` — Doxygen 安装步骤

### 文档

- `openspec/changes/stage4-5-cp-phase6-preemption-timeline-sem-gaps/specs/preemption-spec-correction/spec.md` (new addendum)
- `openspec/changes/archive/2026-07-29-stage4-5-cp-phase6-preemption-timeline-sem/tasks.md` — 6 项 checkbox 同步
- `docs/02_architecture/post-refactor-architecture.md` — 链接到新 spec

### CI

- `.github/workflows/cmake-multi-platform.yml` — 新增 sanitizer job（如果尚未存在）

### ADR 状态

（无 ADR 变更 — ADR-045/046/047/049 已在 v1 中 Accepted，本 change 不修改 ADR 内容）