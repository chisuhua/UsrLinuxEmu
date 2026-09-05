## 1. Preflight & Baseline

- [ ] 1.1 Verify pre-change state: `cd build && ctest 2>&1 | tail -3` shows 155/155 PASS
- [ ] 1.2 Verify Phase 0 gate: `./tools/check_phase0_gate.sh` exits 0 (5/5 PASS)
- [ ] 1.3 Confirm `plugins/gpu_driver/drv/` is git-clean: `git status --short plugins/gpu_driver/drv/` is empty
- [ ] 1.4 Re-read `plugins/gpu_driver/plugin.cpp` lines 1-150 to confirm composition root anatomy before refactor
- [ ] 1.5 Re-read `plugins/pci_driver/include/pci_probe.h` `pci_probe_enumerate_from_sim_hardware` signature + `DiscoveredDevice` struct

## 2. TDD: Bridge regression test (write failing test first)

- [ ] 2.1 Create `tests/test_gpu_sim_hardware_bridge_standalone.cpp` with stubbed `TEST_CASE("gpu_driver bridge: enumerates sim_hardware topology and registers /dev/gpgpu0")`
- [ ] 2.2 Add test to `tests/CMakeLists.txt` CATCH2_TESTS list with `STANDALONE` suffix
- [ ] 2.3 `cmake --build build --target test_gpu_sim_hardware_bridge_standalone -j4` and confirm build succeeds
- [ ] 2.4 Run `./build/bin/test_gpu_sim_hardware_bridge_standalone` and **verify it FAILS** (composition root not yet refactored to call `pci_probe_enumerate_from_sim_hardware`)
- [ ] 2.5 Capture the failure output as evidence

## 3. Implement: Per-device HalHolder composition root

- [ ] 3.0 **Link/include setup (build-blocker)**: edit `plugins/gpu_driver/CMakeLists.txt` to add pci_driver include path and link `pci_driver_plugin` SHARED library:
  - `target_include_directories(gpu_driver_plugin PRIVATE ${CMAKE_SOURCE_DIR}/plugins/pci_driver/include ${CMAKE_SOURCE_DIR}/sim_hardware/include)`
  - `target_link_libraries(gpu_driver_plugin PRIVATE ... pci_driver_plugin ${CMAKE_DL_LIBS})` (DT_NEEDED auto-resolves symbols at runtime; pci_driver_plugin is SHARED per AGENTS.md §"kernel 库必须是 SHARED" rationale)
  - Note: `plugins/pci_driver/` source files are NOT modified — only the gpu_driver plugin gains an include path and link dependency. This is the intended consumer direction (gpu→pci probe contract, see Change-2 design §13.3).
- [ ] 3.1 In `plugins/gpu_driver/plugin.cpp`, replace `static HalHolder hal_holder;` (line 46) with `static std::vector<std::unique_ptr<HalHolder>> hal_holders;` (file-static vector). Delete `g_hal` global (line 28, 48, 129-138) — only referenced in fini itself, replaced by `hal_holders`.
- [ ] 3.2 Move process-global singletons init to BEFORE the enumeration loop, preserving EXACT load-bearing order from current `plugin_init_internal`:
  ```
  kfd_module_init()           // plugin.cpp:88
  g_vram_store.init(256)      // plugin.cpp:95
  g_dma_pool.init()           // plugin.cpp:99
  us_mm_shim_init(&g_plugin_mm_shim, 0)  // plugin.cpp:107 (PID 0 = kernel-internal host)
  kfd_sim_set_mm_shim(&g_plugin_mm_shim) // plugin.cpp:110 (must precede kfd_sim_reset)
  kfd_sim_reset()             // plugin.cpp:113 (depends on mm_shim binding)
  ```
  This block runs exactly once regardless of N. Per-device init follows in 3.5.
