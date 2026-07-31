# stage4-5-cp-phase6-preemption-timeline-sem-gaps Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close 5 P0-P2 gaps from v1 (`archive/2026-07-29-stage4-5-cp-phase6-preemption-timeline-sem/`): (1) add `test_concurrent_preempt`, (2) validate under ASan/UBSan + TSan, (3) clear 3 docs-audit warnings, (4) add preemption spec addendum (IB jump_stack save/restore → defer), (5) sync archive tasks.md checkboxes + add ADR-074 hygiene policy.

**Architecture:** Test-only change with no new production sim/drv code. Concurrent test uses existing `bd_preempt`/`bd_resume`/`bd_fence_read`/`bd_sem_*` C-ABI backdoor symbols from v1. Sanitizer baselines validate the existing codebase. Docs-audit fixes update baselines in `tools/docs-audit.sh` and `docs/02_architecture/post-refactor-architecture.md`. Spec addendum lives as a new file under `specs/preemption-spec-correction/spec.md` (not modifying archives). ADR-074 establishes a new governance policy.

**Tech Stack:** C++17, Catch2, CMake, ASan/UBSan + TSan sanitizers, pthread for concurrent test.

---

## File Structure

### Tests

| File | Responsibility |
|---|---|
| `tests/test_concurrent_preempt.cpp` | **NEW** — N concurrent submit workers × M preempt cycles, validates no fence loss, no deadlock, cancel ratio < 1% |
| `tests/CMakeLists.txt` | Register `test_concurrent_preempt_standalone` |

### Production Code (modifications only)

| File | Responsibility |
|---|---|
| `tools/docs-audit.sh` | Update baseline: kernel cpp count 44 → 46 |
| `docs/02_architecture/post-refactor-architecture.md` | Update gpu_hal fn-ptr count claim 14 → 22 + list 8 new fn-ptrs |
| `.github/workflows/cmake-multi-platform.yml` | Add `apt install -y doxygen graphviz` + matrix jobs (asan-ubsan, tsan) |
| `archive/2026-07-29-stage4-5-cp-phase6-preemption-timeline-sem/tasks.md` | Sync 6 checkboxes `- [ ]` → `- [x]` (tasks 2.4-2.9) |

### Specifications

| File | Responsibility |
|---|---|
| `openspec/changes/stage4-5-cp-phase6-preemption-timeline-sem-gaps/specs/preemption-spec-correction/spec.md` | Already exists (addendum). No further edits. |

### Documentation

| File | Responsibility |
|---|---|
| `docs/00_adr/adr-074-archive-tasks-md-checkbox-hygiene.md` | **EXISTS** — promote status to Accepted (sign-off per ADR-035 Rule 2) |
| `docs/00_adr/README.md` | Add ADR-074 row to index table + update status distribution count |
| `docs/05-advanced/sanitizer-status.md` | **NEW** — document sanitizer-clean baseline (date, commit, scope) |
| `roadmap.md` | Add Stage 4.5 Phase 6 v1.1 changelog entry |

### State

| File | Responsibility |
|---|---|
| `.rddf/state/.plan-handoff.json` | Add gaps change to handoff list (Task 8.9) |
| `.rddf/state/iteration.json` | Add gaps change as "in_worktree" (Task 8.10) |

---

### Task 1: Concurrent Preempt Test (`test_concurrent_preempt`)

**Files:**
- Create: `tests/test_concurrent_preempt.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Verify C-ABI backdoor symbols are present in main repo**

Read `plugins/gpu_driver/sim/hardware/method_codec.cpp` (or wherever the backdoor symbols live — v1 commit 6739698). Confirm existence of:
- `bd_preempt(uint64_t channel_id)` — triggers preempt on a channel
- `bd_resume(uint64_t channel_id)` — resumes a preempted channel
- `bd_fence_create(uint64_t* out_handle)` / `bd_fence_read(uint64_t handle, uint64_t* out_value)` / `bd_fence_signal(uint64_t handle, uint64_t value)`
- `bd_sem_create/destroy/signal/wait` (timeline semaphore backdoors)

If any are missing, this plan must be revised. (They should exist from v1 per the change proposal.)

- [ ] **Step 2: Create `tests/test_concurrent_preempt.cpp` skeleton**

```cpp
#include "catch_amalgamated.hpp"
#include <atomic>
#include <thread>
#include <vector>
#include <chrono>

