# H-5 TaskRunner Scope Clarification Implementation Plan

> **⚠️ PARTIALLY SUPERSEDED (2026-07-09)**: The CMake `TASKRUNNER_BUILD_MODE`
> decision from this plan was reversed by
> [`external/TaskRunner/openspec/changes/umd-evolution-build-default-on`](../external/TaskRunner/openspec/changes/umd-evolution-build-default-on/)
> which **flipped the default** to `umd-evolution` (was `test-fixture`).
> The corresponding TaskRunner ADR `tadr-108-build-mode-selection` is now
> marked SUPERSEDED. **The rest of this plan (TADR remap, 3-phase migration,
> scope separation) is still valid and has been executed** — only the
> build-mode default is reversed. See the linked change for rationale.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-step. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reorganize the TaskRunner submodule (`external/TaskRunner/`) into two clearly-separated scopes (test-fixture default + umd-evolution experimental) to eliminate documentation/code/TADR confusion currently mixing stub-tracker with UMD-vision content.

**Architecture:**
- **Single git repo** with directory-level scope separation (no submodule split)
- **TADR remap**: 8 existing TADRs (tadr-001~008) → 16 reclassified TADRs (1xx test-fixture / 2xx umd-evolution / 3xx shared)
- **CMake `TASKRUNNER_BUILD_MODE` option**: ~~default `test-fixture`~~ → **default `umd-evolution` as of 2026-07-09** (see supersede notice above); `test-fixture` retained as opt-out
- **3-phase migration**: A (docs + TADR) → B (code + CMake) → C (cross-repo sync)
- Phase D (UMD PoC) and Phase E (H-3.5/H-7 parallel work) are independent, not in this plan

**Tech Stack:**
- C++17 (existing constraint, no upgrade)
- CMake ≥ 3.20 (required for `set_property(CACHE ... PROPERTY STRINGS)`)
- doctest framework (existing test infrastructure)
- Git submodule workflow (TaskRunner is submodule of UsrLinuxEmu main repo)
- Cross-repo sync via ADR-035 §Rule 5.1 4-step protocol

**Reference Artifacts:**
- Proposal: `openspec/changes/h5-taskrunner-scope-clarification/proposal.md`
- Design: `openspec/changes/h5-taskrunner-scope-clarification/design.md`
- Tasks: `openspec/changes/h5-taskrunner-scope-clarification/tasks.md` (135 high-level tasks)
- Specs: `openspec/changes/h5-taskrunner-scope-clarification/specs/*/spec.md`

---

## Implementation Pre-Flight

### Task 0: Verify Environment

**Files:**
- Read: `external/TaskRunner/` (submodule)
- Read: `docs/00_adr/README.md` (mirror)

- [ ] **Step 1: Verify current branch is main**

```bash
cd /workspace/project/UsrLinuxEmu
git branch --show-current
# Expected: main
```

- [ ] **Step 2: Verify clean working tree**

```bash
cd /workspace/project/UsrLinuxEmu
git status -s
# Expected: clean (except pre-existing external/TaskRunner submodule pointer)
```

- [ ] **Step 3: Verify TaskRunner submodule is initialized**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
git log --oneline -1
# Expected: 912f0dc tools(audit): add TaskRunner-side docs-audit.sh + pre-commit hook + install-hooks.sh
ls AGENTS.md
# Expected: AGENTS.md exists
```

- [ ] **Step 4: Create H-5 work branch in TaskRunner submodule**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
git checkout -b h5-taskrunner-scope-clarification
# This branch lives in TaskRunner's local repo; will be merged to TaskRunner main
```

- [ ] **Step 5: Verify baseline tests pass**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
mkdir -p build && cd build && cmake .. 2>&1 | tail -5
make -j4 2>&1 | tail -10
ctest --output-on-failure 2>&1 | tail -10
# Expected: all 3 ctest registered executables pass (8 + 11 + 12 = 31 doctest TEST_CASEs total, distributed across test_cuda_scheduler / test_gpu_architecture / test_gpu_phase2)
# Note: `ctest -N` shows 3 registered tests, each containing multiple doctest TEST_CASEs internally
```

---

## Phase A: Documentation Reorganization + TADR Remapping (1-2 weeks)

**Scope:** Move/rename docs/TADRs from `docs/{architecture,roadmap,adr,plan.md}` to scoped subdirectories. Create 8 redirect files + 7 new TADRs.

### Task A.1: Create Scoped Directory Structure

**Files:**
- Create: `external/TaskRunner/docs/test-fixture/{architecture,roadmap,adr}/`
- Create: `external/TaskRunner/docs/umd-evolution/{architecture,roadmap,adr}/`
- Create: `external/TaskRunner/docs/shared/`

- [ ] **Step 1: Verify current docs structure**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
ls docs/
# Expected: architecture  adr  plan.md  roadmap
```

- [ ] **Step 2: Create test-fixture scope directories**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
mkdir -p docs/test-fixture/architecture
mkdir -p docs/test-fixture/roadmap
mkdir -p docs/test-fixture/adr
```

- [ ] **Step 3: Create umd-evolution scope directories**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
mkdir -p docs/umd-evolution/architecture
mkdir -p docs/umd-evolution/roadmap
mkdir -p docs/umd-evolution/adr
```

- [ ] **Step 4: Create shared scope directory**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
mkdir -p docs/shared
```

- [ ] **Step 5: Verify new structure**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
find docs/test-fixture docs/umd-evolution docs/shared -type d | sort
# Expected: 7 directories total (3 + 3 + 1)
```

