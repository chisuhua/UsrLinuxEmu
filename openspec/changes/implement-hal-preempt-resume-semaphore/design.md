# Design: HAL Preempt + Resume + Timeline Semaphore

## Context

ADR-046 (Preemption) and ADR-049 (Timeline Semaphore) define seven HAL fn-pointers on `struct gpu_hal_ops`: `hal_preempt`, `hal_resume`, `hal_sem_create`, `hal_sem_signal`, `hal_sem_wait`, `hal_sem_query`, `hal_sem_destroy`. The current `hal_user.cpp` implementations (lines 293-307) are all no-ops returning `0` (or in the case of `hal_sem_create`, returning an incrementing counter that is never stored). `hal_user.cpp:14` already `#include`s `../sim/semaphore_manager.h`, so `sim::SemaphoreManager` exists; the gap is that none of the seven lambdas delegate to it. The mock HAL remains no-op for parity. This change wires the user HAL end-to-end so drv/ callers see real preemption semantics and a real semaphore lifecycle.

## Goals / Non-Goals

**Goals:**
- Replace all seven no-op lambdas in `hal_user.cpp` with real implementations delegating to `sim::SemaphoreManager` and the sim scheduler.
- Reuse the `fence_create` reference path at `hal_user.cpp:90-109` as the template for `hal_sem_*` wiring.
- Keep `struct hal_user_context` thread-safe (mirror the `fence_lock` pattern).
- Add three new Catch2 unit tests covering semaphore lifecycle, wait-callback firing, and basic preempt/resume.

**Non-Goals:**
- Implementing Green Context / PDL HAL fn-pointers (those belong to `implement-hal-green-context-and-pdl`).
- Extending `SemaphoreManager` itself — assumed complete; the change verifies the public API is sufficient before implementation begins, and any missing methods are flagged for follow-up rather than fixed in-scope.
- Kernel workqueue-style asynchronous dispatch — sem wait uses `SemaphoreManager`'s existing callback model, no new async machinery.

## Decisions

- **Delegate to `sim::SemaphoreManager` for all `hal_sem_*` operations.** `SemaphoreManager` already implements `create` / `signal` / `wait` / `query` / `destroy` per the `fence_create` reference implementation. We do not re-implement semaphore semantics in user HAL.
- **Delegate `hal_preempt` / `hal_resume` to sim scheduler methods** (`GlobalScheduler::preempt(channel_id)` and `GlobalScheduler::resume(channel_id)`); a pre-implementation verification step confirms these methods exist on the scheduler before writing the lambda bodies. If absent, the change flags a follow-up rather than inlining new scheduler logic.
- **Add `sem_handles` map to `struct hal_user_context`** keyed by `uint64_t handle → SemaphoreManager*`, guarded by the same lock used for `fence_lock`. This mirrors the existing fence handle map pattern and reuses the proven locking discipline.
- **Return `-EINVAL` when a handle is unknown to `sem_handles`.** All four handle-accepting fn-pointers validate membership before delegating to `SemaphoreManager`. This preserves the proposal's contract that invalid handles are non-fatal.
- **Reuse the `fence_create` reference path (`hal_user.cpp:90-109`) as the structural template** for `hal_sem_create` (handle storage pattern, error propagation). This is the canonical prior art for `SemaphoreManager` integration in this codebase.

## Risks / Trade-offs

- [Pre-implementation verification of `GlobalScheduler::preempt/resume` may fail — the methods might not exist yet] → Mitigation: a blocking prerequisite task in `tasks.md` confirms the API surface before implementation begins; if absent, the change is split into a follow-up that adds the scheduler methods.
- [Adding `sem_handles` map changes `struct hal_user_context` ABI] → Mitigation: `hal_user_context` is internal to the user HAL implementation (not part of any HAL contract or public ABI), so the change is internal-only.
- [Asynchronous `SemaphoreManager::wait` callback firing under contention with `signal` may surface pre-existing races in `SemaphoreManager`] → Mitigation: thread-safety is `SemaphoreManager`'s contract; if the sanitizer run exposes a bug, it is filed as a separate follow-up rather than worked around in user HAL.
- [Increased user HAL surface area raises test maintenance cost] → Mitigation: three focused unit tests cover the public contract; tests use the mock HAL for unrelated paths so user HAL test churn stays minimal.