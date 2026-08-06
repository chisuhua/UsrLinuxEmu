# Tasks: HAL BAR2 VRAM mmap Completion

## 1. Implementation

- [ ] 1.1 Replace the stub `user_mem_map_bo` body in `plugins/gpu_driver/hal/hal_user.cpp` (lines 245-249) with a real implementation:
  - If `g_vram_store.initialized == false` → return `-ENODEV`, leave `*user_map` untouched (or set to null).
  - If `g_vram_store.pool_backing == nullptr` → return `-ENODEV` (defensive null check).
  - If `bo_offset > g_vram_store.pool_size` → return `-EINVAL` (overflow-safe ordering).
  - If `bo_offset + size > g_vram_store.pool_size` → return `-EINVAL`.
  - Otherwise: `*user_map = g_vram_store.pool_backing + bo_offset;` and return `0`.
- [ ] 1.2 Verify the implementation reads `g_vram_store` fields atomically or under a lock mirroring `fence_lock` discipline. If a new lock is needed, add it to `g_vram_store`'s declaring translation unit (out of `hal_user.cpp`) and document the field addition.

## 2. Tests

- [ ] 2.1 Add Catch2 unit test for BAR2 mmap success path: with a stub-initialized `g_vram_store` (e.g. a small backing allocation + `initialized = true`), call `hal->mem_map_bo(dev, 0, 16, &map)` and assert (a) return value `0`, (b) `map != nullptr`, (c) the returned pointer is `pool_backing + 0`.
- [ ] 2.2 Add Catch2 unit test for uninitialized state: with `g_vram_store.initialized = false`, call `hal->mem_map_bo(...)` and assert return value `-ENODEV` and `map` was not modified.
- [ ] 2.3 Add Catch2 unit test for out-of-bounds: with a stub `pool_size = 4096`, call `hal->mem_map_bo(dev, 4096, 16, &map)` (offset equal to size — past the end) and assert return value `-EINVAL` and `map` was not modified.
- [ ] 2.4 Register any new test binaries in the appropriate `tests/CMakeLists.txt` so `ctest` picks them up.

## 3. Verification

- [ ] 3.1 Run `make -j4` from `build/` and confirm zero compile warnings on changed files.
- [ ] 3.2 Run `ctest --output-on-failure` from `build/` and confirm baseline 130/130 tests PASS plus the new BAR2 mmap tests PASS.
- [ ] 3.3 Run `lsp_diagnostics` on `hal_user.cpp` and any new test file — confirm no errors and no warnings.
- [ ] 3.4 Run `openspec validate complete-hal-mem-map-bo` and confirm the change passes schema validation.