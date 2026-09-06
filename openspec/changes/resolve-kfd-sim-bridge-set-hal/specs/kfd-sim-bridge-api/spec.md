## REMOVED Requirements

### Requirement: kfd_sim_bridge_set_hal() function

The function `kfd_sim_bridge_set_hal()` defined at `plugins/gpu_driver/drv/kfd_sim_bridge.cpp:261` and declared at `plugins/gpu_driver/drv/kfd/kfd_sim_bridge.h:25` SHALL be removed. This function has zero call sites in the repository (verified by `git grep`); the gpu_driver composition root bypasses it by passing `gpu_hal_ops*` directly to `GpgpuDevice`.

#### Scenario: kfd_sim_bridge_set_hal no longer exists

- **WHEN** the gpu_driver plugin is built and linked
- **THEN** `git grep -rn kfd_sim_bridge_set_hal` returns zero hits (excluding the change's own `openspec/changes/` directory)
- **AND** `ctest` still PASSES 157/157 (no test referenced the deleted function)
- **AND** `kfd_sim_bridge.h` compiles cleanly without the declaration