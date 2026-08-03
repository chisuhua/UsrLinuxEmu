# Spec: drv-sim-boundary (delta spec for stage4-l2-foundation-removal-hal-user)

## Purpose

Removes the last of 3 Phase-1 B-class L2 violations by migrating drv/ from direct `hc->heap` field access to HAL inline wrapper call (`hal_heap_ptr`).

## ADDED Requirements

### Requirement: drv/ uses HAL inline wrapper for heap pointer access

`plugins/gpu_driver/drv/gpgpu_device.cpp` SHALL access the HAL user-context heap pointer via the HAL inline wrapper (`hal_heap_ptr`) instead of the direct field access pattern (`hc->heap + (gpu_va - HAL_HEAP_BASE)`).

#### Scenario: drv/ maps GPU VA to host pointer in handleAllocBo

Given `handleAllocBo` in `gpgpu_device.cpp` needs to map a GPU VA to a host pointer for BO allocation

When it computes the host pointer,

Then it uses `hal_heap_ptr(hal_, gpu_va)` (not `hc->heap + (gpu_va - HAL_HEAP_BASE)` directly).

And the file does NOT `#include "hal/hal_user.h"`.

## Notes

- `hal/hal_user.h` still exists and is still included by `hal_user.cpp` (to implement the `heap_ptr` fn-ptr); the change is only about drv/ consumption
- This change is removal #3 of 3 Phase-1 removal changes (fence-id → method-codec → hal-user)
- After this change: 8 of 12 original L2 violations remain (Phase 2 + final removals)
- `HAL_HEAP_BASE` constant is encapsulated inside `hal_heap_ptr` (per the foundation change); drv/ no longer needs to know about it
