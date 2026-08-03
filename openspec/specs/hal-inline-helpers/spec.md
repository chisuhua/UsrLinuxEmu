# hal-inline-helpers Specification

## Purpose
TBD - created by archiving change 2026-08-03-stage4-6-green-context-pdl-closeout. Update Purpose after archive.
## Requirements
### Requirement: Green Context inline HAL wrappers

`plugins/gpu_driver/hal/gpu_hal.h` SHALL provide zero-overhead inline wrappers for the Stage 4.6 (ADR-056) green context HAL fn-pointers, mirroring the Stage 4.5 inline wrapper pattern (line 250-283 of the file). The wrappers are convenience accessors so callers do not need to manually thread `hal->ctx` through every call.

#### Scenario: hal_green_context_create inline wrapper

- **GIVEN** a caller holds a `struct gpu_hal_ops *hal` with valid fn-pointer table
- **AND** `hal->hal_green_context_create` is registered (e.g. `hal_mock.cpp` or `hal_user.cpp` per ADR-056 §4.3-4.5)
- **WHEN** the caller invokes `hal_green_context_create(hal, tsg_id, &out_handle)`
- **THEN** the wrapper SHALL forward to `hal->hal_green_context_create(hal->ctx, tsg_id, &out_handle)`
- **THEN** the return value SHALL match the underlying fn-pointer's return
- **THEN** the overhead SHALL be zero (compiled to a direct fn-pointer call with no extra branch)

#### Scenario: hal_green_context_destroy inline wrapper

- **GIVEN** a green context handle obtained via `hal_green_context_create`
- **WHEN** the caller invokes `hal_green_context_destroy(hal, handle)`
- **THEN** the wrapper SHALL forward to `hal->hal_green_context_destroy(hal->ctx, handle)`
- **THEN** the wrapper SHALL NOT introduce observable side effects beyond the underlying fn-pointer's behavior

### Requirement: PDL inline HAL wrappers

`plugins/gpu_driver/hal/gpu_hal.h` SHALL provide zero-overhead inline wrappers for the Stage 4.6 (ADR-056) Programmatic Dependent Launch (PDL) HAL fn-pointers. The wrappers are convenience accessors matching the Stage 4.5 inline wrapper pattern.

#### Scenario: hal_pdl_launch inline wrapper

- **GIVEN** a caller holds a `struct gpu_hal_ops *hal` with PDL fn-ptrs registered
- **WHEN** the caller invokes `hal_pdl_launch(hal, kernel_addr, kernargs_va, grid_x, block_x, &out_signal_handle)`
- **THEN** the wrapper SHALL forward to `hal->hal_pdl_launch(hal->ctx, kernel_addr, kernargs_va, grid_x, block_x, &out_signal_handle)`
- **THEN** the return value SHALL match the underlying fn-pointer's return

#### Scenario: hal_pdl_signal_completion inline wrapper

- **GIVEN** a PDL completion signal handle obtained via `hal_pdl_launch`
- **WHEN** the caller invokes `hal_pdl_signal_completion(hal, signal_handle, value)`
- **THEN** the wrapper SHALL forward to `hal->hal_pdl_signal_completion(hal->ctx, signal_handle, value)`
- **THEN** the wrapper SHALL preserve the monotonic-increment semantics (`value > current`)

### Requirement: Backward compatibility

The 4 new inline wrappers SHALL be a strict superset of existing capability — adding them SHALL NOT modify any existing fn-pointer field or inline wrapper behavior. All existing drv/ layer callers continue to work without source changes.

#### Scenario: No breakage of existing callers

- **GIVEN** the codebase has callers using `hal->hal_sem_create(hal->ctx, ...)` (pre-existing pattern)
- **WHEN** the new inline wrappers (hal_green_context_create, etc.) are added
- **THEN** all existing `hal->hal_X(...)` call patterns SHALL continue to function identically
- **THEN** the diff SHALL be limited to gpu_hal.h +25 lines (4 wrappers + 2 section dividers)
- **THEN** `grep -rn '#include.*"sim/' plugins/gpu_driver/drv/` SHALL remain empty (ADR-023 HAL boundary enforce)

#### Scenario: HAL fn-ptr count upper bound check

- **GIVEN** the existing `struct gpu_hal_ops` has 33 entries (per Stage 4.6 archive INDEX entry)
- **WHEN** the inline wrappers are added (4 wrappers, no fn-pointer field changes)
- **THEN** the fn-pointer count SHALL remain 33 (inline wrappers are convenience, not new ops)
- **AND** the upper-bound margin (per ADR-072) is ≤ 35 — remaining margin is 2 fn-pointer slots for future HAL extension

