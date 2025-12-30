#pragma once

#include <cstdint>
#include <cstddef>
#include <sys/types.h>

// 基础数据类型定义，兼容Linux内核类型
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using s8 = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;

using __u8 = uint8_t;
using __u16 = uint16_t;
using __u32 = uint32_t;
using __u64 = uint64_t;

using __s8 = int8_t;
using __s16 = int16_t;
using __s32 = int32_t;
using __s64 = int64_t;

// Linux内核中常用的类型定义
typedef uint32_t __kernel_size_t;
typedef int32_t __kernel_ssize_t;
typedef uint64_t __kernel_loff_t;

// 与内核兼容的布尔类型
typedef int bool;
#define true 1
#define false 0

// 常用的NULL定义
#ifndef NULL
#define NULL ((void *)0)
#endif

// Linux内核中的常用宏定义
#define __user      // 用户空间指针标记（在用户态模拟中不需要特殊处理）
#define __kernel    // 内核空间指针标记（在用户态模拟中不需要特殊处理）

// 用于兼容的offsetof和container_of
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#define container_of(ptr, type, member) ({                      \
        const typeof(((type *)0)->member) * __mptr = (ptr);    \
        (type *)((char *)__mptr - offsetof(type, member)); })

// Linux内核中的链表结构
struct list_head {
    struct list_head *next, *prev;
};

// Linux内核中常用的错误码
#define MAX_ERRNO   4095
#define IS_ERR_VALUE(x) ((x) >= (unsigned long)-MAX_ERRNO)

static inline void *ERR_PTR(long error) {
    return (void *)error;
}

static inline long PTR_ERR(const void *ptr) {
    return (long)ptr;
}

static inline bool IS_ERR(const void *ptr) {
    return IS_ERR_VALUE((unsigned long)ptr);
}

static inline bool IS_ERR_OR_NULL(const void *ptr) {
    return (!ptr) || IS_ERR_VALUE((unsigned long)ptr);
}

// 内核内存分配相关的错误码
#define ENOMEM          12  /* Out of memory */
#define EFAULT          14  /* Bad address */
#define EINVAL          22  /* Invalid argument */