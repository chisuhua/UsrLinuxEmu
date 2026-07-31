## 1. Concurrent Preempt Test Implementation

- [x] 1.1 Create `tests/test_concurrent_preempt.cpp` with Catch2 framework
- [x] 1.2 Implement `kSubmitThreads = std::thread::hardware_concurrency()` workers
- [x] 1.3 Implement `kPreemptCycles = 100` cycles per worker
- [x] 1.4 Each worker: fence_create → submit → bd_preempt → submit waiter → bd_resume → fence_read
- [x] 1.5 Track fences_submitted, fences_signaled, fences_canceled atomics
- [x] 1.6 Hard timeout 60s on thread join (catch deadlock; aligns with spec §"No deadlock")
- [x] 1.7 Assertions: fences_submitted == signaled + canceled (no loss); canceled < submitted * 0.01 (cancel ratio < 1%, accounts for legitimate channel-destroy races under TSan; aligns with spec)
- [x] 1.8 Register in `tests/CMakeLists.txt` as standalone test
- [x] 1.9 Verify build: `cmake --build build --target test_concurrent_preempt_standalone`
- [x] 1.10 Run: `./build/bin/test_concurrent_preempt` — PASS

## 2. Sanitizer Validation

### 2.1 ASan + UBSan

- [ ] 2.1.1 Configure `SANITIZER=asan-ubsan ./build.sh` (creates `build-asan/` per AGENTS.md)
- [ ] 2.1.2 Run `./build-asan/bin/test_preemption_standalone` — green
- [ ] 2.1.3 Run `./build-asan/bin/test_timeline_semaphore_standalone` — green
- [ ] 2.1.4 Run `cd build-asan && ctest --output-on-failure` — all green
- [ ] 2.1.5 If any report: classify (memory bug vs UB), create fix commit (separate from this change)

### 2.2 TSan

- [ ] 2.2.1 Configure `SANITIZER=tsan ./build.sh` (requires Clang per AGENTS.md)
- [ ] 2.2.2 Run `./build-tsan/bin/test_preemption_standalone` — green
- [ ] 2.2.3 Run `./build-tsan/bin/test_timeline_semaphore_standalone` — green
- [ ] 2.2.4 Run `./build-tsan/bin/test_concurrent_preempt` — green (critical: validates §1)
- [ ] 2.2.5 Run `cd build-tsan && ctest --output-on-failure` — all green
- [ ] 2.2.6 If any data race reported: fix commit (separate), document in commit message

### 2.3 Baseline Regression

- [ ] 2.3.1 Run `./build/bin/test_*_standalone` (default build) — all green (no sanitizer regression)
- [ ] 2.3.2 Run `cd build && ctest --output-on-failure` — all green
- [ ] 2.3.3 Document sanitizer-clean commit baseline in `docs/05-advanced/sanitizer-status.md` (new file)

### 2.4 CI Integration

- [ ] 2.4.1 Add sanitizer job to `.github/workflows/cmake-multi-platform.yml` (matrix: asan-ubsan, tsan)
- [ ] 2.4.2 Verify CI job runs successfully (push to branch, observe CI)

## 3. Docs-Audit Cleanup

### 3.1 Kernel File Count Warning

- [x] 3.1.1 Verify current count: `find src/kernel -name '*.cpp' | wc -l` (should be 46)
- [x] 3.1.2 Locate baseline in `tools/docs-audit.sh`
- [x] 3.1.3 Update baseline from 44 → 46
- [x] 3.1.4 Add comment explaining baseline source (git tag or known increment)
- [x] 3.1.5 Run `tools/docs-audit.sh --strict` — warning resolved

### 3.2 gpu_hal.h fn-ptr Count Warning

- [x] 3.2.1 Verify current count: `grep -cE '^\s*int \(\*' plugins/gpu_driver/hal/gpu_hal.h` (should be 22)
- [x] 3.2.2 Locate "14 fn-ptrs" claim in `docs/02_architecture/post-refactor-architecture.md` §附录 A
- [x] 3.2.3 Update claim: 14 → 22
- [x] 3.2.4 Add fn-ptr list (hal_preempt, hal_resume, hal_sem_create, hal_sem_signal, hal_sem_wait, hal_sem_query, hal_sem_destroy, interrupt_register — 8 new from v1)
- [x] 3.2.5 Run `tools/docs-audit.sh --strict` — warning resolved

