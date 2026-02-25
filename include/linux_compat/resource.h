#pragma once

#include <cstdlib>
#include <cstring>
#include "types.h"

// Linux资源管理兼容层

// 资源大小类型
typedef unsigned long resource_size_t;

// 资源标志
#define IORESOURCE_BITS     0x000000ff  // 总线相关位
#define IORESOURCE_TYPE_BITS 0x00001f00 // 资源类型
#define IORESOURCE_IO       0x00000100  // PCI/ISA I/O端口
#define IORESOURCE_MEM      0x00000200  // 内存区域
#define IORESOURCE_REG      0x00000300  // 寄存器偏移
#define IORESOURCE_IRQ      0x00000400  // 中断
#define IORESOURCE_DMA      0x00000800  // DMA通道
#define IORESOURCE_BUS      0x00001000  // 总线号

#define IORESOURCE_PREFETCH 0x00002000  // 可预取内存
#define IORESOURCE_READONLY 0x00004000  // 只读资源
#define IORESOURCE_DISABLED 0x10000000  // 资源已禁用
#define IORESOURCE_UNSET    0x20000000  // 未设置的资源
#define IORESOURCE_AUTO     0x40000000  // 自动分配

// 资源结构
struct resource {
    resource_size_t start;
    resource_size_t end;
    const char     *name;
    unsigned long   flags;
    unsigned long   desc;
    struct resource *parent;
    struct resource *sibling;
    struct resource *child;
};

// I/O内存映射
static inline void *ioremap(unsigned long phys_addr, unsigned long size) {
    (void)size;
    // 在用户态，直接返回物理地址作为虚拟地址（模拟用途）
    return (void *)(uintptr_t)phys_addr;
}

static inline void *ioremap_nocache(unsigned long phys_addr, unsigned long size) {
    return ioremap(phys_addr, size);
}

static inline void *ioremap_wc(unsigned long phys_addr, unsigned long size) {
    return ioremap(phys_addr, size);
}

static inline void iounmap(volatile void *addr) {
    (void)addr;
    // 在用户态，不需要解除映射
}

// 资源追踪表（用于在release时正确释放内存）
#define MAX_RESOURCE_ENTRIES 64
static struct {
    unsigned long   start;
    unsigned long   end;
    struct resource *res;
} resource_table[MAX_RESOURCE_ENTRIES];
static int resource_table_count = 0;

static inline void resource_table_add(unsigned long start, unsigned long end,
                                      struct resource *res) {
    if (resource_table_count < MAX_RESOURCE_ENTRIES) {
        resource_table[resource_table_count].start = start;
        resource_table[resource_table_count].end   = end;
        resource_table[resource_table_count].res   = res;
        resource_table_count++;
    }
}

static inline struct resource *resource_table_remove(unsigned long start,
                                                      unsigned long n) {
    for (int i = 0; i < resource_table_count; i++) {
        if (resource_table[i].start == start &&
            resource_table[i].end == start + n - 1) {
            struct resource *res = resource_table[i].res;
            // 移除条目
            for (int j = i; j < resource_table_count - 1; j++) {
                resource_table[j] = resource_table[j + 1];
            }
            resource_table_count--;
            return res;
        }
    }
    return nullptr;
}

// 内存区域请求/释放（模拟）
static inline struct resource *request_mem_region(unsigned long start,
                                                   unsigned long n,
                                                   const char *name) {
    struct resource *res = (struct resource *)malloc(sizeof(struct resource));
    if (!res) return nullptr;
    memset(res, 0, sizeof(struct resource));
    res->start = (resource_size_t)start;
    res->end   = (resource_size_t)(start + n - 1);
    res->name  = name;
    res->flags = IORESOURCE_MEM;
    resource_table_add(start, start + n - 1, res);
    return res;
}

static inline void release_mem_region(unsigned long start, unsigned long n) {
    struct resource *res = resource_table_remove(start, n);
    free(res);
}

// I/O端口区域请求/释放（模拟）
static inline struct resource *request_region(unsigned long start,
                                               unsigned long n,
                                               const char *name) {
    struct resource *res = (struct resource *)malloc(sizeof(struct resource));
    if (!res) return nullptr;
    memset(res, 0, sizeof(struct resource));
    res->start = (resource_size_t)start;
    res->end   = (resource_size_t)(start + n - 1);
    res->name  = name;
    res->flags = IORESOURCE_IO;
    resource_table_add(start, start + n - 1, res);
    return res;
}

static inline void release_region(unsigned long start, unsigned long n) {
    struct resource *res = resource_table_remove(start, n);
    free(res);
}

// I/O读写操作（用户态简化实现）
static inline uint8_t  readb(const volatile void *addr) {
    return *(const volatile uint8_t *)addr;
}
static inline uint16_t readw(const volatile void *addr) {
    return *(const volatile uint16_t *)addr;
}
static inline uint32_t readl(const volatile void *addr) {
    return *(const volatile uint32_t *)addr;
}
static inline uint64_t readq(const volatile void *addr) {
    return *(const volatile uint64_t *)addr;
}

static inline void writeb(uint8_t val, volatile void *addr) {
    *(volatile uint8_t *)addr = val;
}
static inline void writew(uint16_t val, volatile void *addr) {
    *(volatile uint16_t *)addr = val;
}
static inline void writel(uint32_t val, volatile void *addr) {
    *(volatile uint32_t *)addr = val;
}
static inline void writeq(uint64_t val, volatile void *addr) {
    *(volatile uint64_t *)addr = val;
}
