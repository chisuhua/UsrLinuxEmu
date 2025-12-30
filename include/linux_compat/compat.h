#pragma once

// 统一的Linux兼容层头文件
// 包含所有兼容层定义

#include "types.h"
#include "macros.h"
#include "ioctl.h"
#include "memory.h"

// 一些常用的内核头文件别名
#include "types.h"  // 替代 <linux/types.h>
#include "macros.h" // 包含常用宏定义
#include "memory.h" // 替代 <linux/slab.h> 和 <linux/vmalloc.h>
#include "ioctl.h"  // 替代 <linux/ioctl.h>

// 添加一些常用的内核API别名，便于迁移现有代码
#define kmalloc(size, flags) kmalloc(size, flags)
#define kzalloc(size, flags) kzalloc(size, flags)
#define kcalloc(n, size, flags) kcalloc(n, size, flags)
#define kfree(ptr) kfree(ptr)

#define vmalloc(size) vmalloc(size)
#define vfree(ptr) vfree(ptr)

// 常用的数据类型别名
typedef int bool_t;
#define TRUE 1
#define FALSE 0