#pragma once

/**
 * gpu_events.h - MMU page migration event types and context structures
 *
 * Defines the event model used by the MMU Event Dispatcher to communicate
 * page migration and TLB coherence events between the emulation runtime and
 * the algorithm core. The event semantics are platform-independent and match
 * the Linux kernel mmu_interval_notifier interface, enabling zero-change
 * migration from user-space emulation to real kernel driver.
 *
 * Event flow for page migration:
 *   PAGE_INVALIDATE -> (data migration) -> PAGE_REMAP
 *
 * This matches the kernel mmu_notifier_invalidate_range_start/end sequence.
 *
 * Shared via symlink: UsrLinuxEmu/plugins/gpu_driver/shared -> TaskRunner/shared
 */

#include "gpu_types.h"

/* ========================================================================
 * MMU Event Types
 * ======================================================================== */

/**
 * enum gpu_mmu_event_type - Page migration and TLB coherence event types
 *
 * These events are injected by the TTM BO move path and consumed by the
 * MMU Event Handler in the algorithm core. TaskRunner can register callbacks
 * to receive these events via GPU_IOCTL_REGISTER_MMU_EVENT_CB.
 */
enum gpu_mmu_event_type {
    /**
     * GPU_MMU_EVENT_PAGE_INVALIDATE - Page invalidation before migration
     *
     * Injected before data is moved to a new physical location. All GPU
     * access to the affected virtual address range must be halted.
     * Triggers TLB invalidation and cache flush in the emulator.
     *
     * Corresponds to: mmu_notifier_invalidate_range_start()
     */
    GPU_MMU_EVENT_PAGE_INVALIDATE = 1,

    /**
     * GPU_MMU_EVENT_PAGE_REMAP - Page remapping after successful migration
     *
     * Injected after data has been moved to the new physical location.
     * Page table entries (PTEs) must be updated to point to the new PA.
     * old_pa and new_pa fields in the context are valid.
     *
     * Corresponds to: mmu_notifier_invalidate_range_end() + PTE update
     */
    GPU_MMU_EVENT_PAGE_REMAP      = 2,

    /**
     * GPU_MMU_EVENT_TLB_FLUSH_RANGE - TLB range flush without page migration
     *
     * Injected when a virtual address range must be flushed from the TLB
     * without data movement (e.g., permission change, mapping removal).
     */
    GPU_MMU_EVENT_TLB_FLUSH_RANGE = 3,

    /**
     * GPU_MMU_EVENT_CACHE_FLUSH - CXL.cache cache line flush
     *
     * Injected for CXL.cache coherence maintenance. cache_line_mask
     * indicates which specific cache lines within the range are affected.
     *
     * Used for fused CPU/GPU devices where CPU and GPU share a coherent
     * cache domain via the CXL.cache protocol.
     */
    GPU_MMU_EVENT_CACHE_FLUSH     = 4,
};

/* ========================================================================
 * MMU Event Context
 * ======================================================================== */

/**
 * struct gpu_mmu_event_context - Platform-independent event context
 *
 * Pure data structure with no platform-specific dependencies. This enables
 * the algorithm core (libgpu_core) to process events identically in both
 * user-space emulation and kernel driver contexts.
 */
struct gpu_mmu_event_context {
    /** Virtual address range start (inclusive, page-aligned) */
    u64 va_start;

    /** Virtual address range end (exclusive, page-aligned) */
    u64 va_end;

    /**
     * Old physical address - valid for GPU_MMU_EVENT_PAGE_REMAP only.
     * Points to the source location before migration.
     */
    u64 old_pa;

    /**
     * New physical address - valid for GPU_MMU_EVENT_PAGE_REMAP only.
     * Points to the destination location after migration.
     */
    u64 new_pa;

    /**
     * Cache line mask - valid for GPU_MMU_EVENT_CACHE_FLUSH only.
     * 64-bit bitmap where bit N indicates cache line N within the range
     * is affected. Enables fine-grained cache line invalidation for
     * CXL.cache coherence protocol compliance.
     */
    u64 cache_line_mask;
};

/* ========================================================================
 * Callback Type Definitions
 * ======================================================================== */

/**
 * gpu_mmu_event_cb_fn - Type of callback registered via GPU_IOCTL_REGISTER_MMU_EVENT_CB
 *
 * @type: The event type (see enum gpu_mmu_event_type)
 * @ctx:  Event context with address range and physical address information
 * @user_data: Opaque pointer provided during registration
 */
typedef void (*gpu_mmu_event_cb_fn)(enum gpu_mmu_event_type type,
                                    const struct gpu_mmu_event_context *ctx,
                                    void *user_data);

/**
 * gpu_cpu_task_desc - CPU task descriptor passed via OP_LAUNCH_CPU_TASK
 *
 * When the Hardware Puller decodes a GPFIFO entry with method=OP_LAUNCH_CPU_TASK,
 * it extracts this descriptor from the payload and invokes the registered
 * firmware callback in TaskRunner's firmware thread context.
 */
struct gpu_cpu_task_desc {
    u64 task_fn;        /* Function pointer: void (*fn)(void *arg) */
    u64 task_arg;       /* Opaque argument passed to task_fn */
    u64 completion_va;  /* GPU VA of completion semaphore to signal */
    u32 completion_val; /* Value to write to completion semaphore */
    u32 priority;       /* Task scheduling priority */
};

/**
 * gpu_firmware_cb_fn - Type of callback registered via GPU_IOCTL_REGISTER_FIRMWARE_CB
 *
 * @desc: CPU task descriptor extracted from the GPFIFO entry
 * @user_data: Opaque pointer provided during registration
 */
typedef void (*gpu_firmware_cb_fn)(const struct gpu_cpu_task_desc *desc,
                                   void *user_data);
