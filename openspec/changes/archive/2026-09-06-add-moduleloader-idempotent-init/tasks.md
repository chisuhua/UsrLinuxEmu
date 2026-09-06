## 1. Preflight

- [x] 1.1 Read current `src/kernel/module_loader.cpp` `load_plugin` (lines 177-214) and `load_plugins` (the scan loop) to confirm the current behavior matches the Oracle Q3 finding
- [x] 1.2 Read `src/kernel/module_loader.cpp` `unload_plugin` / `unload_plugins` to understand how `ref_count` is currently decremented (verify the Oracle finding that `unload` only dlcloses once → glibc refcount stuck at 1)
- [x] 1.3 Check existing tests via `grep -l 'ModuleLoader' tests/ src/` to identify any test that would break under the new idempotency

## 2. Implement: idempotency check in `load_plugin`

- [x] 2.1 Insert after the `dlsym` block at `src/kernel/module_loader.cpp:~193` (after `mod->name` is available), before `resolve_dependencies`:
  ```cpp
  auto it = loaded_plugins_.find(mod->name);
  if (it != loaded_plugins_.end()) {
    it->second->ref_count++;
    dlclose(handle);
    return 0;
  }
  ```
- [x] 2.2 Verify the existing `unload_plugins` decrement path at `~loaded_plugins_.erase(it)` correctly handles `ref_count` going to 0 (the fix should also reset the orphan `ref_count = 0` overwrite at line ~211 — i.e., **do not** reassign `loaded_plugins_[mod->name] = info` on the already-loaded path; the new code branches before reaching that line, so this should be automatic)
- [x] 2.3 Confirm `load_plugins` scan loop (`scan_candidates` → `topo_sort` → `load_plugin`) still works for first-time loads (the new branch must NOT fire on first dlopen of a plugin path)

## 3. Verify

- [x] 3.1 Build `cmake --build build -j4` — no errors
- [x] 3.2 Run `ctest` — 157/157 still PASS (the gpu_driver idempotency test at `tests/test_gpu_plugin_init_idempotent_standalone.cpp` should now pass at the ModuleLoader level too, but the test asserts on gpu_driver plugin behavior which is unchanged)
- [x] 3.3 Run `./build/bin/test_gpu_plugin_init_idempotent_standalone` — still PASS (proves end-to-end idempotency preserved)
- [x] 3.4 Optional sanity check: from a fresh process, call `load_plugins("plugins")` twice and verify `dlclose` on unload now actually unmaps the libraries (use `cat /proc/self/maps | grep plugin_`)

## 4. Commit + cleanup

- [x] 4.1 `git add src/kernel/module_loader.cpp`
- [x] 4.2 Commit message: `fix(moduleloader): skip init() when plugin already loaded (preserves refcount)`
- [x] 4.3 No spec.md / design.md needed (framework bug fix, no API change to consumers)

## Out of scope (handled elsewhere)

- gpu_driver plugin's own `g_plugin_initialized` guard remains (defense in depth + cleaner log message). Once this fix lands, the plugin guard becomes belt-and-braces but should stay — removing it would re-introduce the F4 leak if a future refactor accidentally changes the plugin's init contract.