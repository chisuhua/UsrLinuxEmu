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

- `plugins/gpu_driver/sim/vram_store.h` + `vram_store.cpp`: add `bool initialized_ = false;` member; `init()` returns `true` early if `initialized_` is already true.
- `plugins/gpu_driver/sim/dma_coherent_pool.h` + `dma_coherent_pool.cpp`: same pattern.
- Optional symmetry: `GpuVramStore::destroy()` (if exists) sets `initialized_ = false` so a teardown-then-reinit cycle works.

## Non-goals (deferred)

- Full lifecycle audit of every `sim/` singleton's `init/destroy` pair (out of scope for this targeted defensive fix; surface later if more leaks found).
- The gpu_driver plugin's `g_plugin_initialized` guard remains (per Oracle Q3 "do NOT remove even if ModuleLoader later gains an idempotency check").

## References

- Oracle audit session `ses_f8b8c32e5ffeGgFiPdo4hE0jiu` (Q2 idempotency table)
- `plugins/gpu_driver/sim/vram_store.cpp` (the `init` to be guarded)
- `plugins/gpu_driver/sim/dma_coherent_pool.cpp:19-28` (the `init` to be guarded)
- Change-3 gpu_driver guard at `plugins/gpu_driver/plugin.cpp` (commit `31dd5e1`) — outer layer complement