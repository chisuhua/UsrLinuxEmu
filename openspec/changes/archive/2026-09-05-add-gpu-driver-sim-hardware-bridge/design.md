## Context

Change-2 (`2026-09-03-sim-hardware-foundation-tier1-tier2`, archived 2026-09-04) delivered `sim_hardware/` (topology JSON + host bridge + CpptlmBridge mock + bypass controller + MSI-X mock) and `plugins/pci_driver/` (Q2→Q3 PCI bridge with `pci_probe_enumerate_from_sim_hardware`). The GPU driver plugin at `plugins/gpu_driver/` is the project's target consumer but currently references none of these (`grep -rln 'sim_hardware\|cpptlm\|pci_probe' plugins/gpu_driver/` returns empty).

The blocker: `plugin.cpp` defines a static `HalHolder` singleton with one `gpu_hal_ops`, one `DoorbellEmu`, one `GlobalScheduler`, one `hal_puller_handle`, one `VramStore`, one `DmaPool`, one `g_plugin_mm_shim`, one KFD subsystem — hard-bound to a single hardcoded `/dev/gpgpu0`. `pci_probe_enumerate_from_sim_hardware` can return N devices but only one ever instantiates.

Oracle's post-impl review of Change-2 (`ses_f8da24eeaffeF4HuyhO41beaio`) investigated all 4 options (multi-HAL, single-HAL+router, single-device PoC, hybrid) plus recommended Option E (A skeleton × N=1 instantiation). The current architecture's HAL ABI (`gpu_hal_ops` 68 fn-ptrs) takes `void *ctx` as first parameter for 67 of 68 fn-ptrs — meaning **ctx itself is the per-device discriminator**, no HAL ABI change needed to support per-device HAL instances.

ADR-088 ("dGPU 参考设计") mandates: "HAL in-place 替换模式; drv/ 零修改; 5 阶段 24-32 周". This change implements the first slice of that mandate: the enumeration-driven composition root in `plugin.cpp` that bridges the new sim_hardware foundation to the existing GpgpuDevice (HAL/drv untouched).

## Goals / Non-Goals

**Goals:**
- Per-device HalHolder instantiation loop in `plugin_init_internal`, driven by `pci_probe_enumerate_from_sim_hardware` output
- `/dev/gpgpu{i}` registered per discovered device, where `i` is the topology index
- N=1 topology fully wired today with N=1 behaviors (existing `test_gpu_ioctl_standalone` continues passing)
- N>1 topologies: explicit WARN + bridge device[0] only (fail-fast path, deferred multi-device singletons to escalation change)
- HAL ABI preserved (68 fn-ptrs unchanged, no signature modifications)
- `drv/` untouched (GpgpuDevice / ioctl table / hal_user.cpp unchanged)
- New regression test `test_gpu_sim_hardware_bridge_standalone` covering enumeration → registration → GET_DEVICE_INFO round-trip

**Non-Goals (deferred):**
- Multi-device BDF-keyed VRAM / DMA pool / mm_shim / KFD routing — escalates when 2nd device appears in any topology
- Per-device `GET_DEVICE_INFO` reflecting topology vendor_id / device_id (GpgpuDevice keeps compile-time SIMULATED_* constants)
- 23 ABI real implementation (CppTLM follow-up per `cpptlm_abi_inventory.json`) — separate Change-4 candidate
- BARs enumeration in `DiscoveredDevice` — separate host_bridge-layer change

## Decisions

### Decision 1: Option E (A skeleton × N=1 instantiation)
**What**: Write the per-device HalHolder enumeration loop structure today, but instantiate N=1 only. The loop, `std::vector<std::unique_ptr<HalHolder>>` shape, and per-device init sequence are written for N devices. The shared singletons (VRAM, DMA, mm_shim, KFD) stay process-global; when `out_count > 1`, log WARN and register only device[0] (or return `-ENOTSUP` per fail-fast policy).

**Rationale**: Pure A (full multi-device) requires BDF-keyed sim singletons — Medium(1-2d) cost to solve a problem that doesn't exist (current topology has 1 device). Pure C (hardcoded `devices[0]`) ships in 1 hour but creates singleton-shaped debt. Option E captures the structural benefit of A (loop ready for N≥2) at N=1 cost (~100-120 LOC, 1-4h).

**Alternatives considered**:
- B (single-HAL + BDF router) — ctx is already the discriminator; router layer is zero-benefit indirection that risks HAL ABI pollution → rejected
- C (hardcoded device[0]) — singleton debt, next change redoes the bridge → rejected
- D (full hybrid with BDF-keyed pools) — YAGNI for current topology → deferred to escalation change

