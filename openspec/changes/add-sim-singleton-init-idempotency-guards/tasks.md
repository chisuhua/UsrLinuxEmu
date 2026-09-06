## 1. Preflight

- [ ] 1.1 Read `plugins/gpu_driver/sim/vram_store.h` and `.cpp` to confirm `init()` shape AND verify that `bool initialized` (no underscore) already exists as a class member — Oracle deep-dive finding: the existing member is at `vram_store.h:21`, set to `true` at `vram_store.cpp:40`. **Do NOT add a new `initialized_` member** — that would create a duplicate/shadow.
- [ ] 1.2 Read `plugins/gpu_driver/sim/dma_coherent_pool.h` and `.cpp` to confirm same — existing `bool initialized` is at `dma_coherent_pool.h:16`, set to `true` at `dma_coherent_pool.cpp:26`
- [ ] 1.3 Confirm `GpuVramStore::init()` parameter is `size_t` and the implementation stores `vram_size` as a member (the different-size semantics check needs to compare against `vram_size`); locate the exact member name
- [ ] 1.4 Check `plugins/gpu_driver/sim/CMakeLists.txt` for any test targets that explicitly call `init()` twice (would need update if behavior changes — but the change makes second call a no-op for same-size, returns false for different-size)

## 2. Implement: check existing `initialized` member at top of `init()`

### VramStore

- [ ] 2.1 At the top of `GpuVramStore::init(size_t)` in `vram_store.cpp`, after argument validation but BEFORE any `mmap`, add:
  ```cpp
  if (initialized) {
    if (vram_size == size_bytes) return true;
    std::cerr << "[GpuVramStore] WARN: init(" << size_bytes
              << ") ignored — existing init was " << vram_size << "\n";
    return false;
  }
  ```
  (`size_bytes` is the parameter name in the existing `init()` signature; confirm in preflight. `vram_size` is the existing member holding the original init size.)
- [ ] 2.2 No change needed to the member declaration in `vram_store.h` (`initialized` already exists)
- [ ] 2.3 No change needed to the success-path assignment (`initialized = true` at `vram_store.cpp:40` is already correct)
- [ ] 2.4 Confirm no `destroy()` method exists (re-init after process restart unsupported — documented)

### DmaCoherentPool

- [ ] 2.5 At the top of `DmaCoherentPool::init()` in `dma_coherent_pool.cpp`, BEFORE any `mmap`, add:
  ```cpp
  if (initialized) return true;
  ```
- [ ] 2.6 No change needed to the member declaration in `dma_coherent_pool.h` (`initialized` already exists)
- [ ] 2.7 No change needed to the success-path assignment (`initialized = true` at `dma_coherent_pool.cpp:26` is already correct)
- [ ] 2.8 Same destroy/reset wiring as 2.4 (none — no destroy exists)

## 3. Verify

- [ ] 3.1 `cmake --build build -j4` — no errors (no header changes; init body edits only)
- [ ] 3.2 `ctest` — 157/157 still PASS (no test exercises double-init today; this is a defensive change)
- [ ] 3.3 Optional micro-test (if time): write a tiny Catch2 test that calls `g_vram_store.init(256); g_vram_store.init(256);` and asserts both return true; pre-fix the second call would `mmap` twice and leak, post-fix it returns true immediately. Add a second case `g_vram_store.init(512);` after the first init, asserting `false` returned.

## 4. Commit

- [ ] 4.1 `git add plugins/gpu_driver/sim/vram_store.cpp plugins/gpu_driver/sim/dma_coherent_pool.cpp`
- [ ] 4.2 Commit message: `fix(sim): make VramStore::init and DmaCoherentPool::init idempotent (guard at top + size-mismatch warning)`