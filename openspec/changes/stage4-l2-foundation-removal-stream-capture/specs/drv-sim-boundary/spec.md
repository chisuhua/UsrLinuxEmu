# Spec: drv-sim-boundary (delta spec for stage4-l2-foundation-removal-stream-capture)

## Purpose

将 ② drv 层的 stream_capture 操作从直接 `sim_stream_capture_*` 调用迁移到 `hal_stream_capture_*` inline wrappers，并移除两个 drv 文件对 `sim/stream_capture.h` 的直接依赖，从而消除 2 个 B-class L2 违规（5 → 3）。本 change 处理 8 处 call site（最低风险的 removal，纯 C 函数）。

## ADDED Requirements

### Requirement: drv/ uses HAL stream_capture wrappers instead of sim stream_capture functions

`plugins/gpu_driver/drv/gpgpu_device.cpp` and `plugins/gpu_driver/drv/gpu_drm_driver.cpp` SHALL invoke stream_capture lifecycle and status functions through the corresponding `hal_stream_capture_*` inline wrappers instead of calling `sim_stream_capture_*` functions directly. All 8 call sites SHALL be migrated.

#### Scenario: gpgpu_device stream_capture operation follows the HAL path

Given `gpgpu_device.cpp` needs to begin, end, or query status of a stream capture

When it invokes the required stream_capture operation

Then it calls the corresponding `hal_stream_capture_*` inline wrapper with the existing HAL instance

And it does not call the corresponding `sim_stream_capture_*` function directly

And the wrapper preserves the original argument order, return handling, and execution semantics.

#### Scenario: gpu_drm_driver stream_capture operation follows the HAL path

Given `gpu_drm_driver.cpp` needs to perform a stream_capture operation

When it invokes that stream_capture operation

Then it calls the corresponding `hal_stream_capture_*` inline wrapper with the existing HAL instance

And it does not call the corresponding `sim_stream_capture_*` function directly.

### Requirement: drv/ stream_capture consumers do not include the sim stream_capture header

`plugins/gpu_driver/drv/gpgpu_device.cpp` and `plugins/gpu_driver/drv/gpu_drm_driver.cpp` SHALL NOT directly include `sim/stream_capture.h`.

#### Scenario: static boundary check scans drv includes

Given the 8 stream_capture call sites in both in-scope drv files have been migrated to `hal_stream_capture_*` wrappers

When the drv source tree is scanned for direct sim includes

Then neither `gpgpu_device.cpp` nor `gpu_drm_driver.cpp` contains `#include "sim/stream_capture.h"`

And `grep -rn '#include.*"sim/' plugins/gpu_driver/drv/` returns exactly 3 lines instead of 5.

#### Scenario: sim stream_capture behavior remains available behind HAL

Given `hal_user_init()` delegates the three stream_capture fn-ptrs to `sim_stream_capture_*`

When drv stream_capture behavior is exercised after removing the direct sim header includes

Then stream_capture operations still reach the sim implementation through `hal_stream_capture_*`

And existing stream_capture tests pass without changing the foundation implementation.

### Requirement: drv/ stream_capture status uses uint32 layout-compatible pass-through

For `sim_stream_capture_status` invocations originating in `plugins/gpu_driver/drv/`, the drv caller SHALL pass a `uint32_t*` aligned with the `sim_stream_capture_status_t` layout (single `uint32_t` field), preserving the foundation pass-through semantics after migration to `hal_stream_capture_status`.

#### Scenario: stream_capture status pass-through preserves layout compatibility

Given a drv caller needs to query stream capture status

When it invokes `hal_stream_capture_status`

Then the status output argument is a `uint32_t*` aligned with the `sim_stream_capture_status_t` layout

And the value reflects the underlying sim status without reinterpretation at the drv call site.
