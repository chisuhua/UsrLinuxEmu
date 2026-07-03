## ADDED Requirements

### Requirement: linux_compat/{mmu_notifier,hmm}.h 头文件扩展

The system MUST add headers under `include/linux_compat/` aligned with Linux 6.12 LTS API signatures for mmu_notifier and HMM subsystems (per ADR-027 decision 3, ABI consistency not guaranteed):

- `mmu_notifier.h` MUST expose `struct mmu_notifier` with `ops` pointer field (full set of callbacks: `invalidate_range_start` / `invalidate_range_end` / `release`)
- `mmu_notifier.h` MUST expose `mmu_notifier_register()` / `mmu_notifier_unregister()` function signatures
- `hmm.h` MUST expose `struct hmm_range` with 7 fields: `notifier`, `notifier_seq`, `start`, `end`, `hmm_pfns`, `default_flags`, `pfn_flags_mask`
- `hmm.h` MUST expose `struct mmu_interval_notifier` + `struct mmu_interval_notifier_ops.invalidate` callback (replacing removed `struct hmm_mirror`)
- `hmm.h` MUST expose sequence number protocol: `mmu_interval_read_begin()` / `mmu_interval_read_retry()` / `mmu_interval_set_seq()`
- `hmm.h` MUST expose HMM PFN flag macros: `HMM_PFN_VALID` (1UL << 63), `HMM_PFN_WRITE` (1UL << 62), `HMM_PFN_ERROR`, `HMM_PFN_REQ_FAULT`, `HMM_PFN_REQ_WRITE`
- `hmm.h` MUST expose `hmm_range_fault()` function signature

#### Scenario: mmu_notifier.h exposes complete struct mmu_notifier

- **WHEN** a translation unit includes `linux_compat/mmu_notifier.h`
- **THEN** `struct mmu_notifier` MUST contain an `ops` field of type `const struct mmu_notifier_ops*`
- **AND** `mmu_notifier_register()` + `mmu_notifier_unregister()` declarations MUST be visible

#### Scenario: hmm.h exposes corrected mmu_interval_notifier API (NOT hmm_mirror)

- **WHEN** a translation unit includes `linux_compat/hmm.h`
- **THEN** `struct mmu_interval_notifier` + `struct mmu_interval_notifier_ops.invalidate` callback MUST be declared
- **AND** `struct hmm_mirror` MUST NOT be declared (removed in Linux 6.x; amdkpu uses mmu_interval_notifier per librarian 2026-07-02)
- **AND** the 5 HMM PFN flag macros MUST be defined with 64-bit encoding (`HMM_PFN_VALID = 1UL << 63`)

### Requirement: src/kernel/uvm/ 实现

The system MUST provide the following implementations:

- `src/kernel/uvm/mmu_notifier.cpp` — mmu_notifier framework (register/unregister + invalidate_range_start/end dispatch)
- `src/kernel/uvm/hmm_range.cpp` — `hmm_range_fault()` implementation (range walk + PFN table allocation + sequence number protocol)
- `src/kernel/uvm/migrate.cpp` — page migration between CPU/GPU memory domain (migrate_to_ram / migrate_to_dev)
- `src/kernel/uvm/fault_inject.cpp` — user-space mmap → page fault → mmu_notifier 通知 device driver 注入路径
- `src/kernel/uvm/zone_device.cpp` — spm vma + page state machine 最简实现
- `src/kernel/uvm/page_state_machine.cpp` — `PAGE_STATE_CPU` / `PAGE_STATE_GPU` / `PAGE_STATE_MIGRATING` 三态机

#### Scenario: mmu_notifier dispatch on munmap

- **WHEN** user-space calls `munmap()` on a region registered with `mmu_notifier_register()`
- **THEN** the registered `ops->invalidate_range_start()` callback MUST be invoked
- **AND** `ops->invalidate_range_end()` MUST be invoked after the invalidation completes

#### Scenario: hmm_range_fault returns valid PFN table

- **WHEN** driver code calls `hmm_range_fault(&range, 0)` on a range with mapped pages
- **THEN** the returned `range.hmm_pfns` MUST be populated with HMM_PFN_VALID entries for mapped pages
- **AND** the function MUST return 0 on success

#### Scenario: page state machine transitions

- **WHEN** a page is migrated from CPU to GPU via `migrate_to_dev()`
- **THEN** the page state MUST transition `PAGE_STATE_CPU → PAGE_STATE_MIGRATING → PAGE_STATE_GPU`
- **AND** any concurrent `migrate_to_ram()` on the same page MUST block until the first migration completes

### Requirement: 1.2/1.3 边界契约 G1-G4 完整化（承接 stage-1-2-drm-subset）

The system MUST fully implement the 4 interface contracts locked in stage-1-2-drm-subset (design.md Decision 5):

