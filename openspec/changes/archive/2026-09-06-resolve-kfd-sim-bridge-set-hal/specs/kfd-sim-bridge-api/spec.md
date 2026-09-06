## ADDED Requirements

### Requirement: kfd_sim_bridge API does not expose setters or getters for the HAL

The `kfd_sim_bridge` module (`plugins/gpu_driver/drv/kfd_sim_bridge.cpp` + `plugins/gpu_driver/drv/kfd/kfd_sim_bridge.h`) SHALL NOT export any function whose name contains `set_hal`, `get_hal`, or any backing static variable whose name contains `bridge_hal`. The module's only exported symbols are the translation functions (`kfd_sim_bridge_*`) plus `kfd_sim_bridge_init` / `kfd_sim_bridge_exit` / `kfd_sim_bridge_destroy`.

This requirement documents the post-deletion invariant: the only path through which the HAL reaches the bridge is the static binding established at gpu_driver plugin init time (commit `31dd5e1` sets `kfd_sim_bridge_init(hal)` from `plugin.cpp`); there is no longer a runtime setter.

#### Scenario: grep for setters/getters/backing state in kfd_sim_bridge module

- **WHEN** `grep -rn 'set_hal\|get_hal\|bridge_hal' plugins/gpu_driver/drv/kfd_sim_bridge.cpp plugins/gpu_driver/drv/kfd/kfd_sim_bridge.h` is run
- **THEN** output is empty (only `set_hal` reference removed in commit `a2701cd`; no replacement added)
- **AND** `grep -n 'kfd_sim_bridge_init\|kfd_sim_bridge_exit\|kfd_sim_bridge_destroy' plugins/gpu_driver/drv/kfd/kfd_sim_bridge.h` lists the remaining exported lifecycle symbols (proves the module still exists and has a real API surface, just not the deleted setters)

## REMOVED Requirements

### Requirement: kfd_sim_bridge_set_hal() function

The function `kfd_sim_bridge_set_hal()` (formerly at `plugins/gpu_driver/drv/kfd_sim_bridge.cpp:261-263`) and its declaration at `plugins/gpu_driver/drv/kfd/kfd_sim_bridge.h:25` SHALL be removed. The sole call site (a self-referential test) is also removed.

#### Scenario: kfd_sim_bridge_set_hal no longer exists

- **WHEN** the gpu_driver plugin and its tests are built
- **THEN** `git grep -rn kfd_sim_bridge_set_hal plugins/ tests/ src/ tools/ openspec/changes/` returns zero hits (the only remaining reference is in this change's own proposal.md at the "References" section)
- **AND** `ctest` still PASSES (was 157/157; after this change the test_kfd_sim_bridge_audit_standalone binary loses one TEST_CASE but retains TEST_CASE 2 and 3, and the overall ctest count is unchanged because we removed a TEST_CASE not a binary)

### Requirement: kfd_sim_bridge_get_hal() function

The function `kfd_sim_bridge_get_hal()` (formerly at `kfd_sim_bridge.cpp:265-267`, header marked `test-only`) and its declaration at `kfd_sim_bridge.h:28` SHALL be removed. It was test-only and called only by the same TEST_CASE being removed.

#### Scenario: kfd_sim_bridge_get_hal no longer exists

- **WHEN** the gpu_driver plugin and its tests are built
- **THEN** `git grep -rn kfd_sim_bridge_get_hal plugins/ tests/ src/ tools/ openspec/changes/` returns zero hits

### Requirement: g_bridge_hal_ static storage

The static variable `g_bridge_hal_` (formerly at `kfd_sim_bridge.cpp:259`) SHALL be removed. It was the sole backing state for `kfd_sim_bridge_get_hal()`.

#### Scenario: g_bridge_hal_ no longer exists

- **WHEN** `git grep -rn g_bridge_hal_ plugins/gpu_driver/drv/kfd_sim_bridge.cpp plugins/gpu_driver/drv/kfd/kfd_sim_bridge.h` is run
- **THEN** output is empty

### Requirement: self-referential set_hal test case

The TEST_CASE "kfd_sim_bridge_set_hal registers pointer" at `tests/test_kfd_sim_bridge_audit_standalone.cpp:52-69` and its `dummy_iommu_map` / `dummy_iommu_unmap` helpers at lines 40-48 SHALL be removed. The remaining TEST_CASE 2 and TEST_CASE 3 of the audit binary cover sim_pm_* legacy markers and LEGACY/CLEAN marker comments — distinct concerns.

#### Scenario: audit binary no longer references set_hal

- **WHEN** `grep -c 'set_hal\|get_hal' tests/test_kfd_sim_bridge_audit_standalone.cpp` is run
- **THEN** output is 0
- **AND** `cmake --build build --target test_kfd_sim_bridge_audit_standalone` builds clean