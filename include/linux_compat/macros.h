#pragma once

// Linux内核常用宏定义

// 条件宏
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

// 位操作宏
#define BIT(x) (1UL << (x))
#define BITS_PER_BYTE 8
#define BITS_PER_LONG (sizeof(long) * BITS_PER_BYTE)

// 对齐相关宏
#define ALIGN(x, a) (((x) + (a) - 1) & ~((a) - 1))
#define ALIGN_DOWN(x, a) ((x) & ~((a) - 1))

// 内存相关宏
#define PAGE_SIZE 4096
#define PAGE_MASK (~(PAGE_SIZE - 1))
#define PAGE_ALIGN(addr) (((addr) + PAGE_SIZE - 1) & PAGE_MASK)

// 编译器相关宏
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define barrier() __asm__ __volatile__("" : : : "memory")

// 函数属性宏
#define __packed __attribute__((packed))
#define __aligned(x) __attribute__((aligned(x)))
#define __maybe_unused __attribute__((unused))
#define __used __attribute__((__used__))

// 条件检查宏
#ifndef min
#define min(x, y) ({                \
    typeof(x) _min1 = (x);          \
    typeof(y) _min2 = (y);          \
    (void) (&_min1 == &_min2);      \
    _min1 < _min2 ? _min1 : _min2; })
#endif

#ifndef max
#define max(x, y) ({                \
    typeof(x) _max1 = (x);          \
    typeof(y) _max2 = (y);          \
    (void) (&_max1 == &_max2);      \
    _max1 > _max2 ? _max1 : _max2; })
#endif

#ifndef clamp
#define clamp(val, min_val, max_val) ({         \
    typeof(val) __val = (val);                  \
    typeof(min_val) __min = (min_val);          \
    typeof(max_val) __max = (max_val);          \
    (void) (&__val == &__min);                  \
    (void) (&__val == &__max);                  \
    __val = __val < __min ? __min : __val;      \
    __val > __max ? __max : __val; })
#endif

// 交换宏
#define swap(a, b) \
    do { typeof(a) __tmp = (a); (a) = (b); (b) = __tmp; } while (0)

// 常用常量定义
#define U8_MAX  ((uint8_t)~0U)
#define S8_MAX  ((int8_t)(U8_MAX >> 1))
#define S8_MIN  ((int8_t)(-S8_MAX - 1))
#define U16_MAX ((uint16_t)~0U)
#define S16_MAX ((int16_t)(U16_MAX >> 1))
#define S16_MIN ((int16_t)(-S16_MAX - 1))
#define U32_MAX ((uint32_t)~0U)
#define S32_MAX ((int32_t)(U32_MAX >> 1))
#define S32_MIN ((int32_t)(-S32_MAX - 1))
#define U64_MAX ((uint64_t)~0ULL)
#define S64_MAX ((int64_t)(U64_MAX >> 1))
#define S64_MIN ((int64_t)(-S64_MAX - 1))

// 内核版本相关宏
#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
#define LINUX_VERSION_CODE KERNEL_VERSION(4, 4, 0)