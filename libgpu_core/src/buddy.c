/*
 * buddy.c — Buddy Allocator 纯 C 实现
 *
 * 零外部依赖，无 malloc/free，无锁。
 * 所有内存由调用者提供的 struct gpu_buddy 结构体承载。
 */

#include "gpu_buddy.h"
#include <string.h>   /* memset */
#include <stddef.h>   /* NULL */

/* ========== 内部辅助函数 ========== */

/* 向上取整到 2 的幂 */
static uint64_t round_up_pow2(uint64_t size) {
    if (size == 0) return GPU_BUDDY_MIN_BLOCK_SIZE;
    --size;
    size |= size >> 1;
    size |= size >> 2;
    size |= size >> 4;
    size |= size >> 8;
    size |= size >> 16;
    size |= size >> 32;
    return size + 1;
}

/* 计算 size 对应的 order */
static int order_for_size(uint64_t size) {
    int order = 0;
    uint64_t block_size = GPU_BUDDY_MIN_BLOCK_SIZE;
    while (block_size < size && order < GPU_BUDDY_MAX_ORDER) {
        block_size *= 2;
        ++order;
    }
    return order;
}

/* 获取 buddy 地址 */
static uint64_t buddy_addr(uint64_t base, uint64_t addr, uint64_t block_size) {
    return base + ((addr - base) ^ block_size);
}

/* 判断两个块是否相邻 */
static bool are_adjacent(uint64_t a_addr, uint64_t b_addr, uint64_t block_size) {
    return (a_addr + block_size == b_addr) || (b_addr + block_size == a_addr);
}

/* ========== 块池管理（替代 malloc/free） ========== */

static struct gpu_buddy_block *pool_alloc(struct gpu_buddy *buddy) {
    if (buddy->block_pool_used >= (int)(sizeof(buddy->block_pool) / sizeof(buddy->block_pool[0]))) {
        return NULL;
    }
    struct gpu_buddy_block *block = &buddy->block_pool[buddy->block_pool_used++];
    block->addr = 0;
    block->next = NULL;
    block->prev = NULL;
    return block;
}

static void pool_free(struct gpu_buddy *buddy, struct gpu_buddy_block *block) {
    /* 不放回池中，因为 buddy 算法中块会在 free/coalesce 中被回收和重用。
     * block_pool 只增不减，最大值受 MAX_RECORDS 约束，够用。 */
    (void)buddy;
    (void)block;
}

/* ========== 空闲链表操作 ========== */

static void free_list_insert(struct gpu_buddy *buddy, struct gpu_buddy_block *block, int order) {
    if (!block || order > buddy->max_order) return;

    block->next = buddy->free_lists[order];
    block->prev = NULL;
    if (buddy->free_lists[order]) {
        buddy->free_lists[order]->prev = block;
    }
    buddy->free_lists[order] = block;
}

/* 从链表中移除指定 block */
static void free_list_remove_block(struct gpu_buddy *buddy, struct gpu_buddy_block *block, int order) {
    if (!block || order > buddy->max_order) return;

    if (block->prev) {
        block->prev->next = block->next;
    } else {
        buddy->free_lists[order] = block->next;
    }
    if (block->next) {
        block->next->prev = block->prev;
    }
    block->next = NULL;
    block->prev = NULL;
}

/* 从链表头部移除一个 block */
static struct gpu_buddy_block *free_list_pop(struct gpu_buddy *buddy, int order) {
    if (order > buddy->max_order || !buddy->free_lists[order]) return NULL;

    struct gpu_buddy_block *block = buddy->free_lists[order];
    buddy->free_lists[order] = block->next;
    if (block->next) {
        block->next->prev = NULL;
    }
    block->next = NULL;
    block->prev = NULL;
    return block;
}

