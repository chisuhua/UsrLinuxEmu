# docs-audit-cleanup Specification

## Purpose
TBD - created by archiving change stage4-5-cp-phase6-preemption-timeline-sem-gaps. Update Purpose after archive.
## Requirements
### Requirement: Docs-Audit Strict Mode PASS

The system SHALL pass `tools/docs-audit.sh --strict` with 0 warnings.

#### Scenario: Strict mode overall result
- **WHEN** `tools/docs-audit.sh --strict` is run
- **THEN** the output shows `Result: ✅ PASS` (or equivalent)
- **AND** `Failed: 0` AND `Warnings: 0`

### Requirement: Kernel File Count Warning Resolved

The system SHALL correctly report the current kernel file count in docs-audit, with no false warning.

#### Scenario: Audit baseline matches reality
- **WHEN** `find src/kernel -name '*.cpp' | wc -l` returns N
- **THEN** `tools/docs-audit.sh` does NOT emit "src/kernel has X cpp files (baseline Y)" warning

#### Scenario: Baseline updated to 46
- **WHEN** `tools/docs-audit.sh` is inspected
- **THEN** the kernel file count baseline is 46 (or dynamically derived)
- **AND** a comment explains the baseline source

### Requirement: gpu_hal.h fn-ptr Count Warning Resolved

The system SHALL correctly report the current gpu_hal.h fn-ptr count in post-refactor-architecture.md, with no false warning.

#### Scenario: Audit fn-ptr count matches reality
- **WHEN** `grep -cE '^\s*int \(\*' plugins/gpu_driver/hal/gpu_hal.h` returns 22
- **THEN** `tools/docs-audit.sh` does NOT emit "gpu_hal.h has X fn-ptrs (doc claims Y)" warning

#### Scenario: Architecture doc fn-ptr count updated
- **WHEN** `docs/02_architecture/post-refactor-architecture.md` §附录 A is inspected
- **THEN** the documented fn-ptr count is 22 (matching gpu_hal.h reality)
- **AND** the new fn-ptrs are listed: `hal_preempt`, `hal_resume`, `hal_sem_create`, `hal_sem_signal`, `hal_sem_wait`, `hal_sem_query`, `hal_sem_destroy`, `interrupt_register` (8 new fn-ptrs from v1, 14 → 22)

### Requirement: Doxygen CI Installation

The CI pipeline SHALL have Doxygen installed so that docs-audit does not emit the "Doxygen not installed" warning.

#### Scenario: CI installs doxygen
- **WHEN** `.github/workflows/cmake-multi-platform.yml` is inspected
- **THEN** an `apt install -y doxygen graphviz` step (or equivalent) exists before docs-audit runs

#### Scenario: docs-audit finds doxygen
- **WHEN** `tools/docs-audit.sh --strict` runs in CI
- **THEN** doxygen is found in PATH and the warning is not emitted

### Requirement: All Three Warnings Resolved Simultaneously

After applying all three fixes, docs-audit --strict SHALL pass with no warnings from any of these sources.

#### Scenario: Comprehensive warning count
- **WHEN** `tools/docs-audit.sh --strict` is run after all fixes applied
- **THEN** `Passed` count equals total checks AND `Failed: 0` AND `Warnings: 0`

