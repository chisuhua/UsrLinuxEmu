## Why

Oracle post-ship audit on Change-3 (`add-gpu-driver-sim-hardware-bridge`, session `ses_f8b8c32e5ffeGgFiPdo4hE0jiu`) Q2 table identified two non-idempotent singleton `init()` methods:

| Function | Non-idempotent because | Double-call consequence |
|---|---|---|
| `GpuVramStore::init(size_t)` (`plugins/gpu_driver/sim/vram_store.cpp`) | `mmap`s `pool_backing` without checking `initialized`; no `munmap` of any prior pool | Each call leaks 256MB virtual memory + any prior pool's contents |
| `DmaCoherentPool::init()` (`plugins/gpu_driver/sim/dma_coherent_pool.cpp:19-28`) | Same pattern — `mmap` without prior `munmap` | Each call leaks the DMA pool backing memory |

Today Change-3's gpu_driver plugin guards itself before reaching these calls (commit `31dd5e1`), so the bug is masked for the gpu_driver plugin specifically. But:

1. `GpuVramStore::init` / `DmaCoherentPool::init` are public API in the `sim/` layer; any future caller (a new plugin, a test, a refactor) that calls them twice inherits the leak.
2. The gpu_driver guard is defense in depth, not load-bearing; if it ever gets removed (refactor mistake), the leak returns.

Per Oracle Q2: "guard placement (top of function, before singletons block) is REQUIRED" — adding idempotency guards at the singletons themselves makes the gpu_driver plugin's outer guard optional, and protects every future caller.

## What Changes

- `plugins/gpu_driver/sim/vram_store.cpp`: at the top of `GpuVramStore::init(size_t)`, add `if (initialized) return true;` before any `mmap`. The class **already owns** a `bool initialized` member (initialized to `false` in the constructor and set to `true` on the success path of `init()` — `vram_store.cpp:10,40`); the fix is to *check* the existing member at the top of `init()`. Do **not** add a new member with a different name (e.g., `initialized_`) — that would create a duplicate/shadow.
- `plugins/gpu_driver/sim/dma_coherent_pool.cpp`: same pattern (member already exists at `dma_coherent_pool.h:16`, set true at `:26`).
- **Different-size semantics** (Oracle deep-dive finding): if `GpuVramStore::init(new_size)` is called with a size that differs from the size used in the original `init()`, the call SHALL return `false` (NOT silently succeed with the old size). This catches a real caller bug — silently returning `true` would mask the caller's wrong assumption that the second `init()` reflected the new size. Same logic does not apply to `DmaCoherentPool::init()` (parameterless).
- No change to the `initialized` member wiring — it's already set to `true` at the end of a successful `init()` and reset to `false` in the constructor. No `destroy()` method exists on either class; re-init after process restart is unsupported (acceptable — these are process-lifetime singletons).

## Non-goals (deferred)

- Full lifecycle audit of every `sim/` singleton's `init/destroy` pair (out of scope for this targeted defensive fix; surface later if more leaks found).
- The gpu_driver plugin's `g_plugin_initialized` guard remains (per Oracle Q3 "do NOT remove even if ModuleLoader later gains an idempotency check").

## References

- Oracle audit session `ses_f8b8c32e5ffeGgFiPdo4hE0jiu` (Q2 idempotency table)
- `plugins/gpu_driver/sim/vram_store.cpp` (the `init` to be guarded)
- `plugins/gpu_driver/sim/dma_coherent_pool.cpp:19-28` (the `init` to be guarded)
- Change-3 gpu_driver guard at `plugins/gpu_driver/plugin.cpp` (commit `31dd5e1`) — outer layer complement