- [ ] **Step 6: Commit directory scaffolding**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
git add docs/test-fixture docs/umd-evolution docs/shared
git commit -m "chore(docs): H-5 A.1 - create scoped directory structure"
```

### Task A.2: Move Existing Docs (use git mv for History)

**Files:**
- Move: `docs/architecture/*` → `docs/test-fixture/architecture/`
- Move: `docs/roadmap/*` → `docs/test-fixture/roadmap/`
- Move: `docs/plan.md` → `docs/umd-evolution/vision-source.md`

- [ ] **Step 1: Move architecture docs (git mv preserves history)**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
git mv docs/architecture/* docs/test-fixture/architecture/
rmdir docs/architecture 2>/dev/null
```

- [ ] **Step 2: Verify architecture history preserved**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
git log --follow docs/test-fixture/architecture/current.md 2>&1 | head -3
# Expected: shows original docs/architecture/current.md commits
```

- [ ] **Step 3: Move roadmap docs**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
git mv docs/roadmap/* docs/test-fixture/roadmap/
rmdir docs/roadmap 2>/dev/null
```

- [ ] **Step 4: Move plan.md to umd-evolution as vision-source**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
git mv docs/plan.md docs/umd-evolution/vision-source.md
```

- [ ] **Step 5: Verify all moves complete**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
ls docs/
# Expected: only test-fixture/, umd-evolution/, shared/
ls docs/test-fixture/architecture/ docs/test-fixture/roadmap/
ls docs/umd-evolution/vision-source.md
```

- [ ] **Step 6: Commit moves**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
git add -A
git commit -m "chore(docs): H-5 A.2 - move existing docs to scoped subdirectories"
```

### Task A.3: Create Scope READMEs

**Files:**
- Create: `external/TaskRunner/docs/test-fixture/README.md`
- Create: `external/TaskRunner/docs/umd-evolution/README.md`
- Create: `external/TaskRunner/docs/shared/README.md`

- [ ] **Step 1: Create test-fixture/README.md**

Write this content to `external/TaskRunner/docs/test-fixture/README.md`:

```markdown
---
SCOPE: TEST-FIXTURE
STATUS: ACTIVE
---

# test-fixture Scope

This directory contains all **test-fixture scope** content for TaskRunner — the currently-shippable state.

## In Scope

- `IGpuDriver` abstraction (28 methods, see `../shared/adr/tadr-301-igpu-driver-contract.md`)
- `GpuDriverClient` (real `/dev/gpgpu0` ioctl)
- `CudaStub` (in-memory mock with `next_handle_` monotonic + existence tracking)
- `MockGpuDriver` (headless test fixture)
- `CudaScheduler` + `cmd_cuda` CLI (6 subcommands)
- Tests: `test_cuda_scheduler` (8 cases), `test_gpu_architecture` (11 cases), `test_gpu_phase2` (12 cases)

## Subdirectories

- `architecture/` — Test-fixture architecture documentation
- `roadmap/` — Test-fixture roadmap (phase-1, phase-1.5, phase-2, phase-3)
- `adr/` — Test-fixture scope TADRs (tadr-101~106 + redirect files for old tadr-004~008)

## Cross-Scope References

- For UMD evolution vision: see `../umd-evolution/README.md` `[UMD-EVOLUTION SCOPE]`
- For shared infrastructure: see `../shared/README.md`
```

- [ ] **Step 2: Create umd-evolution/README.md**

Write this content to `external/TaskRunner/docs/umd-evolution/README.md`:

```markdown
---
SCOPE: UMD-EVOLUTION
STATUS: PROPOSED
IMPLEMENTED: NO
---

# umd-evolution Scope

This directory contains **umd-evolution scope** vision documents. **No production implementation.**

## In Scope (Vision Only)

- Minimal CUDA Runtime API surface (cudaMalloc / cudaMemcpy / cudaLaunchKernel PoC)
- Doorbell mmap bypass (PoC, see AMD ROCm `amd_aql_queue.cpp:482-493`)
- Ring buffer self-management (PoC)
- ELF + CUBIN parser (PoC)
- Stream object model (vision)
- Context (CUcontext) model (vision)

## Status Rules

**`STATUS: ACCEPTED` is FORBIDDEN** for unimplemented features. Use `STATUS: PROPOSED` or `STATUS: DRAFT`.

## Subdirectories

- `vision-source.md` — Original `plan.md` v0.1 content (preserved as historical reference)
- `vision.md` — Curated UMD vision extracted from `vision-source.md`
- `gap-analysis.md` — Gap analysis vs AMD ROCm / NVIDIA CUDA UMDs (2026-06-24 research)
- `architecture/` — UMD architecture vision (future Phase D)
- `roadmap/` — UMD PoC roadmap (deferred until Phase D)
- `adr/` — UMD scope TADRs (tadr-201~205 + redirect files for old tadr-001~003)

## Cross-Scope References

- For test-fixture (current main): see `../test-fixture/README.md`
- For shared infrastructure: see `../shared/README.md`
```

- [ ] **Step 3: Create shared/README.md**

Write this content to `external/TaskRunner/docs/shared/README.md`:

```markdown
---
SCOPE: SHARED
STATUS: ACTIVE
CROSS_REFS: [ADR-036](../../../docs/00_adr/adr-036-three-way-separation.md) (3-Way Architectural Separation), [ADR-035](../../../docs/00_adr/adr-035-governance-policy.md) (Governance Policy)
---

# shared Scope

This directory contains **shared infrastructure** that both test-fixture and umd-evolution scopes depend on.

## Cross-Repo ABI Contract (ADR-036 Alignment)

The TaskRunner `include/shared/` layer is the **TaskRunner-side implementation** of the shared ABI contract defined by [UsrLinuxEmu ADR-036 §Decision](../../../docs/00_adr/adr-036-three-way-separation.md) §Decision line 41:

> **shared**: ABI 契约 — `plugins/gpu_driver/shared/`（`gpu_ioctl.h`, `gpu_types.h`, `gpu_queue.h`）— TaskRunner 与 UsrLinuxEmu 共享头文件 — 既不属于 ② 也不属于 ③，仅作为可移植驱动与外部 consumer 之间的契约。

| TaskRunner side | UsrLinuxEmu side | Contract role |
|---|---|---|
| `include/shared/igpu_driver.hpp` | `plugins/gpu_driver/shared/gpu_ioctl.h` (via UsrLinuxEmu symlink) | IGpuDriver 28-method interface ↔ ioctl 派发表 |
| `include/shared/sync_primitives.hpp` | (no direct counterpart) | TaskRunner-internal cross-cutting sync primitives |
| `include/shared/error_handling.hpp` (H-5 placeholder) | (no direct counterpart) | Result<T> + ErrorCode enum |
| `include/shared/memory_manager.hpp` (H-5 v2) | (no direct counterpart) | Memory manager shared by test-fixture and umd-evolution |

**Cross-repo modification policy**: Per ADR-036 §Risk 表 "shared 头文件双方不同步" — TaskRunner and UsrLinuxEmu maintainers must perform **dual-ack** on every shared contract change. The existing UsrLinuxEmu symlink (`external/TaskRunner/UsrLinuxEmu/`) triggers immediate diff visibility for cross-repo reviewers.

## In Scope

- `IGpuDriver` interface contract (28 methods) — see `adr/tadr-301-igpu-driver-contract.md`
- Sync primitives (MPSC queue, atomic counter, mutex wrappers) — see `adr/tadr-302-sync-primitives.md`
- Error handling abstractions (Result<T> + ErrorCode enum) — see `adr/tadr-303-error-handling.md`
- Memory manager (H-5 v2 addition, used by `cuda_scheduler`) — see `../../include/shared/memory_manager.hpp`

## Review Requirements (Dual Approval)

**Any shared-scope change requires dual approval:**

1. At least 1 test-fixture scope maintainer
2. At least 1 umd-evolution scope maintainer (or designee if no active maintainer)
3. **For ABI-contract-affecting changes** (anything in `igpu_driver.hpp` or related to `gpu_ioctl.h`): also notify UsrLinuxEmu maintainer per ADR-036 cross-repo policy

This prevents unilateral breakage of either scope.

## Subdirectories

- `adr/` — Shared scope TADRs (tadr-107 shared boundary + tadr-301~303 contracts)

## Cross-Scope References

- For test-fixture: see `../test-fixture/README.md`
- For umd-evolution: see `../umd-evolution/README.md`
- For UsrLinuxEmu 3-way architectural principle: see [ADR-036](../../../docs/00_adr/adr-036-three-way-separation.md)
```

- [ ] **Step 4: Commit READMEs**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
git add docs/test-fixture/README.md docs/umd-evolution/README.md docs/shared/README.md
git commit -m "docs(scope): H-5 A.3 - add scope READMEs (test-fixture/umd-evolution/shared)"
```

### Task A.4: Create UMD Vision + Gap Analysis Docs

**Files:**
- Create: `external/TaskRunner/docs/umd-evolution/vision.md`
- Create: `external/TaskRunner/docs/umd-evolution/gap-analysis.md`

- [ ] **Step 1: Read original plan.md v0.1 (vision source)**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
wc -l docs/umd-evolution/vision-source.md
# Expected: ~300+ lines (original plan.md content)
```

- [ ] **Step 2: Create vision.md (curated UMD vision)**

Write this content to `external/TaskRunner/docs/umd-evolution/vision.md`:

```markdown
---
SCOPE: UMD-EVOLUTION
STATUS: PROPOSED
IMPLEMENTED: NO
SOURCE: docs/umd-evolution/vision-source.md
---

# UMD Vision

> **Note:** This document describes a future evolution path. **No code is currently implemented.**

## Goals

1. Provide a minimal CUDA Runtime API surface (cudaMalloc / cudaMemcpy / cudaLaunchKernel) as PoC
2. Implement doorbell mmap bypass for low-latency command submission (following AMD ROCm pattern)
3. Support ring buffer self-management with explicit lifecycle
4. Enable ELF + CUBIN parser for kernel module loading (PoC)

## Non-Goals

- Full CUDA Runtime API coverage (Phase 1: 3 APIs only)
- Production-quality performance optimization
- Multi-GPU support (deferred to later)
- CUDA Graph API
- C++ template support (HIP-style)
- Cross-platform support (Linux x86_64 only)

## API Surface (PoC Target)

| API | Status | Notes |
|-----|--------|-------|
| `cudaMalloc` / `cudaFree` | PoC target | Basic device memory allocation |
| `cudaMemcpy` (H2D / D2H / D2D) | PoC target | Synchronous copy via ioctl |
| `cudaLaunchKernel` | PoC target | Launch by kernel name (not CUfunction handle) |

## Phasing

- **Phase D-1** (1-2 weeks): Doorbell mmap bypass + Ring buffer skeleton
- **Phase D-2** (2-4 weeks): Minimal CUDA Runtime API surface
- **Phase D-3** (4-8 weeks): ELF + CUBIN parser + Kernel launch by name

## Risks

- **Performance**: PoC will not match real NVIDIA libcuda.so performance
- **Scope creep**: Tempting to expand scope; resist
- **Maintenance burden**: Experimental code can rot if not regularly validated

## References

- AMD ROCm UMD: `amd_aql_queue.cpp:482-493` (doorbell bypass)
- NVIDIA CUDA: libcuda.so architecture (cu* Driver API)
- UsrLinuxEmu roadmap: `docs/roadmap/stage-2-multi-device.md`
```

- [ ] **Step 3: Create gap-analysis.md (vs AMD ROCm / NVIDIA CUDA)**

Write this content to `external/TaskRunner/docs/umd-evolution/gap-analysis.md`:

```markdown
---
SCOPE: UMD-EVOLUTION
STATUS: PROPOSED
IMPLEMENTED: NO
---

# Gap Analysis vs Production UMDs

## vs AMD ROCm UMD (libhsa-runtime64.so + libamdhip64.so)

| Capability | ROCm | TaskRunner | Gap |
|------------|------|------------|-----|
| AQL packet construction | User-space | ❌ None | High (need new AQL packet builder) |
| Doorbell direct MMIO write | User-space (WC mmap) | ❌ None | High (need mmap exposure) |
| Ring buffer self-management | User-space | ❌ None | Medium |
| EOP buffer + CWSR allocation | User-space | ❌ None | Low (deferrable) |
| Built-in blit shader | User-space (compiled at runtime) | ❌ None | Medium |
| SDMA packet construction | User-space | ❌ None | Medium |
| Code object (.hsaco) loading | User-space | ❌ None | High |
| Kernel arg serialization | User-space | ❌ None | Medium |
| Stream object | libhsa stream | ❌ None (uses u32 id) | Medium |
| KFD ioctl communication | libhsakmt (thin wrapper) | ✅ Equivalent (GpuDriverClient) | None |

## vs NVIDIA CUDA UMD (libcuda.so)

| Capability | NVIDIA | TaskRunner | Gap |
|------------|--------|------------|-----|
| PTX JIT compilation | libnvidia-ptxjitcompiler | ❌ None | High (huge lib) |
| Fatbin parser | libnvidia-fatbinaryloader | ❌ None | Medium |
| libnvrtc | nvrtcCompileProgram | ❌ None | High |
| Module loading | cuModuleLoad* | ❌ None | High |
| Kernel launch | cuLaunchKernel (by CUfunction) | ❌ None (uses string name) | High |
| Memory allocation | cuMemAlloc | ⚠️ Partial (BO alloc) | Medium |
| Context (CUcontext) | libcuda state | ❌ None | Medium |
| Stream (CUstream) | libcuda state | ❌ None | Medium |
| UVM | libnvidia-uvm | ⚠️ Partial (VA Space) | Medium |
| MPS support | libcuda built-in | ❌ None | Low (out of scope) |

## Critical Missing Capabilities (Ranked)

1. **CUmodule loading** (libcuda + libamdhip equivalent): FATBIN/ELF parsing, symbol resolution
2. **AQL/Command packet construction**: Hardware-specific packet format
3. **Doorbell MMIO exposure**: mmap doorbell page to user-space
4. **Stream/Context object models**: Replace u32 handles with opaque pointers
5. **Kernel arg serialization**: Marshall args into kernarg buffer
6. **PTX JIT** (NVIDIA only): Requires libnvrtc + libnvidia-ptxjitcompiler integration

## Effort Estimates

- **Phase D-1** (Doorbell bypass + Ring skeleton): 2-4 weeks
- **Phase D-2** (Minimal CUDA API surface): 4-8 weeks
- **Phase D-3** (ELF parser + Kernel launch): 8-12 weeks
- **Total**: 14-24 weeks (~3-6 months)

## Recommendation

**Do NOT pursue UMD evolution as primary goal.** Reasons:
1. UsrLinuxEmu blueprint explicitly defers this work
2. Investment vs ROI is poor (3-6 months for PoC that won't match production libcuda.so)
3. Maintenance burden on small team

**Alternative:** Continue strengthening test-fixture scope (CudaStub + GpuDriverClient + CLI) as primary value-add.
```

- [ ] **Step 4: Commit vision + gap-analysis**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
git add docs/umd-evolution/vision.md docs/umd-evolution/gap-analysis.md
git commit -m "docs(scope): H-5 A.4 - UMD vision + gap-analysis (vs ROCm/CUDA UMDs)"
```

### Task A.5: Remap 8 Existing TADRs (git mv for History)

**Files:** Move + rename 8 TADRs

| Old | New |
|-----|-----|
| `docs/adr/tadr-001-cuda-vulkan-runtime-unified-scheduler.md` | `docs/umd-evolution/adr/tadr-201-unified-scheduler.md` |
| `docs/adr/tadr-002-cuda-vulkan-runtime-layered-design.md` | `docs/umd-evolution/adr/tadr-202-layered-design.md` |
| `docs/adr/tadr-003-cuda-vulkan-runtime-sync-unified-internal.md` | `docs/umd-evolution/adr/tadr-203-sync-unified.md` |
| `docs/adr/tadr-004-cuda-vulkan-runtime-stub-tracker.md` | `docs/test-fixture/adr/tadr-101-stub-tracker.md` |
| `docs/adr/tadr-005-h2-5-igpu-driver-consumer-lens.md` | `docs/test-fixture/adr/tadr-102-igpu-driver.md` |
| `docs/adr/tadr-006-h3-phase2-consumer-lens.md` | `docs/test-fixture/adr/tadr-103-h3-phase2.md` |
| `docs/adr/tadr-007-r2-mapping-contract.md` | `docs/test-fixture/adr/tadr-104-r2-mapping.md` |
| `docs/adr/tadr-008-h7-deferred-mirror.md` | `docs/test-fixture/adr/tadr-105-h7-deferred.md` |

- [ ] **Step 1: Move + rename 3 umd-evolution TADRs**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
git mv docs/adr/tadr-001-cuda-vulkan-runtime-unified-scheduler.md docs/umd-evolution/adr/tadr-201-unified-scheduler.md
git mv docs/adr/tadr-002-cuda-vulkan-runtime-layered-design.md docs/umd-evolution/adr/tadr-202-layered-design.md
git mv docs/adr/tadr-003-cuda-vulkan-runtime-sync-unified-internal.md docs/umd-evolution/adr/tadr-203-sync-unified.md
```

- [ ] **Step 2: Move + rename 5 test-fixture TADRs**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
git mv docs/adr/tadr-004-cuda-vulkan-runtime-stub-tracker.md docs/test-fixture/adr/tadr-101-stub-tracker.md
git mv docs/adr/tadr-005-h2-5-igpu-driver-consumer-lens.md docs/test-fixture/adr/tadr-102-igpu-driver.md
git mv docs/adr/tadr-006-h3-phase2-consumer-lens.md docs/test-fixture/adr/tadr-103-h3-phase2.md
git mv docs/adr/tadr-007-r2-mapping-contract.md docs/test-fixture/adr/tadr-104-r2-mapping.md
git mv docs/adr/tadr-008-h7-deferred-mirror.md docs/test-fixture/adr/tadr-105-h7-deferred.md
```

- [ ] **Step 3: Remove empty old docs/adr directory**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
rmdir docs/adr 2>/dev/null || echo "docs/adr not empty, check contents"
ls docs/
# Expected: only test-fixture/ umd-evolution/ shared/
```

- [ ] **Step 4: Verify history preservation (sample 1)**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
git log --follow docs/test-fixture/adr/tadr-101-stub-tracker.md | head -5
# Expected: shows original tadr-004 commit history
```

- [ ] **Step 5: Verify history preservation (sample 2)**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
git log --follow docs/umd-evolution/adr/tadr-201-unified-scheduler.md | head -5
# Expected: shows original tadr-001 commit history
```

- [ ] **Step 6: Add SCOPE + REPLACES metadata to renamed TADRs**

For each renamed TADR, prepend this header (adjust scope/status/replaces per the table):

```markdown
---
SCOPE: <test-fixture|umd-evolution>
STATUS: <ACCEPTED|PROPOSED>
REPLACES: tadr-<NNN>
---

```

Apply via shell:

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
# Test-fixture TADRs (STATUS: ACCEPTED)
for tadr in tadr-101-stub-tracker tadr-102-igpu-driver tadr-103-h3-phase2 tadr-104-r2-mapping tadr-105-h7-deferred; do
    original=$(echo "$tadr" | sed 's/-[^-]*$//')  # e.g., tadr-101
    new_num=$(echo "$original" | grep -oE '[0-9]+')  # e.g., 101
    old_num=$((new_num - 100 + 4))  # e.g., 105 -> 9; this is approximate, use direct mapping
done
# Use direct mapping instead (more reliable)
# tadr-101 replaces tadr-004, etc.
```

For each file, manually prepend the header (shell sed for batch update):

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
# test-fixture TADRs
for f in docs/test-fixture/adr/tadr-10{1,2,3,4,5}-*.md; do
    sed -i '1i---\nSCOPE: TEST-FIXTURE\nSTATUS: ACCEPTED\nREPLACES: tadr-NNN\n---\n' "$f"
done
# umd-evolution TADRs
for f in docs/umd-evolution/adr/tadr-20{1,2,3}-*.md; do
    sed -i '1i---\nSCOPE: UMD-EVOLUTION\nSTATUS: PROPOSED\nREPLACES: tadr-NNN\n---\n' "$f"
done
```

Then manually edit REPLACES field for each file (since mapping is 1:1 with rename):

- tadr-101 → REPLACES: tadr-004
- tadr-102 → REPLACES: tadr-005
- tadr-103 → REPLACES: tadr-006
- tadr-104 → REPLACES: tadr-007
- tadr-105 → REPLACES: tadr-008
- tadr-201 → REPLACES: tadr-001
- tadr-202 → REPLACES: tadr-002
- tadr-203 → REPLACES: tadr-003

- [ ] **Step 7: Commit TADR remapping**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
git add -A
git commit -m "docs(adr): H-5 A.5 - remap 8 TADRs (test-fixture 1xx + umd-evolution 2xx)"
```

### Task A.6: Create 8 Redirect Files (Backward Compatibility)

**Files:** 8 redirect files

- [ ] **Step 1: Create redirect template**

Template for each redirect file (place at `docs/<scope>/adr/tadr-<OLD>-redirect.md`):

```markdown
---
SCOPE: <test-fixture|umd-evolution>
STATUS: DEPRECATED
REPLACES: tadr-<OLD_NUMBER>
REDIRECT_TO: <NEW_RELATIVE_PATH>
---

# DEPRECATED: This document has been renamed

**New location:** [tadr-<NEW>-<new-name>.md](<NEW_RELATIVE_PATH>)

**Reason:** H-5 TaskRunner scope clarification (2026-06-24). Content is unchanged.

**Git history:** Preserved via `git mv` — use `git log --follow <new-path>`.
```

- [ ] **Step 2: Create test-fixture redirect files (5 files)**

Create each file with content matching the redirect template:

- `docs/test-fixture/adr/tadr-004-redirect.md` → REDIRECT_TO: `tadr-101-stub-tracker.md`
- `docs/test-fixture/adr/tadr-005-redirect.md` → REDIRECT_TO: `tadr-102-igpu-driver.md`
- `docs/test-fixture/adr/tadr-006-redirect.md` → REDIRECT_TO: `tadr-103-h3-phase2.md`
- `docs/test-fixture/adr/tadr-007-redirect.md` → REDIRECT_TO: `tadr-104-r2-mapping.md`
- `docs/test-fixture/adr/tadr-008-redirect.md` → REDIRECT_TO: `tadr-105-h7-deferred.md`

- [ ] **Step 3: Create umd-evolution redirect files (3 files)**

- `docs/umd-evolution/adr/tadr-001-redirect.md` → REDIRECT_TO: `tadr-201-unified-scheduler.md`
- `docs/umd-evolution/adr/tadr-002-redirect.md` → REDIRECT_TO: `tadr-202-layered-design.md`
- `docs/umd-evolution/adr/tadr-003-redirect.md` → REDIRECT_TO: `tadr-203-sync-unified.md`

- [ ] **Step 4: Verify all 8 redirect files exist**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
find docs -name "*-redirect.md" | sort
# Expected: 8 files
```

- [ ] **Step 5: Commit redirect files**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
git add docs/test-fixture/adr/*-redirect.md docs/umd-evolution/adr/*-redirect.md
git commit -m "docs(adr): H-5 A.6 - add 8 redirect files for TADR backward compatibility"
```

### Task A.7: Create 7 New TADRs

**Files:** 7 new TADR files

| New TADR | Scope | Status | Path |
|----------|-------|--------|------|
| tadr-106-test-fixture-scope-clarification | test-fixture | ACCEPTED | `docs/test-fixture/adr/` |
| tadr-107-shared-infrastructure-boundary | shared | ACCEPTED | `docs/shared/adr/` |
| tadr-204-umd-evolution-scope-clarification | umd-evolution | PROPOSED | `docs/umd-evolution/adr/` |
| tadr-205-umd-evolution-poc-roadmap | umd-evolution | PROPOSED | `docs/umd-evolution/adr/` |
| tadr-301-igpu-driver-contract | shared | ACCEPTED | `docs/shared/adr/` |
| tadr-302-sync-primitives | shared | ACCEPTED | `docs/shared/adr/` |
| tadr-303-error-handling | shared | ACCEPTED | `docs/shared/adr/` |

- [ ] **Step 1: Create tadr-106-test-fixture-scope-clarification.md**

Path: `external/TaskRunner/docs/test-fixture/adr/tadr-106-test-fixture-scope-clarification.md`

```markdown
---
SCOPE: TEST-FIXTURE
STATUS: ACCEPTED
DECISION_DATE: 2026-06-24
---

# ADR: test-fixture Scope Clarification (H-5)

## Context

TaskRunner submodule previously mixed test-fixture code (H-1/H-2.5/H-3 shippable) with UMD vision content (tadr-001~003 unimplemented). This caused confusion about what is currently working vs what is aspirational.

## Decision

Formally establish the **test-fixture** scope as the default-main state of TaskRunner.

**In scope:**
- `IGpuDriver` 28-method abstraction
- `GpuDriverClient` (real `/dev/gpgpu0` ioctl)
- `CudaStub` (in-memory mock with `next_handle_` + existence tracking)
- `MockGpuDriver` (headless test fixture)
- `CudaScheduler` + `cmd_cuda` CLI (6 subcommands)
- Tests: `test_cuda_scheduler` (8 cases), `test_gpu_architecture` (11 cases), `test_gpu_phase2` (12 cases)

**Out of scope (belongs to umd-evolution):**
- Real CUDA Runtime API surface
- CUmodule/CUfunction loading
- Doorbell mmap bypass
- Ring buffer self-management
- Stream object model
- Context (CUcontext) model
- Unified Memory page table

## Consequences

Positive:
- Clear scope separation eliminates confusion
- New contributors can quickly identify "what's working" vs "what's vision"
- H-3.5 follow-up work has unambiguous scope

Negative:
- Documentation overhead (3 separate scope directories)
- Dual review requirement for shared-scope changes

## References

- tadr-101 through tadr-105 (existing test-fixture ADRs)
- H-5 proposal: `openspec/changes/h5-taskrunner-scope-clarification/proposal.md`
```

- [ ] **Step 2: Create tadr-107-shared-infrastructure-boundary.md**

Path: `external/TaskRunner/docs/shared/adr/tadr-107-shared-infrastructure-boundary.md`

```markdown
---
SCOPE: SHARED
STATUS: ACCEPTED
DECISION_DATE: 2026-06-24
---

# ADR: Shared Infrastructure Boundary (H-5)

## Context

The TaskRunner codebase has cross-cutting abstractions (IGpuDriver interface, sync primitives, error handling) that are used by both the test-fixture scope and the (future) umd-evolution scope. Without a clear boundary, changes to shared abstractions can unilaterally break one scope.

## Decision

Define **shared scope** as the cross-cutting abstractions, with the following rules:

**In scope (shared):**
- `IGpuDriver` interface contract (28 methods)
- Sync primitives (`include/shared/sync_primitives.hpp`)
- Error handling abstractions (`include/shared/error_handling.hpp`)

**Out of scope (scope-specific):**
- Business logic (test-fixture's CudaStub/Scheduler, future umd's CUDA API surface)
- Hardware-specific code (real CUDA Runtime, doorbell MMIO)

## Review Requirements

**Dual approval required for any shared-scope change:**
1. At least 1 test-fixture scope maintainer
2. At least 1 umd-evolution scope maintainer (or designee)

This prevents unilateral breakage.

## References

- tadr-301 through tadr-303 (specific shared contracts)
- H-5 design.md §Decision 4: "Shared area review 严格化"
```

- [ ] **Step 3: Create tadr-204-umd-evolution-scope-clarification.md**

Path: `external/TaskRunner/docs/umd-evolution/adr/tadr-204-umd-evolution-scope-clarification.md`

```markdown
---
SCOPE: UMD-EVOLUTION
STATUS: PROPOSED
DECISION_DATE: 2026-06-24
IMPLEMENTED: NO
---

# ADR: umd-evolution Scope Clarification (H-5)

## Context

The umd-evolution scope describes a future evolution path toward a CUDA Runtime / User-Mode Driver. Without clear scope rules, contributors may assume unimplemented features are available.

## Decision

**Status rules:**
- All umd-evolution content MUST have `STATUS: PROPOSED` or `STATUS: DRAFT`
- **`STATUS: ACCEPTED` is FORBIDDEN** for unimplemented features
- All `.cpp`/`.hpp` files in `src/umd/` and `include/umd/` MUST have `// SCOPE: UMD-EVOLUTION` header comment

**Build mode (SUPERSEDED 2026-07-09 — see file-top notice):**
- ~~Default `TASKRUNNER_BUILD_MODE=test-fixture` does NOT compile umd-evolution code~~ → Now: default `umd-evolution` compiles full UMD + tests
- ~~Opt-in `TASKRUNNER_BUILD_MODE=umd-evolution` compiles experimental skeleton~~ → Now: opt-out `test-fixture` excludes `libcuda_shim` + `tests/umd/`
- See [`external/TaskRunner/openspec/changes/umd-evolution-build-default-on`](../external/TaskRunner/openspec/changes/umd-evolution-build-default-on/) for the reversal rationale

## Consequences

- Contributors cannot accidentally depend on unimplemented UMD features
- umd-evolution code is isolated from main test-fixture build

## References

- H-5 design.md §Decision 5: "umd-evolution 代码骨架仅占位"
- tadr-205-umd-evolution-poc-roadmap (deferred PoC phasing)
```

- [ ] **Step 4: Create tadr-205-umd-evolution-poc-roadmap.md**

Path: `external/TaskRunner/docs/umd-evolution/adr/tadr-205-umd-evolution-poc-roadmap.md`

```markdown
---
SCOPE: UMD-EVOLUTION
STATUS: PROPOSED
DECISION_DATE: 2026-06-24
IMPLEMENTED: NO
---

# ADR: UMD Evolution PoC Roadmap (H-5)

## Context

If/when umd-evolution PoC work begins, it needs a clear phasing to avoid scope creep.

## Decision

Defer until all of the following are true:
1. H-5 cross-repo sync merged (this change)
2. H-3.5 follow-up work shipped
3. Explicit PoC requirement identified (currently none)

## Proposed Phasing (when initiated)

**Phase D-1** (2-4 weeks):
- Doorbell mmap bypass skeleton (following AMD ROCm `amd_aql_queue.cpp:482-493`)
- Ring buffer self-management skeleton
- Build verification under `TASKRUNNER_BUILD_MODE=umd-evolution`

**Phase D-2** (4-8 weeks):
- Minimal CUDA Runtime API surface (cudaMalloc / cudaMemcpy / cudaLaunchKernel)
- Kernel launch by name (no CUfunction handle yet)

**Phase D-3** (8-12 weeks):
- ELF + CUBIN parser
- Kernel arg serialization
- Real kernel execution via UsrLinuxEmu's BasicGpuSimulator

**Total**: 14-24 weeks (~3-6 months)

## Recommendation

**Do NOT pursue this work as primary goal.** See `gap-analysis.md` for ROI analysis.

## References

- `docs/umd-evolution/vision.md` — UMD complete vision
- `docs/umd-evolution/gap-analysis.md` — vs ROCm/CUDA gap analysis
```

- [ ] **Step 5: Create tadr-301-igpu-driver-contract.md**

Path: `external/TaskRunner/docs/shared/adr/tadr-301-igpu-driver-contract.md`

```markdown
---
SCOPE: SHARED
STATUS: ACCEPTED
DECISION_DATE: 2026-06-24
---

# ADR: IGpuDriver Interface Contract (H-5)

## Context

`IGpuDriver` is the 28-method abstract interface that decouples TaskRunner from specific GPU implementations. It was introduced in H-2.5 and extended in H-3. It must remain ABI-stable for both test-fixture (current) and umd-evolution (future) consumers.

## Decision

`IGpuDriver` MUST preserve the following invariants:

**Stability rules:**
1. All 28 method signatures MUST NOT change without ADR + major version bump
2. Adding new methods is allowed (backward-compatible)
3. Deprecating methods is allowed (must provide alternative)
4. Removing methods is FORBIDDEN

**Naming conventions:**
- snake_case for all methods (e.g., `alloc_bo`, `submit_batch`)
- UPPER_SNAKE_CASE for constants (e.g., `GPU_IOCTL_*`)
- CamelCase for classes (e.g., `GpuDriverClient`)

**Error handling:**
- Return 0 on success
- Return negative Linux error code on failure (e.g., -EINVAL, -ENOMEM)
- Never throw exceptions

## References

- `include/shared/igpu_driver.hpp` — Canonical interface definition
- H-2.5 tadr-102 (original design)
- H-3 tadr-103 (Phase 2 extension)
```

- [ ] **Step 6: Create tadr-302-sync-primitives.md**

Path: `external/TaskRunner/docs/shared/adr/tadr-302-sync-primitives.md`

```markdown
---
SCOPE: SHARED
STATUS: ACCEPTED
DECISION_DATE: 2026-06-24
---

# ADR: Sync Primitives Abstraction (H-5)

## Context

TaskRunner needs cross-cutting synchronization primitives (MPSC queue, atomic counter, mutex wrappers) used by both test-fixture (current) and umd-evolution (future) scopes.

## Decision

`include/shared/sync_primitives.hpp` provides:

- `MpscQueue<T>` — Multi-producer single-consumer lock-free queue
- `AtomicCounter` — 64-bit atomic counter with relaxed/acquire/release semantics
- `Mutex` — Thin wrapper around `std::mutex` (test-fixture only) or platform-specific (umd-evolution)
- `ConditionVariable` — Companion to Mutex

**Rules:**
- All primitives MUST be header-only (template-based) for inlining
- All primitives MUST be `noexcept`
- All primitives MUST support both single-threaded (test) and multi-threaded (prod) usage

## References

- `include/shared/sync_primitives.hpp` — Canonical header
- TaskRunner original `EventQueue` implementation (reference)
```

- [ ] **Step 7: Create tadr-303-error-handling.md**

Path: `external/TaskRunner/docs/shared/adr/tadr-303-error-handling.md`

```markdown
---
SCOPE: SHARED
STATUS: ACCEPTED
DECISION_DATE: 2026-06-24
---

# ADR: Error Handling Abstraction (H-5)

## Context

TaskRunner needs a consistent error handling pattern across scopes. Current code mixes Linux error codes (return values), exceptions (none currently), and custom enums.

## Decision

`include/shared/error_handling.hpp` provides:

- `Result<T>` — Tagged union of `T` value or `ErrorCode`
- `ErrorCode` enum — Linux-style error codes (EINVAL, ENOMEM, EREMOTEIO, etc.)
- `make_error(ErrorCode)` — Factory function for error Results

**Rules:**
- All public APIs MUST return `Result<T>` for fallible operations
- All public APIs MUST return `T` directly for infallible operations
- Exceptions are FORBIDDEN in shared-scope code (test-fixture follows same rule)
- Linux error codes MUST be preserved (no custom enum mapping)

## References

- `include/shared/error_handling.hpp` — Canonical header
- Linux kernel error code conventions
```

- [ ] **Step 8: Commit 7 new TADRs**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
git add docs/test-fixture/adr/tadr-106-*.md \
        docs/shared/adr/tadr-107-*.md \
        docs/shared/adr/tadr-30{1,2,3}-*.md \
        docs/umd-evolution/adr/tadr-20{4,5}-*.md
git commit -m "docs(adr): H-5 A.7 - add 7 new TADRs (3 scope + 4 shared contracts)"
```

### Task A.8: Update AGENTS.md with Scope Rules

**Files:**
- Modify: `external/TaskRunner/AGENTS.md`

- [ ] **Step 1: Read current AGENTS.md**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
wc -l AGENTS.md
head -50 AGENTS.md
```

- [ ] **Step 2: Append scope classification section**

Append this content to `external/TaskRunner/AGENTS.md`:

```markdown

## Scope Classification (H-5)

All TaskRunner content MUST be classified into one of three scopes:

- **test-fixture** (`docs/test-fixture/`, `include/test_fixture/`, `src/test_fixture/`, `tests/test_fixture/`):
  Currently-shippable state. STATUS: ACCEPTED. Code is in main branch.
- **umd-evolution** (`docs/umd-evolution/`, `include/umd/`, `src/umd/`, `tests/umd/`):
  Experimental vision + skeleton. STATUS: PROPOSED/DRAFT only. **As of 2026-07-09** (build-default-on change), code compiles **by default** (`cmake -B build`); opt-out via `TASKRUNNER_BUILD_MODE=test-fixture` excludes `libcuda_shim/` and `tests/umd/`.
- **shared** (`docs/shared/`, `include/shared/`, `src/shared/`, `tests/shared/`):
  Cross-cutting abstractions. STATUS: ACCEPTED. Dual review required for changes.

### Required Metadata

Every document header MUST include:

```markdown
---
SCOPE: <test-fixture|umd-evolution|shared>
STATUS: <ACCEPTED|PROPOSED|DRAFT|DEPRECATED>
---
```

Every `.hpp`/`.cpp`/`.h` file MUST have `// SCOPE: <scope>` as first line.

### Cross-Scope References

When test-fixture docs reference umd-evolution content, use relative path `../umd-evolution/...` and tag inline as `[UMD-EVOLUTION SCOPE]`.

### Build Mode Selection

```bash
# Default (test-fixture only)
cmake -B build

# UMD-evolution (experimental)
cmake -B build -DTASKRUNNER_BUILD_MODE=umd-evolution
```
```

- [ ] **Step 3: Commit AGENTS.md update**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
git add AGENTS.md
git commit -m "docs(agents): H-5 A.8 - add scope classification rules"
```

### Task A.9: Phase A Verification

- [ ] **Step 1: Verify directory structure**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
find docs -type d | sort
# Expected:
# docs/shared
# docs/shared/adr
# docs/test-fixture
# docs/test-fixture/adr
# docs/test-fixture/architecture
# docs/test-fixture/roadmap
# docs/umd-evolution
# docs/umd-evolution/adr
# docs/umd-evolution/architecture
# docs/umd-evolution/roadmap
```

- [ ] **Step 2: Verify all TADR files (23 total)**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
find docs -name "tadr-*.md" | wc -l
# Expected: 23 (5 test-fixture + 5 redirects + 1 tadr-106 + 4 umd + 3 redirects + 2 tadr-204/205 + 3 shared + 1 tadr-107)
```

- [ ] **Step 3: Verify all 8 redirect files accessible**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
find docs -name "*-redirect.md" | sort
# Expected: 8 files
```

- [ ] **Step 4: Verify git history preserved for remapped TADRs**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
git log --follow docs/test-fixture/adr/tadr-101-stub-tracker.md | head -3
git log --follow docs/umd-evolution/adr/tadr-201-unified-scheduler.md | head -3
```

- [ ] **Step 5: View commit summary**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
git log --oneline main..HEAD
# Expected: ~8 commits for Phase A (A.1 through A.8)
```

---

## Phase B: Code Reorganization + CMake Refactor (1-2 weeks)

**Scope:** Move 7 source/header files + create umd/ skeleton + refactor CMake with `TASKRUNNER_BUILD_MODE` option.

### Task B.1: Create include/shared/ and Move Interfaces

**Files:**
- Create: `external/TaskRunner/include/shared/`
- Move: `include/igpu_driver.hpp` → `include/shared/igpu_driver.hpp`
- Move: `include/sync_primitives.hpp` → `include/shared/sync_primitives.hpp`
- Move: `include/error_handling.hpp` → `include/shared/error_handling.hpp`

- [ ] **Step 1: Create include/shared/ directory**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
mkdir -p include/shared
```

- [ ] **Step 2: Move igpu_driver.hpp (git mv)**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
git mv include/igpu_driver.hpp include/shared/igpu_driver.hpp
```

- [ ] **Step 3: Move sync_primitives.hpp (git mv)**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
git mv include/sync_primitives.hpp include/shared/sync_primitives.hpp
```

- [ ] **Step 4: Create error_handling.hpp placeholder + move to shared**

> **v2 修正**（Metis 审查）：`include/error_handling.hpp` 当前**不存在**，但 `specs/taskrunner-shared-infrastructure/spec.md` L49-55 REQUIRE 该文件存在于 `include/shared/`。Plan 必须先创建占位文件再移动。

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner

# Step 4a: 创建占位文件（最小实现）
cat > include/error_handling.hpp <<'EOF'
// include/error_handling.hpp - H-5 Phase B placeholder
// Scope: shared (minimal implementation to satisfy spec-shared-infrastructure L49-55)
#pragma once
#include <cstdint>

namespace async_task::gpu {

enum class ErrorCode : int32_t {
    SUCCESS       = 0,
    FAILURE       = -1,
    NOT_IMPL      = -2,
    INVALID_ARG   = -3,
    NO_MEMORY     = -4,
};

template <typename T>
struct Result {
    ErrorCode code;
    T value;
    Result(ErrorCode c, T v) : code(c), value(std::move(v)) {}
    bool ok() const { return code == ErrorCode::SUCCESS; }
};

}  // namespace async_task::gpu
EOF

# Step 4b: 移动到 shared/
git mv include/error_handling.hpp include/shared/error_handling.hpp

# Step 4c: 验证
ls include/
# Expected: cuda_stub.hpp  cuda_scheduler.hpp  gpu_driver_client.h  memory_manager.hpp  shared/  sync_primitives.hpp
ls include/shared/
# Expected: error_handling.hpp  igpu_driver.hpp  memory_manager.hpp  sync_primitives.hpp
```

- [ ] **Step 5: Verify history preserved**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
git log --follow include/shared/igpu_driver.hpp | head -3
```

- [ ] **Step 6: Commit includes moves**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
git add include/shared/
git commit -m "refactor(includes): H-5 B.1 - move shared interfaces to include/shared/"
```

### Task B.2: Create test_fixture/ Directories and Move Files

**Files:**
- Create: `include/test_fixture/`, `src/test_fixture/`, `tests/test_fixture/`
- Move: 4 headers + 4 sources + 4 test files

- [ ] **Step 1: Create test_fixture directories**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
mkdir -p include/test_fixture src/test_fixture tests/test_fixture
```

- [ ] **Step 2: Move test-fixture headers (git mv) — v2 补充遗漏**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner

# 2a: test-fixture 范畴 headers（plan 原清单）
git mv include/cuda_stub.hpp include/test_fixture/cuda_stub.hpp
git mv include/cuda_scheduler.hpp include/test_fixture/cuda_scheduler.hpp
git mv include/gpu_driver_client.h include/test_fixture/gpu_driver_client.h

# 2b: shared 范畴 headers（**v2 新增** — Metis 审查发现遗漏，memory_manager.hpp 被 cuda_scheduler.hpp #include）
git mv include/memory_manager.hpp include/shared/memory_manager.hpp

# 2c: error_handling.hpp **已由 B.1 Step 4 处理**（创建 + 移动到 shared/），此处不重复
# 若 B.1 未执行则报错，但 v2 流程要求 B.1 必须先于 B.2
```

- [ ] **Step 2.5: Move additional source files (git mv) — v2 补充 6 个遗漏 cpp**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner

# shared 范畴源文件（v2 新增）
git mv src/memory_manager.cpp src/shared/memory_manager.cpp
git mv src/sync_primitives.cpp src/shared/sync_primitives.cpp

# test-fixture 范畴源文件（v2 新增 — 原 plan 完全遗漏 CLI 入口 + 辅助模块）
git mv src/CmdProcessor.cpp src/test_fixture/CmdProcessor.cpp
git mv src/TaskRunner.cpp src/test_fixture/TaskRunner.cpp
git mv src/cli_main.cpp src/test_fixture/cli_main.cpp
git mv src/cmd_buffer_v2.cpp src/test_fixture/cmd_buffer_v2.cpp
```

- [ ] **Step 3: Move test-fixture sources (git mv)**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
git mv src/cuda_stub.cpp src/test_fixture/cuda_stub.cpp
git mv src/cuda_scheduler.cpp src/test_fixture/cuda_scheduler.cpp
git mv src/gpu_driver_client.cpp src/test_fixture/gpu_driver_client.cpp
git mv src/cmd_cuda.cpp src/test_fixture/cmd_cuda.cpp
```

- [ ] **Step 4: Move test files (git mv)**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
git mv tests/test_cuda_scheduler.cpp tests/test_fixture/test_cuda_scheduler.cpp
git mv tests/test_gpu_architecture.cpp tests/test_fixture/test_gpu_architecture.cpp
git mv tests/test_gpu_phase2.cpp tests/test_fixture/test_gpu_phase2.cpp
git mv tests/mock_gpu_driver.hpp tests/test_fixture/mock_gpu_driver.hpp
```

- [ ] **Step 5: Verify moves complete**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
ls include/ src/ tests/
# Expected include: shared/  test_fixture/
# Expected src: test_fixture/  shared/  (if shared had cpp) or just test_fixture/
# Expected tests: test_fixture/  umd/
```

- [ ] **Step 6: Commit test-fixture moves**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
git add -A
git commit -m "refactor(src): H-5 B.2 - move test-fixture sources/headers/tests"
```

### Task B.3: Update Include Paths in Moved Files

**Files:** Modify all moved files to use new scoped include paths

- [ ] **Step 1: Find all include patterns that need updating**（v2 补充 memory_manager grep）

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
grep -rn '#include "igpu_driver.hpp"' include/test_fixture src/test_fixture tests/test_fixture 2>/dev/null
grep -rn '#include "sync_primitives.hpp"' include/test_fixture src/test_fixture tests/test_fixture 2>/dev/null
grep -rn '#include "memory_manager.hpp"' include/test_fixture src/test_fixture tests/test_fixture 2>/dev/null
grep -rn '#include "error_handling.hpp"' include/test_fixture src/test_fixture tests/test_fixture 2>/dev/null
grep -rn '#include "cuda_stub.hpp"' src/test_fixture tests/test_fixture 2>/dev/null
grep -rn '#include "cuda_scheduler.hpp"' src/test_fixture tests/test_fixture 2>/dev/null
grep -rn '#include "gpu_driver_client.h"' src/test_fixture tests/test_fixture 2>/dev/null
```

- [ ] **Step 2: Apply shared include path updates (sed)**（v2 补充 memory_manager sed）

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
find include/test_fixture src/test_fixture tests/test_fixture -type f \( -name "*.hpp" -o -name "*.cpp" -o -name "*.h" \) \
    -exec sed -i 's|#include "igpu_driver.hpp"|#include "shared/igpu_driver.hpp"|g' {} +
find include/test_fixture src/test_fixture tests/test_fixture -type f \( -name "*.hpp" -o -name "*.cpp" -o -name "*.h" \) \
    -exec sed -i 's|#include "sync_primitives.hpp"|#include "shared/sync_primitives.hpp"|g' {} +
find include/test_fixture src/test_fixture tests/test_fixture -type f \( -name "*.hpp" -o -name "*.cpp" -o -name "*.h" \) \
    -exec sed -i 's|#include "memory_manager.hpp"|#include "shared/memory_manager.hpp"|g' {} +
find include/test_fixture src/test_fixture tests/test_fixture -type f \( -name "*.hpp" -o -name "*.cpp" -o -name "*.h" \) \
    -exec sed -i 's|#include "error_handling.hpp"|#include "shared/error_handling.hpp"|g' {} +
```

- [ ] **Step 3: Apply test-fixture include path updates (sed)**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
find src/test_fixture tests/test_fixture -type f \( -name "*.cpp" -o -name "*.hpp" \) \
    -exec sed -i 's|#include "cuda_stub.hpp"|#include "test_fixture/cuda_stub.hpp"|g' {} +
find src/test_fixture tests/test_fixture -type f \( -name "*.cpp" -o -name "*.hpp" \) \
    -exec sed -i 's|#include "cuda_scheduler.hpp"|#include "test_fixture/cuda_scheduler.hpp"|g' {} +
find src/test_fixture tests/test_fixture -type f \( -name "*.cpp" -o -name "*.hpp" \) \
    -exec sed -i 's|#include "gpu_driver_client.h"|#include "test_fixture/gpu_driver_client.h"|g' {} +
```

- [ ] **Step 4: Verify no stale includes remain**（v2 补充 memory_manager grep）

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
grep -rn '#include "igpu_driver.hpp"' include/test_fixture src/test_fixture tests/test_fixture 2>/dev/null
grep -rn '#include "sync_primitives.hpp"' include/test_fixture src/test_fixture tests/test_fixture 2>/dev/null
grep -rn '#include "memory_manager.hpp"' include/test_fixture src/test_fixture tests/test_fixture 2>/dev/null
grep -rn '#include "cuda_stub.hpp"' src/test_fixture tests/test_fixture 2>/dev/null
grep -rn '#include "gpu_driver_client.h"' src/test_fixture tests/test_fixture 2>/dev/null
# Expected: empty output (all migrated to shared/ or test_fixture/ paths)
```

- [ ] **Step 5: Build verification (default test-fixture mode)**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
rm -rf build && mkdir build && cd build && cmake .. 2>&1 | tail -3
# Expected: "Building TaskRunner in TEST-FIXTURE mode (default)"
make -j4 2>&1 | tail -10
# Expected: build succeeds
```

- [ ] **Step 6: Run tests**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner/build
ctest --output-on-failure 2>&1 | tail -15
# Expected: 31 tests pass (8 + 11 + 12)
```

- [ ] **Step 7: Commit include updates**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
git add -A
git commit -m "refactor(includes): H-5 B.3 - update include paths for scoped directories"
```

### Task B.4: Create umd/ Skeleton (Placeholders Only)

**Files:**
- Create: `include/umd/{cuda_api.hpp,module_loader.hpp,ring_buffer.hpp}`
- Create: `src/umd/{cuda_api.cpp,module_loader.cpp,ring_buffer.cpp}` (placeholder impl)
- Create: `tests/umd/test_umd_skeleton.cpp`

- [ ] **Step 1: Create umd directories**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
mkdir -p include/umd src/umd tests/umd
```

- [ ] **Step 2: Create include/umd/cuda_api.hpp**

```cpp
// SCOPE: UMD-EVOLUTION
// STATUS: PROPOSED
// IMPLEMENTED: NO

#pragma once

#include <cstddef>

namespace async_task::umd {

// Minimal CUDA Runtime API surface (PoC only — NOT IMPLEMENTED)
class CudaApi {
public:
    // Allocate device memory. Returns nullptr (PoC not implemented).
    void* alloc(std::size_t size);

    // Copy memory. Returns -ENOSYS (38) (PoC not implemented).
    int memcpy(void* dst, const void* src, std::size_t size);

    // Launch kernel by name. Returns -ENOSYS (PoC not implemented).
    int launch_kernel(const char* kernel_name, void** args, int num_args);
};

}  // namespace async_task::umd
```

- [ ] **Step 3: Create include/umd/module_loader.hpp**

```cpp
// SCOPE: UMD-EVOLUTION
// STATUS: PROPOSED
// IMPLEMENTED: NO

#pragma once

namespace async_task::umd {

// ELF + CUBIN parser (PoC only — NOT IMPLEMENTED)
class ModuleLoader {
public:
    // Load module from file. Returns nullptr (PoC not implemented).
    void* load(const char* path);

    // Get kernel handle by name. Returns nullptr (PoC not implemented).
    void* get_kernel(void* module, const char* name);
};

}  // namespace async_task::umd
```

- [ ] **Step 4: Create include/umd/ring_buffer.hpp**

```cpp
// SCOPE: UMD-EVOLUTION
// STATUS: PROPOSED
// IMPLEMENTED: NO

#pragma once

#include <cstddef>
#include <cstdint>

namespace async_task::umd {

// Ring Buffer + Doorbell abstraction (PoC only — NOT IMPLEMENTED)
class RingBuffer {
public:
    // Create ring buffer. Returns nullptr (PoC not implemented).
    void* create(std::size_t entry_count, std::size_t entry_size);

    // Submit entry to ring. Returns -ENOSYS (PoC not implemented).
    int submit(void* ring, const void* entry);

    // Ring doorbell. Returns 0 (PoC not implemented).
    std::uint64_t ring_doorbell(void* doorbell_ptr, std::uint64_t value);
};

}  // namespace async_task::umd
```

- [ ] **Step 5: Create src/umd/cuda_api.cpp**

```cpp
// SCOPE: UMD-EVOLUTION
// STATUS: PROPOSED
// IMPLEMENTED: NO

#include "umd/cuda_api.hpp"

namespace async_task::umd {

void* CudaApi::alloc(std::size_t size) {
    (void)size;
    return nullptr;  // PoC not implemented
}

int CudaApi::memcpy(void* dst, const void* src, std::size_t size) {
    (void)dst; (void)src; (void)size;
    return -38;  // -ENOSYS
}

int CudaApi::launch_kernel(const char* kernel_name, void** args, int num_args) {
    (void)kernel_name; (void)args; (void)num_args;
    return -38;  // -ENOSYS
}

}  // namespace async_task::umd
```

- [ ] **Step 6: Create src/umd/module_loader.cpp**

```cpp
// SCOPE: UMD-EVOLUTION
// STATUS: PROPOSED
// IMPLEMENTED: NO

#include "umd/module_loader.hpp"

namespace async_task::umd {

void* ModuleLoader::load(const char* path) {
    (void)path;
    return nullptr;
}

void* ModuleLoader::get_kernel(void* module, const char* name) {
    (void)module; (void)name;
    return nullptr;
}

}  // namespace async_task::umd
```

- [ ] **Step 7: Create src/umd/ring_buffer.cpp**

```cpp
// SCOPE: UMD-EVOLUTION
// STATUS: PROPOSED
// IMPLEMENTED: NO

#include "umd/ring_buffer.hpp"

namespace async_task::umd {

void* RingBuffer::create(std::size_t entry_count, std::size_t entry_size) {
    (void)entry_count; (void)entry_size;
    return nullptr;
}

int RingBuffer::submit(void* ring, const void* entry) {
    (void)ring; (void)entry;
    return -38;  // -ENOSYS
}

std::uint64_t RingBuffer::ring_doorbell(void* doorbell_ptr, std::uint64_t value) {
    (void)doorbell_ptr; (void)value;
    return 0;
}

}  // namespace async_task::umd
```

- [ ] **Step 8: Create tests/umd/test_umd_skeleton.cpp**

```cpp
// SCOPE: UMD-EVOLUTION
// STATUS: PROPOSED
// IMPLEMENTED: NO

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "umd/cuda_api.hpp"
#include "umd/module_loader.hpp"
#include "umd/ring_buffer.hpp"

TEST_CASE("CudaApi placeholders return nullptr/-ENOSYS") {
    async_task::umd::CudaApi api;
    CHECK(api.alloc(1024) == nullptr);
    CHECK(api.memcpy(nullptr, nullptr, 0) == -38);
    CHECK(api.launch_kernel("vec_add", nullptr, 0) == -38);
}

TEST_CASE("ModuleLoader placeholders return nullptr") {
    async_task::umd::ModuleLoader loader;
    CHECK(loader.load("kernel.hsaco") == nullptr);
    CHECK(loader.get_kernel(nullptr, "vec_add") == nullptr);
}

TEST_CASE("RingBuffer placeholders return nullptr/0/-ENOSYS") {
    async_task::umd::RingBuffer rb;
    CHECK(rb.create(64, 32) == nullptr);
    CHECK(rb.submit(nullptr, nullptr) == -38);
    CHECK_EQ(rb.ring_doorbell(nullptr, 1), 0ULL);
}
```

- [ ] **Step 9: Commit umd skeleton**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
git add include/umd/ src/umd/ tests/umd/
git commit -m "feat(umd): H-5 B.4 - add UMD skeleton (placeholders only, not implemented)"
```

### Task B.5: CMake Refactor — Add TASKRUNNER_BUILD_MODE Option

**Files:**
- Create: `external/TaskRunner/cmake/Shared.cmake`
- Create: `external/TaskRunner/cmake/TestFixture.cmake`
- Create: `external/TaskRunner/cmake/UMDEvolution.cmake`
- Modify: `external/TaskRunner/CMakeLists.txt`

- [ ] **Step 1: Read current CMakeLists.txt**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
cat CMakeLists.txt
```

- [ ] **Step 2: Create cmake/ directory**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
mkdir -p cmake
```

- [ ] **Step 3: Create cmake/Shared.cmake**

```cmake
# cmake/Shared.cmake - Shared infrastructure (always built)
add_library(taskrunner_shared INTERFACE)
target_include_directories(taskrunner_shared INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(taskrunner_shared INTERFACE cxx_std_17)
```

- [ ] **Step 4: Create cmake/TestFixture.cmake**

```cmake
# cmake/TestFixture.cmake - test-fixture scope (default build mode)
# v2: 增加 test_taskrunner.cpp / cli_main / cmd_buffer_v2 注册（原 plan 完全遗漏）

add_library(taskrunner_test_fixture STATIC
    src/test_fixture/cuda_stub.cpp
    src/test_fixture/cuda_scheduler.cpp
    src/test_fixture/gpu_driver_client.cpp
    src/test_fixture/CmdProcessor.cpp       # v2 新增
    src/test_fixture/TaskRunner.cpp         # v2 新增
)
target_link_libraries(taskrunner_test_fixture PUBLIC taskrunner_shared)
target_include_directories(taskrunner_test_fixture PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# CLI 可执行文件（v2 新增 cli_main + cmd_buffer_v2）
add_executable(taskrunner
    src/test_fixture/cli_main.cpp           # v2 新增 CLI 入口
    src/test_fixture/cmd_buffer_v2.cpp      # v2 新增 CLI 子命令
    src/test_fixture/cmd_cuda.cpp
)
target_link_libraries(taskrunner PRIVATE taskrunner_test_fixture)

# Tests
enable_testing()

add_executable(test_cuda_scheduler tests/test_fixture/test_cuda_scheduler.cpp)
target_link_libraries(test_cuda_scheduler PRIVATE taskrunner_test_fixture)
add_test(NAME test_cuda_scheduler COMMAND test_cuda_scheduler)

add_executable(test_gpu_architecture tests/test_fixture/test_gpu_architecture.cpp)
target_link_libraries(test_gpu_architecture PRIVATE taskrunner_test_fixture)
add_test(NAME test_gpu_architecture COMMAND test_gpu_architecture)

add_executable(test_gpu_phase2 tests/test_fixture/test_gpu_phase2.cpp)
target_link_libraries(test_gpu_phase2 PRIVATE taskrunner_test_fixture)
add_test(NAME test_gpu_phase2 COMMAND test_gpu_phase2)

# v2 新增 test_taskrunner.cpp（3 个 TEST_CASEs，原 plan 未注册）
add_executable(test_taskrunner tests/test_fixture/test_taskrunner.cpp)
target_link_libraries(test_taskrunner PRIVATE taskrunner_test_fixture)
add_test(NAME test_taskrunner COMMAND test_taskrunner)
```

- [ ] **Step 5: Create cmake/UMDEvolution.cmake**

```cmake
# cmake/UMDEvolution.cmake - umd-evolution scope (opt-in experimental)

message(WARNING "Building TaskRunner in UMD-EVOLUTION mode (experimental, not for production use)")

add_library(taskrunner_umd_stub SHARED
    src/umd/cuda_api.cpp
    src/umd/module_loader.cpp
    src/umd/ring_buffer.cpp
)
target_link_libraries(taskrunner_umd_stub PUBLIC taskrunner_shared)

# UMD tests (experimental skeleton)
enable_testing()

add_executable(test_umd_skeleton tests/umd/test_umd_skeleton.cpp)
target_link_libraries(test_umd_skeleton PRIVATE taskrunner_umd_stub)
add_test(NAME test_umd_skeleton COMMAND test_umd_skeleton)
```

- [ ] **Step 6: Replace top-level CMakeLists.txt**

Overwrite `external/TaskRunner/CMakeLists.txt` with:

```cmake
# TaskRunner top-level CMakeLists.txt (H-5 refactored, v2 修正)
# See docs/superpowers/plans/2026-06-24-h5-taskrunner-scope-clarification.md Task B.5
# v2 修正:
#   - cmake_minimum_required: 3.20 → 3.10（保持兼容，set_property CACHE STRINGS 在 3.1+ 已支持）
#   - project name: TaskRunner → async_task（保持与 UsrLinuxEmu 端下游引用一致）

cmake_minimum_required(VERSION 3.10)
project(async_task CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build mode option: test-fixture (default) | umd-evolution (experimental)
set(TASKRUNNER_BUILD_MODE "test-fixture" CACHE STRING
    "Build mode: test-fixture (default) | umd-evolution (experimental)")
set_property(CACHE TASKRUNNER_BUILD_MODE
    PROPERTY STRINGS "test-fixture" "umd-evolution")

# Validate TASKRUNNER_BUILD_MODE
if(NOT (TASKRUNNER_BUILD_MODE STREQUAL "test-fixture" OR
        TASKRUNNER_BUILD_MODE STREQUAL "umd-evolution"))
    message(FATAL_ERROR
        "Invalid TASKRUNNER_BUILD_MODE: ${TASKRUNNER_BUILD_MODE}. "
        "Valid values: test-fixture (default) | umd-evolution")
endif()

# Shared infrastructure (always built)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Shared.cmake)

# Conditional scope build
if(TASKRUNNER_BUILD_MODE STREQUAL "test-fixture")
    message(STATUS "Building TaskRunner in TEST-FIXTURE mode (default)")
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/TestFixture.cmake)
elseif(TASKRUNNER_BUILD_MODE STREQUAL "umd-evolution")
    message(WARNING "Building TaskRunner in UMD-EVOLUTION mode (experimental)")
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/UMDEvolution.cmake)
endif()
```

- [ ] **Step 7: Verify default build works**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
rm -rf build && mkdir build && cd build && cmake .. 2>&1 | tail -5
# Expected: "Building TaskRunner in TEST-FIXTURE mode (default)"
make -j4 2>&1 | tail -5
# Expected: build succeeds
```

- [ ] **Step 8: Verify UMD mode build works**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
rm -rf build-umd && mkdir build-umd && cd build-umd && cmake .. -DTASKRUNNER_BUILD_MODE=umd-evolution 2>&1 | tail -5
# Expected: WARNING about experimental + "Building TaskRunner in UMD-EVOLUTION mode"
make -j4 2>&1 | tail -5
# Expected: libtaskrunner_umd_stub.so builds successfully
```

- [ ] **Step 9: Verify invalid mode rejected**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
rm -rf build-invalid && mkdir build-invalid && cd build-invalid && cmake .. -DTASKRUNNER_BUILD_MODE=invalid 2>&1 | tail -5
# Expected: FATAL_ERROR about invalid mode
```

- [ ] **Step 10: Commit CMake refactor**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
git add cmake/ CMakeLists.txt
git commit -m "refactor(cmake): H-5 B.5 - add TASKRUNNER_BUILD_MODE option + cmake/ modules"
```

### Task B.6: Add SCOPE Comments to All Source Files

**Files:** All `.cpp`/`.hpp`/`.h` in `include/shared/`, `include/test_fixture/`, `src/test_fixture/`, `tests/test_fixture/`

- [ ] **Step 1: Add SCOPE comment to shared files**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
for f in include/shared/*.hpp; do
    [ -f "$f" ] && sed -i '1i // SCOPE: SHARED' "$f"
done
```

- [ ] **Step 2: Add SCOPE comment to test-fixture files**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
for f in include/test_fixture/*.hpp include/test_fixture/*.h src/test_fixture/*.cpp tests/test_fixture/*.cpp tests/test_fixture/*.hpp; do
    [ -f "$f" ] && sed -i '1i // SCOPE: TEST-FIXTURE' "$f"
done
```

- [ ] **Step 3: Verify SCOPE comments**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
head -1 include/shared/igpu_driver.hpp
# Expected: // SCOPE: SHARED
head -1 src/test_fixture/cuda_stub.cpp
# Expected: // SCOPE: TEST-FIXTURE
```

- [ ] **Step 4: Commit SCOPE comments**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
git add -A
git commit -m "style(scope): H-5 B.6 - add SCOPE comments to all source files"
```

### Task B.7: Phase B Verification

- [ ] **Step 1: Default build verification**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
rm -rf build && mkdir build && cd build && cmake .. 2>&1 | tail -3
make -j4 2>&1 | tail -5
ctest --output-on-failure 2>&1 | tail -10
# Expected: 31 tests pass
```

- [ ] **Step 2: UMD mode build verification**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
rm -rf build-umd && mkdir build-umd && cd build-umd && cmake .. -DTASKRUNNER_BUILD_MODE=umd-evolution 2>&1 | tail -3
make -j4 2>&1 | tail -5
ctest --output-on-failure 2>&1 | tail -10
# Expected: 3 umd tests pass
```

- [ ] **Step 3: Verify default build excludes umd files**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
rm -rf build && mkdir build && cd build && cmake .. && make -j4 2>&1 | grep -i "umd"
# Expected: no UMD references (umd files not compiled)
```

- [ ] **Step 4: View commit summary**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
git log --oneline main..HEAD
# Expected: ~6 commits for Phase B (B.1 through B.6)
```

---

## Phase C: Cross-Repo Sync (1 week)

**Scope:** Push TaskRunner H-5 branch + update UsrLinuxEmu submodule pointer + update docs/00_adr/README.md mirror.

### Task C.1: Push TaskRunner H-5 Branch

- [ ] **Step 1: Verify TaskRunner commits are clean**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
git status -s
# Expected: clean working tree
```

- [ ] **Step 2: Push H-5 branch to TaskRunner remote**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
git push origin h5-taskrunner-scope-clarification
# Expected: branch pushed successfully
```

- [ ] **Step 3: Verify remote branch exists**

```bash
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
git ls-remote origin h5-taskrunner-scope-clarification
# Expected: shows remote ref
```

### Task C.2: Update UsrLinuxEmu Submodule Pointer

- [ ] **Step 1: Switch to UsrLinuxEmu main branch**

```bash
cd /workspace/project/UsrLinuxEmu
git branch --show-current
# Expected: main
```

- [ ] **Step 2: Update TaskRunner submodule to H-5 branch tip**

```bash
cd /workspace/project/UsrLinuxEmu
cd external/TaskRunner
git fetch origin
git checkout origin/h5-taskrunner-scope-clarification 2>/dev/null || git checkout h5-taskrunner-scope-clarification
cd /workspace/project/UsrLinuxEmu
git status -s external/TaskRunner
# Expected: M external/TaskRunner (submodule pointer modified)
```

- [ ] **Step 3: Commit submodule pointer update**

```bash
cd /workspace/project/UsrLinuxEmu
git -c user.email="Sisyphus@anthropic.com" -c user.name="Sisyphus Agent" \
    commit -am "chore(submodule): bump TaskRunner to H-5 (scope clarification + 16 TADR remap)"
```

### Task C.3: Update UsrLinuxEmu docs/00_adr/README.md Mirror

**Files:**
- Modify: `docs/00_adr/README.md`

- [ ] **Step 1: Read current TaskRunner TADR mirror section**

```bash
cd /workspace/project/UsrLinuxEmu
grep -n -A 20 "TaskRunner TADR" docs/00_adr/README.md | head -40
```

- [ ] **Step 2: Replace TaskRunner TADR mirror table**

Edit `docs/00_adr/README.md` to replace the TaskRunner TADR mirror section with:

```markdown
### TaskRunner TADR mirror (H-5 scope clarification)

| TADR | Scope | Status | Mirror |
|------|-------|--------|--------|
| tadr-101 | test-fixture | ✅ Accepted | [link](../external/TaskRunner/docs/test-fixture/adr/tadr-101-stub-tracker.md) |
| tadr-102 | test-fixture | ✅ Accepted | [link](../external/TaskRunner/docs/test-fixture/adr/tadr-102-igpu-driver.md) |
| tadr-103 | test-fixture | ✅ Accepted | [link](../external/TaskRunner/docs/test-fixture/adr/tadr-103-h3-phase2.md) |
| tadr-104 | test-fixture | ✅ Accepted | [link](../external/TaskRunner/docs/test-fixture/adr/tadr-104-r2-mapping.md) |
| tadr-105 | test-fixture | ✅ Accepted | [link](../external/TaskRunner/docs/test-fixture/adr/tadr-105-h7-deferred.md) |
| tadr-106 | test-fixture | ✅ Accepted | [link](../external/TaskRunner/docs/test-fixture/adr/tadr-106-test-fixture-scope-clarification.md) |
| tadr-201 | umd-evolution | 📋 Proposed | [link](../external/TaskRunner/docs/umd-evolution/adr/tadr-201-unified-scheduler.md) |
| tadr-202 | umd-evolution | 📋 Proposed | [link](../external/TaskRunner/docs/umd-evolution/adr/tadr-202-layered-design.md) |
| tadr-203 | umd-evolution | 📋 Proposed | [link](../external/TaskRunner/docs/umd-evolution/adr/tadr-203-sync-unified.md) |
| tadr-204 | umd-evolution | 📋 Proposed | [link](../external/TaskRunner/docs/umd-evolution/adr/tadr-204-umd-evolution-scope-clarification.md) |
| tadr-205 | umd-evolution | 📋 Proposed | [link](../external/TaskRunner/docs/umd-evolution/adr/tadr-205-umd-evolution-poc-roadmap.md) |
| tadr-107 | shared | ✅ Accepted | [link](../external/TaskRunner/docs/shared/adr/tadr-107-shared-infrastructure-boundary.md) |
| tadr-301 | shared | ✅ Accepted | [link](../external/TaskRunner/docs/shared/adr/tadr-301-igpu-driver-contract.md) |
| tadr-302 | shared | ✅ Accepted | [link](../external/TaskRunner/docs/shared/adr/tadr-302-sync-primitives.md) |
| tadr-303 | shared | ✅ Accepted | [link](../external/TaskRunner/docs/shared/adr/tadr-303-error-handling.md) |

### Dual-Track Classification (H-5)

TaskRunner TADRs are classified into 3 scopes:

- **test-fixture** (1xx): Currently-shippable state (H-1/H-2.5/H-3 work)
- **umd-evolution** (2xx): Experimental UMD vision, deferred until Phase D
- **shared** (3xx + tadr-107): Cross-cutting abstractions, dual review required

Cross-repo sync follows ADR-035 §Rule 5.1 4-step protocol:

1. TaskRunner commit + push
2. UsrLinuxEmu submodule pointer update
3. UsrLinuxEmu docs/00_adr/README.md mirror update (this change)
4. Cross-repo PR
```

- [ ] **Step 3: Commit mirror update**

```bash
cd /workspace/project/UsrLinuxEmu
git add docs/00_adr/README.md
git -c user.email="Sisyphus@anthropic.com" -c user.name="Sisyphus Agent" \
    commit -m "docs(adr): update TaskRunner TADR mirror (H-5 scope clarification, 15 TADRs)"
```

### Task C.4: Create Cross-Repo PR (Manual via GitHub UI)

- [ ] **Step 1: Push UsrLinuxEmu main branch (if user authorizes)**

```bash
cd /workspace/project/UsrLinuxEmu
git push origin main
# Only execute if user explicitly authorizes
```

- [ ] **Step 2: Open GitHub PR for TaskRunner h5-taskrunner-scope-clarification branch**

Title: "H-5 TaskRunner scope clarification — test-fixture + umd-evolution + shared"

Body:
```markdown
## Summary

Implements H-5 TaskRunner scope clarification. Reorganizes docs/code/TADRs into 3 scopes:
- test-fixture (current main, 1xx)
- umd-evolution (experimental, 2xx)
- shared (cross-cutting, 3xx + tadr-107)

## Changes

- 8 TADRs remapped (git mv preserves history)
- 8 redirect files for backward compatibility
- 7 new TADRs (3 scope rules + 4 shared contracts)
- 3 scope READMEs
- Code reorganized into shared/ + test_fixture/ + umd/ skeleton
- CMake `TASKRUNNER_BUILD_MODE` option (default: test-fixture)

## Test Verification

- [x] `test_cuda_scheduler` 8/8 PASS
- [x] `test_gpu_architecture` 11/11 PASS
- [x] `test_gpu_phase2` 12/12 PASS
- [x] `test_umd_skeleton` 3/3 PASS (under `umd-evolution` mode)
- [x] Default build excludes umd files
- [x] Invalid `TASKRUNNER_BUILD_MODE` rejected with FATAL_ERROR

## Reference

- Proposal: `openspec/changes/h5-taskrunner-scope-clarification/proposal.md`
- Design: `openspec/changes/h5-taskrunner-scope-clarification/design.md`
- Tasks: `openspec/changes/h5-taskrunner-scope-clarification/tasks.md`
```

- [ ] **Step 3: Open companion PR for UsrLinuxEmu submodule + mirror update**

Title: "chore: H-5 TaskRunner scope clarification (submodule pointer + TADR mirror)"

Body:
```markdown
## Summary

Companion to TaskRunner PR for H-5 scope clarification. Updates:

1. Submodule pointer to TaskRunner H-5 branch
2. `docs/00_adr/README.md` TaskRunner TADR mirror (15 TADRs total, 3 scopes)

## Reference

- TaskRunner PR: [link]
- OpenSpec change: `openspec/changes/h5-taskrunner-scope-clarification/`
- ADR-035 §Rule 5.1 4-step protocol
```

### Task C.5: Phase C Verification

- [ ] **Step 1: Verify submodule pointer points to H-5 commit**

```bash
cd /workspace/project/UsrLinuxEmu
git ls-tree HEAD external/TaskRunner | awk '{print $3}'
# Compare with TaskRunner H-5 branch tip:
cd external/TaskRunner
git rev-parse h5-taskrunner-scope-clarification
```

- [ ] **Step 2: Verify mirror table has 15 rows**

```bash
cd /workspace/project/UsrLinuxEmu
grep -c "^| tadr-" docs/00_adr/README.md
# Expected: 15 (6 test-fixture + 5 umd-evolution + 4 shared)
```

- [ ] **Step 3: Verify both PRs opened**

Manual check via GitHub UI.

### Task C.6: Archive OpenSpec Change (ADR-035 R5.1 Step 4)

> **v2 新增**：ADR-035 R5.1 实际第 4 步是 "Archive openspec change"，原 plan C.4/C.5 描述的"跨仓 PR"缺失此步骤。必须补全，否则 UsrLinuxEmu maintainer 按 ADR-035 期望看到归档。

- [ ] **Step 1: Move openspec change to archive directory (in UsrLinuxEmu repo)**

```bash
cd /workspace/project/UsrLinuxEmu
git mv openspec/changes/h5-taskrunner-scope-clarification \
        openspec/changes/archive/2026-06-24-h5-taskrunner-scope-clarification
```

- [ ] **Step 2: Update .openspec.yaml status to ARCHIVED**

```bash
# In archive/2026-06-24-h5-taskrunner-scope-clarification/.openspec.yaml:
#   schema: spec-driven
#   created: 2026-06-24
#   archived: 2026-06-24
#   status: ARCHIVED
```

- [ ] **Step 3: Commit archive move**

```bash
cd /workspace/project/UsrLinuxEmu
git add openspec/changes/archive/2026-06-24-h5-taskrunner-scope-clarification/
git commit -m "chore(openspec): archive h5-taskrunner-scope-clarification (ADR-035 R5.1 Step 4)"
```

- [ ] **Step 4: Push archive commit (after companion PR merged)**

```bash
cd /workspace/project/UsrLinuxEmu
git push origin main
# Only after TaskRunner + UsrLinuxEmu companion PRs are merged
```

---

## Self-Review Checklist

After completing all tasks:

- [ ] **1. Spec coverage:** Each spec requirement is covered by a task:
  - `taskrunner-test-fixture-scope` → A.1, A.2, A.5, B.2, B.3, B.6
  - `taskrunner-umd-evolution-scope` → A.1, A.4, A.5, B.4
  - `taskrunner-shared-infrastructure` → A.5, B.1, B.3, B.6
  - `taskrunner-build-mode` → B.5

- [ ] **2. No placeholders:** Plan contains no "TBD"/"TODO"/"implement later". All code blocks are complete.

- [ ] **3. Type consistency:** `CudaApi::alloc(size_t)` returns `void*` (consistent across B.4 Step 2 and Step 5).

- [ ] **4. Cross-scope references:** All umd-evolution references tagged `[UMD-EVOLUTION SCOPE]` in READMEs.

- [ ] **5. Commit history preserved:** `git log --follow` works for all renamed TADRs and moved source files.

- [ ] **6. CMake modes verified:** Default build excludes umd/, `-DTASKRUNNER_BUILD_MODE=umd-evolution` includes umd/, invalid mode rejected.

- [ ] **7. Tests pass:** 31 + 3 = 34 doctest cases all green.

---

## Final Verification Checklist

- [ ] **V.1:** `cmake -B build && make -j4 -C build && ctest --test-dir build --output-on-failure` passes (default mode, 31 tests)
- [ ] **V.2:** `cmake -B build-umd -DTASKRUNNER_BUILD_MODE=umd-evolution && make -j4 -C build-umd && ctest --test-dir build-umd --output-on-failure` passes (UMD mode, 3 tests)
- [ ] **V.3:** `cmake -B build-invalid -DTASKRUNNER_BUILD_MODE=invalid` fails with FATAL_ERROR
- [ ] **V.4:** 23 TADR files exist (8 reclassified + 8 redirects + 7 new)
- [ ] **V.5:** All source files have `// SCOPE: ...` header comment
- [ ] **V.6:** `AGENTS.md` has scope classification section
- [ ] **V.7:** TaskRunner branch `h5-taskrunner-scope-clarification` pushed to origin
- [ ] **V.8:** UsrLinuxEmu `docs/00_adr/README.md` mirror has 15 TADR rows
- [ ] **V.9:** Both PRs opened and linked (TaskRunner + UsrLinuxEmu)

---

## Out-of-Scope (Deferred)

These are explicitly NOT in this plan:

- **Phase D (UMD PoC)**: Doorbell bypass, ring buffer self-management, ELF parser, minimal CUDA API. Deferred until H-3.5 ships + explicit PoC requirement identified.
- **Phase E (H-3.5/H-7 work)**: CudaScheduler `dynamic_cast` refactor + MockGpuDriver guard + H-7 upstream issues. Independent of H-5; runs in parallel.

---

## Execution Handoff

**Plan complete and saved to `/workspace/project/UsrLinuxEmu/docs/superpowers/plans/2026-06-24-h5-taskrunner-scope-clarification.md`.**

Two execution options:

**1. Subagent-Driven (recommended for this plan)** - Dispatch a fresh subagent per task with review checkpoints.

**2. Inline Execution** - Execute tasks in this session using executing-plans.

**Which approach?**