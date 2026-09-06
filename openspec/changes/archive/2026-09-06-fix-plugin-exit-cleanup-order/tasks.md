## 1. Preflight

- [x] 1.1 Reproduce the SIGSEGV with a minimal failing test
- [x] 1.2 Capture gdb stack trace; confirm root cause is `Device::~Device()` virtual dispatch through unmapped vtable after `dlclose`
- [x] 1.3 Oracle consultation: chose Option A (test-side fix + central invariant comment) over architectural refactor
- [x] 1.4 Read `plugins/gpu_driver/plugin.cpp` `plugin_fini_internal` (lines 191-209) to confirm cleanup order

## 2. Implement

- [x] 2.1 Create `tests/test_plugin_exit_cleanup_standalone.cpp` with 4 TEST_CASEs (1 cycle, load+open+unload, 20 cycles, F4-pattern)
- [x] 2.2 Each cycle that opens a device: `dev.reset()` before `unload_plugins()`
- [x] 2.3 Register in `tests/CMakeLists.txt` `CATCH2_SIM_TESTS` list
- [x] 2.4 Add 2-line invariant comment to `unload_plugins()` declaration in `include/kernel/module_loader.h`
- [x] 2.5 Verify non-tautology: temporarily comment out `dev.reset()`, rebuild, run — must SIGSEGV; restore — must pass

## 3. Verify

- [x] 3.1 `cmake --build build --target kernel test_plugin_exit_cleanup_standalone` — clean
- [x] 3.2 `./build/bin/test_plugin_exit_cleanup_standalone` (from project root) — 26 assertions / 4 cases PASS
- [x] 3.3 `./build/bin/test_gpu_plugin_init_idempotent_standalone` (Task #1 + F4) — 14 assertions / 4 cases PASS
- [x] 3.4 `cd build && ctest --output-on-failure` — 160/160 PASS (was 159, +1 new test binary)

## 4. OpenSpec artifacts + archive + commit

- [x] 4.1 Mark all tasks `[x]`
- [x] 4.2 `openspec validate fix-plugin-exit-cleanup-order --strict` — expect valid
- [x] 4.3 `openspec archive fix-plugin-exit-cleanup-order --yes` — moves to `archive/2026-09-07-...` and syncs spec
- [x] 4.4 `git add` test + module_loader.h + CMakeLists.txt + openspec/
- [x] 4.5 Commit with detailed message referencing Oracle session + gdb stack trace
- [x] 4.6 `git push origin main`

## Out of scope

- Architectural fix (VFS open→increase_ref / Device dtor→decrease_ref, defer dlclose while refs > 0)
- pImpl pattern for Device (cannot work: `dlclose` unmaps code segments too)
- Don't-dlclose-on-unload (would re-introduce glibc refcount bug fixed in `d2c6d4b`)
- PluginManager legacy cleanup path (`plugins.json` deprecated)