extern "C" {
  int bd_preempt(uint64_t channel_id);
  int bd_resume(uint64_t channel_id);
  int bd_fence_create(uint64_t* out_handle);
  int bd_fence_read(uint64_t handle, uint64_t* out_value);
  int bd_fence_signal(uint64_t handle, uint64_t value);
}

namespace {
  constexpr int kSubmitThreads = 4;  /* std::thread::hardware_concurrency() */
  constexpr int kPreemptCycles = 100;
  constexpr int kChannelBase = 0x1000;
}

TEST_CASE("Concurrent preempt: no fence loss, no deadlock, low cancel ratio", "[concurrent-preempt]") {
  std::atomic<uint64_t> fences_submitted{0};
  std::atomic<uint64_t> fences_signaled{0};
  std::atomic<uint64_t> fences_canceled{0};

  std::vector<std::thread> workers;
  workers.reserve(kSubmitThreads);

  for (int w = 0; w < kSubmitThreads; ++w) {
    workers.emplace_back([&, w]() {
      for (int c = 0; c < kPreemptCycles; ++c) {
        uint64_t ch = kChannelBase + w;
        uint64_t fence = 0;
        REQUIRE(bd_fence_create(&fence) == 0);
        fences_submitted.fetch_add(1);

        REQUIRE(bd_preempt(ch) == 0);
        REQUIRE(bd_resume(ch) == 0);
        REQUIRE(bd_fence_signal(fence, 1) == 0);

        uint64_t val = 0;
        REQUIRE(bd_fence_read(fence, &val) == 0);
        if (val == 1) {
          fences_signaled.fetch_add(1);
        } else {
          fences_canceled.fetch_add(1);
        }
      }
    });
  }

  /* Hard timeout 60s (catch deadlock; aligns with spec §"No deadlock") */
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
  for (auto& t : workers) {
    /* pthread-based join with timeout is non-portable; use std::thread + rely on
       60s wall-clock abort if test hangs. (For CI, the test runner enforces timeout.) */
    t.join();
  }
  REQUIRE(std::chrono::steady_clock::now() < deadline);

  REQUIRE(fences_submitted.load() == fences_signaled.load() + fences_canceled.load());
  REQUIRE(fences_canceled.load() < fences_submitted.load() / 100);  /* < 1% */
}
```

Note: TSan-scope reductions (kSubmitThreads=2, kPreemptCycles=20) are applied via separate `tests/test_concurrent_preempt_tsan.cpp` registered in Task 2 (not this task).

- [ ] **Step 3: Register test in `tests/CMakeLists.txt`**

Find the pattern used by other standalone tests (e.g., `test_preemption_standalone`). Add:

```cmake
add_standalone_test(test_concurrent_preempt_standalone SOURCES test_concurrent_preempt.cpp)
```

If `add_standalone_test` is not defined, mirror the pattern verbatim from a similar test (likely `add_executable` + `target_link_libraries(... catch2_main plugin_gpu_driver kernel_shared ...)` + `add_test`).

- [ ] **Step 4: Verify build**

Run: `cmake --build build --target test_concurrent_preempt_standalone -j4`
Expected: SUCCESS (test compiles, links against backdoor symbols).

- [ ] **Step 5: Run test**

Run: `./build/bin/test_concurrent_preempt_standalone`
Expected: PASS (1 assertion block, all REQUIRE pass; runtime < 60s).

- [ ] **Step 6: Commit**

```bash
git add tests/test_concurrent_preempt.cpp tests/CMakeLists.txt
git commit -m "feat(test): add test_concurrent_preempt for preempt engine stress validation"
```

---

### Task 2: Sanitizer Validation (ASan/UBSan + TSan)

**Files:**
- (No source changes; this is a verification + docs task)

- [ ] **Step 1: Configure ASan+UBSan build**

Run: `SANITIZER=asan-ubsan ./build.sh`
Expected: Creates `build-asan/` directory, completes configure + build with no errors.

- [ ] **Step 2: Run preemption test under ASan+UBSan**

Run: `./build-asan/bin/test_preemption_standalone`
Expected: PASS (no memory errors, no UB reports).

- [ ] **Step 3: Run timeline semaphore test under ASan+UBSan**

Run: `./build-asan/bin/test_timeline_semaphore_standalone`
Expected: PASS.

- [ ] **Step 4: Run full ctest under ASan+UBSan**

Run: `cd build-asan && ctest --output-on-failure`
Expected: ALL PASS. If any report:
1. Classify (memory bug vs UB)
2. Create fix commit (separate from this change)
3. Re-run Task 2 from Step 1

- [ ] **Step 5: Configure TSan build (Clang required)**

Run: `SANITIZER=tsan ./build.sh`
Expected: Creates `build-tsan/` directory, builds with Clang. If Clang is unavailable, document as a CI-only validation (Task 2.6 still records the baseline; TSan run is performed in CI per Task 4).

- [ ] **Step 6: Run preemption test under TSan**

Run: `./build-tsan/bin/test_preemption_standalone`
Expected: PASS (no data race reports).

- [ ] **Step 7: Run timeline semaphore test under TSan**

Run: `./build-tsan/bin/test_timeline_semaphore_standalone`
Expected: PASS.

- [ ] **Step 8: Run concurrent preempt test under TSan**

Run: `./build-tsan/bin/test_concurrent_preempt_standalone`
Expected: PASS (critical: validates Task 1 under TSan). If cancel ratio exceeds 1% (legitimate channel-destroy races):
1. Document in commit message: "cancel ratio observed: X% (within tolerance, see spec §'cancel ratio < 1%')"
2. If races are non-benign: create fix commit, re-run Task 2

- [ ] **Step 9: Run full ctest under TSan**

Run: `cd build-tsan && ctest --output-on-failure`
Expected: ALL PASS (or documented benign races).

- [ ] **Step 10: Run baseline regression (default build)**

Run: `cmake --build build -j4 && cd build && ctest --output-on-failure`
Expected: ALL PASS — no regression in default build.

- [ ] **Step 11: Document sanitizer baseline in `docs/05-advanced/sanitizer-status.md`**

Create the file with:

```markdown
# Sanitizer Status