- [ ] 3.3 Add enumerate call (corrected signature per `plugins/pci_driver/include/pci_probe.h:38-42`):
  ```cpp
  #include <pci_probe.h>  // add to plugin.cpp includes
  using usr_linux_emu::pci::pci_probe_enumerate_from_sim_hardware;
  // ...
  constexpr size_t kMaxDiscoveredDevices = 16;
  DiscoveredDevice discovered[kMaxDiscoveredDevices];
  size_t out_count = 0;
  int rc = pci_probe_enumerate_from_sim_hardware(
      discovered, kMaxDiscoveredDevices, &out_count,
      "sim_hardware/topology/default_topology.json");
  // PROPAGATE rc on error (per Change 9 decision): if (rc != 0) return rc;
  ```
  API contract: `out_count` returns TOTAL available devices (may exceed `kMaxDiscoveredDevices` if topology grows). The loop bound in 3.5 clamps to 1 for N>1 (fail-fast).
- [ ] 3.4 Add N>1 WARN (fail-fast-but-survive semantics — NO `assert`):
  ```cpp
  if (out_count > 1) {
    std::cerr << "[GpuPlugin] sim_hardware topology has N=" << out_count
              << " devices; multi-device shared singletons not yet supported. "
              << "Bridging device[0] only.\n";
  }
  ```
  The WARN is the only N>1 signal. Debug builds do NOT abort (multi-device is contract-defined to defer, not to fail). This keeps the `assert` out of production paths and avoids the spec scenario contradiction where Debug build aborts before "return 0".
- [ ] 3.5 Replace single-device instantiation block with loop mirroring the COMPLETE existing per-device sequence from `plugin.cpp:62-85`:
  ```cpp
  size_t devices_to_bridge = (out_count > 1) ? 1 : out_count;
  for (size_t i = 0; i < devices_to_bridge; ++i) {
    auto& h = hal_holders.emplace_back(std::make_unique<HalHolder>());
    hal_user_init(&h->hal, &h->ctx);                              // plugin.cpp:47
    hal_puller_create(&h->hal, &h->doorbell, &h->scheduler,        // plugin.cpp:52
                      &h->puller_handle);
    h->scheduler.registerKernel(0, "simple_kernel");                // plugin.cpp:62
    h->scheduler.registerKernel(1, "matmul_kernel");               // plugin.cpp:63
    // setLaunchCallback: 8-arg lambda mirroring plugin.cpp:66-74 EXACTLY (preserve logging semantics)
    h->scheduler.setLaunchCallback(
        [](const char* kernel_name, uint32_t grid_x, uint32_t grid_y, uint32_t grid_z,
           uint32_t block_x, uint32_t block_y, uint32_t block_z, uint32_t shared_mem) {
          std::cout << "[GpuPlugin] LaunchCallback: kernel=" << kernel_name
                    << " grid=(" << grid_x << "," << grid_y << "," << grid_z << ")"
                    << " block=(" << block_x << "," << block_y << "," << block_z << ")"
                    << std::endl;
          (void)shared_mem;
        });
    // hal_user_set_doorbell_cb mirrors plugin.cpp:76-85: ctx-based lambda doing doorbell.write
    int db_ret = hal_user_set_doorbell_cb(&h->ctx,
        [](void* cb_ctx, uint32_t queue_id) {
          static_cast<HalHolder*>(cb_ctx)->doorbell.write(queue_id);
        },
        h.get());
    if (db_ret != 0) {
      std::cerr << "[GpuPlugin] Failed to set doorbell callback: " << db_ret << "\n";
      return db_ret;
    }
    auto dev = std::make_shared<GpgpuDevice>(&h->hal);
    dev->setPuller(h->puller_handle);
    dev->set_mm_shim(&g_plugin_mm_shim);
    dev->setHalContext(&h->ctx);
    std::string dev_name = "gpgpu" + std::to_string(i);
    auto vfs_dev = std::make_shared<Device>(dev_name, 0, dev, nullptr);
    if (int reg_rc = VFS::instance().register_device(vfs_dev); reg_rc != 0) {
      std::cerr << "[GpuPlugin] register_device(" << dev_name << ") failed: " << reg_rc << "\n";
      return reg_rc;  // fail plugin init loudly if duplicate name or VFS error
    }
  }
  ```
  **Critical**: Do NOT use `device->name` — `GpgpuDevice::name` is hardcoded `"gpgpu0"` (`gpgpu_device.cpp:47`), would produce duplicate `/dev/gpgpu0` for all i>0 and `VFS::register_device` rejects duplicates (`vfs.cpp:33-37`).
