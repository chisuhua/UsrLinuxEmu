#pragma once

/**
 * gpu_types.h - Cross-platform data type definitions for GPU driver interface
 *
 * This header defines the common data types used across both UsrLinuxEmu and
 * TaskRunner projects. It ensures ABI compatibility when migrating from user-space
 * emulation to a real kernel driver.
 *
 * Shared via symlink: TaskRunner/UsrLinuxEmu/plugins/gpu_driver/shared → ../../UsrLinuxEmu/plugins/gpu_driver/shared
 */

#include <stdint.h>

/* Fixed-width integer types matching Linux kernel conventions */
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;

/* GPU virtual and physical address types */
typedef u64 gpu_va_t;   /* GPU virtual address */
typedef u64 gpu_pa_t;   /* GPU physical address */

/* Stream and queue identifiers */
typedef u32 gpu_stream_id_t;   /* CUDA stream ID or Vulkan queue ID */
typedef u32 gpu_channel_id_t;  /* GPU channel identifier */

/* GPFIFO entry format (NVIDIA-compatible, supports CPU/GPU task fork) */
struct gpu_gpfifo_entry {
    u32 valid      : 1;   /* Entry is valid */
    u32 priv       : 1;   /* Privileged entry */
    u32 method     : 12;  /* OP_LAUNCH_KERNEL=0x100, OP_LAUNCH_CPU_TASK=0x101 */
    u32 subchannel : 3;   /* Target subchannel */
    u32 _reserved  : 15;
    u64 payload[7];       /* Method arguments (kernel args / CPU task descriptor) */
    u64 semaphore_va;     /* Completion semaphore virtual address */
    u32 semaphore_value;  /* Expected completion value */
    u32 release    : 1;   /* Release semaphore on completion */
    u32 _pad       : 31;
} __attribute__((packed));

/* GPU method opcodes */
#define GPU_OP_LAUNCH_KERNEL    0x100  /* Launch GPU kernel */
#define GPU_OP_LAUNCH_CPU_TASK  0x101  /* Fork CPU task via firmware callback */
#define GPU_OP_MEMCPY           0x102  /* DMA memory copy */
#define GPU_OP_MEMSET           0x103  /* DMA memory set */
#define GPU_OP_FENCE            0x104  /* Insert fence/barrier */

/* Submission flags */
#define GPU_SUBMIT_FENCE        0x1    /* Wait for fence before execution */
#define GPU_SUBMIT_INTERRUPT    0x2    /* Generate MSI-X interrupt on completion */
#define GPU_SUBMIT_PRIORITY_HIGH 0x4   /* High-priority submission */

/* Memory domain definitions (AMD ROCm compatible) */
#define GPU_MEM_DOMAIN_VRAM     0x1    /* GPU local video memory */
#define GPU_MEM_DOMAIN_GTT      0x2    /* GPU-mappable system memory (GART) */
#define GPU_MEM_DOMAIN_CPU      0x4    /* System memory (CPU accessible only) */

/* Handle types for VA Space and Queue abstractions */
typedef u64 gpu_va_space_handle_t;   /* VA Space handle */
typedef u64 gpu_queue_handle_t;        /* Queue handle */
