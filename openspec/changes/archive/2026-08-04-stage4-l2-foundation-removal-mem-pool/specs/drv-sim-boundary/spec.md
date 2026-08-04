# Spec: drv-sim-boundary (delta spec for stage4-l2-foundation-removal-mem-pool)

## Purpose

将 ② drv 层的 mem_pool 操作从直接 `sim_mem_pool_*` 调用迁移到 `hal_mem_pool_*` inline wrappers，并移除两个 drv 文件对 `sim/mem_pool.h` 的直接依赖，从而消除 2 个 B-class L2 违规（7 → 5）。本 change 处理 27 处 call site（call site 最多的一组）以及 `mem_pool_set_attr` / `mem_pool_get_attr` 的 `void* + size` 签名差异。

## ADDED Requirements

### Requirement: drv/ uses HAL mem_pool wrappers instead of sim mem_pool functions

`plugins/gpu_driver/drv/gpgpu_device.cpp` and `plugins/gpu_driver/drv/gpu_drm_driver.cpp` SHALL invoke mem_pool lifecycle and mem_pool operation functions through the corresponding `hal_mem_pool_*` inline wrappers instead of calling `sim_mem_pool_*` functions directly. All 27 call sites SHALL be migrated.

#### Scenario: gpgpu_device mem_pool operation follows the HAL path

Given `gpgpu_device.cpp` needs to create, destroy, allocate, free, or query attributes on a mem_pool

When it invokes the required mem_pool operation

Then it calls the corresponding `hal_mem_pool_*` inline wrapper with the existing HAL instance

And it does not call the corresponding `sim_mem_pool_*` function directly

And the wrapper preserves the original argument order, return handling, and execution semantics.

#### Scenario: gpu_drm_driver mem_pool operation follows the HAL path

Given `gpu_drm_driver.cpp` needs to perform a mem_pool operation

When it invokes that mem_pool operation

Then it calls the corresponding `hal_mem_pool_*` inline wrapper with the existing HAL instance

And it does not call the corresponding `sim_mem_pool_*` function directly.

### Requirement: drv/ mem_pool set_attr and get_attr use the typed buffer signature

For `mem_pool_set_attr` and `mem_pool_get_attr` invocations originating in `plugins/gpu_driver/drv/`, the caller SHALL provide a typed buffer plus its byte size as required by the HAL wrapper signature. Calls SHALL NOT pass a raw `uint64_t` value where the wrapper expects `const void* value, uint64_t value_size`.

#### Scenario: set_attr migration uses typed buffer + size

Given `gpgpu_device.cpp` or `gpu_drm_driver.cpp` needs to set a mem_pool attribute

When it invokes `hal_mem_pool_set_attr`

Then it passes a pointer to the typed value plus `sizeof(*value)` (or the explicit byte size) as the buffer argument

And the call type-checks at compile time against the wrapper signature.

### Requirement: drv/ mem_pool consumers do not include the sim mem_pool header

`plugins/gpu_driver/drv/gpgpu_device.cpp` and `plugins/gpu_driver/drv/gpu_drm_driver.cpp` SHALL NOT directly include `sim/mem_pool.h`.

#### Scenario: static boundary check scans drv includes

Given the 27 mem_pool call sites in both in-scope drv files have been migrated to `hal_mem_pool_*` wrappers

When the drv source tree is scanned for direct sim includes

Then neither `gpgpu_device.cpp` nor `gpu_drm_driver.cpp` contains `#include "sim/mem_pool.h"`

And `grep -rn '#include.*"sim/' plugins/gpu_driver/drv/` returns exactly 5 lines instead of 7.

#### Scenario: sim mem_pool behavior remains available behind HAL

Given `hal_user_init()` delegates the nine mem_pool fn-ptrs to `sim_mem_pool_*` (with `mem_pool_free` and the async variants as foundation-stage stubs)

When drv mem_pool behavior is exercised after removing the direct sim header includes

Then mem_pool operations still reach the sim implementation through `hal_mem_pool_*`

And existing mem_pool tests pass without changing the foundation implementation, with stub fn-ptrs returning 0 (no-op) as expected by drv callers.
