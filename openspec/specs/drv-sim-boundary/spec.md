# drv-sim-boundary Specification

## Purpose
TBD - created by archiving change stage4-l2-foundation-removal-gpu-queue-emu. Update Purpose after archive.
## Requirements
### Requirement: drv/ holds hal_queue_handle_t instead of GpuQueueEmu class reference

`plugins/gpu_driver/drv/gpgpu_device.cpp` SHALL hold queue state through the opaque `hal_queue_handle_t` handle type instead of `std::shared_ptr<GpuQueueEmu>`. drv SHALL NOT directly include `sim/gpu_queue_emu.h` or reference the `GpuQueueEmu` class type in any form (no forward declarations, no type aliases).

#### Scenario: gpgpu_device uses opaque queue handle

Given `gpgpu_device.cpp` needs to track a created GPU queue

When the queue is created via `hal_queue_create`

Then drv stores the returned `hal_queue_handle_t` as its queue state

And drv does not hold a `std::shared_ptr<GpuQueueEmu>` or any direct reference to the `GpuQueueEmu` class.

### Requirement: drv/ invokes GpuQueueEmu behavior through HAL queue wrappers

`plugins/gpu_driver/drv/gpgpu_device.cpp` SHALL invoke every queue operation through the corresponding `hal_queue_*` inline wrappers (`hal_queue_attach_shmem`, `hal_queue_submit`, `hal_queue_register_puller`, `hal_queue_destroy`, plus any read-only accessor wrappers). drv SHALL NOT call any `GpuQueueEmu` class method directly.

#### Scenario: gpgpu_device queue operations follow the HAL path

Given `gpgpu_device.cpp` needs to attach shared memory, submit, register a puller, or destroy a queue

When it invokes the required queue operation

Then it calls the corresponding `hal_queue_*` inline wrapper with the existing HAL instance and the queue's `hal_queue_handle_t`

And it does not invoke the corresponding `GpuQueueEmu` class method directly.

### Requirement: hal_user owns GpuQueueEmu instances behind opaque handles

`plugins/gpu_driver/hal/hal_user.cpp` SHALL maintain GpuQueueEmu instances internally (e.g., via `std::unordered_map<hal_queue_handle_t, std::shared_ptr<GpuQueueEmu>>` or a vector with monotonic handle assignment). The `queue_create` / `queue_attach_shmem` / `queue_submit` / `queue_destroy` lambdas SHALL allocate, look up, and release real `GpuQueueEmu` instances via the handle, not return stubs. `queue_register_puller` MAY remain stub in this change pending `removal-hardware-puller-emu`.

#### Scenario: hal_user queue_create allocates a real GpuQueueEmu instance

Given drv calls `hal_queue_create`

When the `queue_create` lambda runs

Then it constructs a real `GpuQueueEmu` instance, stores it in the `hal_user_context` instance map

And it returns a fresh opaque `hal_queue_handle_t` for drv to use.

#### Scenario: hal_user queue_attach_shmem looks up the instance behind the handle

Given drv calls `hal_queue_attach_shmem` with a `hal_queue_handle_t`

When the lambda runs

Then it finds the corresponding `GpuQueueEmu` instance in the `hal_user_context` map

And it invokes the underlying `attachSharedMemory` method on that instance.

#### Scenario: hal_user queue_destroy releases the instance

Given drv calls `hal_queue_destroy` with a `hal_queue_handle_t`

When the lambda runs

Then it removes the corresponding `GpuQueueEmu` instance from the `hal_user_context` map

And the handle becomes invalid for subsequent operations.

### Requirement: drv/ queue consumers do not include the sim gpu_queue_emu header

`plugins/gpu_driver/drv/gpgpu_device.cpp` SHALL NOT directly include `sim/gpu_queue_emu.h`.

#### Scenario: static boundary check scans drv includes

Given the GpuQueueEmu class usages in `gpgpu_device.cpp` have been migrated to `hal_queue_*` wrappers and `hal_queue_handle_t`

When the drv source tree is scanned for direct sim includes

Then `gpgpu_device.cpp` does not contain `#include "sim/gpu_queue_emu.h"`

And `grep -rn '#include.*"sim/' plugins/gpu_driver/drv/` returns exactly 2 lines instead of 3.

#### Scenario: sim queue behavior remains available behind HAL

Given `hal_user_init()` delegates the five queue fn-ptrs to real `GpuQueueEmu` instance management

When drv queue behavior is exercised after removing the direct sim header include

Then queue operations still reach the sim implementation through `hal_queue_*` and `hal_queue_handle_t`

And existing queue tests pass without changing the `sim/gpu_queue_emu` implementation files.

### Requirement: queue register_puller stub remains pending hardware-puller-emu

The `queue_register_puller` lambda in `hal_user_init()` MAY remain a stub until `stage4-l2-foundation-removal-hardware-puller-emu` ships, because the corresponding `hal_puller_handle_t` side is introduced in that change. drv SHALL pass a `hal_puller_handle_t` opaque value through `hal_queue_register_puller` regardless of stub or real implementation.

#### Scenario: queue register_puller stub accepts opaque puller handle

Given drv calls `hal_queue_register_puller` with a `hal_puller_handle_t` value before hardware-puller-emu change ships

When the stub lambda runs

Then it records the opaque puller handle for later wire-up

And the call returns success without dereferencing the handle.

### Requirement: drv/ holds hal_puller_handle_t instead of HardwarePullerEmu class reference

