/*
 * gpu_hal_handles.h — Opaque handles for HAL class types
 *
 * Per ADR-023 Decision 4: HAL interface is C-compatible, no C++ classes
 * in fn-ptr signatures. Class types like GpuQueueEmu/HardwarePullerEmu
 * are exposed as uint64_t handles. The drv/ side casts back when
 * subsequent removal changes land.
 *
 * Append-only per ADR-023 Decision 4.
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration of opaque HAL-side queue handle.
 * Maps to class GpuQueueEmu* in C++ (see hal_user.cpp cast sites).
 * drv/ removal changes will cast this back to shared_ptr<GpuQueueEmu>
 * via shared/hal_queue_handle.h (separate header, added in removal change). */
typedef uint64_t hal_queue_handle_t;

/* Forward declaration of opaque HAL-side hardware puller handle.
 * Maps to class HardwarePullerEmu* in C++ (see hal_user.cpp cast sites).
 * drv/ removal changes will cast back via shared/hal_puller_handle.h. */
typedef uint64_t hal_puller_handle_t;

#ifdef __cplusplus
}
#endif