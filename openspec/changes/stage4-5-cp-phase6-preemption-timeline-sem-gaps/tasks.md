## 1. Concurrent Preempt Test Implementation

- [ ] 1.1 Create `tests/test_concurrent_preempt.cpp` with Catch2 framework
- [ ] 1.2 Implement `kSubmitThreads = std::thread::hardware_concurrency()` workers
- [ ] 1.3 Implement `kPreemptCycles = 100` cycles per worker
- [ ] 1.4 Each worker: fence_create → submit → bd_preempt → submit waiter → bd_resume → fence_read
- [ ] 1.5 Track fences_submitted, fences_signaled, fences_canceled atomics
- [ ] 1.6 Hard timeout 30s on thread join (catch deadlock)
- [ ] 1.7 Assertions: fences_submitted == signaled + canceled (no loss); canceled == 0 (no deadlock)
- [ ] 1.8 Register in `tests/CMakeLists.txt` as standalone test
- [ ] 1.9 Verify build: `cmake --build build --target test_concurrent_preempt`
- [ ] 1.10 Run: `./build/bin/test_concurrent_preempt` — PASS

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

- [ ] 3.1.1 Verify current count: `find src/kernel -name '*.cpp' | wc -l` (should be 46)
- [ ] 3.1.2 Locate baseline in `tools/docs-audit.sh`
- [ ] 3.1.3 Update baseline from 44 → 46
- [ ] 3.1.4 Add comment explaining baseline source (git tag or known increment)
- [ ] 3.1.5 Run `tools/docs-audit.sh --strict` — warning resolved

### 3.2 gpu_hal.h fn-ptr Count Warning

- [ ] 3.2.1 Verify current count: `grep -c '^\s*int (\*' plugins/gpu_driver/hal/gpu_hal.h` (should be 29)
- [ ] 3.2.2 Locate "14 fn-ptrs" claim in `docs/02_architecture/post-refactor-architecture.md` §附录 A
- [ ] 3.2.3 Update claim: 14 → 29
- [ ] 3.2.4 Add fn-ptr list (sem_create, sem_signal, sem_wait, sem_destroy, preempt_channel, etc.)
- [ ] 3.2.5 Run `tools/docs-audit.sh --strict` — warning resolved

### 3.3 Doxygen Not Installed

- [ ] 3.3.1 Check current CI image: doxygen installed?
- [ ] 3.3.2 If not: add `apt install -y doxygen graphviz` to `.github/workflows/cmake-multi-platform.yml`
- [ ] 3.3.3 Verify CI install step works
- [ ] 3.3.4 Run `tools/docs-audit.sh --strict` — warning resolved
- [ ] 3.3.5 All 3 warnings gone → overall `--strict` PASS

## 4. Preemption Spec Correction (New Spec, Not Modifying Archive)

- [ ] 4.1 Create `openspec/changes/stage4-5-cp-phase6-preemption-timeline-sem-gaps/specs/preemption-engine/spec.md`
- [ ] 4.2 Write Requirement: "Preemption Deferred During IB Nested Execution" (canonical semantics)
- [ ] 4.3 Write Scenario: "Preemption check skips during IB nested"
- [ ] 4.4 Write Scenario: "Saved state excludes jump_stack"
- [ ] 4.5 Write Scenario: "jump_stack pop allows subsequent preempt"
- [ ] 4.6 Add reference link to `archive/2026-07-29-stage4-5-cp-phase6-preemption-timeline-sem/IMPLEMENTATION_NOTES.md` §"已知 spec/implementation 不一致"
- [ ] 4.7 Update `docs/02_architecture/post-refactor-architecture.md` §"Stage 4.5 GPU Compute Pipeline" — add link to new spec
- [ ] 4.8 Update `roadmap.md` Stage 4.5 section — add link to new spec
- [ ] 4.9 Verify: archive spec.md NOT modified (read-only check via `git diff archive/.../spec.md`)

