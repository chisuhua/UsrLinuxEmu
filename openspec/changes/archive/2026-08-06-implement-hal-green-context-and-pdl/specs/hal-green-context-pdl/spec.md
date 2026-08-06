# HAL Green Context + PDL Wiring

## ADDED Requirements

### Requirement: HAL user green context and PDL wiring

`plugins/gpu_driver/hal/hal_user.cpp` SHALL replace the four Stage 4.6 (ADR-056) HAL fn-pointer lambdas (`hal_green_context_create`, `hal_green_context_destroy`, `hal_pdl_launch`, `hal_pdl_signal_completion`) with real implementations. If the sim layer lacks `plugins/gpu_driver/sim/green_context.{h,cpp}` or `plugins/gpu_driver/sim/pdl.{h,cpp}`, minimal skeletal headers and implementations SHALL be created in scope. The user HAL SHALL delegate green-context lifecycle and PDL kernel dispatch to the sim layer (reusing `HardwarePullerEmu` for kernel dispatch where applicable and `sim::SemaphoreManager` for the PDL completion semaphore), SHALL keep `struct gpu_hal_ops` signature unchanged, SHALL keep `hal_mock.cpp` unchanged, and SHALL maintain TSG (`tsg_id`) binding semantics. `openspec/specs/green-context/spec.md` and `openspec/specs/pdl-launch/spec.md` SHALL have their `TBD Purpose` headers updated to full descriptions referencing this change at archive time.

#### Scenario: hal_green_context_create binds TSG and returns a handle

- **GIVEN** drv/ invokes `hal->hal_green_context_create(ctx, tsg_id, &out_handle)`
- **WHEN** the call returns
- **THEN** the sim layer SHALL have created a green context bound to `tsg_id`
- **AND** `out_handle` SHALL be a non-zero handle that drv/ can later pass to `hal_green_context_destroy`
- **AND** the function SHALL return `0` on success
- **AND** the function SHALL NOT return `-ENOSYS`

#### Scenario: hal_green_context_destroy releases the green context

- **GIVEN** drv/ holds a valid green context handle from `hal_green_context_create`
- **WHEN** drv/ invokes `hal->hal_green_context_destroy(ctx, handle)`
- **THEN** the sim layer SHALL release the green context and unbind the TSG
- **AND** subsequent operations on `handle` SHALL return `-EINVAL`
- **AND** the function SHALL return `0` on success

#### Scenario: hal_pdl_launch dispatches a kernel and returns a completion semaphore

- **GIVEN** drv/ invokes `hal->hal_pdl_launch(ctx, kernel_addr, kernargs_va, grid_x, block_x, &out_signal_handle)`
- **WHEN** the call returns
- **THEN** the sim layer SHALL have dispatched the PDL kernel via `HardwarePullerEmu` (or the existing GPU queue submit path)
- **AND** `out_signal_handle` SHALL be a real `SemaphoreManager` handle that drv/ can pass to `hal_sem_wait` (or `hal_pdl_signal_completion` once it fires)
- **AND** the function SHALL return `0` on success
- **AND** the function SHALL NOT return `-ENOSYS`
- **AND** if `grid_x == 0 || block_x == 0` the function SHALL return `-EINVAL` (basic dim sanity check)

#### Scenario: hal_pdl_signal_completion signals the PDL completion semaphore

- **GIVEN** drv/ holds `signal_handle` from a prior `hal_pdl_launch`
- **WHEN** drv/ invokes `hal->hal_pdl_signal_completion(ctx, signal_handle, value)`
- **THEN** the underlying `SemaphoreManager` instance SHALL be signaled to `value` (monotonic increment)
- **AND** any waiter registered via `SemaphoreManager::wait` SHALL be triggered if `value >= expected`
- **AND** the function SHALL return `0` on success
- **AND** if the handle is unknown the function SHALL return `-EINVAL`

#### Scenario: struct gpu_hal_ops signature unchanged

- **GIVEN** `struct gpu_hal_ops` declares the four fn-pointers
- **WHEN** the user HAL implementation is upgraded
- **THEN** the four fn-pointer signatures SHALL remain identical (no ABI break)
- **AND** all existing callers SHALL continue to compile without source changes

#### Scenario: hal_mock implementation unchanged

- **GIVEN** `hal_mock.cpp` currently implements the four fn-pointers (likely as `-ENOSYS` stubs or simple counters)
- **WHEN** the user HAL fix is applied
- **THEN** `hal_mock.cpp` SHALL NOT be modified
- **AND** existing mock-based tests SHALL continue to pass without changes

#### Scenario: TSG binding is preserved across create/destroy

- **GIVEN** drv/ creates two green contexts with different `tsg_id` values
- **WHEN** drv/ inspects (via the sim-layer observable state or test hook) which TSG each context is bound to
- **THEN** each context SHALL remain bound to its original `tsg_id` for its lifetime
- **AND** `hal_green_context_destroy` SHALL release the TSG binding

#### Scenario: Build and ctest verification

- **GIVEN** the four lambdas have been replaced with real implementations and sim-layer skeletons are in place
- **WHEN** the developer runs `make -j4` and `ctest --output-on-failure`
- **THEN** the build SHALL compile with zero warnings
- **AND** `ctest` SHALL report baseline 130/130 tests PASS plus the 2 new tests (`test_green_context_create_destroy`, `test_pdl_launch_signal_completion`) PASS

#### Scenario: lsp_diagnostics and sanitizer runs clean

- **GIVEN** the modified `hal_user.cpp`, `hal_user.h`, any new sim-layer files, and any new test files
- **WHEN** `lsp_diagnostics` is run on the changed files AND a sanitizer build (`SANITIZER=asan-ubsan ./build.sh test`) is executed
- **THEN** `lsp_diagnostics` SHALL report no errors and no warnings on modified lines
- **AND** the sanitizer run SHALL report zero failures

#### Scenario: spec TBD-Purpose headers updated at archive

- **GIVEN** `openspec/specs/green-context/spec.md` and `openspec/specs/pdl-launch/spec.md` currently begin with `# <Name> Specification` followed by `## Purpose` `TBD - created by archiving change <X>. Update Purpose after archive.`
- **WHEN** this change is archived
- **THEN** both files' `## Purpose` sections SHALL be replaced with a complete description referencing the change's proposal and the implemented HAL wiring
- **AND** the change's archive metadata SHALL include the spec update