### Decision 2: HAL ABI zero-change
**What**: `gpu_hal_ops` 68 fn-ptrs retain current signatures. `GpgpuDevice(struct gpu_hal_ops* hal)` constructor signature unchanged. `hal_user_init` / `hal_user_context` reentrant usage unchanged.

**Rationale**: ADR-023 D4 mandates append-only HAL discipline (proved by ADR-076/090 PTX-EMU extension). 67 of 68 fn-ptrs already take `void *ctx` first — ctx is per-instance discriminator, no BDF parameter needed. The 1 exception (`mem_map_bo(struct gpgpu_device *dev, ...)` at `gpu_hal.h:85`) already takes a device pointer.

**Verification**: `git diff --stat HEAD~1 -- plugins/gpu_driver/hal/gpu_hal.h` returns empty (byte-identical header is the authoritative ABI preservation evidence) + `grep -c '(\*' plugins/gpu_driver/hal/gpu_hal.h` returns 70 (fn-ptr member count in `struct gpu_hal_ops` — baseline established post-Change-2; pre-Change-2 was 68 per ADR-023 D4 append-only accumulation). Replaces the earlier proposal of `nm -D ... | grep gpu_hal | wc -l` which was vacuous (`gpu_hal_ops` is a struct type that emits no symbols; the `gpu_hal` library is STATIC).

### Decision 3: `drv/` zero-change
**What**: `plugins/gpu_driver/drv/{gpgpu_device.cpp,gpu_drm_driver.cpp,kfd/*}` untouched. `drv-sim-boundary` spec preserved.

**Rationale**: ADR-088 explicit principle. `GpgpuDevice` already takes `gpu_hal_ops*` per-instance — no constructor signature change needed. Per-device routing happens at the HAL ops pointer passed to each GpgpuDevice, not inside GpgpuDevice itself.

### Decision 4: Composition-root responsibility model
**What**: `plugin_init_internal` owns:
- `pci_probe_enumerate_from_sim_hardware` invocation
- `std::vector<std::unique_ptr<HalHolder>>` lifecycle
- `VFS::register_device` per discovered device
- Process-global singletons (`g_vram_store`, `g_dma_pool`, `g_plugin_mm_shim`, KFD) initialization remains at top, before loop

**Rationale**: Mirrors Change-2 `pci_setup_bus.cpp` pattern (reload topology, match by vendor/device/bdf, call `emu->assign_bar`). Single owner for failure paths and ordering. Per-device init sequence is uniform: `hal_user_init → hal_puller_create → scheduler.registerKernel → make_shared<GpgpuDevice> → setPuller/set_mm_shim/setHalContext → VFS::register_device`.

### Decision 5: Fail-fast policy for N>1
**What**: When `pci_probe_enumerate_from_sim_hardware` returns `out_count > 1`:
- Log WARN: `"[GpuPlugin] sim_hardware topology has N={out_count} devices; multi-device shared singletons not yet supported. Bridging device[0] only."`
- Continue with `for (size_t i = 0; i < 1; ++i)` (loop bound effectively 1)
- Return 0 (success; user-facing ioctl paths on `/dev/gpgpu0` work)

**Rationale**: Explicit deferred, not silent failure. Documents the limitation in run-time logs. Failure mode (cross-device memory corruption) avoided because we never bridge the extra devices.

**Escalation trigger** (`escalation:` comment in code): when any `default_topology.json` (or test fixture topology) contains ≥2 devices, file a follow-up change. The follow-up's change surface is confined to BDF-keyed sim singletons — `plugin.cpp`'s per-device loop carries forward unchanged.

### Decision 6: Link dependency on `pci_driver_plugin` (SHARED)

**What**: `plugins/gpu_driver/CMakeLists.txt` gains `target_link_libraries(gpu_driver_plugin PRIVATE pci_driver_plugin)` plus `target_include_directories` for `plugins/pci_driver/include` and `sim_hardware/include/{,pcie,cpptlm}`. This is a **consumer-only** dependency: `plugins/pci_driver/` source is not modified.

**Rationale**: `pci_driver_plugin` is built as a SHARED library per AGENTS.md §"kernel 库必须是 SHARED" rationale (the same kernel-singleton principle). DT_NEEDED resolution makes `pci_probe_enumerate_from_sim_hardware` (and any future pci-side helpers) available at dlopen time without explicit `dlsym`. No `.depends` field is added to `module mod` (`plugin.cpp:145`) — link-time resolution is sufficient and ordering is irrelevant because `pci_driver_init` is a no-op (the probe is state-free).

**Alternatives considered**:
- `dlopen("plugins/plugin_pci_driver.so") + dlsym` — adds a runtime dependency dance with the existing ModuleLoader; rejected (over-engineering for a same-tree link).
- Move `pci_probe_enumerate_from_sim_hardware` into `sim_hardware_mock` and link that — duplicates the SHARED library rationale into sim_hardware; rejected.
- Reverse load priority + `.depends` — unnecessary because DT_NEEDED auto-resolves.

