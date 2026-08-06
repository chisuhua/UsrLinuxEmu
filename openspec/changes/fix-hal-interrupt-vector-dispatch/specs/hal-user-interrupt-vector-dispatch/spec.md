# HAL Interrupt Vector Dispatch

## ADDED Requirements

### Requirement: HAL user interrupt vector dispatch

`plugins/gpu_driver/hal/hal_user.cpp::user_interrupt_raise` SHALL be upgraded to route interrupts by `vector` to the handler previously registered via `hal->interrupt_register`, dispatching synchronously when a handler is registered. This depends on the `interrupt_handlers[vector]` array established by the sibling `fix-hal-user-missing-interrupt-wiring` change. The implementation SHALL keep vector range semantics consistent with the mock (`vector < 4` is the valid range; out-of-range increments the counter and no-ops, never crashes), SHALL keep `struct gpu_hal_ops` signature unchanged, and SHALL NOT modify `hal_mock.cpp`.

#### Scenario: Vector dispatch invokes the registered handler with the supplied payload

- **GIVEN** drv/ has called `hal->interrupt_register(ctx, vector=2, my_handler)` (via the sibling change's wiring) so `hc->interrupt_handlers[2] == my_handler`
- **AND** drv/ invokes `hal->interrupt_raise_ex(ctx, vector=2, user_data=0xDEADBEEF)`
- **WHEN** the call returns
- **THEN** `my_handler(0xDEADBEEF)` SHALL have been invoked exactly once
- **AND** the `interrupt_count` atomic counter SHALL have been incremented
- **AND** the process SHALL NOT crash

#### Scenario: Vector dispatch with no registered handler no-ops safely

- **GIVEN** `hc->interrupt_handlers[3] == nullptr` (no handler registered for vector 3)
- **WHEN** drv/ invokes `hal->interrupt_raise_ex(ctx, vector=3, user_data=0xCAFEBABE)`
- **THEN** `interrupt_count` SHALL be incremented
- **AND** no handler SHALL be invoked (since none is registered)
- **AND** the call SHALL return without crashing
- **AND** no segfault or null-pointer dereference SHALL occur

#### Scenario: Out-of-range vector is silently ignored

- **GIVEN** drv/ invokes `hal->interrupt_raise_ex(ctx, vector=4, user_data=0)` (vector >= 4, outside the mock-bounded range)
- **WHEN** the call is evaluated
- **THEN** the function SHALL return without invoking any handler
- **AND** no `interrupt_count` increment SHALL occur (consistent with the mock's `if (vector >= 4) return;` early-out)
- **AND** the process SHALL NOT crash

#### Scenario: Vector dispatch is synchronous

- **GIVEN** `hc->interrupt_handlers[1] == my_handler` and the mock semantics are followed
- **WHEN** drv/ invokes `hal->interrupt_raise_ex(ctx, vector=1, user_data=0x42)`
- **THEN** by the time the call returns, `my_handler(0x42)` SHALL have been invoked (no thread spawn, no async deferral)
- **AND** the caller's next instruction SHALL see any side effects of `my_handler`

#### Scenario: Null handler array slot is defended

- **GIVEN** `hc->interrupt_handlers` is initialized such that all slots are `nullptr` at startup
- **WHEN** drv/ invokes `hal->interrupt_raise_ex(ctx, vector=0, user_data=0)` without prior register
- **THEN** no null-dereference SHALL occur
- **AND** the function SHALL behave per the "no registered handler no-ops safely" scenario

#### Scenario: struct gpu_hal_ops signature unchanged

- **GIVEN** `struct gpu_hal_ops::interrupt_raise_ex(ctx, vector, user_data)` is declared
- **WHEN** the user HAL implementation is upgraded
- **THEN** the fn-pointer signature SHALL remain identical (no ABI break)
- **AND** all existing callers SHALL continue to compile without source changes

#### Scenario: hal_mock implementation unchanged

- **GIVEN** `hal_mock.cpp` already implements `interrupt_raise_ex` per the mock semantics (counter increment + no handler dispatch)
- **WHEN** the user HAL fix is applied
- **THEN** `hal_mock.cpp` SHALL NOT be modified
- **AND** existing mock-based tests SHALL continue to pass without changes

#### Scenario: TODO(vector-dispatch) comment removed

- **GIVEN** `hal_user.cpp` previously contains a `// TODO(vector-dispatch)` (or similar) comment near the unwired `interrupt_raise` lambda
- **WHEN** the fix is applied
- **THEN** the TODO comment SHALL be removed (the work is complete)

#### Scenario: Build and ctest verification

- **GIVEN** `user_interrupt_raise` has been upgraded to vector-routed dispatch
- **WHEN** the developer runs `make -j4` and `ctest --output-on-failure`
- **THEN** the build SHALL compile with zero warnings
- **AND** `ctest` SHALL report baseline 130/130 tests PASS with no regressions
- **AND** at least one new vector-dispatch test SHALL be added and PASS

#### Scenario: lsp_diagnostics clean

- **GIVEN** the modified `hal_user.cpp` and any new test file
- **WHEN** `lsp_diagnostics` is invoked on the changed files
- **THEN** the output SHALL report no errors and no warnings on the modified lines