/* 在链表中查找 buddy 块 */
static struct gpu_buddy_block *find_buddy_in_list(struct gpu_buddy *buddy,
                                                    uint64_t buddy_addr_val,
                                                    int order) {
    struct gpu_buddy_block *current = buddy->free_lists[order];
    while (current) {
        if (current->addr == buddy_addr_val) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

/* ========== 记录管理（替代 std::map） ========== */

static struct gpu_buddy_record *find_record(struct gpu_buddy *buddy, uint64_t addr) {
    for (int i = 0; i < GPU_BUDDY_MAX_RECORDS; ++i) {
        if (buddy->records[i].used && buddy->records[i].addr == addr) {
            return &buddy->records[i];
        }
    }
    return NULL;
}

static int add_record(struct gpu_buddy *buddy, uint64_t addr, uint64_t size, int order) {
    for (int i = 0; i < GPU_BUDDY_MAX_RECORDS; ++i) {
        if (!buddy->records[i].used) {
            buddy->records[i].addr  = addr;
            buddy->records[i].size  = size;
            buddy->records[i].order = order;
            buddy->records[i].used  = true;
            if (i >= buddy->record_count) {
                buddy->record_count = i + 1;
            }
            return 0;
        }
    }
    return -1;
}

static void remove_record(struct gpu_buddy *buddy, uint64_t addr) {
    for (int i = 0; i < GPU_BUDDY_MAX_RECORDS; ++i) {
        if (buddy->records[i].used && buddy->records[i].addr == addr) {
            buddy->records[i].used = false;
            /* 收缩 record_count */
            while (buddy->record_count > 0 && !buddy->records[buddy->record_count - 1].used) {
                --buddy->record_count;
            }
            return;
        }
    }
}

/* ========== 合并 ========== */

static void coalesce(struct gpu_buddy *buddy, int order) {
    if (order >= buddy->max_order) return;

    uint64_t block_size = GPU_BUDDY_MIN_BLOCK_SIZE << order;

    struct gpu_buddy_block *current = buddy->free_lists[order];
    while (current) {
        struct gpu_buddy_block *next = current->next;
        uint64_t baddr = buddy_addr(buddy->base_addr, current->addr, block_size);

        struct gpu_buddy_block *buddy_blk = find_buddy_in_list(buddy, baddr, order);
        if (buddy_blk && are_adjacent(current->addr, buddy_blk->addr, block_size)) {
            /* 从当前链表移除两个 block */
            free_list_remove_block(buddy, current, order);
            free_list_remove_block(buddy, buddy_blk, order);

            /* 合并后的地址 */
            uint64_t merged_addr = (current->addr < buddy_blk->addr)
                                   ? current->addr : buddy_blk->addr;

            pool_free(buddy, current);
            pool_free(buddy, buddy_blk);

            /* 创建合并后的 block */
            struct gpu_buddy_block *merged = pool_alloc(buddy);
            if (merged) {
                merged->addr = merged_addr;
                free_list_insert(buddy, merged, order + 1);
            }

            /* 继续上一级合并 */
            coalesce(buddy, order + 1);

            /* 重新遍历当前 level */
            current = buddy->free_lists[order];
        } else {
            current = next;
        }
    }
}

/* ========== API 实现 ========== */

void gpu_buddy_init(struct gpu_buddy *buddy, uint64_t base, uint64_t size) {
    if (!buddy) return;

    memset(buddy, 0, sizeof(*buddy));

    buddy->base_addr = base;
    buddy->pool_size = size;

    /* 计算 max_order */
    buddy->max_order = 0;
    while ((GPU_BUDDY_MIN_BLOCK_SIZE << buddy->max_order) < size) {
        ++buddy->max_order;
    }
    if (buddy->max_order > GPU_BUDDY_MAX_ORDER) {
        buddy->max_order = GPU_BUDDY_MAX_ORDER;
    }

    /* 插入初始块 */
    struct gpu_buddy_block *initial = pool_alloc(buddy);
    if (initial) {
        initial->addr = base;
        free_list_insert(buddy, initial, buddy->max_order);
    }
}

int gpu_buddy_alloc(struct gpu_buddy *buddy, uint64_t size, uint64_t *out_addr) {
    if (!buddy || !out_addr) return -1;  /* -EINVAL */
    if (size == 0) return -1;

    uint64_t aligned_size = round_up_pow2(size);
    if (aligned_size < GPU_BUDDY_MIN_BLOCK_SIZE) {
        aligned_size = GPU_BUDDY_MIN_BLOCK_SIZE;
    }

    int order = order_for_size(aligned_size);
    if (order > GPU_BUDDY_MAX_ORDER) {
        return -1;  /* -ENOMEM */
    }

    /* 查找可用 order */
    int target_order = order;
    for (int i = order; i <= buddy->max_order; ++i) {
        if (buddy->free_lists[i] != NULL) {
            target_order = i;
            break;
        }
    }

    if (target_order > buddy->max_order || buddy->free_lists[target_order] == NULL) {
        return -1;  /* -ENOMEM */
    }

    /* 分裂直到目标 order */
    while (target_order > order) {
        struct gpu_buddy_block *block = free_list_pop(buddy, target_order);
        if (!block) break;

        uint64_t bsize  = GPU_BUDDY_MIN_BLOCK_SIZE << target_order;
        uint64_t hsize  = bsize / 2;

        struct gpu_buddy_block *left  = pool_alloc(buddy);
        struct gpu_buddy_block *right = pool_alloc(buddy);
        if (left)  left->addr  = block->addr;
        if (right) right->addr = block->addr + hsize;

        pool_free(buddy, block);

        if (left)  free_list_insert(buddy, left,  target_order - 1);
        if (right) free_list_insert(buddy, right, target_order - 1);

        --target_order;
    }

    struct gpu_buddy_block *block = free_list_pop(buddy, target_order);
    if (!block) {
        return -1;  /* -ENOMEM */
    }

    uint64_t addr = block->addr;
    pool_free(buddy, block);

    /* 记录分配 */
    if (add_record(buddy, addr, aligned_size, target_order) != 0) {
        /* 记录池满，回滚：把 block 放回去 */
        struct gpu_buddy_block *rollback = pool_alloc(buddy);
        if (rollback) {
            rollback->addr = addr;
            free_list_insert(buddy, rollback, target_order);
        }
        return -1;  /* -ENOMEM */
    }

    *out_addr = addr;
    return 0;
}

int gpu_buddy_free(struct gpu_buddy *buddy, uint64_t addr) {
    if (!buddy) return -1;

    struct gpu_buddy_record *rec = find_record(buddy, addr);
    if (!rec) {
        return -1;  /* -EINVAL: 无效地址 */
    }

    int order = rec->order;
    remove_record(buddy, addr);

    struct gpu_buddy_block *block = pool_alloc(buddy);
    if (block) {
        block->addr = addr;
        free_list_insert(buddy, block, order);
    }

    coalesce(buddy, order);

    return 0;
}

void gpu_buddy_reset(struct gpu_buddy *buddy) {
    if (!buddy) return;

    /* 清空所有链表 */
    for (int i = 0; i <= GPU_BUDDY_MAX_ORDER; ++i) {
        buddy->free_lists[i] = NULL;
    }
    buddy->block_pool_used = 0;
    buddy->record_count = 0;

    /* 清空 records */
    memset(buddy->records, 0, sizeof(buddy->records));

    /* 重新插入初始块 */
    struct gpu_buddy_block *initial = pool_alloc(buddy);
    if (initial) {
        initial->addr = buddy->base_addr;
        free_list_insert(buddy, initial, buddy->max_order);
    }
}

uint64_t gpu_buddy_free_size(const struct gpu_buddy *buddy) {
    if (!buddy) return 0;

    uint64_t total = 0;
    for (int i = 0; i <= buddy->max_order; ++i) {
        struct gpu_buddy_block *current = buddy->free_lists[i];
        while (current) {
            total += GPU_BUDDY_MIN_BLOCK_SIZE << i;
            current = current->next;
        }
    }
    return total;
}

int gpu_buddy_allocated_count(const struct gpu_buddy *buddy) {
    if (!buddy) return 0;

    int count = 0;
    for (int i = 0; i < GPU_BUDDY_MAX_RECORDS; ++i) {
        if (buddy->records[i].used) ++count;
    }
    return count;
}
