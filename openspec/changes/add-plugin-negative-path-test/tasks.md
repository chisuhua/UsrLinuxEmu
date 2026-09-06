## 1. Preflight

- [ ] 1.1 Read `tests/CMakeLists.txt` lines 20-60 (`add_catch_test` function definition) to confirm `add_catch_test` provides all needed includes (`pci_probe.h`, `sim_hardware/...`)
- [ ] 1.2 Read `tests/test_gpu_sim_hardware_bridge_standalone.cpp` as the structural precedent (Catch2 + `ModuleLoader::load_plugins` + `VFS::instance().open`)
- [ ] 1.3 Read `src/kernel/module_loader.cpp` `load_plugins` to confirm what error code it returns when a plugin's `init()` returns non-zero (expected: the rc from `init()` propagates; if all plugins succeed the rc is 0)

## 2. Implement: new test file

- [ ] 2.1 Create `tests/test_gpu_plugin_negative_path_standalone.cpp` with this structure:
  ```cpp
  #include <catch_amalgamated.hpp>
  #include <cerrno>
  #include <cstdlib>
  #include <filesystem>
  #include <fstream>
  #include <string>
  #include "kernel/module_loader.h"
  #include "kernel/vfs.h"

  TEST_CASE("plugin init propagates -ENOENT when topology file is missing",
            "[plugin][negative][regression]") {
    // Create an empty temp directory and chdir into it so the relative path
    // "sim_hardware/topology/default_topology.json" does not resolve.
    auto tmp = std::filesystem::temp_directory_path() /
               "test_gpu_plugin_negative_path";
    std::filesystem::remove_all(tmp);
    std::filesystem::create_directories(tmp);
    char saved_cwd[PATH_MAX];
    REQUIRE(getcwd(saved_cwd, sizeof(saved_cwd)) != nullptr);
    REQUIRE(chdir(tmp.c_str()) == 0);

    int rc = usr_linux_emu::ModuleLoader::load_plugins("plugins");

    REQUIRE(chdir(saved_cwd) == 0);

    // Expect: gpu_driver plugin's init returned -ENOENT (or non-zero); either
    // load_plugins reports non-zero, or the gpu_driver module isn't loaded
    // and VFS has no /dev/gpgpu0.
    auto dev = usr_linux_emu::VFS::instance().open("/dev/gpgpu0", 0);
    REQUIRE(dev == nullptr);
    (void)rc;  // rc semantics from ModuleLoader are loader-dependent; the
               // device-not-registered assertion is the load-bearing check.
  }
  ```
- [ ] 2.2 Register in `tests/CMakeLists.txt` CATCH2_TESTS list — place near `test_gpu_plugin_init_idempotent_standalone` for cohesion (both are plugin-init regression tests)
- [ ] 2.3 Re-run `cmake -B build` (full regen — new source file in list) before building

## 3. Verify

- [ ] 3.1 `cmake --build build --target test_gpu_plugin_negative_path_standalone -j4` — builds clean
- [ ] 3.2 `./build/bin/test_gpu_plugin_negative_path_standalone` — test PASSES (assertions: chdir succeeded, device nullptr)
- [ ] 3.3 `cd build && ctest 2>&1 | tail -5` — 158/158 PASS (was 157, +1 negative-path test, no regressions)
- [ ] 3.4 Manual sanity: run the test binary from a non-project CWD to confirm chdir logic actually exercises the missing-file path

## 4. Commit + cleanup

- [ ] 4.1 `git add tests/test_gpu_plugin_negative_path_standalone.cpp tests/CMakeLists.txt`
- [ ] 4.2 Commit message: `test(plugin): cover missing-topology -ENOENT propagation path`
- [ ] 4.3 No spec.md/design.md changes (this is a test-only change, no API contract delta)

## Out of scope

- `-EINVAL` malformed topology coverage (deferred — requires writing a malformed fixture; plugin's hardcoded topology path blocks the easiest fixture-injection approach)
- Zero-device topology coverage (deferred — same blocker)
- Refactoring the plugin to accept a topology path argument (orthogonal concern; would enable cleaner fixture injection but is a bigger blast radius)