### 3.3 Doxygen Not Installed

- [x] 3.3.1 Check current CI image: doxygen installed?
- [x] 3.3.2 If not: add `apt install -y doxygen graphviz` to `.github/workflows/cmake-multi-platform.yml`
- [x] 3.3.3 Verify CI install step works
- [x] 3.3.4 Run `tools/docs-audit.sh --strict` — warning resolved
- [x] 3.3.5 All 3 warnings gone → overall `--strict` PASS

## 4. Preemption Spec Correction (Addendum Spec, Not Modifying Archive)

- [ ] 4.1 Create `openspec/changes/stage4-5-cp-phase6-preemption-timeline-sem-gaps/specs/preemption-spec-correction/spec.md` (NOT `preemption-engine/spec.md` — different intent)
- [ ] 4.2 Write CANONICAL REFERENCE block at top, pointing to `archive/2026-07-30-stage4-5-cp-phase6-preemption-engine-finish/specs/preemption-engine-finish/spec.md` §"Preemption deferred during IB execution"
- [ ] 4.3 Write Requirement: "Preemption Saved State Field Constraints (Addendum)" — 2 scenarios (saved state includes expected fields, excludes jump_stack)
- [ ] 4.4 Write Requirement: "Preemption Resume Trigger Conditions (Addendum)" — 2 scenarios (jump_stack pop allows subsequent preempt, multiple deferred preempts coalesce)
- [ ] 4.5 Write Requirement: "Defer Guard Mechanism (Addendum — Internal)" — 1 scenario (tick() in-line defer check)
- [ ] 4.6 Add References section linking to canonical spec + ADR-046 + IMPLEMENTATION_NOTES.md
- [ ] 4.7 Update `docs/02_architecture/post-refactor-architecture.md` §"Stage 4.5 GPU Compute Pipeline" — add link to new addendum (NOT to archive spec)
- [ ] 4.8 Update `roadmap.md` Stage 4.5 section — add link to new addendum (NOT to archive spec)
- [ ] 4.9 Verify: archive `preemption-engine-finish/spec.md` NOT modified (read-only check via `git diff archive/2026-07-30-stage4-5-cp-phase6-preemption-engine-finish/specs/preemption-engine-finish/spec.md`)
- [ ] 4.10 Verify: archive `preemption-timeline-sem/spec.md` NOT modified (per IMPLEMENTATION_NOTES.md policy)

## 5. Archive Tasks.md Checkbox Sync

- [x] 5.1 Read current `archive/2026-07-29-stage4-5-cp-phase6-preemption-timeline-sem/tasks.md`
- [x] 5.2 Update task 2.4: `- [ ]` → `- [x]` (mqd_state_preempt wiring, commit d1f569b)
- [x] 5.3 Update task 2.5: `- [ ]` → `- [x]` (mqd_state_resume wiring, commit de620b5)
- [x] 5.4 Update task 2.6: `- [ ]` → `- [x]` (IDLE/double-preempt, commit d9728e8)
- [x] 5.5 Update task 2.7: `- [ ]` → `- [x]` (pending fence table, commit 91b1fbf)
- [x] 5.6 Update task 2.8: `- [ ]` → `- [x]` (fence freeze, commit d1f569b)
- [x] 5.7 Update task 2.9: `- [ ]` → `- [x]` (test_preemption_standalone, commit cbe5bf7, PASS)
- [x] 5.8 Verify: spec.md NOT modified
- [x] 5.9 Verify: IMPLEMENTATION_NOTES.md NOT modified

## 6. Final Verification

