/*
 * gpu_buddy.h — Buddy Allocator (pure C, zero-dependency)
 *
 * 纯地址运算算法，不进行任何内存分配，不自加锁。
 * 调用者负责外部同步。
 *
 * 可直接嵌入 Linux 内核驱动中使用。
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========== 配置常量 ========== */

#define GPU_BUDDY_MIN_BLOCK_SHIFT 12  /* 4KB */
#define GPU_BUDDY_MIN_BLOCK_SIZE  (1ULL << GPU_BUDDY_MIN_BLOCK_SHIFT)
#define GPU_BUDDY_MAX_ORDER       21  /* 2^21 * 4KB = 8GB */

/* 最大并发分配数（调用者设置 records 数组大小） */
#define GPU_BUDDY_MAX_RECORDS     4096

/* ========== 数据结构 ========== */

/* 空闲链表节点（嵌入在 buddy 结构体中的固定池） */
struct gpu_buddy_block {
    uint64_t addr;
    struct gpu_buddy_block *next;
    struct gpu_buddy_block *prev;
};

/* 已分配块追踪记录 */
struct gpu_buddy_record {
    uint64_t addr;
    uint64_t size;
    int      order;
    bool     used;   /* true = 槽位被占用 */
};

/* Buddy Allocator 主结构体（调用者分配，不自己 malloc） */
struct gpu_buddy {
    /* 内存池信息 */
    uint64_t base_addr;
    uint64_t pool_size;
    int      max_order;

    /* 空闲链表 (free_lists[order] 指向该 order 的空闲块链表头) */
    struct gpu_buddy_block *free_lists[GPU_BUDDY_MAX_ORDER + 1];

    /* 空闲链表节点池（避免动态分配） */
    struct gpu_buddy_block block_pool[GPU_BUDDY_MAX_RECORDS + GPU_BUDDY_MAX_ORDER + 1];
    int block_pool_used;

    /* 已分配块追踪 */
    struct gpu_buddy_record records[GPU_BUDDY_MAX_RECORDS];
    int record_count;
};

/* ========== API 函数 ========== */

/**
 * @brief 初始化 buddy allocator
 * @param buddy   分配器结构体（调用者分配）
 * @param base    内存基地址
 * @param size    内存大小（必须为 GPU_BUDDY_MIN_BLOCK_SIZE 的整数倍）
 */
void gpu_buddy_init(struct gpu_buddy *buddy, uint64_t base, uint64_t size);

/**
 * @brief 分配内存块
 * @param buddy   分配器
 * @param size    请求大小
 * @param out_addr 输出：分配到的地址
 * @return 0=成功，-ENOMEM=内存不足，-EINVAL=参数错误
 */
int  gpu_buddy_alloc(struct gpu_buddy *buddy, uint64_t size, uint64_t *out_addr);

/**
 * @brief 释放内存块
 * @param buddy   分配器
 * @param addr    要释放的地址（必须由 gpu_buddy_alloc 返回）
 * @return 0=成功，-EINVAL=无效地址
 */
int  gpu_buddy_free(struct gpu_buddy *buddy, uint64_t addr);

/**
 * @brief 重置分配器（释放所有块）
 * @param buddy   分配器
 */
void gpu_buddy_reset(struct gpu_buddy *buddy);

/**
 * @brief 查询剩余空闲内存
 * @param buddy   分配器
 * @return 剩余空闲字节数
 */
uint64_t gpu_buddy_free_size(const struct gpu_buddy *buddy);

/**
 * @brief 查询已分配块数
 * @param buddy   分配器
 * @return 当前已分配的块数
 */
int gpu_buddy_allocated_count(const struct gpu_buddy *buddy);

#ifdef __cplusplus
}
#endif