| Contract | 1.2 Guarantee (from stage-1-2) | 1.3 MUST Implement |
|----------|-------------------------------|---------------------|
| `struct drm_device` lifetime | Same as `GpgpuDevice` (init on create, shutdown before dtor) | uvm module MUST hold `drm_device*` valid for device lifetime; `drm_device` MUST outlive all `mmu_interval_notifier` and `hmm_range` instances |
| BO refcount | `close(fd)` releases all GEM handle refs owned by that fd; BO refcount may remain >0 (held by dma_buf, SVG ranges, GPU page tables) | `mmu_interval_notifier.invalidate` and `hmm_range_fault()` MUST never reference a BO whose refcount has reached 0 |
| prime import buffer release order | `dma_buf_unmap → dma_buf_detach → dma_buf_put` (Linux 6.12) | mmu_notifier invalidate MUST complete before `dma_buf_detach` is called |
| fence timing | All fences signal before GEM object release | hmm_range fault MUST complete before triggering GEM release on the same BO |

#### Scenario: drm_device outlives mmu_interval_notifier (G1 full)

- **WHEN** `tests/test_uvm_drm_lifecycle_standalone.cpp` runs
- **THEN** it MUST verify that BO release order: drain fence → release GEM object → unregister mmu_interval_notifier → shutdown drm_device
- **AND** the test MUST extend the G1 skeleton (no longer just BO release order)

#### Scenario: 1.3 does not pre-implement 1.4 dependencies (G4 continued)

- **WHEN** `git grep` searches stage 1.3 codebase for `mmu_notifier` / `hmm_range_fault` / `hmm_migrate` / `mmu_interval_notifier`
- **THEN** implementations MUST appear (these are 1.3's scope)
- **AND** `kfd_svm.c` / `kfd_process.c` integration MUST NOT appear (these belong to stage 1.4)

### Requirement: 错误码对照表（docs/05-advanced/uvm-error-semantics.md）

The system MUST document errno semantics for UVM/HMM operations in `docs/05-advanced/uvm-error-semantics.md` (analogous to 1.1 iommu-error-semantics.md and 1.2 drm-error-semantics.md). The document MUST include at least 5 rows covering:

- `mmu_notifier_register` failure modes (e.g., `-ENOMEM` for allocation failure)
- `hmm_range_fault` failure modes (e.g., `-EBUSY` for sequence number conflict, `-EFAULT` for invalid range)
- `migrate` failure modes (e.g., `-EBUSY` for already-migrating, `-ENOMEM` for OOM)
- `mmu_interval_notifier_insert` failure modes

#### Scenario: uvm-error-semantics.md exists with at least 5 rows

- **WHEN** `cat docs/05-advanced/uvm-error-semantics.md` runs
- **THEN** the file MUST exist
- **AND** it MUST contain rows for at least `-ENOMEM`, `-EBUSY`, `-EFAULT`, `-EINVAL`, `-ENOSPC` each with code value

### Requirement: HMM 兼容矩阵文档（Linux 6.6 ↔ 6.12 LTS）

The system MUST produce `docs/05-advanced/hmm-compat-matrix.md` documenting differences between Linux 6.6 LTS and 6.12 LTS in the HMM/mmu_notifier subset. The document MUST include at least 3 categories of differences:

#### Scenario: compat matrix exists with required categories

- **WHEN** `cat docs/05-advanced/hmm-compat-matrix.md` runs
- **THEN** the file MUST exist and contain sections covering:
  - struct layout changes (must include `struct hmm_mirror` removal in 6.x)
  - function signature changes (must note `mmu_interval_notifier_insert` signature evolution)
  - new required ops (must note `mmu_interval_notifier_ops.invalidate` callback)
- **AND** each row MUST state UsrLinuxEmu's simulation strategy

### Requirement: 3 个 Catch2 standalone 测试交付

The system MUST provide 3 Catch2 standalone test executables:

| Test | Range |
|------|-------|
| `test_mmu_notifier_standalone` | mmu_notifier register/unregister + invalidate_range dispatch |
| `test_hmm_range_standalone` | hmm_range_fault + sequence number protocol + PFN table |
| `test_svm_ioctl_standalone` | SVM (Shared Virtual Memory) ioctl path through mmu_notifier + hmm_range |

All MUST pass with `ctest --output-on-failure` from project root.

#### Scenario: all 3 new tests pass after stage 1.3

- **WHEN** stage 1.3 implementation completes and `ctest --output-on-failure` runs
- **THEN** the 3 new standalone tests MUST exit 0
- **AND** all 52 pre-existing tests MUST remain green (zero regression)

### Requirement: 不引入 HAL 接口扩展（ADR-035 guardrail，承接 stage-1-2）

This change MUST NOT add any `hal_uvm_*` entries to `struct gpu_hal_ops` (the 11-function-pointer table defined per ADR-023). HAL extensions are FORBIDDEN per ADR-035 unless KFD driver code in stage 1.4 demonstrably requires them. This requirement acts as a guardrail and is captured in Oracle's Launch Condition for stage 1.3.

#### Scenario: gpu_hal_ops unchanged by stage 1.3

- **WHEN** `git diff plugins/gpu_driver/hal/include/hal_ops.h` is computed
- **THEN** `struct gpu_hal_ops` MUST have zero additions or modifications
- **AND** `openspec/changes/stage-1-3-uvm-hmm/specs/hal-uvm-ops-audit.md` MUST exist (even if 0 ops added) recording the decision rationale

## REMOVED Requirements

None.

## RENAMED Requirements

None.
