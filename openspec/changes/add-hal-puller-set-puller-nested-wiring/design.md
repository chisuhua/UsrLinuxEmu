# Design: HAL Puller — `setSimPuller` Nested Wiring

## Context

`plugins/gpu_driver/hal/hal_user.cpp` `puller_set_puller` lambda (line 548-560) currently self-documents as a no-op reserved for future nested-puller wiring. The `HardwarePullerEmu` class does not expose a `setPuller` / `setSimPuller` method, so any drv/ call to `hal_puller_set_puller` returns `0` without affecting internal state. This change closes the documented debt by adding a minimal `setSimPuller(uint64_t)` setter on `HardwarePullerEmu` and upgrading the HAL lambda to delegate to it.

## Goals / Non-Goals

**Goals:**

- Minimize the `HardwarePullerEmu` surface expansion (exactly one setter method + one atomic field).
- Provide thread-safe storage via `std::atomic<uint64_t>` (no mutex — a single handle write is naturally lock-free).
- Wire the HAL `puller_set_puller` lambda to the real delegate and remove the "currently a no-op" comment.
- Add a focused unit test that verifies the success path plus both `-EINVAL` error paths.

**Non-Goals:**

- Actual nested-puller runtime dispatch (one `HardwarePullerEmu` monitoring multiple `sim_puller` instances). This change only delivers the API surface; the multi-listener FSM remains future work.
- Modifying `struct gpu_hal_ops` signature (fn-pointer field stays as-is).
- Modifying the mock HAL implementation (`hal_mock.cpp` keeps its no-op behavior per proposal MUST NOT).
- Adding a `getSimPuller` getter — internal field visibility is exposed for testing only.

## Decisions

1. **Atomic field over mutex.** `std::atomic<uint64_t>` is sufficient for single-handle storage: a write-only atomic store is naturally lock-free, and no read-modify-write is needed. A mutex would add overhead without semantic benefit.
2. **No `getSimPuller` getter.** The proposal explicitly scopes the change to API completeness; querying the field is **out of scope**. Tests observe the internal state by reading the atomic directly via a test-only accessor path (friend declaration or `protected` visibility — to be decided at implementation time).
3. **HAL lambda delegates via `shared_ptr<HardwarePullerEmu>`.** `hc->pullers` is a `std::unordered_map<hal_puller_handle_t, std::shared_ptr<HardwarePullerEmu>>`; the `it->second->setSimPuller(handle)` call is type-safe and lifetime-safe (the `shared_ptr` keeps the emulator alive for the call).
4. **Mock HAL stays no-op.** Per proposal.md MUST NOT — `hal_mock.cpp::puller_set_puller` remains a no-op. Only `hal_user.cpp` (the real HAL) gets the wired delegate.

## Risks / Trade-offs

- **API completeness without runtime use.** The new `setSimPuller` is callable but no caller exercises it yet, so the wire is unverified by production paths. The unit test covers both error paths and the success path; TSan run under §4.3 confirms no race. Real nested-puller usage is deferred to a follow-up change.
- **Test-only access to atomic field.** Exposing `sim_puller_handle_` for assertions risks leaking internals. Mitigation: keep the exposure minimal (friend class or `protected` field) and document it as test-only.
- **No migration path documented for existing callers.** No drv/ code calls `hal_puller_set_puller` today, so no caller is broken; future drv/ callers will silently start observing the stored handle, which is the intended forward-compatibility win.