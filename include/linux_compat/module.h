#pragma once

// Linux模块加载兼容层

// 模块属性宏（用户态下为空操作）
#define MODULE_LICENSE(license)         // 模块许可证声明
#define MODULE_AUTHOR(author)           // 模块作者声明
#define MODULE_DESCRIPTION(description) // 模块描述声明
#define MODULE_VERSION(version)         // 模块版本声明
#define MODULE_ALIAS(alias)             // 模块别名声明

// 模块参数（简化，不实现真实参数解析）
#define module_param(name, type, perm)  // 模块参数声明
#define MODULE_PARM_DESC(name, desc)    // 模块参数描述

// THIS_MODULE（用户态为NULL）
#define THIS_MODULE ((struct module *)0)

// 符号导出宏（用户态为空操作）
#define EXPORT_SYMBOL(sym)          // 导出符号
#define EXPORT_SYMBOL_GPL(sym)      // 仅GPL许可导出符号

// 模块初始化/退出宏
// module_init(fn): 声明模块初始化函数，在用户态下直接定义为函数指针保存
// module_exit(fn): 声明模块退出函数，在用户态下直接定义为函数指针保存

typedef int (*module_init_fn)(void);
typedef void (*module_exit_fn)(void);

#ifdef USR_LINUX_EMU
// 用户态模拟：保存初始化/退出函数指针以便测试框架调用
extern module_init_fn __emu_module_init_fn;
extern module_exit_fn __emu_module_exit_fn;

#define module_init(fn)                                     \
    static int __module_init_wrapper(void) { return fn(); } \
    module_init_fn __emu_module_init_fn = __module_init_wrapper

#define module_exit(fn)                                      \
    static void __module_exit_wrapper(void) { fn(); }        \
    module_exit_fn __emu_module_exit_fn = __module_exit_wrapper

#else
// 非USR_LINUX_EMU环境（例如用于编译兼容头文件的普通代码）：提供空定义
#define module_init(fn)     // 用于兼容
#define module_exit(fn)     // 用于兼容
#endif

// __init/__exit函数属性标记（用户态下无特殊含义）
#define __init
#define __exit
#define __devinit
#define __devexit
#define __devinitdata

// printk（如果未包含debug.h）
#ifndef KERN_INFO
#include "debug.h"
#endif
