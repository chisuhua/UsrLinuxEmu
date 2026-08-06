# Design: HAL User Interrupt Wiring Fix

## Context

`struct gpu_hal_ops` (`plugins/gpu_driver/hal/gpu_hal.h:90-96`) declares two Stage 4.3 interrupt-model fn-pointers (`interrupt_register` and `interrupt_raise_ex`) that the drv/ layer invokes. `hal_mock.cpp` wires them correctly at lines 298-299, but `hal_user_init` in `hal_user.cpp` (line 253-583) never assigns them, so any drv/ call to `hal->interrupt_register(...)` or `hal->interrupt_raise_ex(...)` dereferences a null function pointer and SIGSEGVs. This change adds the missing wiring plus two user-side handler implementations.

## Goals / Non-Goals

**Goals:**
- Eliminate the SIGSEGV path in user HAL interrupt fn-pointer dispatch.
- Mirror the existing `hal_mock` wiring and handler patterns so behavior stays symmetric between mock and user HAL.
- Add (or extend) a Catch2 test that exercises the user HAL `interrupt_register` / `interrupt_raise_ex` path end-to-end.
- Keep `struct gpu_hal_ops` and `hal_mock.cpp` byte-identical (no ABI change, no test churn).

**Non-Goals:**
- Real MSI-X hardware register simulation (out of scope — deferred to `fix-hal-interrupt-vector-dispatch`).
- Kernel workqueue-style asynchronous dispatch — synchronous handler invocation is sufficient for parity with the mock.
- Modifying the HAL interface contract (`struct gpu_hal_ops`) — only the user-side implementation is touched.

## Decisions

- **Mirror mock semantics exactly.** Use the same 4-slot handler array and the same `if (vector >= 4) return -1` guard as `mock_interrupt_register`. This keeps drv/ behavior identical regardless of which HAL backend is loaded.
- **Synchronous handler dispatch in `user_interrupt_raise_ex`.** Mock implementation spawns a thread, but for the user HAL parity fix a direct synchronous call is sufficient and avoids introducing thread lifecycle complexity that is explicitly deferred (Out of Scope, per proposal). Async optimization is tracked as an independent follow-up.
- **Reuse the existing `hc->interrupt_count` atomic counter** where applicable, to stay consistent with the mock's observability hooks and avoid introducing a new counter that drv/ would need to know about.
- **Add a single `interrupt_handlers[4]` array to `struct hal_user_context`** (declared in `hal_user.h`) so handlers persist for the lifetime of the user HAL instance. The array size matches the mock (4 slots) and is documented in the field comment.
- **Do not modify `hal_mock.cpp` or `struct gpu_hal_ops`.** The bug is one-sided (user-side missing wiring only); keeping the mock untouched preserves all existing test coverage without regression risk.

## Risks / Trade-offs

- [Synchronous dispatch blocks the caller in user HAL] → Mitigation: clearly documented in design and tasks.md as acceptable for parity fix; async optimization tracked as independent follow-up proposal.
- [Adding `interrupt_handlers[4]` field changes `struct hal_user_context` ABI] → Mitigation: `hal_user_context` is a private internal struct to the user HAL implementation (not part of any HAL contract or public ABI), so the change is internal-only and carries no external compatibility risk.
- [Test coverage of user HAL path is new — risk of test missing edge cases] → Mitigation: at minimum assert (a) handler stored after `interrupt_register`, (b) handler invoked on `interrupt_raise_ex`, (c) out-of-range vector returns negative error.