`plugins/gpu_driver/drv/gpgpu_device.cpp` SHALL hold puller state through the opaque `hal_puller_handle_t` handle type instead of `std::shared_ptr<HardwarePullerEmu>`. drv SHALL NOT directly include `sim/hardware/hardware_puller_emu.h` or reference the `HardwarePullerEmu` class type in any form (no forward declarations, no type aliases).

#### Scenario: gpgpu_device uses opaque puller handle

Given `gpgpu_device.cpp` needs to track a created hardware puller

When the puller is created via `hal_puller_create`

Then drv stores the returned `hal_puller_handle_t` as its puller state

And drv does not hold a `std::shared_ptr<HardwarePullerEmu>` or any direct reference to the `HardwarePullerEmu` class.

### Requirement: drv/ invokes HardwarePullerEmu behavior through HAL puller wrappers

`plugins/gpu_driver/drv/gpgpu_device.cpp` SHALL invoke every puller operation through the corresponding `hal_puller_*` inline wrappers (`hal_puller_set_puller`, `hal_puller_register_queue`, `hal_puller_unregister_queue`, plus a `hal_puller_submit_batch` wrapper if foundation does not cover submitBatch). drv SHALL NOT call any `HardwarePullerEmu` class method directly.

#### Scenario: gpgpu_device puller operations follow the HAL path

Given `gpgpu_device.cpp` needs to set, register/unregister queues with, or submit batches through a puller

When it invokes the required puller operation

Then it calls the corresponding `hal_puller_*` inline wrapper with the existing HAL instance and the puller's `hal_puller_handle_t`

And it does not invoke the corresponding `HardwarePullerEmu` class method directly.

#### Scenario: submitBatch semantic gap is handled in scope or recorded as follow-up

Given drv calls `HardwarePullerEmu::submitBatch` and foundation's 3 puller fn-ptrs may not cover it

When the implementation runs

Then either a new `hal_puller_submit_batch` fn-ptr is added within this change scope (append-only, total fn-ptr count 46 → 47)

Or the gap is recorded as a separate follow-up change with explicit proposal.

### Requirement: hal_user owns HardwarePullerEmu instances behind opaque handles

`plugins/gpu_driver/hal/hal_user.cpp` SHALL maintain HardwarePullerEmu instances internally (e.g., via `std::unordered_map<hal_puller_handle_t, std::shared_ptr<HardwarePullerEmu>>`). The `puller_create` lambda SHALL construct a real `HardwarePullerEmu` instance passing `struct gpu_hal_ops* hal`, and the `puller_set_puller` / `puller_register_queue` / `puller_unregister_queue` lambdas (and any new submitBatch lambda) SHALL look up instances via the handle and invoke the corresponding method.

#### Scenario: hal_user puller_create allocates a real HardwarePullerEmu instance

Given drv calls `hal_puller_create` with HAL pointer

When the `puller_create` lambda runs

Then it constructs a real `HardwarePullerEmu` instance with the supplied HAL pointer, stores it in the `hal_user_context` instance map

And it returns a fresh opaque `hal_puller_handle_t` for drv to use.

#### Scenario: hal_user puller_register_queue looks up the instance behind the handle

Given drv calls `hal_puller_register_queue` with a `hal_puller_handle_t` and a `hal_queue_handle_t`

When the lambda runs

Then it finds the corresponding `HardwarePullerEmu` instance in the `hal_user_context` map

And it invokes the underlying `registerQueue` method on that instance with the queue handle.

### Requirement: drv/ puller consumers do not include the sim hardware_puller_emu header

`plugins/gpu_driver/drv/gpgpu_device.cpp` SHALL NOT directly include `sim/hardware/hardware_puller_emu.h`.

#### Scenario: static boundary check scans drv includes

Given the HardwarePullerEmu class usages in `gpgpu_device.cpp` have been migrated to `hal_puller_*` wrappers and `hal_puller_handle_t`

When the drv source tree is scanned for direct sim includes

Then `gpgpu_device.cpp` does not contain `#include "sim/hardware/hardware_puller_emu.h"`

And `grep -rn '#include.*"sim/' plugins/gpu_driver/drv/` returns exactly 1 line (only `sim/sim_event.h` in kfd_events.c, explicitly out of Phase 2 scope).

#### Scenario: sim puller behavior remains available behind HAL

Given `hal_user_init()` delegates the three puller fn-ptrs to real `HardwarePullerEmu` instance management

When drv puller behavior is exercised after removing the direct sim header include

Then puller operations still reach the sim implementation through `hal_puller_*` and `hal_puller_handle_t`

And existing puller tests pass without changing the `sim/hardware/hardware_puller_emu` implementation files.

### Requirement: queue↔puller wire-up requires gpu-queue-emu removal to ship first

The full queue↔puller bidirectional wire-up (queue side uses puller handle; puller side uses queue handle) requires `stage4-l2-foundation-removal-gpu-queue-emu` to ship before this change can complete real cross-handle operations end-to-end. Until then, each side SHALL hold opaque handles and pass them through HAL wrappers without dereferencing.

#### Scenario: queue-register-puller and puller-register-queue use opaque handles

Given drv calls `hal_queue_register_puller` and `hal_puller_register_queue` after this change ships

When both lambdas run

Then they exchange opaque `hal_queue_handle_t` / `hal_puller_handle_t` values

And they look up the corresponding real instances in `hal_user_context` only when both sides have shipped their respective removal changes.

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

