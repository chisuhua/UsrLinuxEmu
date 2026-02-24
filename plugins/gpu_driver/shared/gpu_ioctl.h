#pragma once

/**
 * gpu_ioctl.h - Standard ioctl interface between TaskRunner and UsrLinuxEmu
 *
 * This header defines all ioctl commands for the /dev/gpgpu0 device node.
 * The ioctl numbers and argument structures must remain identical between the
 * user-space emulator (UsrLinuxEmu) and the real kernel driver (.ko), enabling
 * zero-change migration of TaskRunner when switching from emulation to hardware.
 *
 * Shared via symlink: UsrLinuxEmu/plugins/gpu_driver/shared -> TaskRunner/shared
 */

#ifdef __KERNEL__
#include <linux/ioctl.h>
#include <linux/types.h>
#else
#include <sys/ioctl.h>
#include <stdint.h>
#endif

#include "gpu_types.h"

/* ioctl magic number - must not conflict with other drivers */
#define GPU_IOCTL_BASE 'G'

/* ========================================================================
 * Command Submission (TaskRunner -> UsrLinuxEmu)
 * ======================================================================== */

/**
 * GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH - Submit a batch of GPFIFO entries
 *
 * TaskRunner fills an array of gpu_gpfifo_entry structures and submits them
 * via this ioctl. UsrLinuxEmu DMA-writes them into device memory and triggers
 * the Hardware Puller state machine via a doorbell register write.
 */
#define GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH \
    _IOW(GPU_IOCTL_BASE, 0x01, struct gpu_pushbuffer_args)

struct gpu_pushbuffer_args {
    u32 stream_id;                          /* CUDA stream ID or Vulkan queue ID */
    const struct gpu_gpfifo_entry *entries; /* User-space pointer (built by TaskRunner) */
    u32 count;                              /* Number of entries */
    u32 flags;                              /* GPU_SUBMIT_FENCE | GPU_SUBMIT_INTERRUPT */
};

/* ========================================================================
 * Event Registration (UsrLinuxEmu -> TaskRunner callbacks)
 * ======================================================================== */

/**
 * GPU_IOCTL_REGISTER_MMU_EVENT_CB - Register a callback for MMU page migration events
 *
 * TaskRunner registers a callback function that UsrLinuxEmu will invoke when
 * page migration events occur (PAGE_INVALIDATE, PAGE_REMAP, TLB_FLUSH, etc.).
 * The callback is invoked from the MMU Event Dispatcher with a gpu_mmu_event_context.
 */
#define GPU_IOCTL_REGISTER_MMU_EVENT_CB \
    _IOW(GPU_IOCTL_BASE, 0x02, struct gpu_mmu_event_cb_args)

struct gpu_mmu_event_cb_args {
    u64 callback_fn;    /* Function pointer: void (*cb)(const struct gpu_mmu_event_context *) */
    u64 user_data;      /* Opaque pointer passed to callback */
};

/**
 * GPU_IOCTL_REGISTER_FIRMWARE_CB - Register a firmware callback for CPU task dispatch
 *
 * When the Hardware Puller decodes an OP_LAUNCH_CPU_TASK entry, it invokes this
 * callback in TaskRunner's firmware thread context, passing the CPU task descriptor.
 */
#define GPU_IOCTL_REGISTER_FIRMWARE_CB \
    _IOW(GPU_IOCTL_BASE, 0x03, struct gpu_firmware_cb_args)

struct gpu_firmware_cb_args {
    u64 callback_fn;    /* Function pointer: void (*cb)(const struct gpu_cpu_task_desc *) */
    u64 user_data;      /* Opaque pointer passed to callback */
};

/* ========================================================================
 * Memory Management (TaskRunner -> UsrLinuxEmu)
 * ======================================================================== */

/**
 * GPU_IOCTL_ALLOC_BO - Allocate a GPU buffer object (GEM/TTM backed)
 *
 * Allocates device memory managed by the TTM BO manager. The returned handle
 * can be used for mmap and DMA operations.
 */
#define GPU_IOCTL_ALLOC_BO \
    _IOWR(GPU_IOCTL_BASE, 0x10, struct gpu_alloc_bo_args)

struct gpu_alloc_bo_args {
    u64 size;           /* Size in bytes (input) */
    u32 flags;          /* Allocation flags: GPU_BO_DEVICE_LOCAL, GPU_BO_HOST_VISIBLE */
    u32 handle;         /* Returned GEM handle (output) */
    u64 gpu_va;         /* Returned GPU virtual address (output) */
};

#define GPU_BO_DEVICE_LOCAL     0x1   /* Allocate in device-local memory */
#define GPU_BO_HOST_VISIBLE     0x2   /* Accessible from host CPU */
#define GPU_BO_CXL_SHARED       0x4   /* CXL.cache coherent (fused device) */

/**
 * GPU_IOCTL_FREE_BO - Free a GPU buffer object
 */
#define GPU_IOCTL_FREE_BO \
    _IOW(GPU_IOCTL_BASE, 0x11, u32)   /* Argument: GEM handle */

/**
 * GPU_IOCTL_MAP_BO - Map a GPU buffer object into the GPU virtual address space
 */
#define GPU_IOCTL_MAP_BO \
    _IOWR(GPU_IOCTL_BASE, 0x12, struct gpu_map_bo_args)

struct gpu_map_bo_args {
    u32 handle;         /* GEM handle (input) */
    u32 flags;          /* Mapping flags (input) */
    u64 gpu_va;         /* GPU virtual address (output) */
};

/* ========================================================================
 * Device Information
 * ======================================================================== */

/**
 * GPU_IOCTL_GET_DEVICE_INFO - Query device capabilities
 */
#define GPU_IOCTL_GET_DEVICE_INFO \
    _IOR(GPU_IOCTL_BASE, 0x20, struct gpu_device_info)

struct gpu_device_info {
    u32 vendor_id;          /* PCI vendor ID */
    u32 device_id;          /* PCI device ID */
    u64 vram_size;          /* Total device-local memory in bytes */
    u64 bar0_size;          /* BAR0 (register space) size in bytes */
    u32 max_channels;       /* Maximum number of GPU channels */
    u32 compute_units;      /* Number of compute units */
    u32 gpfifo_capacity;    /* Maximum GPFIFO entries per channel */
    u32 cache_line_size;    /* Cache line size in bytes (for CXL.cache) */
};
