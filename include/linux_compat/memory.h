#pragma once

#include <cstdlib>
#include <cstring>
#include <new>
#include "types.h"

// Linux内核内存管理函数的用户态模拟实现

// 内存分配标志定义
#define GFP_KERNEL      0x00000001
#define GFP_ATOMIC      0x00000002
#define GFP_USER        0x00000003
#define GFP_DMA         0x00000004
#define GFP_HIGHUSER    0x00000005

// kmalloc/kfree 模拟实现
static inline void *kmalloc(size_t size, int flags) {
    (void)flags; // 忽略分配标志，因为我们是在用户态模拟
    if (size == 0) {
        return nullptr;
    }
    
    void *ptr = malloc(size);
    return ptr;
}

static inline void *kzalloc(size_t size, int flags) {
    (void)flags; // 忽略分配标志
    if (size == 0) {
        return nullptr;
    }
    
    void *ptr = calloc(1, size);
    return ptr;
}

static inline void *kcalloc(size_t n, size_t size, int flags) {
    (void)flags; // 忽略分配标志
    if (n == 0 || size == 0) {
        return nullptr;
    }
    
    // 检查溢出
    if (n > SIZE_MAX / size) {
        return nullptr;
    }
    
    void *ptr = calloc(n, size);
    return ptr;
}

static inline void kfree(const void *ptr) {
    free((void *)ptr);
}

// vmalloc/vfree 模拟实现（在用户态中，vmalloc和kmalloc没有本质区别）
static inline void *vmalloc(unsigned long size) {
    if (size == 0) {
        return nullptr;
    }
    
    void *ptr = malloc(size);
    return ptr;
}

static inline void vfree(const void *ptr) {
    free((void *)ptr);
}

// 内存拷贝函数（兼容内核函数名）
static inline void *memcpy_toio(void *dest, const void *src, size_t n) {
    return memcpy(dest, src, n);
}

static inline void *memcpy_fromio(void *dest, const void *src, size_t n) {
    return memcpy(dest, src, n);
}

// 内存设置函数
static inline void *memset_io(void *s, int c, size_t n) {
    return memset(s, c, n);
}

// 页面分配相关（简化模拟）
#define PAGE_SHIFT      12
#define PAGE_SIZE       (1UL << PAGE_SHIFT)
#define PAGE_MASK       (~(PAGE_SIZE - 1))

#define round_up(x, y) ((((x) + (y) - 1) / (y)) * (y))
#define round_down(x, y) (((x) / (y)) * (y))

// 物理地址和虚拟地址转换（在用户态模拟中简化处理）
static inline unsigned long __pa(void *x) {
    // 在用户态，直接返回地址值作为"物理"地址
    return (unsigned long)(uintptr_t)x;
}

static inline void *__va(unsigned long x) {
    // 在用户态，直接返回地址值
    return (void *)(uintptr_t)x;
}