# Spec: drv-sim-boundary (delta spec for stage4-l2-foundation-removal-hardware-puller-emu)

## Purpose

将 ② drv 层对 `HardwarePullerEmu` C++ class 的直接依赖迁移到 `hal_puller_*` inline wrappers 与 `hal_puller_handle_t` opaque handle，并移除 `plugins/gpu_driver/drv/gpgpu_device.cpp` 对 `sim/hardware/hardware_puller_emu.h` 的直接依赖，从而清除 1 个 B-class L2 违规（2 → 1），并使 Phase 2 5 个 removal 全部完成后 L2 违规从 8 降至 0。本 change 处理 class 类型抽象（与 gpu-queue-emu 同样复杂），并升级 `hal_user.cpp` 中 `puller_*` lambdas 从 stub 到真实实例管理。

## ADDED Requirements

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
