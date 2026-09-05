## Why

Change-2 (`2026-09-03-sim-hardware-foundation-tier1-tier2`) delivered `pci_probe_enumerate_from_sim_hardware` (Wave 4 Q2→Q3 thin wrapper) and `sim_hardware/topology/default_topology.json`, but **no consumer bridges the GPU driver plugin to this output**. `plugins/gpu_driver/` references `sim_hardware`/`cpptlm`/`pci_probe` zero times today (verified `grep -rln`), so the entire Change-1+2 refactor (kernel SHARED + sim_hardware foundation + PCI bridge) has no driver-side endpoint. ADR-088 explicitly mandates "drv/ 零修改 + HAL in-place 替换"; this change realizes that mandate by introducing an enumeration-driven composition root in `plugin.cpp` while leaving HAL ABI and `drv/` untouched. Oracle's post-impl review of Change-2 flagged this as the highest-priority remaining gap.

## What Changes

- **`plugins/gpu_driver/plugin.cpp`**: Replace static `HalHolder` singleton with a per-device loop driven by `pci_probe_enumerate_from_sim_hardware` output. For each `DiscoveredDevice`, construct a `HalHolder` (`gpu_hal_ops` + `hal_user_context` + `DoorbellEmu` + `GlobalScheduler` + `hal_puller_handle`) and register `/dev/gpgpu{i}` with VFS.
- **Shared singletons stay process-global** (`g_vram_store`, `g_dma_pool`, `g_plugin_mm_shim`, KFD subsystem). When `pci_probe_enumerate_from_sim_hardware` returns `out_count > 1`, log WARN and bridge only `device[0]` (`-ENOTSUP` for multi-device is deferred to escalation change).
- **Zero HAL ABI change** (68 `gpu_hal_ops` fn-ptrs preserved per ADR-023 D4 append-only discipline).
- **Zero `drv/` change** (GpgpuDevice untouched; HAL/drv boundary unchanged per `drv-sim-boundary` spec).
- **New test binary** `tests/test_gpu_sim_hardware_bridge_standalone.cpp` (location per design.md Decision; not `tests/sim_hardware/`): enumerates `default_topology.json`, asserts one `/dev/gpgpu0` registration, exercises `GPU_IOCTL_GET_DEVICE_INFO` via the bridge path.

## Capabilities

### New Capabilities
- `gpu-driver-sim-hardware-bridge`: enumeration-driven composition root in gpu_driver plugin.cpp; per-device HalHolder instantiation; N=1 fail-fast contract for shared sim singletons.

### Modified Capabilities
- *(none — HAL ABI and `drv/` untouched; existing `drv-sim-boundary`, `3-way-separation-multi-device`, `kfd-tier2-runtime-penetration` specs unchanged at requirement level)*

## Impact

| Affected | Impact |
|----------|--------|
| `plugins/gpu_driver/plugin.cpp` | composition root rewritten: static `HalHolder` + `g_hal` → `std::vector<std::unique_ptr<HalHolder>>` driven by enumerate loop |
| `plugins/gpu_driver/CMakeLists.txt` | add `plugins/pci_driver/include` + `sim_hardware/include/{,pcie,cpptlm}` include paths; link `pci_driver_plugin` SHARED via DT_NEEDED (no source modification to pci_driver, only new consumer dependency) |
| `plugins/gpu_driver/hal/` | zero change |
| `plugins/gpu_driver/drv/` | zero change |
| `plugins/gpu_driver/sim/` | zero change |
| `sim_hardware/` | zero change (already provides `pci_probe_enumerate_from_sim_hardware`) |
| `plugins/pci_driver/` | zero change (only consumed via link; per `sim_hardware/topology/default_topology.json` baseline) |
| `tests/test_gpu_ioctl_standalone` | behavior equivalent for N=1 (single fixture `/dev/gpgpu0`); must continue passing without modification |
| `tools/check_phase0_gate.sh` | +P0-G6 (`default_topology.json` device count == 1) — materializes design.md Risks table promise |
| New test | `tests/test_gpu_sim_hardware_bridge_standalone.cpp` |
| TaskRunner System C contract | zero change |
| Build artifacts | `plugins/pci_driver/libpci_driver_plugin.so` + `plugins/gpu_driver/libgpu_driver_plugin.so` rebuild |

## Non-goals (deferred)

- Multi-device BDF-keyed VRAM / DMA / mm_shim / KFD routing — explicit fail-fast to follow-up change triggered by 2nd-device topology.
- Per-device GET_DEVICE_INFO reflection of topology vendor_id / device_id — `handleGetDeviceInfo` keeps compile-time SIMULATED_* constants; per-instance topology attrs listed as optional follow-up.
- 23 ABI real implementation (CppTLM follow-up) — separate Change-4 candidate per `cpptlm_abi_inventory.json`.
- BAR enumeration in `DiscoveredDevice` — separate change at `host_bridge` layer.

## Escalation trigger

When `sim_hardware/topology/default_topology.json` (or any test topology) contains **≥2 GPU devices**, this change is superseded by a follow-up that BDF-keys the shared singletons. The per-device HalHolder loop introduced here carries forward unchanged — the follow-up's change surface is confined to `sim/` + `g_vram_store`/`g_dma_pool`/`g_plugin_mm_shim` + KFD subsystem.