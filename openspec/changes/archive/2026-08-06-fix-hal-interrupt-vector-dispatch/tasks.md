# Tasks: HAL Interrupt Vector Dispatch

## 1. Prerequisite

- [ ] 1.1 Confirm `struct hal_user_context` has `interrupt_handlers[4]` field and `interrupt_count` atomic counter (provided by the sibling `fix-hal-user-missing-interrupt-wiring` change). If the sibling change has not yet landed, this change's tasks §2 onwards are blocked.

## 2. Implementation

- [ ] 2.1 Replace the `user_interrupt_raise_ex` body in `plugins/gpu_driver/hal/hal_user.cpp` with a vector-routed synchronous dispatch:
  - If `vector >= 4` → return early (no counter increment, no handler invocation; matches mock).
  - Look up `handler = hc->interrupt_handlers[vector]`.
  - If `handler != nullptr` → invoke `handler(user_data)` synchronously.
  - Increment `hc->interrupt_count` (atomic).
  - Return.
- [ ] 2.2 Remove the `// TODO(vector-dispatch)` (or equivalent) comment near the previously-stubbed `user_interrupt_raise_ex` body.

## 3. Tests

- [ ] 3.1 Add Catch2 unit test for vector dispatch success: register a handler at vector 2, call `hal->interrupt_raise_ex(ctx, 2, 0xDEADBEEF)`, assert the handler was invoked exactly once with the supplied `user_data` and `interrupt_count` was incremented.
- [ ] 3.2 Add Catch2 unit test for no-handler no-op: with no handler registered at vector 1, call `hal->interrupt_raise_ex(ctx, 1, 0x42)`, assert no handler was invoked and `interrupt_count` was incremented.
- [ ] 3.3 Add Catch2 unit test for out-of-range vector: call `hal->interrupt_raise_ex(ctx, 4, 0)`, assert no handler was invoked and `interrupt_count` was NOT incremented (early-out semantics).
- [ ] 3.4 Register any new test binaries in the appropriate `tests/CMakeLists.txt` so `ctest` picks them up.

## 4. Verification

- [ ] 4.1 Run `make -j4` from `build/` and confirm zero compile warnings on changed files.
- [ ] 4.2 Run `ctest --output-on-failure` from `build/` and confirm baseline 130/130 tests PASS plus the new vector-dispatch tests PASS.
- [ ] 4.3 Run `lsp_diagnostics` on `hal_user.cpp` and any new test file — confirm no errors and no warnings.
- [ ] 4.4 Run `openspec validate fix-hal-interrupt-vector-dispatch` and confirm the change passes schema validation.