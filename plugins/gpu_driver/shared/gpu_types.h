#pragma once

/**
 * gpu_types.h - Cross-platform data type definitions for GPU driver interface
 *
 * This header defines the common data types used across both UsrLinuxEmu and
 * TaskRunner projects. It ensures ABI compatibility when migrating from user-space
 * emulation to a real kernel driver.
 *
 * Shared via symlink: TaskRunner/UsrLinuxEmu/plugins/gpu_driver/shared →
 * ../../UsrLinuxEmu/plugins/gpu_driver/shared
 */

#include <stdint.h>

/* Fixed-width integer types matching Linux kernel conventions */
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

/* GPU virtual and physical address types */
typedef u64 gpu_va_t; /* GPU virtual address */
typedef u64 gpu_pa_t; /* GPU physical address */

/* Stream and queue identifiers */
typedef u32 gpu_stream_id_t;  /* CUDA stream ID or Vulkan queue ID */
typedef u32 gpu_channel_id_t; /* GPU channel identifier */

/* GPFIFO entry format (NVIDIA-compatible, supports CPU/GPU task fork) */
struct gpu_gpfifo_entry {
  u32 valid : 1;      /* Entry is valid */
  u32 priv : 1;       /* Privileged entry */
  u32 method : 12;    /* OP_LAUNCH_KERNEL=0x100, OP_LAUNCH_CPU_TASK=0x101 */
  u32 subchannel : 3; /* Target subchannel */
  u32 _reserved : 15;
  u64 payload[7];      /* Method arguments (kernel args / CPU task descriptor) */
  u64 semaphore_va;    /* Completion semaphore virtual address */
  u32 semaphore_value; /* Expected completion value */
  u64 ts_query;         /* Stage 4.3 (ADR-057): timestamp query handle */
  u32 release : 1;     /* Release semaphore on completion */
  u32 _pad : 31;
  /* Stage 4.5 (ADR-049): timeline semaphore — batch completion signal / pre-dispatch wait */
  u64 tl_sem_handle;       /* Timeline semaphore handle (0 = none) */
  u64 tl_signal_value;     /* Value to signal on completion (0 = no signal) */
  u64 tl_wait_value;       /* Minimum value to wait before dispatch (0 = no wait) */
} __attribute__((packed));

/* GPU method opcodes */
#define GPU_OP_LAUNCH_KERNEL 0x100   /* Launch GPU kernel */
#define GPU_OP_LAUNCH_CPU_TASK 0x101 /* Fork CPU task via firmware callback */
#define GPU_OP_MEMCPY 0x102          /* DMA memory copy */
#define GPU_OP_MEMSET 0x103          /* DMA memory set */
#define GPU_OP_FENCE 0x104           /* Insert fence/barrier */
#define GPU_OP_SEM_WAIT 0x105        /* Stage 4.4: Semaphore WAIT (block until value >= threshold) */
#define GPU_OP_SEM_RELEASE 0x106     /* Stage 4.4: Semaphore RELEASE (write value on completion) */
#define GPU_OP_BARRIER_AND 0x107     /* Stage 4.4: Barrier AND (all streams arrive) */
#define GPU_OP_BARRIER_OR 0x108      /* Stage 4.4: Barrier OR (first stream arrives) */
#define GPU_OP_IB_JUMP 0x109         /* Stage 4.4: Indirect Buffer JUMP (switch fetch address) */
#define GPU_OP_SET_PREDICATE 0x10A    /* Stage 4.5: Set predicate register (ADR-051) */

/* GPFIFO entry format identifiers (Stage 4.5 Phase 6: AQL/PM4, ADR-051/052) */
#define FORMAT_USR_NATIVE 0           /* UsrLinuxEmu native GPFIFO format (default) */
#define FORMAT_AQL 1                  /* AMD AQL (Architected Queuing Language) packet format */
#define FORMAT_PM4 2                  /* PM4 packet format (stub - returns -ENOSYS) */

/* Channel priority levels (Stage 4.4: Priority Scheduling) */
#define GPU_CHAN_PRI_IDLE    0  /* No pending work */
#define GPU_CHAN_PRI_LOW     1  /* Low priority */
#define GPU_CHAN_PRI_NORMAL  2  /* Normal/default priority */
#define GPU_CHAN_PRI_HIGH    3  /* High priority */
#define GPU_CHAN_PRI_REALTIME 4 /* Realtime priority */

/* Indirect Buffer reference (Stage 4.4) */
#define MAX_IB_NEST 4  /* Maximum IB JUMP nesting depth */

/**
 * gpu_ib_ref - Indirect Buffer reference descriptor (Stage 4.4: IB JUMP)
 *
 * Describes a jump target for GPU_OP_IB_JUMP. The Puller saves its
 * current fetch position, switches to gpu_va, and (if continue_flag
 * is set) resumes at the saved position after the target batch completes.
 *
 * Layout matches payload[] usage in gpu_gpfifo_entry:
 *   payload[0] = gpu_va (target fetch address)
 *   payload[1] = continue_flag (1 = resume after, 0 = terminate)
 *   payload[2] = size (target batch entry count)
 */
struct gpu_ib_ref {
  u64 gpu_va;    /* Target GPU virtual address to jump to */
  u64 size;      /* Number of entries at the target address */
  u32 flags;     /* Bit 0: continue_flag (resume after target batch) */
};

/* Submission flags */
#define GPU_SUBMIT_FENCE 0x1         /* Wait for fence before execution */
#define GPU_SUBMIT_INTERRUPT 0x2     /* Generate MSI-X interrupt on completion */
#define GPU_SUBMIT_PRIORITY_HIGH 0x4 /* High-priority submission */

/* Memory domain definitions (AMD ROCm compatible) */
#define GPU_MEM_DOMAIN_VRAM 0x1 /* GPU local video memory */
#define GPU_MEM_DOMAIN_GTT 0x2  /* GPU-mappable system memory (GART) */
#define GPU_MEM_DOMAIN_CPU 0x4  /* System memory (CPU accessible only) */

/* Handle types for VA Space and Queue abstractions */
typedef u64 gpu_va_space_handle_t; /* VA Space handle */
typedef u64 gpu_queue_handle_t;    /* Queue handle */

/* Stage 4.1: BAR2 VRAM offset range (ADR-064 D2, ADR-069 D4) */
#define BAR2_OFFSET_BASE  0x200000000ULL  /* 8GB offset (VRAM BAR2 typical base) */
#define BAR2_OFFSET_SIZE  0x10000000ULL   /* 256MB */
