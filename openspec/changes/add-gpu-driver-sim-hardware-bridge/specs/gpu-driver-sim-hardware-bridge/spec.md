## ADDED Requirements

### Requirement: gpu_driver composition root enumerates from sim_hardware topology

`plugins/gpu_driver/plugin.cpp` `plugin_init_internal` SHALL replace its static `HalHolder` singleton with an enumeration-driven composition root that calls `pci_probe_enumerate_from_sim_hardware` (defined at `plugins/pci_driver/probe.cpp:158`) and iterates over the returned `DiscoveredDevice[]` array.

For each discovered device the composition root SHALL construct a per-device `HalHolder` instance (`gpu_hal_ops` + `hal_user_context` + `DoorbellEmu` + `GlobalScheduler` + `hal_puller_handle`), instantiate a `GpgpuDevice` bound to that `HalHolder`'s `gpu_hal_ops*`, and register `/dev/gpgpu{i}` with `VFS::instance()` where `i` is the device's topology index.

#### Scenario: single-device topology registers /dev/gpgpu0

- **WHEN** `sim_hardware/topology/default_topology.json` contains exactly one device at BDF `0000:01:00.0` (vendor `0x10DE`, device `0x1234`)
- **THEN** `plugin_init_internal` returns 0
- **AND** `VFS::instance().open("/dev/gpgpu0", O_RDWR)` returns a valid file descriptor
- **AND** `ioctl(fd, GPU_IOCTL_GET_DEVICE_INFO, &info)` returns 0

#### Scenario: zero-device topology completes without registering

- **WHEN** `pci_probe_enumerate_from_sim_hardware` returns `out_count == 0`
- **THEN** `plugin_init_internal` returns 0
- **AND** no `/dev/gpgpu*` device is registered
- **AND** no WARN log is emitted (zero devices is a legal empty topology)

### Requirement: N=1 fail-fast contract for shared sim singletons

When `pci_probe_enumerate_from_sim_hardware` returns `out_count > 1`, `plugin_init_internal` SHALL log a WARN message identifying the limitation and SHALL bridge only `device[0]`. Subsequent devices (index 1..N-1) SHALL NOT be registered.

The WARN message SHALL include the literal text `"sim_hardware topology has N={N} devices; multi-device shared singletons not yet supported. Bridging device[0] only."` where `{N}` is replaced with the actual count.

This contract is **WARN-only** (no `assert`). Debug builds do NOT abort — the multi-device path is contract-deferred, not contract-failed. This keeps the plugin loadable in mixed-debug/CI environments where a transient 2-device topology fixture might exist.

#### Scenario: multi-device topology bridges only device[0] with WARN

- **WHEN** `sim_hardware/topology/default_topology.json` contains 2 GPU devices
- **THEN** `plugin_init_internal` returns 0
- **AND** `[GpuPlugin] sim_hardware topology has N=2 devices; multi-device shared singletons not yet supported. Bridging device[0] only.` is logged at WARN level to stderr
- **AND** `VFS::instance().open("/dev/gpgpu0")` succeeds
- **AND** `VFS::instance().open("/dev/gpgpu1")` fails with `-ENODEV`
- **AND** in debug builds (CMAKE_BUILD_TYPE=Debug), the same path does NOT abort — the WARN is the only N>1 signal

#### Scenario: single-device topology does not emit WARN

- **WHEN** `pci_probe_enumerate_from_sim_hardware` returns `out_count == 1`
- **THEN** no multi-device WARN is logged
- **AND** `/dev/gpgpu0` is registered normally

### Requirement: HAL ABI preserved (zero fn-ptr signature change)

The change SHALL NOT modify the `gpu_hal_ops` struct definition at `plugins/gpu_driver/hal/gpu_hal.h`. The 68 fn-ptr members SHALL retain their existing signatures. `GpgpuDevice(struct gpu_hal_ops* hal)` constructor SHALL retain its signature.