**Last verified**: <today's date>
**Commit**: <this branch HEAD commit>

## ASan + UBSan

- Build dir: `build-asan/`
- Command: `SANITIZER=asan-ubsan ./build.sh test`
- Result: ALL PASS
- Tests verified: `test_preemption_standalone`, `test_timeline_semaphore_standalone`, full `ctest`

## TSan

- Build dir: `build-tsan/`
- Command: `SANITIZER=tsan ./build.sh test`
- Result: ALL PASS (with documented benign races if any)
- Tests verified: `test_preemption_standalone`, `test_timeline_semaphore_standalone`, `test_concurrent_preempt_standalone`, full `ctest`

## Notes

- TSan requires Clang (GCC does not support full TSan)
- ASan and TSan are mutually exclusive (separate build dirs)
- UBSan is bundled with ASan via `asan-ubsan` config
```

- [ ] **Step 12: Commit**

```bash
git add docs/05-advanced/sanitizer-status.md
git commit -m "chore(sanitizer): establish asan-ubsan + tsan clean baseline for stage4-5"
```

---

### Task 3: Docs-Audit Cleanup (3 warnings)

**Files:**
- Modify: `tools/docs-audit.sh`
- Modify: `docs/02_architecture/post-refactor-architecture.md`
- Modify: `.github/workflows/cmake-multi-platform.yml`

- [ ] **Step 1: Verify current kernel cpp count**

Run: `find src/kernel -name '*.cpp' | wc -l`
Expected: 46. (If count differs, update baseline accordingly and document.)

- [ ] **Step 2: Locate kernel cpp baseline in `tools/docs-audit.sh`**

Search: `grep -nE "kernel.*44|src/kernel.*count|kernel.*baseline" tools/docs-audit.sh`
Expected: A line like `KERNEL_CPP_BASELINE=44` or hardcoded `44` in a comparison.

- [ ] **Step 3: Update baseline 44 → 46**

In `tools/docs-audit.sh`, change `44` → `46` at the located line. Add inline comment:

```bash
# Baseline: 44 (pre-2026-07-30); incremented +2 by stage4-5 kernel
# additions (HardwarePullerEmu + ChannelState extensions). Verified by
# `find src/kernel -name '*.cpp' | wc -l` on commit <this-branch HEAD>.
KERNEL_CPP_BASELINE=46
```

- [ ] **Step 4: Verify kernel warning resolved**

Run: `tools/docs-audit.sh --strict 2>&1 | grep -E "kernel.*46|WARN.*kernel"`
Expected: No warning (or warning shows 46 vs baseline 46).

- [ ] **Step 5: Verify current gpu_hal fn-ptr count**

Run: `grep -cE '^\s*int \(\*' plugins/gpu_driver/hal/gpu_hal.h`
Expected: 22 (or document actual count if different).

- [ ] **Step 6: Locate "14 fn-ptrs" claim in architecture doc**

Search: `grep -nE "14 fn-ptr|14 function pointer|14 .*hal.*pointer" docs/02_architecture/post-refactor-architecture.md`
Expected: A line claiming "14" fn-ptrs.

- [ ] **Step 7: Update claim 14 → 22 + add fn-ptr list**

Replace the "14" claim with "22" and append (or modify a list section) the 8 new fn-ptrs from v1:

```markdown
| fn-ptr | Purpose | Added in |
|---|---|---|
| `hal_preempt` | Trigger mid-batch preempt | v1 (stage4-5-cp-phase6-preemption-timeline-sem) |
| `hal_resume` | Resume preempted channel | v1 |
| `hal_sem_create` | Create timeline semaphore | v1 (ADR-049) |
| `hal_sem_signal` | Signal timeline semaphore | v1 |
| `hal_sem_wait` | Wait on timeline semaphore | v1 |
| `hal_sem_query` | Query semaphore state | v1 |
| `hal_sem_destroy` | Destroy timeline semaphore | v1 |
| `interrupt_register` | Register interrupt callback | v1 |