- [ ] 3.6 Replace `plugin_fini_internal` (currently lines 127-140) with per-device teardown. The ordering invariant is load-bearing: `kfd_module_exit` MUST precede per-holder `hal_user_destroy` (per `plugin.cpp:134` comment "KFD subsystem exit (must precede HAL destroy)"); `VFS::unregister_device` must precede both (vtable still valid). Final order:
  ```cpp
  static void plugin_fini_internal() {
    std::cout << "[GpuPlugin] Shutting down...\n";
    // Phase 1: VFS unregister per device (vtable still valid before HAL destroy).
    //          Mirrors the original plugin.cpp:139 line but moved BEFORE HAL teardown
    //          (original order was unregister-after-destroy — pre-existing bug).
    for (size_t i = 0; i < hal_holders.size(); ++i) {
      std::string dev_name = "gpgpu" + std::to_string(i);
      VFS::instance().unregister_device(dev_name);
    }
    // Phase 2: KFD exit (must precede per-holder HAL destroy per plugin.cpp:134 invariant)
    kfd_module_exit();
    // Phase 3: per-holder HAL teardown
    for (auto& h : hal_holders) {
      if (h->puller_handle != 0) {
        hal_puller_destroy(&h->hal, h->puller_handle);
        h->puller_handle = 0;
      }
      hal_user_destroy(&h->ctx);
    }
    hal_holders.clear();
    // g_hal removed (replaced by hal_holders vector — was only referenced in fini itself)
  }
  ```
  This fixes the pre-existing oddity where the original fini (line 139) unregistered VFS AFTER kfd/HAL destroy — corrected to unregister-first order AND preserves KFD-before-HAL invariant.
- [ ] 3.7 Build: `cmake --build build -j4 2>&1 | tail -10` and confirm no errors (LSP errors in `pci_probe.h`/`pcie/host_bridge.h` are pre-existing config artifacts — CMake build success is the authoritative signal)

## 4. Verify tests pass

- [ ] 4.1 Run `./build/bin/test_gpu_sim_hardware_bridge_standalone` and **verify it PASSES**
- [ ] 4.2 Run `./build/bin/test_gpu_ioctl_standalone` and verify N=1 behavior is preserved (PUSHBUFFER → doorbell → fence chain catches dropped per-holder init steps)
- [ ] 4.3 Run `cd build && ctest 2>&1 | tail -5` and verify 156/156 PASS (was 155; +1 new bridge test) — first verify actual baseline count via task 1.1, then use that exact number
- [ ] 4.4 Run `./tools/check_phase0_gate.sh` and verify still 6/6 PASS (after P0-G6 added in task 3.0.1 below)

### P0-G6 follow-up

- [ ] 3.0.1 Add `P0-G6: default_topology.json devices == 1` check to `tools/check_phase0_gate.sh`. Append after P0-G5 block:
  ```bash
  # P0-G6: default_topology.json must contain exactly 1 GPU device (N=1 fail-fast contract)
  p0g6_count=$(python3 -c 'import json; print(len(json.load(open("sim_hardware/topology/default_topology.json"))["devices"]))' 2>/dev/null)
  if [ "$p0g6_count" = "1" ]; then
    report PASS "P0-G6" "default_topology.json devices == 1 (N=1 fail-fast contract)"
  else
    report FAIL "P0-G6" "default_topology.json has $p0g6_count devices (expected 1; N>1 triggers WARN bridge-only-device[0])"
  fi
  ```
  This materializes the P0-G6 promise from `design.md` "Risks / Trade-offs" table. Future escalation change (BDF-keyed sim singletons) may retire this gate.

