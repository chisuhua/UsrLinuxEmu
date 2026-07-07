#pragma once

/**
 * gpu_ioctl.h - Standard ioctl interface between TaskRunner and UsrLinuxEmu
 *
 * This header defines all ioctl commands for the /dev/gpgpu0 device node.
 * The ioctl numbers and argument structures must remain identical between the
 * user-space emulator (UsrLinuxEmu) and the real kernel driver (.ko), enabling
 * zero-change migration of TaskRunner when switching from emulation to hardware.
 *
 * Shared via symlink: TaskRunner/UsrLinuxEmu/plugins/gpu_driver/shared →
 * ../../UsrLinuxEmu/plugins/gpu_driver/shared
 */

#ifdef __KERNEL__
#include <linux/ioctl.h>
#include <linux/types.h>
#else
#include <stdint.h>
#include <sys/ioctl.h>
#endif

#include "gpu_types.h"
#include "gpu_queue.h"

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
#define GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH _IOW(GPU_IOCTL_BASE, 0x01, struct gpu_pushbuffer_args)

struct gpu_pushbuffer_args {
  u64 stream_id;              /* 64-bit queue handle (widened from u32) */
  u64 entries_addr;           /* User-space address of gpu_gpfifo_entry array */
  u32 count;                  /* Number of entries */
  u32 flags;                  /* Submission flags */
  u64 fence_id;               /* OUT: Fence ID for async completion tracking */
  /**
   * VA Space handle for validation (Phase 2 contract).
   *
   * - 0 (sentinel): skip VA Space + Queue attachment validation.
   *   Preserves backward compatibility with existing call sites that
   *   value-initialize the struct.
   * - non-zero: handler MUST verify (a) the VA Space exists and (b) the
   *   target stream_id (queue handle) is attached to that VA Space,
   *   else return -EINVAL.
   *
   * Appended at the end to preserve existing field offsets for ABI
   * compatibility with callers that do not initialize the new field.
   */
  u64 va_space_handle;
  u32 stream_id_compat;       /* Deprecated: backward compat alias for old u32 callers */
  u32 flags_extended;         /* Reserved flag space for future use */
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
#define GPU_IOCTL_REGISTER_MMU_EVENT_CB _IOW(GPU_IOCTL_BASE, 0x02, struct gpu_mmu_event_cb_args)

struct gpu_mmu_event_cb_args {
  u64 callback_fn; /* Function pointer: void (*cb)(const struct gpu_mmu_event_context *) */
  u64 user_data;   /* Opaque pointer passed to callback */
};

/**
 * GPU_IOCTL_REGISTER_FIRMWARE_CB - Register a firmware callback for CPU task dispatch
 *
 * When the Hardware Puller decodes an OP_LAUNCH_CPU_TASK entry, it invokes this
 * callback in TaskRunner's firmware thread context, passing the CPU task descriptor.
 */
#define GPU_IOCTL_REGISTER_FIRMWARE_CB _IOW(GPU_IOCTL_BASE, 0x03, struct gpu_firmware_cb_args)

struct gpu_firmware_cb_args {
  u64 callback_fn; /* Function pointer: void (*cb)(const struct gpu_cpu_task_desc *) */
  u64 user_data;   /* Opaque pointer passed to callback */
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
#define GPU_IOCTL_ALLOC_BO _IOWR(GPU_IOCTL_BASE, 0x10, struct gpu_alloc_bo_args)

struct gpu_alloc_bo_args {
  u64 size;   /* Size in bytes (input) */
  u32 domain; /* GPU_MEM_DOMAIN_VRAM/GTT/CPU (input) */
  u32 flags;  /* Allocation flags: GPU_BO_DEVICE_LOCAL, GPU_BO_HOST_VISIBLE (input) */
  u32 handle; /* Returned GEM handle (output) */
  u64 gpu_va; /* Returned GPU virtual address (output) */
};

#define GPU_BO_DEVICE_LOCAL 0x1 /* Allocate in device-local memory */
#define GPU_BO_HOST_VISIBLE 0x2 /* Accessible from host CPU */
#define GPU_BO_CXL_SHARED 0x4   /* CXL.cache coherent (fused device) */

/**
 * GPU_IOCTL_FREE_BO - Free a GPU buffer object
 */
#define GPU_IOCTL_FREE_BO _IOW(GPU_IOCTL_BASE, 0x11, u32) /* Argument: GEM handle */

/**
 * GPU_IOCTL_MAP_BO - Map a GPU buffer object into the GPU virtual address space
 */
#define GPU_IOCTL_MAP_BO _IOWR(GPU_IOCTL_BASE, 0x12, struct gpu_map_bo_args)

struct gpu_map_bo_args {
  u32 handle; /* GEM handle (input) */
  u32 flags;  /* Mapping flags (input) */
  u64 gpu_va; /* GPU virtual address (output) */
};

/* ========================================================================
 * Synchronization
 * ======================================================================== */

/**
 * GPU_IOCTL_WAIT_FENCE - Wait for a fence to be signaled
 *
 * TaskRunner calls this to block until the specified fence_id is signaled
 * by the GPU (via doorbell interrupt or polling).
 *
 * Phase 1: Simple poll-based implementation (busy-wait, acceptable for CLI)
 * Phase 2: Event-driven with GPU interrupt callback
 */
#define GPU_IOCTL_WAIT_FENCE _IOW(GPU_IOCTL_BASE, 0x13, struct gpu_wait_fence_args)

struct gpu_wait_fence_args {
  u64 fence_id;   /* Fence ID to wait on (input) */
  u32 timeout_ms; /* Timeout in milliseconds, 0 = infinite (input) */
  u32 status;     /* OUT: 1=signaled, 0=timeout, -1=error */
};

/* ========================================================================
 * VA Space Management (Phase 2)
 * ======================================================================== */

/**
 * GPU_IOCTL_CREATE_VA_SPACE - Create a GPU virtual address space
 *
 * A VA Space manages GPU virtual address allocations and registered GPUs.
 * All GPU memory operations occur within a VA Space context.
 */
#define GPU_IOCTL_CREATE_VA_SPACE _IOWR(GPU_IOCTL_BASE, 0x30, struct gpu_va_space_args)

struct gpu_va_space_args {
  u32 page_size;                         /* Page size: 0=4KB, 1=64KB (input) */
  u32 flags;                             /* VA Space flags (input) */
  gpu_va_space_handle_t va_space_handle; /* OUT: VA Space handle */
};

/**
 * GPU_IOCTL_DESTROY_VA_SPACE - Destroy a GPU virtual address space
 */
#define GPU_IOCTL_DESTROY_VA_SPACE _IOW(GPU_IOCTL_BASE, 0x31, gpu_va_space_handle_t)

/**
 * GPU_IOCTL_REGISTER_GPU - Register a GPU to a VA Space
 *
 * Required for multi-GPU and peer-to-peer scenarios.
 */
#define GPU_IOCTL_REGISTER_GPU _IOW(GPU_IOCTL_BASE, 0x32, struct gpu_register_gpu_args)

struct gpu_register_gpu_args {
  gpu_va_space_handle_t va_space_handle; /* VA Space (input) */
  u32 gpu_id;                            /* GPU ID (input) */
  u32 flags;                             /* Registration flags (input) */
};

/* ========================================================================
 * Queue Management (Phase 2 - ADR-024)
 * ======================================================================== */

/**
 * GPU_IOCTL_CREATE_QUEUE - Create a GPU command queue
 *
 * Queues belong to a VA Space and are used for command submission.
 * Phase 1 uses implicit default queue; Phase 2 exposes explicit queues.
 * Returns queue_handle + doorbell_pgoff for mmap.
 *
 * Maps to AMDKFD_IOC_CREATE_QUEUE (0x02 in KFD space).
 * Extension fields (added for Stage 1.2 KFD compatibility) are appended
 * at the END of the struct to preserve binary compatibility with
 * TaskRunner's existing 5-field usage.
 */
#define GPU_IOCTL_CREATE_QUEUE _IOWR(GPU_IOCTL_BASE, 0x40, struct gpu_queue_args)

/* Queue format (KFD: enum kfd_queue_format) */
#define GPU_QUEUE_FORMAT_PM4  0  /* Legacy PM4 packet format (pre-GCN) */
#define GPU_QUEUE_FORMAT_AQL  1  /* HSA AQL (Architected Queue Language) */

/* Queue create flags (KFD: kfd_ioctl_create_queue_args flags) */
#define GPU_QUEUE_FLAG_SPARSE         (1u << 0)  /* SLT/SVM bounds checking */
#define GPU_QUEUE_FLAG_AQL            (1u << 1)  /* AQL access */
#define GPU_QUEUE_FLAG_GWS            (1u << 2)  /* GWS (global wave sync) cap */
#define GPU_QUEUE_FLAG_LIGHTWEIGHT_SVM (1u << 3) /* Skip HWS registration */
#define GPU_QUEUE_FLAG_TBA_TMA        (1u << 4)  /* Trap handler buffer + TMA */

struct gpu_queue_args {
  gpu_va_space_handle_t va_space_handle; /* VA Space (input) */
  u32 queue_type;                        /* GPU_QUEUE_COMPUTE/COPY/GRAPHICS (input) */
  u32 priority;                          /* Queue priority 0-100 (input) */
  u64 ring_buffer_size;                  /* Ring buffer size in entries (input) */
  gpu_queue_handle_t queue_handle;       /* OUT: Queue handle */
  u64 doorbell_pgoff;                    /* OUT: Doorbell mmap page offset */

