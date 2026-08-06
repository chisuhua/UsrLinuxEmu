# hal-user-interrupt-wiring Specification

## Purpose
TBD - created by archiving change fix-hal-user-missing-interrupt-wiring. Update Purpose after archive.
## Requirements
### Requirement: HAL user interrupt wiring

`plugins/gpu_driver/hal/hal_user.cpp` SHALL explicitly assign `hal->interrupt_register` and `hal->interrupt_raise_ex` during `hal_user_init` (line 253-583), so that drv/ callers invoking these Stage 4.3 fn-pointers never dereference a null function pointer. The wiring SHALL mirror the existing `hal_mock` pattern (`hal_mock.cpp:298-299`), and the new user-side handler implementations SHALL mirror the existing mock handlers (`mock_interrupt_register` at `hal_mock.cpp:117-125`, `mock_interrupt_raise_ex` at `hal_mock.cpp:127-138`). The `struct gpu_hal_ops` signature SHALL NOT change, and `hal_mock.cpp` SHALL NOT be modified.

#### Scenario: hal_user_init wires interrupt_register fn-pointer

- **GIVEN** `hal_user_init` has executed successfully
- **WHEN** the drv/ layer invokes `hal->interrupt_register(ctx, vector, handler)`
- **THEN** `hal->interrupt_register` SHALL NOT be null — it SHALL have been assigned in `hal_user_init`
- **AND** the handler SHALL be stored at `hc->interrupt_handlers[vector]`
- **AND** the function SHALL return `0` on success and `-1` (or a negative errno) when `vector >= 4`

#### Scenario: hal_user_init wires interrupt_raise_ex fn-pointer

- **GIVEN** `hal_user_init` has executed successfully
- **WHEN** the drv/ layer invokes `hal->interrupt_raise_ex(ctx, vector, user_data)`
- **THEN** `hal->interrupt_raise_ex` SHALL NOT be null — it SHALL have been assigned in `hal_user_init`
- **AND** the handler previously registered at `vector` SHALL be invoked with `user_data`
- **AND** the call SHALL NOT crash the process

#### Scenario: Before-fix SIGSEGV path

- **GIVEN** the current state of `hal_user.cpp` where the two fn-pointers are unwired
- **WHEN** drv/ invokes `hal->interrupt_register(ctx, vector, handler)`
- **THEN** the call dereferences a null function pointer and triggers SIGSEGV
- **AND** the user-mode process terminates abnormally before any handler can run

#### Scenario: After-fix handler dispatch end-to-end

- **GIVEN** `hal_user_init` has wired both fn-pointers and drv/ has called `hal->interrupt_register(ctx, vector, handler)` for some `vector < 4`
- **WHEN** drv/ invokes `hal->interrupt_raise_ex(ctx, vector, user_data)`
- **THEN** the stored handler SHALL be invoked with the `user_data` payload
- **AND** the dispatch SHALL succeed without SIGSEGV

#### Scenario: struct gpu_hal_ops signature unchanged

- **GIVEN** `struct gpu_hal_ops` declares `interrupt_register` and `interrupt_raise_ex` at `plugins/gpu_driver/hal/gpu_hal.h:90-96`
- **WHEN** the fix is applied
- **THEN** the fn-pointer signatures SHALL remain identical (no ABI break)
- **AND** all existing callers of these fn-pointers SHALL continue to compile without source changes

#### Scenario: hal_mock implementation unchanged

- **GIVEN** `hal_mock.cpp` already wires both fn-pointers correctly at lines 298-299
- **WHEN** the user HAL fix is applied
- **THEN** `hal_mock.cpp` SHALL NOT be modified
- **AND** existing mock-based tests SHALL continue to pass without changes

#### Scenario: Vector slot count matches mock

- **GIVEN** the mock implementation uses `if (vector >= 4) return -1` to bound the handler array
- **WHEN** the user HAL implements `user_interrupt_register`
- **THEN** it SHALL use 4 vector slots consistent with the mock semantics
- **AND** out-of-range vectors SHALL return a negative error code

#### Scenario: Build and ctest verification

- **GIVEN** the wiring and handler functions have been added to `hal_user.cpp` (and supporting context fields to `hal_user.h` if needed)
- **WHEN** the developer runs `make -j4` and `ctest --output-on-failure`
- **THEN** the build SHALL compile with zero warnings
- **AND** `ctest` SHALL report all baseline tests PASS (130/130, no regressions)
- **AND** at least one new or extended test SHALL cover the user HAL `interrupt_register` and `interrupt_raise_ex` path

#### Scenario: lsp_diagnostics clean

- **GIVEN** the modified source files (`hal_user.cpp`, `hal_user.h`, and any new test file)
- **WHEN** `lsp_diagnostics` is invoked on the changed files
- **THEN** the output SHALL report no errors and no warnings on the modified lines

