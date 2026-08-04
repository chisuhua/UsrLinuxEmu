# Spec: drv-sim-boundary (delta spec for stage4-l2-foundation-removal-gpu-queue-emu)

## Purpose

将 ② drv 层对 `GpuQueueEmu` C++ class 的直接依赖迁移到 `hal_queue_*` inline wrappers 与 `hal_queue_handle_t` opaque handle，并移除 `plugins/gpu_driver/drv/gpgpu_device.cpp` 对 `sim/gpu_queue_emu.h` 的直接依赖，从而消除 1 个 B-class L2 违规（3 → 2）。本 change 处理 class 类型抽象（最复杂的 removal 之一），并升级 `hal_user.cpp` 中 `queue_*` lambdas 从 stub 到真实实例管理。

## ADDED Requirements

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
