# Spec: l2-portability-foundation (delta spec for stage4-l2-foundation-phase2-hal)

## Purpose

Extends the L2 portability foundation capability with ~28 new HAL fn-ptrs needed to migrate the remaining 8 B-class L2 violations across 5 headers (graph.h, mem_pool.h, stream_capture.h, gpu_queue_emu.h, hardware_puller_emu.h). After this change, drv/ can be migrated header-by-header via 5 dedicated removal changes.

## ADDED Requirements

### Requirement: HAL exposes graph lifecycle fn-ptrs

`struct gpu_hal_ops` SHALL expose 8 new fn-ptrs for sim graph lifecycle (per ADR-041, ADR-042): graph_create, graph_destroy, graph_add_kernel_node, graph_add_memcpy_node, graph_instantiate, graph_launch, graph_destroy_exec, and graph_launch_ex (or equivalent).

Inline wrappers in `gpu_hal.h` (zero-overhead call forwarding).

#### Scenario: drv/ creates a sim graph

Given `handleGraphCreate` in `gpgpu_device.cpp` needs to create a new sim graph

When it calls the graph creation function,

Then it uses `hal_graph_create(hal_)` (not `sim_graph_create()` directly).

And the file does NOT `#include "sim/graph.h"`.

### Requirement: HAL exposes mem_pool lifecycle fn-ptrs

`struct gpu_hal_ops` SHALL expose 9 new fn-ptrs for sim mem_pool lifecycle: mem_pool_create, mem_pool_destroy, mem_pool_alloc, mem_pool_alloc_async, mem_pool_free, mem_pool_free_async, mem_pool_set_attr, mem_pool_get_attr, mem_pool_trim.

Inline wrappers in `gpu_hal.h`.

#### Scenario: drv/ allocates from a sim mem_pool

Given `handleMemPoolAlloc` in `gpgpu_device.cpp` needs to allocate from a sim mem_pool

When it calls the allocation function,

Then it uses `hal_mem_pool_alloc(hal_, handle, size, &va_out)` (not `sim_mem_pool_alloc()` directly).

And the file does NOT `#include "sim/mem_pool.h"`.

### Requirement: HAL exposes stream_capture lifecycle fn-ptrs

`struct gpu_hal_ops` SHALL expose 3 new fn-ptrs for sim stream_capture: stream_capture_begin, stream_capture_end, stream_capture_status.

Inline wrappers in `gpu_hal.h`.

#### Scenario: drv/ begins a stream capture

Given `handleStreamCaptureBegin` in `gpgpu_device.cpp` needs to begin a sim stream capture

When it calls the begin function,

Then it uses `hal_stream_capture_begin(hal_, stream_id, mode)` (not `sim_stream_capture_begin()` directly).

And the file does NOT `#include "sim/stream_capture.h"`.

### Requirement: HAL exposes gpu_queue_emu operation fn-ptrs

`struct gpu_hal_ops` SHALL expose ~5 new fn-ptrs for GpuQueueEmu class operations: queue_create, queue_attach_shmem, queue_submit, and 2 more TBD per full audit of `GpuQueueEmu` methods.

Inline wrappers in `gpu_hal.h`.

#### Scenario: drv/ creates a sim queue

Given `handleCreateQueue` in `gpgpu_device.cpp` needs to create a new sim queue

When it calls the queue creation function,

Then it uses `hal_queue_create(hal_, ...)` (not `std::make_shared<GpuQueueEmu>()` directly).

And the file does NOT `#include "sim/gpu_queue_emu.h"`.

### Requirement: HAL exposes hardware_puller_emu operation fn-ptrs

`struct gpu_hal_ops` SHALL expose ~3 new fn-ptrs for HardwarePullerEmu class operations: set_puller, register_queue, and 1 more TBD per full audit of `HardwarePullerEmu` methods.

Inline wrappers in `gpu_hal.h`.

#### Scenario: drv/ sets the sim puller

Given `setPuller` in `gpgpu_device.cpp` needs to configure the sim puller

When it calls the set function,

Then it uses `hal_set_puller(hal_, sim_puller_handle)` (not `puller_ = ...` directly).

And the file does NOT `#include "sim/hardware/hardware_puller_emu.h"`.

## Notes

- This change adds ~28 new fn-ptrs to `struct gpu_hal_ops` (total: 19 → ~47)
- No existing fn-ptr modified (append-only per ADR-023 Decision 4)
- No drv/ changes in this change (separate removal changes will follow)
- This is Phase 2 of the B-class foundation; enables 5 removal changes to clear the remaining 8 L2 violations
- After all 5 removal changes ship: L2 violation count 8 → 0 (gate passes for the first time)
- The 5 removal changes are out of scope for this spec (each is its own change)
