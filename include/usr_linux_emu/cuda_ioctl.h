#ifndef _USR_LINUX_EMU_CUDA_IOCTL_H
#define _USR_LINUX_EMU_CUDA_IOCTL_H

#include <linux/ioctl.h>
#include <stdint.h>

/**
 * @deprecated System A (CUDA_IOCTL_*) is deprecated. Use System C (GPU_IOCTL_*)
 *             defined in plugins/gpu_driver/shared/gpu_ioctl.h instead.
 *             See ADR-015 for migration details.
 */

#define CUDA_IOCTL_MAGIC 'C'

// ============================================================================
// 内存管理结构体 (细粒度)
// ============================================================================

struct cuda_mem_alloc_request {
    uint64_t size;
    uint64_t device_ptr;  // out
    uint64_t fence_id;    // out
};

struct cuda_mem_free_request {
    uint64_t device_ptr;
    uint64_t fence_id;    // out
};

struct cuda_memcpy_h2d_request {
    uint64_t device_ptr;
    uint64_t offset;
    const void* host_ptr;
    uint64_t size;
    uint64_t fence_id;    // out
};

struct cuda_memcpy_d2h_request {
    void* host_ptr;
    uint64_t device_ptr;
    uint64_t offset;
    uint64_t size;
    uint64_t fence_id;    // out
};

// ============================================================================
// Kernel 启动结构体
// ============================================================================

struct cuda_launch_kernel_request {
    const char* kernel_name;
    void* params;
    uint32_t grid_dim_x, grid_dim_y, grid_dim_z;
    uint32_t block_dim_x, block_dim_y, block_dim_z;
    uint64_t task_id;   // out
    uint64_t fence_id;  // out
};

// ============================================================================
// 同步原语结构体 (分层)
// ============================================================================

struct cuda_wait_fence_request {
    uint64_t fence_id;
    uint64_t timeout_ms;  // 0 = infinite
};

struct cuda_query_fence_request {
    uint64_t fence_id;
    int32_t signaled;     // out: 1=signaled, 0=unsignaled
};

// ============================================================================
// Phase 2 预留结构体 (Graph/Batch) - 占位定义
// ============================================================================

struct cuda_graph_create_request {
    uint64_t reserved[8];  // Phase 2: graph nodes definition
};

struct cuda_graph_launch_request {
    uint64_t graph_id;
    uint64_t reserved[7];  // Phase 2: launch parameters
};

// ============================================================================
// ioctl 命令定义
// ============================================================================

// 内存管理 (细粒度)
#define CUDA_IOCTL_MEM_ALLOC      _IOWR(CUDA_IOCTL_MAGIC, 0x01, struct cuda_mem_alloc_request)
#define CUDA_IOCTL_MEM_FREE       _IOWR(CUDA_IOCTL_MAGIC, 0x02, struct cuda_mem_free_request)
#define CUDA_IOCTL_MEMCPY_H2D     _IOWR(CUDA_IOCTL_MAGIC, 0x03, struct cuda_memcpy_h2d_request)
#define CUDA_IOCTL_MEMCPY_D2H     _IOWR(CUDA_IOCTL_MAGIC, 0x04, struct cuda_memcpy_d2h_request)

// Kernel 启动
#define CUDA_IOCTL_LAUNCH_KERNEL  _IOWR(CUDA_IOCTL_MAGIC, 0x10, struct cuda_launch_kernel_request)

// 同步原语 (分层)
#define CUDA_IOCTL_WAIT_FENCE     _IOWR(CUDA_IOCTL_MAGIC, 0x20, struct cuda_wait_fence_request)
#define CUDA_IOCTL_QUERY_FENCE    _IOWR(CUDA_IOCTL_MAGIC, 0x21, struct cuda_query_fence_request)

// Phase 2 预留 (Graph/Batch)
#define CUDA_IOCTL_GRAPH_CREATE   _IOWR(CUDA_IOCTL_MAGIC, 0x30, struct cuda_graph_create_request)
#define CUDA_IOCTL_GRAPH_LAUNCH   _IOWR(CUDA_IOCTL_MAGIC, 0x31, struct cuda_graph_launch_request)

#endif // _USR_LINUX_EMU_CUDA_IOCTL_H
