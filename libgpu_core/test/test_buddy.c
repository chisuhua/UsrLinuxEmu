/*
 * test_buddy.c — 独立 C 单元测试（不依赖任何 C++ 框架）
 *
 * 编译: gcc -o test_buddy test_buddy.c ../src/buddy.c -I../include -I.
 * 运行: ./test_buddy
 */

#include "gpu_buddy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* 简化的测试框架 */
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { \
    printf("  TEST: %s ... ", #name); \
    if (test_##name()) { \
        printf("PASS\n"); \
        tests_passed++; \
    } else { \
        printf("FAIL\n"); \
        tests_failed++; \
    } \
} while(0)

/* ========== 测试用例 ========== */

static int test_init_free(void) {
    struct gpu_buddy buddy;
    gpu_buddy_init(&buddy, 0x10000000, 64ULL * 1024 * 1024); /* 64MB pool */

    /* 初始状态：应该有且仅有一个最大 order 的块 */
    if (gpu_buddy_allocated_count(&buddy) != 0) return 0;
    if (gpu_buddy_free_size(&buddy) != 64ULL * 1024 * 1024) return 0;

    return 1;
}

static int test_alloc_small(void) {
    struct gpu_buddy buddy;
    gpu_buddy_init(&buddy, 0x10000000, 64ULL * 1024 * 1024);

    uint64_t addr = 0xDEAD;
    int ret = gpu_buddy_alloc(&buddy, 4096, &addr);
    if (ret != 0) return 0;
    /* 地址应在 pool 范围内 */
    if (addr < 0x10000000 || addr >= 0x10000000 + 64ULL*1024*1024) return 0;
    if (gpu_buddy_allocated_count(&buddy) != 1) return 0;

    return 1;
}

static int test_alloc_large(void) {
    struct gpu_buddy buddy;
    gpu_buddy_init(&buddy, 0x10000000, 64ULL * 1024 * 1024);

    uint64_t addr;
    int ret = gpu_buddy_alloc(&buddy, 32ULL * 1024 * 1024, &addr);
    if (ret != 0) return 0;
    if (addr < 0x10000000 || addr >= 0x10000000 + 64ULL*1024*1024) return 0;
    if (gpu_buddy_allocated_count(&buddy) != 1) return 0;

    return 1;
}

static int test_alloc_free(void) {
    struct gpu_buddy buddy;
    gpu_buddy_init(&buddy, 0x10000000, 64ULL * 1024 * 1024);

    uint64_t addr;
    int ret = gpu_buddy_alloc(&buddy, 4096, &addr);
    if (ret != 0) return 0;

    uint64_t before = gpu_buddy_free_size(&buddy);
    ret = gpu_buddy_free(&buddy, addr);
    if (ret != 0) return 0;
    if (gpu_buddy_allocated_count(&buddy) != 0) return 0;
    if (gpu_buddy_free_size(&buddy) != before + 4096) return 0;

    return 1;
}

static int test_alloc_coalesce(void) {
    struct gpu_buddy buddy;
    gpu_buddy_init(&buddy, 0x10000000, 64ULL * 1024 * 1024);

    uint64_t a1, a2;
    int ret;

    /* 分配两个 4KB 块 */
    ret = gpu_buddy_alloc(&buddy, 4096, &a1);
    if (ret != 0) return 0;

    ret = gpu_buddy_alloc(&buddy, 4096, &a2);
    if (ret != 0) return 0;

    /* 释放后合并 */
    ret = gpu_buddy_free(&buddy, a1);
    if (ret != 0) return 0;

    ret = gpu_buddy_free(&buddy, a2);
    if (ret != 0) return 0;

    /* 合并后应该完全恢复 */
    if (gpu_buddy_free_size(&buddy) != 64ULL * 1024 * 1024) return 0;
    if (gpu_buddy_allocated_count(&buddy) != 0) return 0;

    return 1;
}

static int test_alloc_oom(void) {
    struct gpu_buddy buddy;
    gpu_buddy_init(&buddy, 0x10000000, 64ULL * 1024 * 1024);

    uint64_t addr;
    /* 试图分配超过 pool 大小的内存 */
    int ret = gpu_buddy_alloc(&buddy, 128ULL * 1024 * 1024, &addr);
    if (ret == 0) return 0;  /* 应该失败 */

    return 1;
}

static int test_alloc_multiple(void) {
    struct gpu_buddy buddy;
    gpu_buddy_init(&buddy, 0x10000000, 64ULL * 1024 * 1024);

    uint64_t addrs[16];
    for (int i = 0; i < 16; ++i) {
        int ret = gpu_buddy_alloc(&buddy, 4096, &addrs[i]);
        if (ret != 0) return 0;
    }

    if (gpu_buddy_allocated_count(&buddy) != 16) return 0;

    /* 释放所有 */
    for (int i = 0; i < 16; ++i) {
        int ret = gpu_buddy_free(&buddy, addrs[i]);
        if (ret != 0) return 0;
    }

    if (gpu_buddy_free_size(&buddy) != 64ULL * 1024 * 1024) return 0;

    return 1;
}

static int test_invalid_free(void) {
    struct gpu_buddy buddy;
    gpu_buddy_init(&buddy, 0x10000000, 64ULL * 1024 * 1024);

    /* 释放从未分配的地址 */
    int ret = gpu_buddy_free(&buddy, 0xDEAD);
    if (ret == 0) return 0;  /* 应该失败 */

    return 1;
}

static int test_reset(void) {
    struct gpu_buddy buddy;
    gpu_buddy_init(&buddy, 0x10000000, 64ULL * 1024 * 1024);

    uint64_t a1, a2;
    gpu_buddy_alloc(&buddy, 4096, &a1);
    gpu_buddy_alloc(&buddy, 8192, &a2);

    gpu_buddy_reset(&buddy);

    if (gpu_buddy_allocated_count(&buddy) != 0) return 0;
    if (gpu_buddy_free_size(&buddy) != 64ULL * 1024 * 1024) return 0;

    return 1;
}

static int test_alloc_all(void) {
    struct gpu_buddy buddy;
    gpu_buddy_init(&buddy, 0x10000000, 64ULL * 1024 * 1024);

    uint64_t addr;
    /* 分配全部内存 */
    int ret = gpu_buddy_alloc(&buddy, 64ULL * 1024 * 1024, &addr);
    if (ret != 0) return 0;

    /* 应该无法再分配 */
    ret = gpu_buddy_alloc(&buddy, 4096, &addr);
    if (ret == 0) return 0;  /* 应该 OOM */

    /* 释放后又能分配了 */
    gpu_buddy_free(&buddy, addr);

    ret = gpu_buddy_alloc(&buddy, 4096, &addr);
    if (ret != 0) return 0;

    return 1;
}

static int test_get_free_size(void) {
    struct gpu_buddy buddy;
    gpu_buddy_init(&buddy, 0x10000000, 64ULL * 1024 * 1024);
    uint64_t full_size = 64ULL * 1024 * 1024;

    if (gpu_buddy_free_size(&buddy) != full_size) return 0;

    uint64_t a1;
    gpu_buddy_alloc(&buddy, 4096, &a1);
    if (gpu_buddy_free_size(&buddy) >= full_size) return 0;  /* 应该减少了 */

    gpu_buddy_free(&buddy, a1);
    if (gpu_buddy_free_size(&buddy) != full_size) return 0;

    return 1;
}

/* ========== 主函数 ========== */

int main(void) {
    printf("=== libgpu_core: gpu_buddy test suite ===\n\n");

    TEST(init_free);
    TEST(alloc_small);
    TEST(alloc_large);
    TEST(alloc_free);
    TEST(alloc_coalesce);
    TEST(alloc_oom);
    TEST(alloc_multiple);
    TEST(invalid_free);
    TEST(reset);
    TEST(alloc_all);
    TEST(get_free_size);

    printf("\n=== Results: %d passed, %d failed ===\n",
           tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
