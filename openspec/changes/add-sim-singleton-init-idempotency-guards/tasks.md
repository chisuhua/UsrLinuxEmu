## 1. Preflight

- [ ] 1.1 Read `plugins/gpu_driver/sim/vram_store.h` and `.cpp` to confirm `init()` shape (return type, params, current member layout) — verify `initialized_` doesn't already exist by another name
- [ ] 1.2 Read `plugins/gpu_driver/sim/dma_coherent_pool.h` and `.cpp` to confirm same
- [ ] 1.3 Check if `GpuVramStore` / `DmaCoherentPool` have an existing `destroy()` or `reset()` method — if so, the new `initialized_ = false` must be wired into it
- [ ] 1.4 Check `plugins/gpu_driver/sim/CMakeLists.txt` for any test targets that explicitly call `init()` twice (would need update if behavior changes — but the change makes second call a no-op, so tests should still pass)

## 2. Implement: `initialized_` guard

### VramStore

- [ ] 2.1 Add `bool initialized_ = false;` to `GpuVramStore` private members in `vram_store.h`
- [ ] 2.2 At the top of `GpuVramStore::init()`, add `if (initialized_) return true;` after any argument validation
- [ ] 2.3 At the end of `GpuVramStore::init()` (success path), set `initialized_ = true`
- [ ] 2.4 If a `destroy()` method exists, set `initialized_ = false` in it; if no destroy, document that re-init after process restart is unsupported (acceptable — these are process-lifetime singletons)

### DmaCoherentPool

- [ ] 2.5 Add `bool initialized_ = false;` to `DmaCoherentPool` private members in `dma_coherent_pool.h`
- [ ] 2.6 At the top of `DmaCoherentPool::init()`, add `if (initialized_) return true;`
- [ ] 2.7 At the end of `DmaCoherentPool::init()` (success path), set `initialized_ = true`
- [ ] 2.8 Same destroy/reset wiring as 2.4

## 3. Verify

- [ ] 3.1 `cmake --build build -j4` — no errors (header changes affect any TU that uses the class)
- [ ] 3.2 `ctest` — 157/157 still PASS (no test exercises double-init today; this is a defensive change)
- [ ] 3.3 Optional micro-test (if time): write a tiny Catch2 test that calls `g_vram_store.init(256); g_vram_store.init(256);` and asserts both return true; pre-fix the second call would `mmap` twice and leak, post-fix it returns true immediately

## 4. Commit

- [ ] 4.1 `git add plugins/gpu_driver/sim/vram_store.h plugins/gpu_driver/sim/vram_store.cpp plugins/gpu_driver/sim/dma_coherent_pool.h plugins/gpu_driver/sim/dma_coherent_pool.cpp`
- [ ] 4.2 Commit message: `fix(sim): make VramStore::init and DmaCoherentPool::init idempotent`