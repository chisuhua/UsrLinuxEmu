/*
 * kfd_dispatch.h — KFD ioctl dispatch table scaffolding (C-12 B.2.1)
 *
 * Provides a lightweight ioctl routing layer that maps KFD_IOC_* command
 * IDs (0x40-0x47) to registered handler function pointers.  Designed as
 * pure scaffolding: kfd_module_init() registers handlers after B.2.1;
 * kfd_dispatch() routes ioctls without changing the existing
 * drm_ioctl_desc[] table (per B.2.3 constraint).
 *
 * Thread-safety: handlers_ is write-once at init, read-only afterward.
 * call_count_ uses atomic increment on each successful dispatch.
 *
 * Migration to real kernel:
 *   1. Delete kfd_dispatch.h and kfd_dispatch.c
 *   2. Replace with `<drm/drm_ioctl.h>` DRM_IOCTL_DEF_DRV() macro
 *   3. Handlers become `drm_ioctl_t` function pointers in a standard
 *      `struct drm_ioctl_desc[]` table
 */
#pragma once

#include "kfd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * KFD_IOC_COUNT — number of active KFD ioctl commands
 *
 * MUST be consistent with the KFD_IOC_* enum below.  Currently 5 commands
 * (CREATE_QUEUE through UPDATE_QUEUE) are tracked.  MAP_MEMORY, UNMAP_MEMORY,
 * and SET_TRAP_HANDLER are reserved for future phases.
 */
#define KFD_IOC_COUNT 5

/*
 * kfd_ioctl_id — KFD ioctl command identifiers
 *
 * Mirror the Linux amdkfd ioctl numbering.  In the real Linux kernel,
 * these come from the DRM command encoding macro in uapi/kfd_ioctl.h.
 * Here we use raw values for simplicity; the mapping is 1:1 with the
 * GPU_IOCTL_* macros in gpu_ioctl.h (0x40-0x47 range).
 */
enum kfd_ioctl_id {
    KFD_IOC_CREATE_QUEUE        = 0x40,
    KFD_IOC_DESTROY_QUEUE       = 0x41,
    KFD_IOC_SET_MEMORY_POLICY   = 0x42,
    KFD_IOC_GET_PROCESS_APERTURE = 0x43,
    KFD_IOC_UPDATE_QUEUE        = 0x44,
    KFD_IOC_MAP_MEMORY          = 0x45,
    KFD_IOC_UNMAP_MEMORY        = 0x46,
    KFD_IOC_SET_TRAP_HANDLER    = 0x47,
};

/*
 * kfd_ioctl_handler_t — KFD ioctl handler function signature
 *
 * @cmd: ioctl command ID (KFD_IOC_*)
 * @arg: pointer to user-space ioctl argument (caller guarantees validity)
 * Returns 0 on success, negative errno on failure.
 */
typedef int (*kfd_ioctl_handler_t)(u32 cmd, void *arg);

/*
 * kfd_dispatch_init — register KFD ioctl handlers
 *
 * Stores the handler array for subsequent dispatch.  The array must have
 * KFD_IOC_COUNT entries, indexed by (cmd - KFD_IOC_CREATE_QUEUE).
 * Resets the call counter to zero.
 *
 * @handlers: array of KFD_IOC_COUNT function pointers
 * Returns 0 on success, -EINVAL if handlers is NULL.
 */
int kfd_dispatch_init(const kfd_ioctl_handler_t *handlers);

/*
 * kfd_dispatch — route an ioctl to the registered handler
 *
 * Looks up the handler for @cmd in the table registered by
 * kfd_dispatch_init().  If no handler has been registered (dispatch
 * not initialized or has been shut down), returns -EIO.
 *
 * @cmd: ioctl command ID (KFD_IOC_*)
 * @arg: pointer to user-space ioctl argument
 * Returns handler return value on success, -ENOSYS if cmd is
 * out of range, -EIO if dispatch has not been initialized.
 */
int kfd_dispatch(u32 cmd, void *arg);

/*
 * kfd_dispatch_exit — unregister all handlers
 *
 * After this call, kfd_dispatch() returns -EIO for all commands.
 * Idempotent (safe to call multiple times).
 */
void kfd_dispatch_exit(void);

/*
 * kfd_dispatch_call_count — test-only introspection
 *
 * Returns the number of successful dispatches since the last
 * kfd_dispatch_init() call.  Incremented atomically on each
 * successful dispatch (i.e., after the handler returns).
 */
int kfd_dispatch_call_count(void);

#ifdef __cplusplus
}  /* extern "C" */
#endif