# Spec: drv-sim-boundary (delta spec for stage4-l2-foundation-removal-method-codec)

## Purpose

Removes 1 of 12 B-class L2 violations by migrating drv/ from direct `method_codec_encode()` call to HAL inline wrapper call (`hal_method_codec_encode`).

## ADDED Requirements

### Requirement: drv/ uses HAL inline wrapper for method_codec_encode

`plugins/gpu_driver/drv/gpgpu_device.cpp` SHALL call `method_codec_encode` via the HAL inline wrapper (`hal_method_codec_encode`) instead of the sim-layer function directly.

#### Scenario: drv/ validates method codec encoding

Given `handleSubmitGraph` in `gpgpu_device.cpp` needs to validate that `method_codec_encode` works correctly for each GPFIFO entry

When it calls the encode function,

Then it uses `hal_method_codec_encode(hal_, pkt, nullptr)` (not `method_codec_encode(pkt, nullptr)` directly).

And the file does NOT `#include "sim/hardware/method_codec.h"`.

## Notes

- `sim/hardware/method_codec.h` still exists and is still included by `hal_user.cpp` (to implement the fn-ptr); the change is only about drv/ consumption
- This change is removal #2 of 3 Phase-1 removal changes (fence-id → method-codec → hal-user)