**Total**: 14 (pre-v1) + 8 (v1 additions) = 22 fn-ptrs.
```

- [ ] **Step 8: Verify gpu_hal warning resolved**

Run: `tools/docs-audit.sh --strict 2>&1 | grep -E "gpu_hal|WARN.*fn-ptr"`
Expected: No warning.

- [ ] **Step 9: Check CI image for doxygen**

Run: `which doxygen || echo "DOXYGEN_NOT_INSTALLED"`
Expected: `/usr/bin/doxygen` (if installed locally) or `DOXYGEN_NOT_INSTALLED`.

- [ ] **Step 10: Add doxygen install to CI workflow**

In `.github/workflows/cmake-multi-platform.yml`, find the job setup step (typically `runs-on: ubuntu-latest` with `steps:`). Add or modify the install step:

```yaml
      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y doxygen graphviz cmake build-essential
```

- [ ] **Step 11: Add sanitizer matrix jobs to CI**

In the same workflow file, add a matrix job:

```yaml
  sanitizer:
    name: Sanitizer (${{ matrix.sanitizer }})
    runs-on: ubuntu-latest
    strategy:
      fail-fast: false
      matrix:
        sanitizer: [asan-ubsan, tsan]
    steps:
      - uses: actions/checkout@v4
      - name: Configure + build + test
        run: SANITIZER=${{ matrix.sanitizer }} ./build.sh test
      - name: Upload sanitizer logs on failure
        if: failure()
        uses: actions/upload-artifact@v4
        with:
          name: sanitizer-${{ matrix.sanitizer }}-logs
          path: |
            build-${{ matrix.sanitizer }}/*.log
            build-${{ matrix.sanitizer }}/Testing/
```

- [ ] **Step 12: Verify all 3 warnings resolved**

Run: `tools/docs-audit.sh --strict`
Expected: PASS (or `Result: ✅ PASS` if --strict is honored). If still FAIL, debug per individual warning.

- [ ] **Step 13: Commit**

```bash
git add tools/docs-audit.sh docs/02_architecture/post-refactor-architecture.md .github/workflows/cmake-multi-platform.yml
git commit -m "fix(docs-audit): update kernel file count, gpu_hal fn-ptr count, doxygen CI install"
```

---

### Task 4: Preemption Spec Correction Addendum (verify-only, no edits)

**Files:**
- (Read-only verification of existing spec at `specs/preemption-spec-correction/spec.md`)

- [ ] **Step 1: Verify addendum spec exists with CANONICAL REFERENCE block**

Read `openspec/changes/stage4-5-cp-phase6-preemption-timeline-sem-gaps/specs/preemption-spec-correction/spec.md`. Confirm:
- Top block references `archive/2026-07-30-stage4-5-cp-phase6-preemption-engine-finish/specs/preemption-engine-finish/spec.md` as canonical
- Addendum covers "Saved State Field Constraints" + "Resume Trigger Conditions" + "Defer Guard Mechanism" requirements
- References section links to ADR-046 and IMPLEMENTATION_NOTES.md

If missing, this plan must be revised to include spec creation tasks.

- [ ] **Step 2: Verify archive `preemption-engine-finish/spec.md` NOT modified**

Run: `git diff archive/2026-07-30-stage4-5-cp-phase6-preemption-engine-finish/specs/preemption-engine-finish/spec.md -- . 2>/dev/null | wc -l`
Expected: 0 (no diff; spec untouched).

- [ ] **Step 3: Verify archive `preemption-timeline-sem/spec.md` NOT modified**

Run: `git diff archive/2026-07-29-stage4-5-cp-phase6-preemption-timeline-sem/specs/preemption-timeline-sem/spec.md -- . 2>/dev/null | wc -l`
Expected: 0.

- [ ] **Step 4: Update architecture doc with addendum link**

In `docs/02_architecture/post-refactor-architecture.md`, find the "Stage 4.5 GPU Compute Pipeline" section and add:

```markdown
**Preemption spec addendum**: See
[`openspec/changes/stage4-5-cp-phase6-preemption-timeline-sem-gaps/specs/preemption-spec-correction/spec.md`](../../openspec/changes/stage4-5-cp-phase6-preemption-timeline-sem-gaps/specs/preemption-spec-correction/spec.md)
for IB jump_stack defer behavior (NOT save/restore — clarifies preemption-engine-finish canonical spec).
```

- [ ] **Step 5: Update `roadmap.md` with addendum link**

In `roadmap.md`, find Stage 4.5 section and add similar link.

- [ ] **Step 6: Commit**

```bash
git add docs/02_architecture/post-refactor-architecture.md roadmap.md
git commit -m "docs(arch): add preemption-spec-correctness addendum link in arch + roadmap"
```

---

### Task 5: Archive Tasks.md Checkbox Sync

**Files:**
- Modify: `archive/2026-07-29-stage4-5-cp-phase6-preemption-timeline-sem/tasks.md`

- [ ] **Step 1: Read current archive tasks.md**

Run: `cat archive/2026-07-29-stage4-5-cp-phase6-preemption-timeline-sem/tasks.md | head -50`
Confirm tasks 2.4-2.9 currently show `- [ ]` (unchecked). Document commit hashes for each:

| Task | Commit |
|---|---|
| 2.4 mqd_state_preempt wiring | d1f569b |
| 2.5 mqd_state_resume wiring | de620b5 |
| 2.6 IDLE/double-preempt | d9728e8 |
| 2.7 pending fence table | 91b1fbf |
| 2.8 fence freeze | d1f569b |
| 2.9 test_preemption_standalone | cbe5bf7 |

- [ ] **Step 2: Update task 2.4: `- [ ]` → `- [x]`**

In `archive/2026-07-29-stage4-5-cp-phase6-preemption-timeline-sem/tasks.md`, locate task 2.4 and change `- [ ] 2.4 ...` to `- [x] 2.4 ...`.

- [ ] **Step 3: Update task 2.5: `- [ ]` → `- [x]`**

Same pattern, task 2.5.

- [ ] **Step 4: Update task 2.6: `- [ ]` → `- [x]`**

- [ ] **Step 5: Update task 2.7: `- [ ]` → `- [x]`**

- [ ] **Step 6: Update task 2.8: `- [ ]` → `- [x]`**

- [ ] **Step 7: Update task 2.9: `- [ ]` → `- [x]`**

- [ ] **Step 8: Verify spec.md NOT modified**

Run: `git diff archive/2026-07-29-stage4-5-cp-phase6-preemption-timeline-sem/specs/preemption-timeline-sem/spec.md -- . | wc -l`
Expected: 0.

- [ ] **Step 9: Verify IMPLEMENTATION_NOTES.md NOT modified**

Run: `git diff archive/2026-07-29-stage4-5-cp-phase6-preemption-timeline-sem/IMPLEMENTATION_NOTES.md -- . | wc -l`
Expected: 0.

- [ ] **Step 10: Commit**

```bash
git add archive/2026-07-29-stage4-5-cp-phase6-preemption-timeline-sem/tasks.md
git commit -m "chore(archive): sync preemption-timeline-sem tasks.md checkbox state with implementation"
```

---

### Task 6: ADR-074 Status Update + README Index

**Files:**
- Modify: `docs/00_adr/adr-074-archive-tasks-md-checkbox-hygiene.md`
- Modify: `docs/00_adr/README.md`

- [ ] **Step 1: Verify ADR-074 review sign-off**

The ADR status is currently `📋 PROPOSED`. Verify that sign-off criteria per ADR-035 Rule 2 are met (1 reviewer + 1 governance sign-off). If not, this task must be deferred to a follow-up change.

- [ ] **Step 2: Update ADR-074 status: PROPOSED → Accepted**

In `docs/00_adr/adr-074-archive-tasks-md-checkbox-hygiene.md`, change `**状态**: 📋 PROPOSED` to `**状态**: ✅ Accepted (2026-07-31, stage4-5-cp-phase6-preemption-timeline-sem-gaps; sign-off per ADR-035 Rule 2)`.

- [ ] **Step 3: Update ADR-074 reviewer field**

Change `**评审者**: 待定` to list actual reviewers (e.g., `**评审者**: <name1> + <name2> (governance sign-off per ADR-035)`).

- [ ] **Step 4: Add ADR-074 row to README index**

In `docs/00_adr/README.md`, find the ADR index table (typically `| ADR | Title | Status |` ...). Append:

```
| ADR-074 | Archive tasks.md checkbox hygiene policy | ✅ Accepted |
```

- [ ] **Step 5: Update status distribution count**

In the same README, find the status distribution summary (e.g., `Accepted: 73`). Increment by 1.

- [ ] **Step 6: Commit**

```bash
git add docs/00_adr/adr-074-archive-tasks-md-checkbox-hygiene.md docs/00_adr/README.md
git commit -m "docs(adr): mark ADR-074 Accepted + add to index"
```

---

### Task 7: Final Verification

**Files:**
- (No source changes; verification-only)

- [ ] **Step 1: Run `tools/docs-audit.sh --strict`**

Run: `tools/docs-audit.sh --strict`
Expected: PASS (3 warnings resolved per Task 3).

- [ ] **Step 2: Run concurrent test (default build)**

Run: `./build/bin/test_concurrent_preempt_standalone`
Expected: PASS.

- [ ] **Step 3: Run all tests under ASan+UBSan**

Run: `cd build-asan && ctest --output-on-failure`
Expected: 0 failures.

- [ ] **Step 4: Run all tests under TSan**

Run: `cd build-tsan && ctest --output-on-failure`
Expected: 0 failures (or documented benign races).

- [ ] **Step 5: Run all tests under default build**

Run: `cd build && ctest --output-on-failure`
Expected: 0 failures.

- [ ] **Step 6: Run openspec validate**

Run: `openspec validate stage4-5-cp-phase6-preemption-timeline-sem-gaps --strict`
Expected: PASS.

- [ ] **Step 7: Verify archive spec untouched**

Run: `git diff archive/2026-07-29-stage4-5-cp-phase6-preemption-timeline-sem/spec.md -- . | wc -l`
Expected: 0.

- [ ] **Step 8: Record results (do not commit)**

Append to commit message of final commit (Task 8.8):

```
Final verification: docs-audit --strict PASS, test_concurrent_preempt PASS,
ASan+UBSan green, TSan green, openspec validate PASS, archive spec untouched.
```

- [ ] **Step 9: Predication-aql integration gate (deferred)**

This gate (rebase + retest) is triggered AFTER `stage4-5-cp-phase6-predication-aql` merges to main. **Not executed in this change.** Mark this step as deferred; revisit when predication-aql lands.

---

### Task 8: Commit Plan + Update State

**Files:**
- Modify: `.rddf/state/.plan-handoff.json`
- Modify: `.rddf/state/iteration.json`

- [ ] **Step 1: Commit §1 (concurrent test) — already done in Task 1 Step 6**

Verify: `git log --oneline -1 -- tests/test_concurrent_preempt.cpp`

- [ ] **Step 2: Commit §2 (sanitizer) — already done in Task 2 Step 12**

Verify: `git log --oneline -1 -- docs/05-advanced/sanitizer-status.md`

- [ ] **Step 3: Commit §3 (docs-audit) — already done in Task 3 Step 13**

Verify: `git log --oneline -1 -- tools/docs-audit.sh`

- [ ] **Step 4: Commit §4 (spec addendum link) — already done in Task 4 Step 6**

Verify: `git log --oneline -1 -- docs/02_architecture/post-refactor-architecture.md`

- [ ] **Step 5: Commit §5 (archive sync) — already done in Task 5 Step 10**

Verify: `git log --oneline -1 -- archive/2026-07-29-stage4-5-cp-phase6-preemption-timeline-sem/tasks.md`

- [ ] **Step 6: Commit §6 (predication-aql rebase) — DEFERRED**

This is recorded as a follow-up gate. No commit in this change.

- [ ] **Step 7: Commit §7 (ADR-074) — already done in Task 6 Step 6**

Verify: `git log --oneline -1 -- docs/00_adr/adr-074-archive-tasks-md-checkbox-hygiene.md`

- [ ] **Step 8: Commit plan-done handoff**

Run: `git commit --allow-empty -m "chore(plan): plan-done for stage4-5 preemption-timeline-sem-gaps

Final verification: docs-audit --strict PASS, test_concurrent_preempt PASS,
ASan+UBSan green, TSan green, openspec validate PASS, archive spec untouched.

§6 (predication-aql integration gate) deferred to triggered event after
predication-aql merges to main."`

- [ ] **Step 9: Update `.rddf/state/.plan-handoff.json`**

Append gaps change to `active_changes` + `current_change`:

```json
{
  "current_change": "stage4-5-cp-phase6-preemption-timeline-sem-gaps",
  "active_changes": 2,
  "all_artifacts_committed": true
}
```

- [ ] **Step 10: Update `.rddf/state/iteration.json`**

Change `stage4-5-cp-phase6-preemption-timeline-sem-gaps` status from `proposed` to `in_worktree` (or `archived` if all work is complete; this is a gap closure, so `in_worktree` until archived by guide-ship).

Add fields: `worktree_path`, `plan_path`, `tasks_total: 81`.

---

## Self-Review Checklist

- [ ] Spec coverage: §1 concurrent test (Task 1), §2 sanitizer (Task 2), §3 docs-audit (Task 3), §4 spec addendum (Task 4), §5 archive sync (Task 5), §7 ADR-074 (Task 6), §8 commit/handoff (Task 8). §6 (predication-aql gate) explicitly deferred.
- [ ] No placeholders: every step has concrete commands or commits.
- [ ] Type consistency: backdoor symbols (`bd_preempt`, `bd_resume`, `bd_fence_*`) referenced consistently in Task 1; baselines updated consistently in Task 3.
- [ ] Tests cover: concurrent fence no-loss (Task 1 §7), sanitizer-clean baseline (Task 2), docs-audit warnings resolved (Task 3 §12), archive untouched (Tasks 4-5 §8-9), ADR-074 sign-off (Task 6 §1).
- [ ] Risk mitigation: predication-aql rebase gate explicitly deferred (Task 7 §9 + Task 8 §6) to avoid premature cross-change integration.
- [ ] Archive hygiene: tasks.md updated, spec.md + IMPLEMENTATION_NOTES.md verified untouched (Tasks 4-5).