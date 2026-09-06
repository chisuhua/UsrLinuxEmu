## Why

Oracle post-ship audit on Change-3 (`add-gpu-driver-sim-hardware-bridge`, session `ses_f8b8c32e5ffeGgFiPdo4hE0jiu`) initially identified `kfd_sim_bridge_set_hal()` as dead code (Q1). The deep-dive Oracle review (session `ses_f8f0f9467ffe3w1M4uCHZTcFmS`) corrected this: the setter **does have a call site** — `tests/test_kfd_sim_bridge_audit_standalone.cpp:62`. Deleting only the setter breaks that test binary's build. The full dead cluster is:

| Symbol | File | Status |
|---|---|---|
| `kfd_sim_bridge_set_hal()` | `plugins/gpu_driver/drv/kfd_sim_bridge.cpp:261-263` | Called only by `tests/test_kfd_sim_bridge_audit_standalone.cpp:62` |
| `kfd_sim_bridge_get_hal()` | `plugins/gpu_driver/drv/kfd_sim_bridge.cpp:265-267` | Called only by `tests/test_kfd_sim_bridge_audit_standalone.cpp:58, :65` (header marks it `test-only`) |
| `g_bridge_hal_` (static) | `plugins/gpu_driver/drv/kfd_sim_bridge.cpp:259` | Sole reader is `kfd_sim_bridge_get_hal` |
| TEST_CASE "kfd_sim_bridge_set_hal registers pointer" | `tests/test_kfd_sim_bridge_audit_standalone.cpp:52-69` | Sole consumer of all four symbols above |
| `dummy_iommu_map` / `dummy_iommu_unmap` helpers | `tests/test_kfd_sim_bridge_audit_standalone.cpp:40-48` | Only used by the deleted TEST_CASE |
| Stale comment at `kfd_sim_bridge.cpp:11` | `plugins/gpu_driver/drv/kfd_sim_bridge.cpp` | References the deleted setter |

Oracle recommendation (Q1): "wire into composition root or delete". This change picks **delete** because:

1. The composition root (Change-3) was deliberately designed around `gpu_hal_ops*` per-device ownership (per ADR-023 D4 append-only discipline); a global `set_hal` setter conflicts with the per-device model.
2. `kfd_sim_bridge` already has `kfd_sim_set_mm_shim()` (`kfd_sim_bridge.cpp:271-274`), which IS called from `plugin_init_internal` at the singleton init block — the HAL setter is redundant alongside the mm_shim setter.
3. The only consumer of the setter is a single TEST_CASE whose entire purpose was to verify the setter worked. Removing both the setter and the test removes the verification burden without losing coverage of any production path.
4. Dead code accumulates maintenance cost (someone reading `kfd_sim_bridge.cpp` must understand why this function exists; future debuggers waste time looking for callers).

## What Changes

- Delete `kfd_sim_bridge_set_hal()` definition from `plugins/gpu_driver/drv/kfd_sim_bridge.cpp`
- Delete `kfd_sim_bridge_get_hal()` definition from `plugins/gpu_driver/drv/kfd_sim_bridge.cpp`
- Delete the static `g_bridge_hal_` storage in `plugins/gpu_driver/drv/kfd_sim_bridge.cpp`
- Delete both function declarations and the explanatory comment block from `plugins/gpu_driver/drv/kfd/kfd_sim_bridge.h`
- Update the top file comment in `kfd_sim_bridge.h` to remove the "C-12 B.3.5 set_hal" reference
- Update the include comment at `kfd_sim_bridge.cpp:11` to remove the "B.3.5 set_hal declaration" annotation
- Delete TEST_CASE "kfd_sim_bridge_set_hal registers pointer" from `tests/test_kfd_sim_bridge_audit_standalone.cpp`
- Delete the `dummy_iommu_map` / `dummy_iommu_unmap` helpers (only used by the deleted test) and the `#include "gpu_hal.h"` they required
- Update the file header comment in the test file to drop the "set_hal test" subtitle and renumber remaining checks
- Verify TEST_CASE 2 ("sim_pm_* still callable") and TEST_CASE 3 ("every kfd_sim_handle_* function has LEGACY/CLEAN marker") still build and pass

## Non-goals (deferred)

- Wider cleanup of `kfd_sim_bridge.cpp` (out of scope; surface later if more dead code accumulates)

## References

- Oracle audit session `ses_f8b8c32e5ffeGgFiPdo4hE0jiu` (Q1 dead code observation)
- Oracle deep-dive session `ses_f8f0f9467ffe3w1M4uCHZTcFmS` (call-site verification + scope expansion)
- `plugins/gpu_driver/drv/kfd_sim_bridge.cpp:259-267` (dead cluster)
- `plugins/gpu_driver/drv/kfd/kfd_sim_bridge.h:17-28` (declarations)
- `tests/test_kfd_sim_bridge_audit_standalone.cpp:40-69` (sole consumer)
- Change-3 composition root at `plugins/gpu_driver/plugin.cpp` (does NOT use this setter — confirms dead for production)