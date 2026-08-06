# Tasks: HAL User Interrupt Wiring Fix

## 1. Implementation

- [ ] 1.1 Add `interrupt_handlers[4]` (function pointer array) field to `struct hal_user_context` in `plugins/gpu_driver/hal/hal_user.h`, sized to match the 4-slot mock semantics.
- [ ] 1.2 Implement `static int user_interrupt_register(void *ctx, uint32_t vector, void (*handler)(uint64_t user_data))` in `plugins/gpu_driver/hal/hal_user.cpp`, mirroring `mock_interrupt_register` (`hal_mock.cpp:117-125`): store the handler at `hc->interrupt_handlers[vector]`, return `-1` (or `-EINVAL`) when `vector >= 4`, and bump `hc->interrupt_count` on success.
- [ ] 1.3 Implement `static void user_interrupt_raise_ex(void *ctx, uint32_t vector, uint64_t user_data)` in `plugins/gpu_driver/hal/hal_user.cpp`, mirroring `mock_interrupt_raise_ex` (`hal_mock.cpp:127-138`): look up `hc->interrupt_handlers[vector]`, invoke it synchronously with `user_data` if non-null, and no-op when out-of-range.
- [ ] 1.4 In `hal_user_init` (`hal_user.cpp:253-583`), add the two wiring assignments `hal->interrupt_register = user_interrupt_register;` and `hal->interrupt_raise_ex = user_interrupt_raise_ex;` next to the existing HAL fn-pointer assignments.

## 2. Verification

- [ ] 2.1 Run `make -j4` from `build/` and confirm zero compile warnings on changed files.
- [ ] 2.2 Run `ctest --output-on-failure` from `build/` and confirm the baseline 130/130 tests PASS with no regressions.
- [ ] 2.3 Run `lsp_diagnostics` on `hal_user.cpp`, `hal_user.h`, and any new test file — confirm no errors and no warnings on modified lines.
- [ ] 2.4 Grep verification: `grep -n 'hal->interrupt_register\s*=' plugins/gpu_driver/hal/hal_user.cpp` returns exactly 1 wiring line; same check for `hal->interrupt_raise_ex` returns exactly 1.
- [ ] 2.5 Add or extend a Catch2 test (e.g. extend `tests/test_hal_user_standalone.cpp` or create a new test binary) that: (a) loads the user HAL, (b) calls `hal->interrupt_register` for vector 0, (c) calls `hal->interrupt_raise_ex` for vector 0 and asserts the handler was invoked with the supplied `user_data`, (d) asserts out-of-range vector returns a negative error code. Register the test in the appropriate `CMakeLists.txt` if a new binary is created.

## 3. Documentation

- [ ] 3.1 Update `openspec/changes/INDEX.md` (if it exists) to register the `fix-hal-user-missing-interrupt-wiring` change with a one-line summary and status.

## 4. OpenSpec Validation

- [ ] 4.1 Run `openspec validate fix-hal-user-missing-interrupt-wiring` and confirm the change passes schema validation (no proposal/spec/design/tasks errors).