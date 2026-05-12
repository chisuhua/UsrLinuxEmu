#pragma once

/**
 * gpu_queue.h - 用户态队列共享内存结构体定义 (ADR-024)
 *
 * 定义 Ring Buffer 和 Queue Descriptor 的数据结构。
 * 与 TaskRunner 通过 shared/ 符号链接共享。
 *
 * 设计对标:
 * - AMD User Mode Queue: 用户态直接写 Ring Buffer (GFX11+)
 * - NVIDIA GPFIFO: GP_PUT/GP_GET 环形缓冲区
 * - Intel Xe: LRCA Ring Buffer
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Ring Buffer - 共享内存命令队列
 * ======================================================================== */

/**
 * Ring Buffer 头部（位于共享内存起始处）
 *
 * 布局:
 *   [gpu_ring_header]          — 控制结构
 *   [gpu_gpfifo_entry entries] — 命令条目
 */
struct gpu_ring_header {
  volatile uint32_t write_idx;   /* Producer index (用户态写入) */
  volatile uint32_t read_idx;    /* Consumer index (Puller 读取) */
  uint32_t capacity;             /* 环形缓冲区容量 (entry 数) */
  uint32_t flags;                /* 标志位 */
  uint64_t fence_value;          /* 完成 fence (Puller 写入) */
  uint8_t  reserved[32];         /* 预留 + 缓存行对齐 */
};

/** Ring Buffer 最大 entry 数 */
#define GPU_MAX_RING_ENTRIES 1024

/* ========================================================================
 * Queue Descriptor - 队列属性
 * ======================================================================== */

/** Queue 类型 */
enum gpu_queue_type {
  GPU_QUEUE_COMPUTE = 0,  /* 计算队列 */
  GPU_QUEUE_COPY = 1,     /* 拷贝队列 (SDMA) */
};

/** Queue 创建参数 (GPU_IOCTL_CREATE_QUEUE) */
struct gpu_create_queue_args {
  uint32_t queue_type;         /* GPU_QUEUE_COMPUTE / COPY */
  uint32_t priority;           /* 0-100 */
  uint32_t ring_size;          /* Ring Buffer 大小 (entry 数) */
  uint32_t reserved;
  uint64_t queue_handle;       /* OUT: Queue 句柄 */
  uint64_t doorbell_pgoff;     /* OUT: Doorbell mmap page offset */
};

/** Ring Buffer 映射参数 (GPU_IOCTL_MAP_QUEUE_RING) */
struct gpu_queue_map_ring_args {
  uint64_t queue_handle;       /* INPUT: 由 CREATE_QUEUE 返回 */
  uint64_t ring_addr;          /* INPUT: 共享内存地址 */
};

/** Queue 信息查询 (GPU_IOCTL_QUERY_QUEUE) */
struct gpu_queue_info_args {
  uint64_t queue_handle;       /* INPUT: Queue 句柄 */
  uint32_t queue_type;         /* OUT: Queue 类型 */
  uint32_t queue_id;           /* OUT: 内部 ID */
  uint64_t doorbell_offset;    /* OUT: Doorbell offset */
  uint64_t ring_addr;          /* OUT: Ring Buffer 共享内存地址 */
  uint32_t ring_size;          /* OUT: Ring Buffer 大小 */
  uint32_t pending_count;      /* OUT: 待处理 entry 数 */
};

#ifdef __cplusplus
}
#endif
