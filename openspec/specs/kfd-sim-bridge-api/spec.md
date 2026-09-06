# kfd-sim-bridge-api Specification

## Purpose
TBD - created by archiving change resolve-kfd-sim-bridge-set-hal. Update Purpose after archive.
## Requirements
### Requirement: kfd_sim_bridge API does not expose setters or getters for the HAL

The `kfd_sim_bridge` module (`plugins/gpu_driver/drv/kfd_sim_bridge.cpp` + `plugins/gpu_driver/drv/kfd/kfd_sim_bridge.h`) SHALL NOT export any function whose name contains `set_hal`, `get_hal`, or any backing static variable whose name contains `bridge_hal`. The module's real exported surface (per `kfd_sim_bridge.cpp`) consists of `kfd_sim_reset` and the seven `kfd_sim_*` translation handlers used by the kfd_ioctl dispatch layer:

- `kfd_sim_reset`
- `kfd_sim_lookup_pfn`
- `kfd_sim_get_page_count`
- `kfd_sim_handle_map_memory`
- `kfd_sim_handle_unmap_memory`
- `kfd_sim_handle_get_process_aperture`
- `kfd_sim_handle_update_queue`
- `kfd_sim_register_mmu_cb`

This requirement documents the post-deletion invariant: the only path through which the kfd ioctl dispatch layer reaches the sim hardware backend is the static function call ABI above; there is no longer any runtime setter/getter.

#### Scenario: grep for setters/getters/backing state in kfd_sim_bridge module

- **WHEN** `grep -rn 'set_hal\|get_hal\|bridge_hal' plugins/gpu_driver/drv/kfd_sim_bridge.cpp plugins/gpu_driver/drv/kfd/kfd_sim_bridge.h` is run
- **THEN** output is empty (the only `set_hal` reference removed in commit `a2701cd`; no replacement added)
- **AND** `grep -n '^void kfd_sim\|^u32 kfd_sim\|^u64 kfd_sim\|^long kfd_sim' plugins/gpu_driver/drv/kfd_sim_bridge.cpp` lists all 8 real exported symbols above (proves the module still has its real API surface, just not the deleted setters)

