# kfd-sim-bridge-api Specification

## Purpose
TBD - created by archiving change resolve-kfd-sim-bridge-set-hal. Update Purpose after archive.
## Requirements
### Requirement: kfd_sim_bridge API does not expose setters or getters for the HAL

The `kfd_sim_bridge` module (`plugins/gpu_driver/drv/kfd_sim_bridge.cpp` + `plugins/gpu_driver/drv/kfd/kfd_sim_bridge.h`) SHALL NOT export any function whose name contains `set_hal`, `get_hal`, or any backing static variable whose name contains `bridge_hal`. The module's only exported symbols are the translation functions (`kfd_sim_bridge_*`) plus `kfd_sim_bridge_init` / `kfd_sim_bridge_exit` / `kfd_sim_bridge_destroy`.

This requirement documents the post-deletion invariant: the only path through which the HAL reaches the bridge is the static binding established at gpu_driver plugin init time (commit `31dd5e1` sets `kfd_sim_bridge_init(hal)` from `plugin.cpp`); there is no longer a runtime setter.

#### Scenario: grep for setters/getters/backing state in kfd_sim_bridge module

- **WHEN** `grep -rn 'set_hal\|get_hal\|bridge_hal' plugins/gpu_driver/drv/kfd_sim_bridge.cpp plugins/gpu_driver/drv/kfd/kfd_sim_bridge.h` is run
- **THEN** output is empty (only `set_hal` reference removed in commit `a2701cd`; no replacement added)
- **AND** `grep -n 'kfd_sim_bridge_init\|kfd_sim_bridge_exit\|kfd_sim_bridge_destroy' plugins/gpu_driver/drv/kfd/kfd_sim_bridge.h` lists the remaining exported lifecycle symbols (proves the module still exists and has a real API surface, just not the deleted setters)