**Verification**: `nm -D build/plugins/libgpu_driver_plugin.so | grep pci_probe_enumerate_from_sim_hardware` shows the symbol as `U` (undefined, resolved at runtime via DT_NEEDED); `ldd build/plugins/libgpu_driver_plugin.so` lists `libpci_driver_plugin.so` as a dependency.

### Decision 7: Enumerate error semantics — propagate errno

**What**: When `pci_probe_enumerate_from_sim_hardware` returns a negative value (e.g., -ENOENT for missing topology file), `plugin_init_internal` returns that errno to the ModuleLoader. The plugin is not silently degraded.

**Rationale**: Silent degradation (log WARN + return 0 + register zero devices) makes CWD-sensitive regressions invisible — `/dev/gpgpu0` disappears but the plugin load succeeds. Propagating the error keeps the change aligned with the existing fail-fast philosophy and ensures `ctest` reports the missing-topology regression loudly instead of "Device not found" at test runtime.

**Reference**: `module_loader.cpp:199-203` (init failure → dlclose + return -1) and `:227-233` (load_plugins does not abort on a single plugin failure, continues with remaining plugins).

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| Hidden global state in `hal_user_init` (false reentrancy assumption) | Oracle confirmed via `hal_user.cpp` inspection (all 68 implementations `static`, ctx-from-caller). Add regression test that creates 2 HalHolders in one process (force-fail expected for now). |
| `GpgpuDevice` per-instance state pollutes across devices if N≥2 ever silently passes | Fail-fast + WARN explicitly enumerates limitation (WARN-only contract — no `assert`, see Decision 5). P0-G6 gate blocks the next commit that adds a 2nd device. |
| `default_topology.json` evolves to 2+ devices before escalation change ships | Pre-commit `tools/check_phase0_gate.sh` gains P0-G6 (`devices == 1`) in task 3.0.1; when N>1, P0-G6 FAILs and blocks the commit. This IS the escalation trigger. |
| `handleGetDeviceInfo` returns SIMULATED_* constants → doesn't reflect topology vendor/device | Documented as non-goal; users querying real device attrs need per-device follow-up. |
| CWD-sensitive plugin init (topology file missing → silent degrade) | Propagate errno (per spec Requirement "enumerate error semantics"); module_loader.cpp:199-203 surfaces as plugin-init failure in ctest output. |

## Migration Plan

This change is internal to `plugins/gpu_driver/`. No external API or TaskRunner contract changes. Migration steps:

1. **Phase 1** (this change, 1-4h): Rewrite `plugin_init_internal` composition root; add regression test; update `tests/CMakeLists.txt` to register the new binary.
2. **Phase 2** (validation): `ctest 155 → 156/156 PASS` (baseline count must be confirmed via task 1.1 before being written in stone); `tools/check_phase0_gate.sh 6/6 PASS` (after P0-G6 added in task 3.0.1); docs-audit 68/68 pass.
3. **Phase 3** (rollback if needed): `git revert` the single commit. Single composition-root rewrite is easy to revert; no schema migration, no DB, no IPC contract.

## Open Questions

1. **`kfd_sim_bridge_set_hal()` fate — DEFERRED to escalation change.** The setter exists at `plugins/gpu_driver/drv/kfd_sim_bridge.cpp:261` (per Oracle verification) and has at least one test caller (`tests/test_kfd_sim_bridge_audit_standalone.cpp:52-62`). Per Metis+Oracle review, the cleanup/annotation decision conflicts with this change's "drv/ zero-modify" invariant. **Decision**: defer the entire fate question to the escalation change that actually wires per-device KFD HAL (when N≥2 topology arrives and BDF-keyed KFD becomes real). This change does not touch the setter.

2. **`escalation:` code comment** — resolved as runtime WARN only (no `assert`). See Decision 5.

3. **`DiscoveredDevice` carries vendor/device/class but no BARs.** If `handleGetDeviceInfo` ever wants `bar0_size`, we'd need host_bridge BAR query API. **Decision**: out of scope; defer.

4. **CWD-relative topology path** — the literal `"sim_hardware/topology/default_topology.json"` assumes CWD == project root, which is true for `ctest` (WORKING_DIRECTORY default) and `tools/cli/main.cpp` (entry cwd). However, if a future user invokes `./build/bin/cli` from elsewhere, plugin_init will fail with -ENOENT (now propagated, see Decision 7). **Decision**: document in the test binary's CWD assumption; if needed later, switch to `__FILE__`-relative resolution mirroring `pci_setup_bus.cpp`.