  /* ── KFD-compat extension fields (Stage 1.2) — optional, end of struct ── */

  u64 ring_base_address;    /* Ring buffer base (user virt addr, 0=default) */
  u64 gpu_va;               /* Reserved queue VA (0=auto-allocate) */
  u32 queue_format;         /* GPU_QUEUE_FORMAT_PM4/AQL (input) */
  u32 flags;                /* OR of GPU_QUEUE_FLAG_* (input) */
  u32 doorbell_off;         /* Doorbell offset within doorbell BAR (input) */
  u32 eop_buffer_size;      /* EOP buffer size for COMPUTE queues (input) */
};

/**
 * GPU_IOCTL_DESTROY_QUEUE - Destroy a GPU command queue
 */
#define GPU_IOCTL_DESTROY_QUEUE _IOW(GPU_IOCTL_BASE, 0x41, gpu_queue_handle_t)

/**
 * GPU_IOCTL_MAP_QUEUE_RING - Map Ring Buffer to user-space
 *
 * TaskRunner passes queue_handle and a shared memory address.
 * UsrLinuxEmu maps the shared memory and uses it as the Ring Buffer.
 */
#define GPU_IOCTL_MAP_QUEUE_RING _IOWR(GPU_IOCTL_BASE, 0x42, struct gpu_queue_map_ring_args)

/**
 * GPU_IOCTL_QUERY_QUEUE - Query queue information
 *
 * Returns queue state including pending count and fence value.
 */
#define GPU_IOCTL_QUERY_QUEUE _IOWR(GPU_IOCTL_BASE, 0x43, struct gpu_queue_info_args)

/* Queue type definitions */
#define GPU_QUEUE_COMPUTE 0x0  /* Compute queue (GFX/SDMA) */
#define GPU_QUEUE_COPY 0x1     /* Copy engine queue (SDMA) */
#define GPU_QUEUE_GRAPHICS 0x2 /* Graphics queue (future) */

/**
 * GPU_IOCTL_GET_DEVICE_INFO - Query device capabilities
 */
#define GPU_IOCTL_GET_DEVICE_INFO _IOR(GPU_IOCTL_BASE, 0x20, struct gpu_device_info)

struct gpu_device_info {
  u32 vendor_id;               /* PCI vendor ID */
  u32 device_id;               /* PCI device ID */
  u64 vram_size;               /* Total device-local memory in bytes */
  u64 bar0_size;               /* BAR0 (register space) size in bytes */
  u32 max_channels;            /* Maximum number of GPU channels */
  u32 compute_units;           /* Number of compute units (CUs or SMs) */
  u32 gpfifo_capacity;          /* Maximum GPFIFO entries per channel */
  u32 cache_line_size;          /* Cache line size in bytes (for CXL.cache) */

