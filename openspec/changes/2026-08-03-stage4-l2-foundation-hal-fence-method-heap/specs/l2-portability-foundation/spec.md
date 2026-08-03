# Spec: l2-portability-foundation (delta spec for stage4-l2-foundation-hal-fence-method-heap)

## Purpose

Defines the HAL fn-ptr extension pattern that enables `plugins/gpu_driver/drv/` to call sim-layer functions without including sim/ headers, per ADR-023 Decision 4 spec-driven "append-only" rule.

## ADDED Requirements

### Requirement: HAL exposes fence_id allocation/management via fn-ptrs

`struct gpu_hal_ops` SHALL expose 3 new fn-ptrs for sim fence_id lifecycle:
- `int64_t (*fence_id_alloc)(void *ctx)` — allocate next sim-layer fence_id (range `[SIM_FENCE_ID_BASE, SIM_FENCE_ID_MAX]`)
- `void (*fence_id_signal)(void *ctx, uint64_t fence_id)` — mark fence_id as triggered
- `int (*fence_id_check)(void *ctx, uint64_t fence_id, bool *signaled)` — query triggered state

Inline wrappers in `gpu_hal.h`:
- `hal_fence_id_alloc(hal)` — forward `hal->fence_id_alloc(hal->ctx)`
- `hal_fence_id_signal(hal, fence_id)` — forward `hal->fence_id_signal(hal->ctx, fence_id)`
- `hal_fence_id_check(hal, fence_id, signaled)` — forward `hal->fence_id_check(hal->ctx, fence_id, signaled)`

#### Scenario: drv/ requests new sim fence_id

Given `plugins/gpu_driver/drv/gpgpu_device.cpp` currently calls `sim_fence_id_alloc()` directly (requires `#include "sim/fence_id.h"`)

When this change is applied and the follow-up removal change migrates drv/ to use `hal_fence_id_alloc(hal_)`,

Then drv/ no longer needs to `#include "sim/fence_id.h"` (per ADR-072 §Decision 4 removal pattern).

#### Scenario: HAL production impl delegates to sim

Given `hal_user_init()` (production path) sets `hal->fence_id_alloc = lambda { return sim_fence_id_alloc(); }`

When the lambda is called via the inline wrapper,

Then it returns the same value as calling `sim_fence_id_alloc()` directly (no behavior change).

#### Scenario: HAL mock impl returns deterministic counter

Given `hal_mock_init()` (test path) sets `hal->fence_id_alloc = lambda { static std::atomic<uint64_t> next{0x1000}; return ++next; }`

When tests call `hal_fence_id_alloc(mock_hal)` repeatedly,

Then each call returns a unique monotonic counter starting at 0x1001 (deterministic for test reproducibility).

### Requirement: HAL exposes method_codec_encode via fn-ptr

`struct gpu_hal_ops` SHALL expose:
- `int (*method_codec_encode)(void *ctx, /* TODO: confirm exact signature in impl */)` — encode GPFIFO entry payload

Inline wrapper in `gpu_hal.h`:
- `hal_method_codec_encode(hal /*, ...args... */)` — forward to fn-ptr

#### Scenario: drv/ encodes method payload

Given `plugins/gpu_driver/drv/gpgpu_device.cpp` currently calls `method_codec_encode()` directly (requires `#include "sim/hardware/method_codec.h"`)

When this change is applied and the follow-up removal change migrates drv/ to use `hal_method_codec_encode(hal_)`,

Then drv/ no longer needs to `#include "sim/hardware/method_codec.h"`.

### Requirement: HAL exposes heap pointer accessor via fn-ptr

`struct gpu_hal_ops` SHALL expose:
- `void* (*heap_ptr)(void *ctx, uint64_t gpu_va)` — return host pointer for a given GPU VA (handles HAL_HEAP_BASE offset)

Inline wrapper in `gpu_hal.h`:
- `hal_heap_ptr(hal, gpu_va)` — forward `hal->heap_ptr(hal->ctx, gpu_va)`

#### Scenario: drv/ maps GPU VA to host pointer

Given `plugins/gpu_driver/drv/gpgpu_device.cpp` currently does `hc->heap + (gpu_va - HAL_HEAP_BASE)` (requires `#include "hal/hal_user.h"` for `hc->heap` field access)

When this change is applied and the follow-up removal change migrates drv/ to use `hal_heap_ptr(hal_, gpu_va)`,

Then drv/ no longer needs to `#include "hal/hal_user.h"` (no `hc->heap` field access required).

#### Scenario: HAL production impl returns heap-relative pointer

Given `hal_user_init()` sets `hal->heap_ptr = lambda { auto* hc = static_cast<hal_user_context*>(ctx); return hc->heap + (gpu_va - HAL_HEAP_BASE); }`

When called via `hal_heap_ptr(hal, gpu_va)`,

Then it returns the same pointer as the direct `hc->heap + (gpu_va - HAL_HEAP_BASE)` expression.

## Modified Requirements

None (append-only per ADR-023 Decision 4 — existing 14 fn-ptrs unchanged).

## Notes

- This change adds 5 fn-ptrs to `struct gpu_hal_ops` (total: 14 → 19)
- No existing fn-ptr modified (append-only)
- No drv/ code changed in this change (that's follow-up removal changes)
- The 3 follow-up removal changes (fence_id, method_codec, hal_user) are out of scope and will be separate changes
- After all 3 removals ship: L2 violation count 12 → 9 (remaining: 6 B-class headers for Phase 2 foundation)