The change SHALL NOT modify `hal_user.cpp` HAL implementation functions (all `static`, ctx-from-caller per Oracle's reentrancy audit).

Verification mechanism: header diff + fn-ptr member count, NOT `nm` (which cannot see struct members or static-local symbols). This addresses the design.md:46 / spec.md:47-48 nm-method contradiction that previously made the verification vacuous.

#### Scenario: gpu_hal.h header unchanged (the actual ABI preservation check)

- **WHEN** `git diff --stat HEAD~1 -- plugins/gpu_driver/hal/gpu_hal.h` is run before and after the change
- **THEN** both invocations return no output (header file is byte-identical)
- **AND** this is the authoritative ABI preservation evidence (not nm)

#### Scenario: fn-ptr member count is 68 in the header

- **WHEN** `grep -c '(\*' plugins/gpu_driver/hal/gpu_hal.h` is run
- **THEN** output is 70 (fn-ptr member count in `struct gpu_hal_ops`; baseline established post-Change-2 — pre-Change-2 was 68 per ADR-023 D4 append-only accumulation)
- **AND** this count matches the pre-change baseline recorded in `tools/check_phase0_gate.sh` (or in a regression test fixture)

#### Scenario: hal_user functions remain file-static

- **WHEN** `nm build/plugins/libgpu_driver_plugin.so | grep hal_user` is run after the change
- **THEN** no `hal_user_*` symbols are exported as global (all implementations remain `static` in the plugin .so)

### Requirement: drv/ directory unchanged

The change SHALL NOT modify any file under `plugins/gpu_driver/drv/` (including `gpgpu_device.cpp`, `gpu_drm_driver.cpp`, `kfd/*`). The `drv-sim-boundary` spec invariant ("drv/ holds opaque hal_queue_handle_t; drv/ invokes behavior through HAL inline wrappers") SHALL be preserved.

#### Scenario: drv/ git diff is empty

- **WHEN** `git diff --stat HEAD~1 -- plugins/gpu_driver/drv/` is run after the change
- **THEN** output is empty (no files modified)

### Requirement: regression test covers enumeration → registration → ioctl

A new Catch2 test binary `tests/test_gpu_sim_hardware_bridge_standalone.cpp` MUST be added that:
1. Loads `sim_hardware/topology/default_topology.json` via `pci_probe_enumerate_from_sim_hardware`
2. Asserts `out_count >= 1`
3. Asserts `discovered[0].bdf == 0x08` (packed BDF value as emitted by `sim_hardware/src/topology.cpp:27-48` `pack_bdf`: the parser reads `bus=hex(bdf,0,4)`, `device=hex(bdf,5,2)`, `func=hex(bdf,8,2)` from the BDF string `"0000:01:00.0"`, yielding `bus=0x00, device=0x01, func=0x0` → `(bus << 8) | (device << 3) | func` = `(0 << 8) | (1 << 3) | 0` = `0x08`. NOTE: this is a pre-existing parser quirk (the encoding treats offset [0:4] as bus rather than domain, so the packed value differs from the standard PCI BDF encoding which would give `0x0100`); out of scope for Change-3 — the test asserts the ACTUAL packed value produced by `pack_bdf`, not the standard-PCI value). Test MUST compare against the packed uint16, not the string form.
4. Exercises the gpu_driver composition root indirectly by verifying the registered device responds to `GPU_IOCTL_GET_DEVICE_INFO`

The test MUST be registered in `tests/CMakeLists.txt` CATCH2_TESTS list.

#### Scenario: bridge test passes at HEAD

- **WHEN** `./build/bin/test_gpu_sim_hardware_bridge_standalone` is run after the change
- **THEN** all Catch2 assertions pass
- **AND** ctest total count is 156 (was 155)

#### Scenario: existing single-device ioctl test continues passing

- **WHEN** `./build/bin/test_gpu_ioctl_standalone` is run after the change
- **THEN** all Catch2 assertions pass (N=1 behavior equivalent to pre-change)

### Requirement: enumerate error semantics — propagate errno

When `pci_probe_enumerate_from_sim_hardware` returns a negative value (e.g., `-ENOENT` if `sim_hardware/topology/default_topology.json` is missing, `-EINVAL` on malformed topology), `plugin_init_internal` SHALL return that errno to the caller (the ModuleLoader).

Rationale: silent degradation (logging WARN + returning 0 + registering zero devices) would make CWD-sensitive regressions invisible — `/dev/gpgpu0` would disappear but the plugin load would succeed. Propagating the error keeps the change aligned with the existing fail-fast philosophy and ensures `ctest` reports the missing-topology regression loudly instead of "Device not found" at test runtime.

#### Scenario: missing topology file fails plugin init with -ENOENT

- **WHEN** `sim_hardware/topology/default_topology.json` does not exist (e.g., CWD not at project root)
- **THEN** `pci_probe_enumerate_from_sim_hardware` returns `-ENOENT`
- **AND** `plugin_init_internal` returns `-ENOENT` (propagated, NOT swallowed)
- **AND** no `/dev/gpgpu*` device is registered
- **AND** ModuleLoader logs the failure and proceeds with other plugins (consistent with current `module_loader.cpp:199-203` behavior)

#### Scenario: malformed topology fails plugin init with -EINVAL

- **WHEN** `sim_hardware/topology/default_topology.json` exists but fails schema validation
- **THEN** `pci_probe_enumerate_from_sim_hardware` returns `-EINVAL`
- **AND** `plugin_init_internal` returns `-EINVAL` (propagated)
- **AND** no `/dev/gpgpu*` device is registered

#### Scenario: zero-device topology succeeds (positive-path edge case)

- **WHEN** `pci_probe_enumerate_from_sim_hardware` returns `0` with `out_count == 0`
- **THEN** `plugin_init_internal` returns 0 (success)
- **AND** no `/dev/gpgpu*` device is registered
- **AND** no error or WARN is logged (this is a legal empty topology, not an error)