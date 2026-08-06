# Design: HAL BAR2 VRAM mmap Completion

## Context

ADR-064 §Decision 2 + ADR-069 §Decision 4 specify the BAR2 VRAM mmap path via the HAL fn-pointer `mem_map_bo(dev, bo_offset, size, *user_map)` on `struct gpu_hal_ops`. The user HAL implementation (`plugins/gpu_driver/hal/hal_user.cpp:245-249`) is currently a stub returning `-ENOSYS`. The mock HAL implements it. A global `usr_linux_emu::g_vram_store` singleton already exists in the codebase with fields `pool_backing` (the allocation base), `pool_size`, and `initialized`. This change upgrades the user HAL to delegate to `g_vram_store` so drv/ can BAR2-map VRAM regions without bypassing the HAL contract.

## Goals / Non-Goals

**Goals:**
- Replace the stub `user_mem_map_bo` with a real implementation that delegates to `g_vram_store`.
- Add mandatory bounds checking and an uninitialized-state early return.
- Mirror the mock implementation's behavior so drv/ sees identical semantics regardless of which HAL backend is loaded.
- Add a Catch2 unit test covering the success / uninitialized / out-of-bounds paths.

**Non-Goals:**
- Implementing `g_vram_store` itself — assumed complete; the change relies on the existing global.
- DRM ioctl mmap path — separate concern, separate task.
- Multi-process BAR isolation — explicitly deferred per ADR-011.
- Introducing any new allocation primitive (`mmap`, `malloc`) — the change only returns pointers into the existing `pool_backing`.

## Decisions

- **Delegate to the global `usr_linux_emu::g_vram_store` singleton.** `g_vram_store` is the single source of truth for VRAM allocation state; the HAL fn-pointer is a thin adapter that translates drv/ calls into the singleton's invariants. This mirrors the mock implementation's pattern.
- **Mandatory bounds check.** `bo_offset + size <= pool_size` is enforced before computing the return pointer. This is a defense-in-depth check that catches drv/ misuse before it can read past the end of `pool_backing`. Out-of-bounds returns `-EINVAL`.
- **`-ENODEV` for uninitialized state.** When `g_vram_store.initialized == false` the function returns `-ENODEV`, matching the mock's behavior. This prevents drv/ from dereferencing `pool_backing` when VRAM setup is incomplete.
- **Null `pool_backing` defense.** If `initialized == true` but `pool_backing == nullptr` (defensive against future torn-init bugs), the function returns `-ENODEV` rather than returning a null user pointer.
- **No new allocation mechanisms.** The implementation reads `g_vram_store.pool_backing` and `g_vram_store.pool_size` only. No `mmap`, `malloc`, or any other allocation primitive is introduced — this preserves the proposal's "no new allocations" constraint.
- **Thread-safe reads of `g_vram_store`.** Reads of `initialized`, `pool_backing`, and `pool_size` SHALL be performed atomically or under a lock mirroring the existing `fence_lock` discipline, to prevent torn reads against concurrent init/teardown.

## Risks / Trade-offs

- [Bounds check on `bo_offset + size` could overflow if `bo_offset` is near `SIZE_MAX`] → Mitigation: the implementation checks `bo_offset > pool_size` first, then `bo_offset + size > pool_size` only if the first check passed; this avoids the overflow path. The check sequence is documented in tasks.md.
- [Thread-safe reads of `g_vram_store` may require a lock that doesn't yet exist on this struct] → Mitigation: a dedicated `g_vram_store.mutex` (or reuse of `fence_lock`) is added; the change is minimal (one field + lock acquisition).
- [Test coverage of BAR2 mmap is new — risk of test missing edge cases] → Mitigation: at minimum the test asserts (a) in-bounds returns valid pointer, (b) uninitialized returns `-ENODEV`, (c) out-of-bounds returns `-EINVAL` without writing `user_map`.