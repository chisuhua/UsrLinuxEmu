## Why

Oracle post-ship audit on Change-3 (`add-gpu-driver-sim-hardware-bridge`, session `ses_f8b8c32e5ffeGgFiPdo4hE0jiu`) Q1 identified dead code:

- `plugins/gpu_driver/drv/kfd_sim_bridge.cpp:261` — `kfd_sim_bridge_set_hal()` setter function
- Header declaration: `plugins/gpu_driver/drv/kfd/kfd_sim_bridge.h:25`
- Zero call sites in the entire repository (verified by `git grep -n kfd_sim_bridge_set_hal plugins/ tests/ src/ tools/`)
- Pre-existing dead code — the old `plugin.cpp` (pre-Change-3) also never called it; the gpu_driver composition root bypasses this setter by passing `gpu_hal_ops*` directly to `GpgpuDevice`

Oracle recommendation (Q1): "wire into composition root or delete, choose one". This change picks **delete** because:

1. The composition root (Change-3) was deliberately designed around `gpu_hal_ops*` per-device ownership (per ADR-023 D4 append-only discipline); a global `set_hal` setter conflicts with the per-device model.
2. `kfd_sim_bridge` already has `kfd_sim_set_mm_shim()` (`kfd_sim_bridge.cpp:271-274`), which IS called from `plugin_init_internal` at the singleton init block — the HAL setter is redundant alongside the mm_shim setter.
3. Dead code accumulates maintenance cost (someone reading `kfd_sim_bridge.cpp` must understand why this function exists; future debuggers waste time looking for callers).

## What Changes

- Delete `kfd_sim_bridge_set_hal()` definition from `plugins/gpu_driver/drv/kfd_sim_bridge.cpp` (~10 lines)
- Delete `kfd_sim_bridge_set_hal()` declaration from `plugins/gpu_driver/drv/kfd/kfd_sim_bridge.h`
- Confirm no test or doc references the function

## Non-goals (deferred)

- Wider cleanup of `kfd_sim_bridge.cpp` (out of scope for this targeted deletion; surface later if more dead code accumulates)

## References

- Oracle audit session `ses_f8b8c32e5ffeGgFiPdo4hE0jiu` (Q1 dead code observation)
- `plugins/gpu_driver/drv/kfd_sim_bridge.cpp:261` (function definition)
- `plugins/gpu_driver/drv/kfd/kfd_sim_bridge.h:25` (function declaration)
- Change-3 composition root at `plugins/gpu_driver/plugin.cpp` (does NOT use this setter — confirms dead)