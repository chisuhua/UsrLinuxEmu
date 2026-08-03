# Spec: drv-sim-boundary (delta spec for stage4-l2-foundation-removal-fence-id)

## Purpose

Removes 2 of 12 B-class L2 violations by migrating drv/ from direct `sim_fence_id_*()` calls to HAL inline wrapper calls (`hal_fence_id_alloc`, `hal_fence_id_check`).

## ADDED Requirements

### Requirement: drv/ uses HAL inline wrappers for fence_id lifecycle

`plugins/gpu_driver/drv/gpgpu_device.cpp` and `plugins/gpu_driver/drv/gpu_drm_driver.cpp` SHALL call fence_id lifecycle functions via the HAL inline wrappers (`hal_fence_id_alloc`, `hal_fence_id_check`) instead of the sim-layer functions directly.

#### Scenario: drv/ requests a new sim fence_id in pushbuffer submit

Given `handlePushbufferSubmitBatch` in `gpgpu_device.cpp` needs a new sim-layer fence_id

When it calls the fence_id lifecycle function,

Then it uses `hal_fence_id_alloc(hal_)` (not `sim_fence_id_alloc()` directly).

And the file does NOT `#include "sim/fence_id.h"`.

#### Scenario: drv/ checks sim fence_id state in wait fence

Given `handleWaitFence` in `gpgpu_device.cpp` or DRM fence wait in `gpu_drm_driver.cpp` needs to check sim fence state

When it calls the fence_id check function,

Then it uses `hal_fence_id_check(hal_, fence_id, &signaled)` (not `sim_fence_id_check(fence_id, &signaled)` directly).

And the file does NOT `#include "sim/fence_id.h"`.

## Notes

- `sim_fence_id_signal()` is NOT called by drv/ (only by sim layer internals); no migration needed
- `sim/fence_id.h` still exists and is still included by `hal_user.cpp` (to implement the fn-ptrs); the change is only about drv/ consumption
- This change is the first of 3 Phase-1 removal changes (fence-id → method-codec → hal-user)
