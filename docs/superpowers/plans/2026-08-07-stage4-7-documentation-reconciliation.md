# Stage 4.7 Documentation Reconciliation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Record Stage 4.7 as an implemented architectural outcome, synchronize the HAL contract documentation with the current source, define the trigger-gated Stage 5 roadmap, and link the Stage 4 gap analysis to its archived proposals.

**Architecture:** Use ADR-075 as a retrospective implementation record, not a replacement for ADR-023 or ADR-072. Treat `plugins/gpu_driver/hal/gpu_hal.h` as the canonical HAL inventory: 64 function-pointer fields plus the `hal_heap_ptr` inline helper, reported as 65 total callable entries. Keep Stage 5 explicitly trigger-gated and avoid committing to PM4 or multi-engine implementation before their ADR conditions are met.

**Tech Stack:** Markdown, existing ADR/roadmap conventions, shell-based link and text verification.

---

### Task 1: Add the retrospective Stage 4.7 ADR

**Files:**
- Create: `docs/00_adr/adr-075-stage4-7-bclass-l2-foundation-removal.md`
- Reference: `docs/00_adr/adr-023-hal-interface.md`
- Reference: `docs/00_adr/adr-072-portability-validation.md`
- Reference: `docs/architecture/stage4-gpu-cp-completion-gap-analysis.md`

- [ ] **Step 1: Write the ADR with retrospective scope**

Create an Accepted ADR containing:

- Status: `✅ 已接受 (Accepted)`
- Date: `2026-08-07`
- Context: Stage 4.7 had B-class `drv/ -> sim/` boundary violations after Stage 4.6.
- Decision: record the already-shipped `1 foundation + 5 removals` execution pattern as the implementation outcome of ADR-023 §Decision 4/5 and ADR-072 §Decision 4 revised; do not introduce a new HAL principle.
- Implementation inventory:
  - Foundation Phase 1: fence-id, method codec, heap helper.
  - Foundation Phase 2: graph, mem-pool, stream-capture, queue, and puller HAL operations.
  - Five removals: graph, mem-pool, stream-capture, gpu-queue-emu, hardware-puller-emu.
- Verification: current `drv/` boundary check leaves only the separately scoped `sim_event.h` case, and all five changes are archived.
- Consequences: HAL remains append-only; current inventory is 64 fn-ptrs plus one inline helper; future additions need ADR justification under ADR-023.
- Non-goals: PM4 implementation, independent multi-engine Puller instances, multiprocess support, and removal of `sim_event.h`.
- References: all five proposal files, relevant archive paths, ADR-023, ADR-072, and the gap analysis.

- [ ] **Step 2: Verify the ADR has no unresolved placeholders**

Run:

```bash
grep -nE 'TBD|TODO|待定|未完成' docs/00_adr/adr-075-stage4-7-bclass-l2-foundation-removal.md
```

Expected: no output.

---

### Task 2: Synchronize ADR-023 with the current HAL contract

**Files:**
- Modify: `docs/00_adr/adr-023-hal-interface.md`
- Reference: `plugins/gpu_driver/hal/gpu_hal.h`
- Reference: `docs/02_architecture/post-refactor-architecture.md`

- [ ] **Step 1: Preserve the original ADR decisions and add a current-contract appendix**

Append a dated section to ADR-023 rather than rewriting historical Decision 1/2 text. The section must state:

```text
2026-08-07 current contract:
- 64 function-pointer fields in struct gpu_hal_ops
- hal_heap_ptr is a static inline helper, not a function-pointer field
- documentation shorthand: 65 total callable HAL entries
- canonical source: plugins/gpu_driver/hal/gpu_hal.h
```

List the 15 current groups and their counts: base 11, IOMMU 2, events 3, `mem_map_bo` 1, extended interrupts 2, preemption 2, semaphore 5, green context 2, PDL 2, fence/method 4 plus heap helper 1, graph 7, memory pool 9, stream capture 3, queue 5, puller 5.

Record that Stage 4.7 did not change the append-only rule and that future additions should prefer composition or a grouped ops table once the interface requires another substantial expansion.

- [ ] **Step 2: Verify the documented count against source**

Run:

```bash
awk '/^struct gpu_hal_ops/,/^};/' plugins/gpu_driver/hal/gpu_hal.h \
  | grep -E '^\s+(int|void|u[0-9]+|uint|bool)\s+\(\*[a-z_]+' \
  | wc -l
```

Expected: `64`.

Also run:

