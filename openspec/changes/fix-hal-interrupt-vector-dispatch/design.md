# Design: HAL Interrupt Vector Dispatch

## Context

The sibling `fix-hal-user-missing-interrupt-wiring` change wires `hal->interrupt_register` and `hal->interrupt_raise_ex` in `hal_user_init` and establishes `hc->interrupt_handlers[4]` for handler storage. This change goes one step further: `user_interrupt_raise_ex` is currently a stub that increments a counter but never invokes the registered handler. The proposal makes it actually route by `vector` to the handler, mirroring the mock's behavior. Without this change, even with the wiring in place, raising an interrupt would never trigger a drv/-registered handler.

## Goals / Non-Goals

**Goals:**
- Replace the counter-only `user_interrupt_raise_ex` with a vector-routed synchronous dispatch.
- Match mock semantics exactly (vector range, counter behavior, no crash on missing handler).
- Remove the `// TODO(vector-dispatch)` comment.
- Add a Catch2 unit test covering dispatch / no-handler / out-of-range paths.

**Non-Goals:**
- Wiring `interrupt_register` / `interrupt_raise_ex` themselves (handled by `fix-hal-user-missing-interrupt-wiring`).
- Kernel workqueue-style asynchronous dispatch — synchronous handler invocation is the chosen model.
- MSI-X hardware register simulation.
- Nested interrupt handling.

## Decisions

- **Synchronous dispatch (matches mock).** The handler is invoked directly in the caller's thread. This matches the mock's behavior (after its thread-spawn refactor is consolidated) and is simpler to test. Asynchronous dispatch is explicitly out of scope.
- **Mirror mock vector range.** Use `if (vector >= 4) return;` as the early-out, matching the mock's `interrupt_register` bounds. The 4-slot array size was established by the sibling change and is not re-decided here.
- **Counter increments only on in-range dispatch.** Increment `hc->interrupt_count` when a valid `vector < 4` is raised, even if no handler is registered. This matches the mock and gives observers a way to detect "raised but unhandled" interrupts.
- **Null-handler defense.** Before invoking `hc->interrupt_handlers[vector]`, the implementation SHALL check for null and skip invocation. This is belt-and-suspenders against future code paths that might leave a slot uninitialized.
- **No modification to `hal_mock.cpp`.** The bug is one-sided; the mock already works.
- **Remove the `// TODO(vector-dispatch)` comment** as part of the implementation. Leaving the TODO in place after the fix would be misleading.

## Risks / Trade-offs

- [Synchronous dispatch means a misbehaving handler blocks the caller] → Mitigation: this is the chosen model and matches the mock; handler correctness is the drv/ layer's responsibility. If async is later desired, it is a separate, additive change.
- [Counter increment on "raised but unhandled" could mask real bugs by giving false observability] → Mitigation: the counter is the same one used by the mock; observability semantics are consistent across HAL backends.
- [Null-handler defense is a tiny perf cost (one extra branch per dispatch)] → Mitigation: a single predictable branch is negligible; the alternative (crash on null) is unacceptable.
- [Depends on the sibling `fix-hal-user-missing-interrupt-wiring` change landing first] → Mitigation: the implementation MUST be reviewed against the sibling change's actual `hc->interrupt_handlers[4]` field declaration; if the sibling change's field name or type differs, this change adapts accordingly. The dependency is documented in tasks.md.