  /* ── Phase 1.5 扩展 ── */

  /** Warp 大小: NVIDIA=32, AMD CDNA=64, AMD RDNA=32 */
  u32 warp_size;

  /** 最大引擎时钟频率 (MHz) */
  u32 max_clock_frequency;

  /** 驱动版本号 (主.次.修订, 如 0x000500 = v0.5.0) */
  u32 driver_version;

  /** Firmware/PSP 版本号 (主.次) */
  u32 firmware_version;

  /** SIMD 单元数量 (AMD CU 或 NVIDIA SM) */
  u32 simd_count;

  /** 最大内存时钟频率 (MHz) */
  u32 max_memory_clock_frequency;

  /** 内存位宽 (bits) */
  u32 memory_bus_width;

  /** 峰值 FP32 理论性能 (GFLOPS) */
  u32 peak_fp32_gflops;

  /** PCIe 带宽 (Mbps, 如 16000 = PCIe 4.0 x16) */
  u32 pcie_bandwidth;

  /** 架构标识符 (厂商特定: AMD family 或 NVIDIA compute capability) */
  u32 architecture_id;

  /** 市场营销名称 (UTF-8, 以 null 结尾) */
  char marketing_name[64];
};

/* ========================================================================
 * KFD Portability IOCTLs (Stage 1.2 — reserved for Stage 1.4 integration)
 *
 * These are 1:1 mappings of AMD KFD ioctls into the System C numbering
 * space.  KFD reference: include/uapi/linux/kfd_ioctl.h (Linux 6.12 LTS).
 * System C uses 'G' base; KFD uses 'K' base — numbering is independent
 * but struct layouts are aligned for zero-logic-change portability.
 *
 * IOCTL Numbering Map (System C → KFD):
 *   0x44 GET_PROCESS_APERTURE ←  AMDKFD_IOC_GET_PROCESS_APERTURES_NEW (0x14)
 *   0x45 UPDATE_QUEUE          ←  AMDKFD_IOC_UPDATE_QUEUE (0x07)
 *   0x46 MAP_MEMORY            ←  AMDKFD_IOC_MAP_MEMORY_TO_GPU (0x16)
 *   0x47 UNMAP_MEMORY          ←  AMDKFD_IOC_UNMAP_MEMORY_FROM_GPU (0x18)
 *
 * The CREATE_QUEUE (0x40) struct is extended in-place with KFD-compat
 * fields at the END (see above) to map AMDKFD_IOC_CREATE_QUEUE (0x02)
 * without breaking existing TaskRunner code.
 * ======================================================================== */

/**
 * GPU_IOCTL_GET_PROCESS_APERTURE — Query GPU address aperture per process
 *
 * Maps to AMDKFD_IOC_GET_PROCESS_APERTURES_NEW (0x14 in KFD space).
 * Returns per-node aperture information (GPU-local + scratch base/limit).
 */
#define GPU_IOCTL_GET_PROCESS_APERTURE \
    _IOWR(GPU_IOCTL_BASE, 0x44, struct gpu_get_process_aperture_args)

struct gpu_get_process_aperture_args {
  u32 num_nodes;          /* Number of GPU nodes (input) */
  u32 pad;                /* Padding for 8-byte alignment */
  u64 apertures_ptr;      /* User-space pointer to gpu_aperture_info[num_nodes] (output) */
};

struct gpu_aperture_info {
  u32 gpu_id;             /* GPU node ID */
  u32 pad;
  u64 lds_base;           /* LDS (Local Data Share) base address */
  u64 lds_limit;          /* LDS limit address */
  u64 scratch_base;       /* Scratch memory base */
  u64 scratch_limit;      /* Scratch memory limit */
  u64 gpuvm_base;         /* GPU VM base address */
  u64 gpuvm_limit;        /* GPU VM limit address */
};

/**
 * GPU_IOCTL_UPDATE_QUEUE — Update queue properties at runtime
 *
 * Maps to AMDKFD_IOC_UPDATE_QUEUE (0x07 in KFD space).
 * Modifies properties of an already-created queue without destroying it.
 */
#define GPU_IOCTL_UPDATE_QUEUE \
    _IOWR(GPU_IOCTL_BASE, 0x45, struct gpu_update_queue_args)

struct gpu_update_queue_args {
  gpu_queue_handle_t queue_handle;   /* Target queue (input) */
  u64 ring_base_address;             /* New ring buffer base (input, 0 = keep current) */
  u64 ring_size;                     /* New ring buffer size (input, 0 = keep current) */
  u32 queue_percent;                 /* Queue percentage of GPU resources (input) */
  u32 queue_priority;                /* Queue priority 0-100 (input) */
  u32 queue_flags;                   /* Update flags: QUEUE_UPDATE_* (input) */
  u32 pad;
};

#define GPU_QUEUE_UPDATE_RING_BASE   (1u << 0)  /* Update ring_base_address */
#define GPU_QUEUE_UPDATE_RING_SIZE   (1u << 1)  /* Update ring_size */
#define GPU_QUEUE_UPDATE_PERCENT     (1u << 2)  /* Update queue_percent */
#define GPU_QUEUE_UPDATE_PRIORITY    (1u << 3)  /* Update queue_priority */

/**
 * GPU_IOCTL_MAP_MEMORY — Map system memory to GPU address space
 *
 * Maps to AMDKFD_IOC_MAP_MEMORY_TO_GPU (0x16 in KFD space).
 * Maps host system memory (not BO/GEM) into the GPU's IOMMU page tables.
 */
#define GPU_IOCTL_MAP_MEMORY \
    _IOWR(GPU_IOCTL_BASE, 0x46, struct gpu_map_memory_args)

struct gpu_map_memory_args {
  u32 handle;            /* Memory handle from alloc (input) */
  u32 n_devices;         /* Number of GPU devices (input) */
  u32 device_ids[8];     /* GPU device IDs to map to (input) */
  u32 n_success;         /* Number of successful mappings (output) */
  u64 gpu_va;            /* GPU virtual address (output) */
  u64 size;              /* Size in bytes (input) */
  u32 flags;             /* Mapping flags (input, reserved) */
  u32 pad;
};

/**
 * GPU_IOCTL_UNMAP_MEMORY — Unmap system memory from GPU
 *
 * Maps to AMDKFD_IOC_UNMAP_MEMORY_FROM_GPU (0x18 in KFD space).
 * Removes host system memory mappings from GPU IOMMU page tables.
 */
#define GPU_IOCTL_UNMAP_MEMORY \
    _IOWR(GPU_IOCTL_BASE, 0x47, struct gpu_unmap_memory_args)

struct gpu_unmap_memory_args {
  u32 handle;            /* Memory handle to unmap (input) */
  u32 n_devices;         /* Number of GPU devices (input) */
  u32 device_ids[8];     /* GPU device IDs to unmap from (input) */
  u32 n_success;         /* Number of successful unmaps (output) */
  u32 flags;             /* Unmap flags (input, reserved) */
  u32 pad;
};

/* ========================================================================
 * Phase 3.1 / 3.2 — Stream Capture + CUDA Graph + Memory Pool
 * (sim-stream-primitive-support, ACCEPTED 2026-07-05)
 *
 * Numbering reservation:
 *   0x50-0x59  Stream Capture + Graph (10 IOCTLs)
 *   0x60-0x67  Memory Pool (8 IOCTLs)
 *   0x68-0x6F  Reserved (internal driver extensions)
 *   0x70-0x7F  Reserved for future use
 *
 * IOCTL direction table — Oracle H2 fix: any IOCTL that returns data to
 * user space MUST be declared _IOWR (not _IOW), otherwise the kernel
 * will NOT copy the output fields back to user space, causing silent
 * data loss. See proposal.md / design.md §IOCTL Directions Table for the
 * authoritative mapping.
 * ======================================================================== */

/* ── Stream Capture (0x50-0x52, 2 struct) ──────────────────────────────── */

/**
 * GPU_IOCTL_STREAM_CAPTURE_BEGIN - Begin stream capture on a queue/stream
 *
 * `_IOW`: input-only (stream_id + mode). No output fields.
 * capture mode: 0=GLOBAL, 1=THREAD_LOCAL, 2=RELAXED (cuStreamCaptureMode).
 * Only SIM_CAPTURE_MODE_GLOBAL (0) is recognized in this change; other
 * modes return -EINVAL.
 */
#define GPU_IOCTL_STREAM_CAPTURE_BEGIN _IOW(GPU_IOCTL_BASE, 0x50, struct gpu_stream_capture_args)

/**
 * GPU_IOCTL_STREAM_CAPTURE_END - End stream capture, returns captured graph
 *
 * `_IOWR`: graph_handle_out must be filled on return.
 */
#define GPU_IOCTL_STREAM_CAPTURE_END _IOWR(GPU_IOCTL_BASE, 0x51, struct gpu_stream_capture_args)

/**
 * GPU_IOCTL_STREAM_CAPTURE_STATUS - Query current capture state
 *
 * `_IOWR`: status_out is filled with one of SIM_STREAM_CAPTURE_{NONE,ACTIVE,INVALID}.
 */
#define GPU_IOCTL_STREAM_CAPTURE_STATUS _IOWR(GPU_IOCTL_BASE, 0x52, struct gpu_stream_capture_status_args)

/* Capture mode + status constants are defined in sim/stream_capture.h
 * (typed enums). Do NOT redefine here — both this header and stream_capture.h
 * may be included in a single translation unit; the preprocessor would
 * otherwise substitute these names into the enum body. */

struct gpu_stream_capture_args {
  u32 stream_id;        /* Queue/stream handle (input for BEGIN/END) */
  u32 mode;             /* SIM_CAPTURE_MODE_* (input, BEGIN only) */
  u64 graph_handle_out; /* OUT (END only): captured graph handle */
};

struct gpu_stream_capture_status_args {
  u32 stream_id;        /* Queue/stream handle (input) */
  u32 _pad;
  u32 status_out;       /* OUT: SIM_STREAM_CAPTURE_* */
  u32 _pad2;
};

/* ── Graph (0x53-0x59, 7 structs) ─────────────────────────────────────── */

/**
 * GPU_IOCTL_GRAPH_CREATE - Create an empty graph
 */
#define GPU_IOCTL_GRAPH_CREATE _IOWR(GPU_IOCTL_BASE, 0x53, struct gpu_graph_create_args)

/**
 * GPU_IOCTL_GRAPH_DESTROY - Destroy a graph
 */
#define GPU_IOCTL_GRAPH_DESTROY _IOW(GPU_IOCTL_BASE, 0x54, struct gpu_graph_destroy_args)

/**
 * GPU_IOCTL_GRAPH_ADD_KERNEL_NODE - Append a kernel node to the graph
 */
#define GPU_IOCTL_GRAPH_ADD_KERNEL_NODE _IOW(GPU_IOCTL_BASE, 0x55, struct gpu_graph_add_kernel_node_args)

/**
 * GPU_IOCTL_GRAPH_ADD_MEMCPY_NODE - Append a memcpy node to the graph
 */
#define GPU_IOCTL_GRAPH_ADD_MEMCPY_NODE _IOW(GPU_IOCTL_BASE, 0x56, struct gpu_graph_add_memcpy_node_args)

/**
 * GPU_IOCTL_GRAPH_INSTANTIATE - Validate graph and produce executable
 *
 * `_IOWR`: exec_handle_out must be filled on return.
 */
#define GPU_IOCTL_GRAPH_INSTANTIATE _IOWR(GPU_IOCTL_BASE, 0x57, struct gpu_graph_instantiate_args)

/**
 * GPU_IOCTL_GRAPH_LAUNCH - Launch a graph executable
 *
 * `_IOWR`: fence_id_out must be filled with a sim-layer fence_id (>= 1<<32).
 * See design.md §fence_id Lifecycle Migration Plan.
 */
#define GPU_IOCTL_GRAPH_LAUNCH _IOWR(GPU_IOCTL_BASE, 0x58, struct gpu_graph_launch_args)

/**
 * GPU_IOCTL_GRAPH_DESTROY_EXEC - Destroy a graph executable
 */
#define GPU_IOCTL_GRAPH_DESTROY_EXEC _IOW(GPU_IOCTL_BASE, 0x59, struct gpu_graph_destroy_exec_args)

/* Graph node type enum is defined in sim/graph.h (typed enum). */

struct gpu_graph_create_args {
  u64 graph_handle_out;  /* OUT: graph handle */
};

struct gpu_graph_destroy_args {
  u64 graph_handle;      /* Graph to destroy (input) */
};

struct gpu_graph_add_kernel_node_args {
  u64 graph_handle;          /* Target graph (input) */
  u32 kernel_index;          /* Kernel table index (input) */
  u32 grid_x, grid_y, grid_z;
  u32 block_x, block_y, block_z;
  u64 kernargs_bo_handle;    /* BO handle for kernel arguments, 0 = no args */
};

struct gpu_graph_add_memcpy_node_args {
  u64 graph_handle;      /* Target graph (input) */
  u64 src_va;            /* Source GPU virtual address (input) */
  u64 dst_va;            /* Destination GPU virtual address (input) */
  u64 size;              /* Transfer size in bytes (input) */
  u32 is_h2d;            /* 0 = D2D (device-to-device), 1 = H2D (host-to-device) */
  u32 _pad;
};

struct gpu_graph_instantiate_args {
  u64 graph_handle;      /* Graph to instantiate (input) */
  u64 exec_handle_out;   /* OUT: executable handle */
};

struct gpu_graph_launch_args {
  u64 exec_handle;       /* Executable to launch (input) */
  u32 stream_id;         /* Target queue (input) */
  u32 _pad;
  s64 fence_id_out;      /* OUT: sim fence_id (>= 1<<32 on success, <0 = error) */
};

struct gpu_graph_destroy_exec_args {
  u64 exec_handle;       /* Executable to destroy (input) */
  u64 _pad;
};

/* ── Memory Pool (0x60-0x67, 8 structs) ───────────────────────────────── */

/**
 * GPU_IOCTL_MEM_POOL_CREATE - Create a memory pool within a VA Space
 *
 * `_IOWR`: pool_handle_out must be filled. Pool semantics per Fix-2:
 * VA subrange approach — pool reserves a contiguous VA range
 * [va_base, va_base + size) inside the parent VA Space.
 */
#define GPU_IOCTL_MEM_POOL_CREATE _IOWR(GPU_IOCTL_BASE, 0x60, struct gpu_mem_pool_create_args)

/**
 * GPU_IOCTL_MEM_POOL_DESTROY - Destroy a memory pool
 */
#define GPU_IOCTL_MEM_POOL_DESTROY _IOW(GPU_IOCTL_BASE, 0x61, struct gpu_mem_pool_destroy_args)

/**
 * GPU_IOCTL_MEM_POOL_ALLOC - Synchronous allocation from a pool
 *
 * `_IOWR`: va_out must be filled.
 */
#define GPU_IOCTL_MEM_POOL_ALLOC _IOWR(GPU_IOCTL_BASE, 0x62, struct gpu_mem_pool_alloc_args)

/**
 * GPU_IOCTL_MEM_POOL_ALLOC_ASYNC - Asynchronous allocation (returns fence)
 *
 * `_IOWR`: va_out AND fence_id_out must be filled.
 */
#define GPU_IOCTL_MEM_POOL_ALLOC_ASYNC _IOWR(GPU_IOCTL_BASE, 0x63, struct gpu_mem_pool_alloc_async_args)

/**
 * GPU_IOCTL_MEM_POOL_FREE_ASYNC - Asynchronous free (returns fence)
 *
 * `_IOWR`: fence_id_out must be filled.
 */
#define GPU_IOCTL_MEM_POOL_FREE_ASYNC _IOWR(GPU_IOCTL_BASE, 0x64, struct gpu_mem_pool_free_async_args)

/**
 * GPU_IOCTL_MEM_POOL_SET_ATTR - Set pool attribute (recorded, not enforced)
 */
#define GPU_IOCTL_MEM_POOL_SET_ATTR _IOW(GPU_IOCTL_BASE, 0x65, struct gpu_mem_pool_attr_args)

/**
 * GPU_IOCTL_MEM_POOL_GET_ATTR - Get pool attribute
 *
 * `_IOWR`: value_out must be filled.
 */
#define GPU_IOCTL_MEM_POOL_GET_ATTR _IOWR(GPU_IOCTL_BASE, 0x66, struct gpu_mem_pool_attr_args)

/**
 * GPU_IOCTL_MEM_POOL_TRIM - Trim pool to retain at least min_bytes
 */
#define GPU_IOCTL_MEM_POOL_TRIM _IOW(GPU_IOCTL_BASE, 0x67, struct gpu_mem_pool_trim_args)

/**
 * GPU_IOCTL_MEM_POOL_EXPORT - Export pool as shareable handle (POSIX FD).
 * Phase 4 (2026-07-07) added for cuMemPoolExportToShareableHandle support.
 * Currently only CU_MEM_HANDLE_TYPE_POSIX_FILE_DESCRIPTOR=1 is supported.
 * Imported Phase 5+. See docs/00_adr/adr-039-mem-pool-export-ioctl.md
 */
#define GPU_IOCTL_MEM_POOL_EXPORT _IOWR(GPU_IOCTL_BASE, 0x68, struct gpu_mem_pool_export_args)

/* Pool attribute constants + error codes are defined in sim/mem_pool.h
 * (typed enum + #defines). Do NOT redefine here — including both this
 * header and sim/mem_pool.h in a single translation unit would otherwise
 * trigger the preprocessor to mangle the enum body into numeric literals. */

/* Pool error codes (Fix-2) */
#define SIM_POOL_ERR_OK               0
#define SIM_POOL_ERR_INVALID_HANDLE  -1
#define SIM_POOL_ERR_NOSPC           -2
#define SIM_POOL_ERR_INVAL           -3
#define SIM_POOL_ERR_NOT_SUPPORTED   -4

struct gpu_mem_pool_props {
  u64 va_space_handle;  /* Parent VA Space handle (input) */
  u64 size;             /* Pool size in bytes (input) */
  u64 va_base;          /* Pool VA subrange base (OUT, set by MEM_POOL_CREATE) */
  u64 va_limit;         /* Pool VA subrange end = va_base + size (OUT) */
  u32 flags;            /* GPU_MEM_POOL_* flags (input) */
  u32 _pad;
};

/* GPU_MEM_POOL flags (subset of CU_MEMPOOL_* for Phase 3.2) */
#define GPU_MEM_POOL_FLAGS_DEFAULT  0u

struct gpu_mem_pool_create_args {
  struct gpu_mem_pool_props props;  /* Input */
  u64 pool_handle_out;              /* OUT: pool handle */
};

struct gpu_mem_pool_destroy_args {
  u64 pool_handle;   /* Pool handle (input) */
};

struct gpu_mem_pool_alloc_args {
  u64 pool_handle;   /* Pool handle (input) */
  u64 size;          /* Requested allocation size (input) */
  u64 va_out;        /* OUT: allocated VA in [va_base, va_limit) */
};

struct gpu_mem_pool_alloc_async_args {
  u64 pool_handle;   /* Pool handle (input) */
  u64 size;          /* Requested allocation size (input) */
  u32 stream_id;     /* Target stream (input) */
  u32 _pad;
  u64 va_out;        /* OUT: allocated VA */
  s64 fence_id_out;  /* OUT: sim fence_id (>= 1<<32) */
};

struct gpu_mem_pool_free_async_args {
  u64 va;             /* VA to free (input, from prior alloc) */
  u32 stream_id;      /* Target stream (input) */
  u32 _pad;
  s64 fence_id_out;   /* OUT: sim fence_id (>= 1<<32) */
  u64 _pad2;
};

struct gpu_mem_pool_attr_args {
  u64 pool_handle;       /* Pool handle (input) */
  u32 attr;              /* SIM_MEM_POOL_ATTR_* (input) */
  u32 _reserved;         /* Padding, must be 0 */
  u64 value[4];          /* 32-byte blob (Fix-7 documented layout):
                          *  - SIM_MEM_POOL_ATTR_RELEASE_THRESHOLD:
                          *      value[0]=uint64_t threshold bytes; value_size must == 8
                          *  - SIM_MEM_POOL_ATTR_REUSE_FOLLOW_EVENT_DEPENDENCIES:
                          *      value[0]=uint32_t enable 0/1; value_size must == 4
                          *  For get_attr, value_out is filled.
                          *  For set_attr, value is input.
                          *  Errors:
                          *   - value_size > 32 → -EINVAL
                          *   - unknown attr  → -ENOSYS
                          */
};

struct gpu_mem_pool_trim_args {
  u64 pool_handle;   /* Pool handle (input) */
  u64 min_bytes;     /* Retain at least this many bytes (input) */
};

struct gpu_mem_pool_export_args {
  u64 pool_handle;   /* input: pool handle from cuMemPoolCreate */
  u32 handle_type;   /* input: CU_MEM_HANDLE_TYPE_POSIX_FILE_DESCRIPTOR=1 */
  u32 flags;         /* input: reserved, must be 0 */
  s32 fd_out;        /* OUT: POSIX FD (>= 0) or -1 */
  u32 _pad;          /* alignment padding */
};