```bash
grep -n 'hal_heap_ptr' plugins/gpu_driver/hal/gpu_hal.h
```

Expected: the helper is present outside the function-pointer count.

---

### Task 3: Create the Stage 5 trigger-gated roadmap placeholder

**Files:**
- Create: `docs/roadmap/stage-5-multi-engine-pm4.md`
- Reference: `docs/00_adr/adr-049-cross-engine-synchronization.md`
- Reference: `docs/00_adr/adr-052-aql-pm4-native-support.md`
- Reference: `roadmap.md`

- [ ] **Step 1: Write the Stage 5 placeholder**

Create a roadmap document with:

- Status: `📋 规划中（trigger-gated）`
- Scope: independent COPY/GRAPHICS Puller instances and engine-fence registry; PM4 packet decoding beyond the current `FORMAT_PM4` false-return stub.
- Entry conditions copied from ADR-049 and ADR-052, including the need for a real driver-validation scenario.
- Current baseline: timeline semaphore is delivered; AQL parsing is delivered; PM4 remains deferred; scheduler has COMPUTE/COPY/FIRMWARE classification but no independent graphics Puller.
- Non-goals: multiprocess support (ADR-011), `sim_event.h` boundary cleanup, and unrelated Stage 4 work.
- Readiness checklist requiring a new ADR/design review, standalone tests, integration coverage, and updated gap analysis before implementation.
- Explicit statement that this document does not authorize implementation.

- [ ] **Step 2: Verify all referenced ADR links exist**

Run:

```bash
test -f docs/00_adr/adr-049-cross-engine-synchronization.md
test -f docs/00_adr/adr-052-aql-pm4-native-support.md
```

Expected: exit code `0`.

---

### Task 4: Add proposal cross-links to the Stage 4 gap analysis

**Files:**
- Modify: `docs/architecture/stage4-gpu-cp-completion-gap-analysis.md`
- Reference: `improvements/stage4-l2-foundation-removal-graph.md`
- Reference: `improvements/stage4-l2-foundation-removal-mem-pool.md`
- Reference: `improvements/stage4-l2-foundation-removal-stream-capture.md`
- Reference: `improvements/stage4-l2-foundation-removal-gpu-queue-emu.md`
- Reference: `improvements/stage4-l2-foundation-removal-hardware-puller-emu.md`

- [ ] **Step 1: Add an explicit Phase G proposal table**

Add a table under Phase G with one row per removal, linking the proposal file and recording the implementation/archive commit pair already documented in the roadmap. Preserve the existing historical narrative and do not alter old changelog entries.

- [ ] **Step 2: Verify all five links resolve**

Run:

```bash
for f in \
  improvements/stage4-l2-foundation-removal-graph.md \
  improvements/stage4-l2-foundation-removal-mem-pool.md \
  improvements/stage4-l2-foundation-removal-stream-capture.md \
  improvements/stage4-l2-foundation-removal-gpu-queue-emu.md \
  improvements/stage4-l2-foundation-removal-hardware-puller-emu.md; do
  test -f "$f"
done
```

Expected: exit code `0`.

---

### Task 5: Run final documentation verification

**Files:**
- Verify: `docs/00_adr/adr-075-stage4-7-bclass-l2-foundation-removal.md`
- Verify: `docs/00_adr/adr-023-hal-interface.md`
- Verify: `docs/roadmap/stage-5-multi-engine-pm4.md`
- Verify: `docs/architecture/stage4-gpu-cp-completion-gap-analysis.md`

- [ ] **Step 1: Check repository status and diff scope**

Run:

```bash
git status --short
git diff --stat -- \
  docs/00_adr/adr-075-stage4-7-bclass-l2-foundation-removal.md \
  docs/00_adr/adr-023-hal-interface.md \
  docs/roadmap/stage-5-multi-engine-pm4.md \
  docs/architecture/stage4-gpu-cp-completion-gap-analysis.md
```

Expected: only the four P2 target documents are changed by this plan.

- [ ] **Step 2: Check current-state claims and links**

Run:

```bash
grep -nE '待启动|未启动|TBD|TODO' \
  docs/00_adr/adr-075-stage4-7-bclass-l2-foundation-removal.md \
  docs/roadmap/stage-5-multi-engine-pm4.md
```

Expected: no stale Stage 4 pending claims; Stage 5 may use `trigger-gated` wording but no unresolved placeholders.

Run the five proposal-link checks from Task 4 and the HAL count check from Task 2 again. No commit is part of this plan.
