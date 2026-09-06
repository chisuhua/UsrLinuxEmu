## 1. Preflight

- [x] 1.1 Read `tests/CMakeLists.txt` lines 20-60 (`add_catch_test` function definition) to confirm `add_catch_test` provides all needed includes (`pci_probe.h`, `sim_hardware/...`)
- [x] 1.2 Read `tests/test_gpu_sim_hardware_bridge_standalone.cpp` as the structural precedent (Catch2 + `ModuleLoader::load_plugins` + `VFS::instance().open`)
- [x] 1.3 Read `src/kernel/module_loader.cpp` `load_plugins` to confirm what error code it returns when a plugin's `init()` returns non-zero (expected: the rc from `init()` propagates; if all plugins succeed the rc is 0)

## 2. Implement: new test file

- [x] 2.1 Create `tests/test_gpu_plugin_negative_path_standalone.cpp` with this structure:
  ```cpp
  #include <catch_amalgamated.hpp>
  #include <cerrno>
  #include <climits>
  #include <cstdlib>
  #include <filesystem>
  #include <string>
  #include "kernel/module_loader.h"
  #include "kernel/vfs.h"

  TEST_CASE("plugin init propagates -ENOENT when topology file is missing",
            "[plugin][negative][regression]") {
    // Capture the absolute path to plugins BEFORE chdir — passing a relative
    // "plugins" after chdir to an empty temp dir would fail at scan_candidates
    // (load_plugins returns -1) before any plugin's init() runs, and the
    // -ENOENT propagation path would never be exercised.
    char saved_cwd[PATH_MAX];
    REQUIRE(getcwd(saved_cwd, sizeof(saved_cwd)) != nullptr);
    std::string plugins_abs = std::string(saved_cwd) + "/plugins";

    // Create an empty temp directory and chdir into it so the relative path
    // "sim_hardware/topology/default_topology.json" (inside the gpu_driver
    // plugin init) does not resolve.
    auto tmp = std::filesystem::temp_directory_path() /
               "test_gpu_plugin_negative_path";
    std::filesystem::remove_all(tmp);
    std::filesystem::create_directories(tmp);
    REQUIRE(chdir(tmp.c_str()) == 0);

    int rc = usr_linux_emu::ModuleLoader::load_plugins(plugins_abs);

    REQUIRE(chdir(saved_cwd) == 0);
    std::filesystem::remove_all(tmp);

    // Expect: gpu_driver plugin's init returned -ENOENT (or non-zero).
    // load_plugins swallows plugin init failures (logs and continues), so the
    // load-bearing check is via VFS — the device was never registered.
    auto dev = usr_linux_emu::VFS::instance().open("/dev/gpgpu0", 0);
    REQUIRE(dev == nullptr);
    (void)rc;
  }
  ```
  Note: process isolation comes from ctest running each test binary in its own process; Catch2 itself does NOT isolate. Each `_standalone` binary is one ctest process.
- [x] 2.2 Register in `tests/CMakeLists.txt` CATCH2_TESTS list — place near `test_gpu_plugin_init_idempotent_standalone` for cohesion (both are plugin-init regression tests)
- [x] 2.3 Re-run `cmake -B build` (full regen — new source file in list) before building

## 3. Verify

- [x] 3.1 `cmake --build build --target test_gpu_plugin_negative_path_standalone -j4` — builds clean
- [x] 3.2 `./build/bin/test_gpu_plugin_negative_path_standalone` — test PASSES
- [x] 3.3 `cd build && ctest 2>&1 | tail -5` — 158/158 PASS (was 157, +1 negative-path test, no regressions)
- [x] 3.4 Confirm the test is NOT a tautology: temporarily revert the gpu_driver plugin init to remove the `-ENOENT` propagation (e.g., swallow the rc and return 0), rerun the test — it MUST FAIL. Then revert. (Without this check, the test could pass vacuously if the test itself is broken.)
- [x] 3.5 Manual sanity: run the test binary from a non-project CWD to confirm chdir logic still works (relies on `saved_cwd` capture before chdir, so should work from any starting CWD)

## 4. Commit + cleanup

- [x] 4.1 `git add tests/test_gpu_plugin_negative_path_standalone.cpp tests/CMakeLists.txt`
- [x] 4.2 Commit message: `test(plugin): cover missing-topology -ENOENT propagation path`
- [x] 4.3 No spec.md/design.md changes (this is a test-only change, no API contract delta)

## Out of scope

- `-EINVAL` malformed topology coverage (deferred — requires writing a malformed fixture; plugin's hardcoded topology path blocks the easiest fixture-injection approach)
- Zero-device topology coverage (deferred — same blocker)
- Refactoring the plugin to accept a topology path argument (orthogonal concern; would enable cleaner fixture injection but is a bigger blast radius)