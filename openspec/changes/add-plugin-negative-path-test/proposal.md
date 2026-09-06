## Why

Change-3 (`add-gpu-driver-sim-hardware-bridge`) added spec Requirement "enumerate error semantics — propagate errno" (`openspec/changes/archive/2026-09-05-.../specs/gpu-driver-sim-hardware-bridge/spec.md:101-127`). The spec defines three Scenarios:

- Missing topology file → `pci_probe_enumerate_from_sim_hardware` returns `-ENOENT`, `plugin_init_internal` returns `-ENOENT` (propagated, NOT swallowed), no `/dev/gpgpu*` registered.
- Malformed topology → returns `-EINVAL`, propagated.
- Zero-device topology → returns 0 with no WARN.

None of these three negative paths are exercised by any ctest today (verified by Oracle post-ship audit session `ses_f8b8c32e5ffeGgFiPdo4hE0jiu` Q2). The behavior was manually verified during Oracle's audit (running from `/tmp` showed the expected failure), but a ctest regression guard does not exist — any future refactor that re-introduces silent degradation would not be caught.

The first Scenario is reachable today without code change: `pci_probe_enumerate_from_sim_hardware` is called with a hardcoded relative path `"sim_hardware/topology/default_topology.json"`. If the test process's CWD lacks that path, the call returns `-ENOENT`. The cheapest way to inject a missing-topology state in a test is `chdir` to a fresh empty temp directory before `load_plugins`, then assert:

- `load_plugins` returns non-zero (or returns success but gpu_driver module init returns `-ENOENT`)
- `VFS::instance().open("/dev/gpgpu0", O_RDWR)` returns `nullptr` (no device registered)
- No zombie device nodes from the failed init leak across the rest of ctest (handled by Catch2 process isolation)

This must be a separate test binary — process-global plugin loading is one-shot per process; the existing `test_gpu_sim_hardware_bridge_standalone` calls `load_plugins` once with the canonical CWD and cannot also exercise the missing-topology path.

## What Changes

- New `tests/test_gpu_plugin_negative_path_standalone.cpp` (Catch2):
  - `mkdir` a temp dir, `chdir` into it, call `ModuleLoader::load_plugins("plugins")`, restore CWD, then assert:
    - The captured stdout/stderr contains `pci_probe_enumerate_from_sim_hardware failed:` (proves the propagate-error path ran)
    - `VFS::instance().open("/dev/gpgpu0", 0)` returns `nullptr`
- `tests/CMakeLists.txt`: register the new file in `CATCH2_TESTS` (the first list, processed by `add_catch_test` — NOT `add_catch_sim_test`, which lacks the required include paths)

## Non-goals (deferred)

- Malformed topology (`-EINVAL`) coverage: requires writing a malformed JSON fixture to a temp path, which the plugin's hardcoded `default_topology.json` literal doesn't allow. Either (a) extend this change to also test -EINVAL by writing a custom fixture, or (b) leave for a follow-up that parameterizes the topology path (bigger blast radius — requires changing the plugin or adding a ModuleLoader hook).
- Zero-device topology coverage: requires writing a 0-devices JSON fixture. Same blocker as `-EINVAL`.

## References

- Oracle audit session `ses_f8b8c32e5ffeGgFiPdo4hE0jiu` (Q2 omissions table)
- `openspec/changes/archive/2026-09-05-add-gpu-driver-sim-hardware-bridge/specs/gpu-driver-sim-hardware-bridge/spec.md:101-127` (REQ-ENUM-ERR-SEM)
- `tests/test_gpu_sim_hardware_bridge_standalone.cpp` (precedent for bridge tests using `add_catch_test`)
- `tests/CMakeLists.txt` line ~220 area (CATCH2_TESTS list location)