## 5. Self-review against Oracle's findings

- [ ] 5.1 Verify HAL ABI via header diff + fn-ptr count (replaces vacuous nm check):
  - `git diff --stat HEAD -- plugins/gpu_driver/hal/gpu_hal.h` returns empty
  - `grep -c '(\*.*)(.*)' plugins/gpu_driver/hal/gpu_hal.h` returns 68 (fn-ptr member count)
  - Optional compile-time: `static_assert(sizeof(struct gpu_hal_ops) == <pre-change-size>)` in test file
- [ ] 5.2 `git diff --stat HEAD -- plugins/gpu_driver/drv/` returns empty (drv/ untouched — kfd_sim_bridge_set_hal deferred per scope decision in `design.md`)
- [ ] 5.3 `git diff --stat HEAD -- plugins/gpu_driver/hal/` returns empty
- [ ] 5.4 `git diff --stat HEAD -- plugins/gpu_driver/sim/` returns empty (kfd_sim_bridge fate removed from scope)
- [ ] 5.5 `git grep -c pci_probe_enumerate_from_sim_hardware plugins/gpu_driver/` returns ≥1 (consumer wired)
- [ ] 5.6 `git diff --stat HEAD -- plugins/gpu_driver/CMakeLists.txt` shows include + link additions (per task 3.0)

## 6. Commit + docs sync

- [ ] 6.1 `git add plugins/gpu_driver/plugin.cpp plugins/gpu_driver/CMakeLists.txt tests/test_gpu_sim_hardware_bridge_standalone.cpp tests/CMakeLists.txt tools/check_phase0_gate.sh`
- [ ] 6.2 Commit with conventional message: `feat(gpu-driver): bridge composition root to sim_hardware via per-device HalHolder loop`
- [ ] 6.3 Run docs-audit: `bash tools/docs-audit.sh --strict 2>&1 | tail -7` and verify Passed:68, Failed:0, Warnings:1 (pre-existing Doxygen warning)
- [ ] 6.4 Run pre-commit hook (if installed): `git commit --no-verify` only if necessary; prefer letting pre-commit run phase0-gate (now 6/6 with P0-G6)
- [ ] 6.5 `openspec status --change add-gpu-driver-sim-hardware-bridge` returns `apply-ready: true`

## 7. Implementation review (optional but recommended)

- [ ] 7.1 Self-trigger Oracle post-impl review on this change to catch regressions (Oracle's REJECT pattern in Change-2 caught a real BDF writer bug — same discipline applies here)
- [ ] 7.2 If Oracle finds issues, fix per F1/F2/etc. before proceeding to ship

## 8. Ship & escalate trigger setup

- [ ] 8.1 `openspec validate add-gpu-driver-sim-hardware-bridge --strict` passes
- [ ] 8.2 Archive change: `openspec archive add-gpu-driver-sim-hardware-bridge --yes --skip-specs` (per Change-1/Change-2 precedent)
- [ ] 8.3 Verify `openspec list` shows zero active changes after archive
- [ ] 8.4 Document the escalation trigger (`≥2 devices in topology → start BDF-keyed sim singletons change`) in `docs/02_architecture/post-refactor-architecture.md` §1.5 (or via a follow-up doc change)
- [ ] 8.5 When 2nd device is ever added to `sim_hardware/topology/default_topology.json`: P0-G6 will FAIL, blocking the commit; that's the escalation trigger. Follow-up change BDF-keys `g_vram_store`/`g_dma_pool`/`g_plugin_mm_shim`/KFD subsystem, then removes P0-G6 from `check_phase0_gate.sh`.