- [ ] 6.1 Run `tools/docs-audit.sh --strict` — PASS (3 warnings resolved)
- [ ] 6.2 Run `./build/bin/test_concurrent_preempt` — PASS
- [ ] 6.3 Run `./build-asan/bin/test_*_standalone` — all green
- [ ] 6.4 Run `./build-tsan/bin/test_*_standalone` — all green (including test_concurrent_preempt)
- [ ] 6.5 Run `cd build && ctest --output-on-failure` — 0 failures
- [ ] 6.6 Run `openspec validate stage4-5-cp-phase6-preemption-timeline-sem-gaps --strict` — pass
- [ ] 6.7 Run `git diff archive/2026-07-29-stage4-5-cp-phase6-preemption-timeline-sem/spec.md` — empty (archive spec untouched)
- [ ] 6.8 **Predication-aql integration gate**: After predication-aql merges to main, rebase this change and re-run `./build/bin/test_concurrent_preempt` (validates no channel_state.h integration regression)

## 7. ADR-074 (Hygiene Policy Documents Archive Tasks.md Sync)

- [ ] 7.1 Create `docs/00_adr/adr-074-archive-tasks-md-checkbox-hygiene.md` (establishes policy that archive tasks.md checkboxes may be updated, separate from "archive spec not modified" policy)
- [ ] 7.2 ADR-074 reaches "✅ Accepted" status per `docs/00_adr/adr-035-governance-policy.md` Rule 2
- [ ] 7.3 Update `docs/00_adr/README.md` index to add ADR-074 entry (add row to table + update status distribution)

## 8. Commit & Handoff

- [ ] 8.1 Commit §1 (concurrent test): `feat(test): add test_concurrent_preempt for preempt engine stress validation`
- [ ] 8.2 Commit §2 (sanitizer): `chore(sanitizer): establish asan-ubsan + tsan clean baseline for stage4-5`
- [ ] 8.3 Commit §3 (docs-audit): `fix(docs-audit): update kernel file count, gpu_hal fn-ptr count, doxygen CI install`
- [ ] 8.4 Commit §4 (spec addendum): `docs(arch): add preemption-spec-correctness addendum referencing preemption-engine-finish canonical`
- [ ] 8.5 Commit §5 (archive sync): `chore(archive): sync preemption-timeline-sem tasks.md checkbox state with implementation`
- [ ] 8.6 Commit §6 (predication-aql rebase gate) — defer to gate triggered event
- [ ] 8.7 Commit §7 (ADR-074): `docs(adr): add ADR-074 archive-tasks.md checkbox hygiene policy`
- [ ] 8.8 Commit §8 (verification + handoff): `chore(plan): plan-done for stage4-5 preemption-timeline-sem-gaps`
- [ ] 8.9 Update `.rddf/state/.plan-handoff.json` to add this change
- [ ] 8.10 Update `.rddf/state/iteration.json` to add this change as "proposed"

## Cross-Change Coordination

**优先级**: P1 (gap closure, blocking CI gate via docs-audit)

**与 `stage4-5-cp-phase6-preemption-engine-finish` 的关系 (canonical)**:
- 该 change (2026-07-30 已 archived) 在 `specs/preemption-engine-finish/spec.md` §"Preemption deferred during IB execution" 是 **canonical** 语义
- 本 change §4 的 spec 是该 canonical 的 **ADDENDUM**（不重复、不覆盖）
- 任何 drift 由 CANONICAL REFERENCE 块 + 顶部 RISK 块显式声明
- 两者均属于 stage4-5-phase6-gpu-cp parent feature

**与 `stage4-5-cp-phase6-preemption-timeline-sem` (v1) 的关系**:
- v1 实施 (2026-07-29 archived) 有 IMPLEMENTATION_NOTES.md 记录 "归档 spec 不修改"
- 本 change §4 严格遵守：既不修改 v1 spec，也不修改 canonical spec（preemption-engine-finish）
- spec 文件落点 `specs/preemption-spec-correction/spec.md`（带 -correction 后缀，避免与 `preemption-engine` 命名混淆）

**与 `stage4-5-cp-phase6-predication-aql` 的关系**:
- predication-aql 也在 `channel_state.{h,cpp}` 添加 `PredicateState`
- 本 change 不修改 channel_state.h（避免 rebase 冲突）
- predication-aql 在并行 worktree 中独立实施
- **集成 gate（新增）**：predication-aql 合并到 main 后,本 change rebase 必须通过 `test_concurrent_preempt` 重新验证（task 6.8）