# Design: HAL Green Context + PDL Wiring

## Context

ADR-056 defines two Stage 4.6 features — Green Context (a low-priority preemptable CUDA context) and Programmatic Dependent Launch (PDL) — and four HAL fn-pointers on `struct gpu_hal_ops`: `hal_green_context_create`, `hal_green_context_destroy`, `hal_pdl_launch`, `hal_pdl_signal_completion`. The current `hal_user.cpp` implementations return `-ENOSYS` (stubs), and the sim layer (`plugins/gpu_driver/sim/`) is not yet known to contain `green_context.{h,cpp}` or `pdl.{h,cpp}`. This change brings the user HAL end-to-end and creates minimal sim-layer skeletons if needed. The existing `HardwarePullerEmu` is the canonical kernel dispatch surface and `sim::SemaphoreManager` is the canonical completion-semaphore surface, so PDL reuses both.

## Goals / Non-Goals

**Goals:**
- Replace the four `-ENOSYS` lambdas in `hal_user.cpp` with real implementations delegating to the sim layer.
- Create minimal `green_context.{h,cpp}` and `pdl.{h,cpp}` sim skeletons if they do not yet exist.
- Reuse `HardwarePullerEmu` for PDL kernel dispatch (it already handles kernel lifecycle).
- Reuse `sim::SemaphoreManager` for the PDL completion semaphore (already wired by the sibling `implement-hal-preempt-resume-semaphore` change).
- Update `openspec/specs/green-context/spec.md` and `openspec/specs/pdl-launch/spec.md` `TBD Purpose` headers at archive time.

**Non-Goals:**
- Preemption / Semaphore fn-pointers (those belong to `implement-hal-preempt-resume-semaphore`).
- Advanced Green Context scheduling policies — only the basic API surface is wired.
- Multi-kernel PDL dependency chaining — only basic `launch + signal` is supported.
- Implementing actual TSG state machine logic beyond the minimum needed for `create` / `destroy` binding.

## Decisions

- **Reuse `HardwarePullerEmu` for PDL kernel dispatch.** `HardwarePullerEmu` already implements the kernel pull FSM (IDLE → FETCH → DECODE → DISPATCH → EXECUTE → COMPLETE → IDLE). PDL is a kernel launch with a completion semaphore; the kernel-dispatch portion is the same. The new `hal_pdl_launch` body submits to `HardwarePullerEmu` and registers a completion callback that signals the returned `SemaphoreManager` handle.
- **Reuse `sim::SemaphoreManager` for PDL completion signal handle** — the same primitive used by `implement-hal-preempt-resume-semaphore`. `hal_pdl_signal_completion` becomes a thin wrapper around `SemaphoreManager::signal`.
- **Minimal sim-layer skeletons if absent.** If `green_context.{h,cpp}` or `pdl.{h,cpp}` are missing, create them with the minimum surface (`GreenContext::create(tsg_id)`, `GreenContext::destroy()`, `PdlLauncher::launch(...)`). Bodies can be no-op-with-storage initially; full behavior is out of scope per the proposal.
- **Add `green_context_handles` map to `struct hal_user_context`** (mirroring `sem_handles` from the sibling change), guarded by the same lock pattern. This isolates green-context lifetime from any sim-layer map.
- **Sanity-check `grid_x` and `block_x`** in `hal_pdl_launch`. Zero or absurdly large values are rejected with `-EINVAL` so the sim layer never sees malformed dispatch parameters. This is a defense-in-depth check, not a full validation suite.
- **Update `TBD Purpose` headers at archive time** — done as part of the archive workflow, not as part of implementation, to keep the change's diff focused on the user HAL and sim skeleton.

## Risks / Trade-offs

- [Pre-implementation verification of `HardwarePullerEmu::submit` API surface may surface mismatches with PDL expectations] → Mitigation: a blocking prerequisite task confirms the API surface before implementation begins; if mismatches exist, the change is split or scoped down (per proposal: minimal skeleton is acceptable).
- [Creating new sim-layer files introduces two new translation units with low test coverage] → Mitigation: the skeletons expose only the methods used by the four HAL fn-pointers; coverage is added via the two new unit tests that exercise the user HAL end-to-end.
- [Adding `green_context_handles` map to `struct hal_user_context` changes its internal layout] → Mitigation: `hal_user_context` is internal to the user HAL implementation (not part of any HAL contract or public ABI), so the change is internal-only.
- [Coordination with sibling `implement-hal-preempt-resume-semaphore` change for `SemaphoreManager` semantics] → Mitigation: both changes share the same `SemaphoreManager` contract; if one change exposes a contract gap, both are updated in lockstep. `tasks.md` cross-references the sibling change.