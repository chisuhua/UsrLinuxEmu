# Spec: drv-sim-boundary (delta spec for stage4-l2-foundation-removal-graph)

## Purpose

将 ② drv 层的 graph 操作从直接 `sim_graph_*` 调用迁移到 `hal_graph_*` inline wrappers，并移除两个 drv 文件对 `sim/graph.h` 的直接依赖，从而消除 2 个 B-class L2 违规（9 → 7）。

## ADDED Requirements

### Requirement: drv/ uses HAL graph wrappers instead of sim graph functions

`plugins/gpu_driver/drv/gpgpu_device.cpp` and `plugins/gpu_driver/drv/gpu_drm_driver.cpp` SHALL invoke graph lifecycle and graph operation functions through the corresponding `hal_graph_*` inline wrappers instead of calling `sim_graph_*` functions directly.

#### Scenario: gpgpu_device graph operation follows the HAL path

Given `gpgpu_device.cpp` needs to create, update, instantiate, launch, or destroy a graph or graph executable

When it invokes the required graph operation

Then it calls the corresponding `hal_graph_*` inline wrapper with the existing HAL instance

And it does not call the corresponding `sim_graph_*` function directly

And the wrapper preserves the original argument order, return handling, and execution semantics.

#### Scenario: gpu_drm_driver graph operation follows the HAL path

Given `gpu_drm_driver.cpp` needs to perform a graph operation

When it invokes that graph operation

Then it calls the corresponding `hal_graph_*` inline wrapper with the existing HAL instance

And it does not call the corresponding `sim_graph_*` function directly.

### Requirement: drv/ graph consumers do not include the sim graph header

`plugins/gpu_driver/drv/gpgpu_device.cpp` and `plugins/gpu_driver/drv/gpu_drm_driver.cpp` SHALL NOT directly include `sim/graph.h`.

#### Scenario: static boundary check scans drv includes

Given the graph call sites in both in-scope drv files have been migrated to `hal_graph_*` wrappers

When the drv source tree is scanned for direct sim includes

Then neither `gpgpu_device.cpp` nor `gpu_drm_driver.cpp` contains `#include "sim/graph.h"`

And `grep -rn '#include.*"sim/' plugins/gpu_driver/drv/` returns exactly 7 lines instead of 9.

#### Scenario: sim graph behavior remains available behind HAL

Given `hal_user_init()` delegates the seven graph fn-ptrs to `sim_graph_*`

When drv graph behavior is exercised after removing the direct sim header includes

Then graph operations still reach the sim implementation through `hal_graph_*`

And `test_sim_graph_standalone` passes without changing the foundation implementation.