## 5. Archive Tasks.md Checkbox Sync

- [ ] 5.1 Read current `archive/2026-07-29-stage4-5-cp-phase6-preemption-timeline-sem/tasks.md`
- [ ] 5.2 Update task 2.4: `- [ ]` → `- [x]` (mqd_state_preempt wiring, commit d1f569b)
- [ ] 5.3 Update task 2.5: `- [ ]` → `- [x]` (mqd_state_resume wiring, commit de620b5)
- [ ] 5.4 Update task 2.6: `- [ ]` → `- [x]` (IDLE/double-preempt, commit d9728e8)
- [ ] 5.5 Update task 2.7: `- [ ]` → `- [x]` (pending fence table, commit 91b1fbf)
- [ ] 5.6 Update task 2.8: `- [ ]` → `- [x]` (fence freeze, commit d1f569b)
- [ ] 5.7 Update task 2.9: `- [ ]` → `- [x]` (test_preemption_standalone, commit cbe5bf7, PASS)
- [ ] 5.8 Verify: spec.md NOT modified
- [ ] 5.9 Verify: IMPLEMENTATION_NOTES.md NOT modified

## 6. Final Verification

- [ ] 6.1 Run `tools/docs-audit.sh --strict` — PASS (3 warnings resolved)
- [ ] 6.2 Run `./build/bin/test_concurrent_preempt` — PASS
- [ ] 6.3 Run `./build-asan/bin/test_*_standalone` — all green
- [ ] 6.4 Run `./build-tsan/bin/test_*_standalone` — all green (including test_concurrent_preempt)
- [ ] 6.5 Run `cd build && ctest --output-on-failure` — 0 failures
- [ ] 6.6 Run `openspec validate stage4-5-cp-phase6-preemption-timeline-sem-gaps --strict` — pass
- [ ] 6.7 Run `git diff archive/2026-07-29-stage4-5-cp-phase6-preemption-timeline-sem/spec.md` — empty (archive spec untouched)

## 7. Commit & Handoff

- [ ] 7.1 Commit §1 (concurrent test): `feat(test): add test_concurrent_preempt for preempt engine stress validation`
- [ ] 7.2 Commit §2 (sanitizer): `chore(sanitizer): establish asan-ubsan + tsan clean baseline for stage4-5`
- [ ] 7.3 Commit §3 (docs-audit): `fix(docs-audit): update kernel file count, gpu_hal fn-ptr count, doxygen CI install`
- [ ] 7.4 Commit §4 (spec correction): `docs(arch): add canonical preemption-engine spec clarifying IB jump_stack defer semantics`
- [ ] 7.5 Commit §5 (archive sync): `chore(archive): sync preemption-timeline-sem tasks.md checkbox state with implementation`
- [ ] 7.6 Commit §6+7 (verification + handoff): `chore(plan): plan-done for stage4-5 preemption-timeline-sem-gaps`
- [ ] 7.7 Update `.rddf/state/.plan-handoff.json` to add this change
- [ ] 7.8 Update `.rddf/state/iteration.json` to add this change as "proposed"

## Cross-Change Coordination

**优先级**: P1 (gap closure, blocking CI gate via docs-audit)

**与 `stage4-5-cp-phase6-preemption-engine-finish` 的关系**:
- 该 change (2026-07-30 已 archived) 在 `specs/preemption-engine-finish/spec.md` §"Preemption deferred during IB execution" 已记录 canonical 语义
- 本 change §4 的 spec 内容应**对齐**该 change 的语义校正（避免 spec 间再次不一致）
- 两者均属于 stage4-5-phase6-gpu-cp parent feature

**与 `stage4-5-cp-phase6-predication-aql` 的关系**:
- predication-aql 也在 `channel_state.{h,cpp}` 添加 `PredicateState`
- 本 change 不修改 channel_state.h（避免 rebase 冲突）
- predication-aql 在并行 worktree 中独立实施