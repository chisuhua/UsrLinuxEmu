#pragma once

#include <cstdio>
#include <cstdarg>

// Linux调试和日志接口兼容层

// 日志级别定义
#define KERN_EMERG   "<0>"   // 系统不可用
#define KERN_ALERT   "<1>"   // 必须立即采取行动
#define KERN_CRIT    "<2>"   // 严重错误
#define KERN_ERR     "<3>"   // 错误
#define KERN_WARNING "<4>"   // 警告
#define KERN_NOTICE  "<5>"   // 一般信息（重要）
#define KERN_INFO    "<6>"   // 一般信息
#define KERN_DEBUG   "<7>"   // 调试信息
#define KERN_DEFAULT "<d>"   // 默认级别
#define KERN_CONT    "<c>"   // 续行

// printk模拟（用户态输出到stderr）
static inline int printk(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = vfprintf(stderr, fmt, args);
    va_end(args);
    return ret;
}

// pr_* 系列宏
#define pr_emerg(fmt, ...)   fprintf(stderr, KERN_EMERG fmt, ##__VA_ARGS__)
#define pr_alert(fmt, ...)   fprintf(stderr, KERN_ALERT fmt, ##__VA_ARGS__)
#define pr_crit(fmt, ...)    fprintf(stderr, KERN_CRIT fmt, ##__VA_ARGS__)
#define pr_err(fmt, ...)     fprintf(stderr, KERN_ERR fmt, ##__VA_ARGS__)
#define pr_warn(fmt, ...)    fprintf(stderr, KERN_WARNING fmt, ##__VA_ARGS__)
#define pr_notice(fmt, ...)  fprintf(stderr, KERN_NOTICE fmt, ##__VA_ARGS__)
#define pr_info(fmt, ...)    fprintf(stdout, KERN_INFO fmt, ##__VA_ARGS__)
#define pr_debug(fmt, ...)   fprintf(stderr, KERN_DEBUG fmt, ##__VA_ARGS__)
#define pr_cont(fmt, ...)    fprintf(stderr, fmt, ##__VA_ARGS__)

// dev_* 系列宏（device相关日志）
#define dev_emerg(dev, fmt, ...)  fprintf(stderr, KERN_EMERG "[%s] " fmt, \
    (dev) ? "dev" : "null", ##__VA_ARGS__)
#define dev_alert(dev, fmt, ...)  fprintf(stderr, KERN_ALERT "[%s] " fmt, \
    (dev) ? "dev" : "null", ##__VA_ARGS__)
#define dev_crit(dev, fmt, ...)   fprintf(stderr, KERN_CRIT "[%s] " fmt, \
    (dev) ? "dev" : "null", ##__VA_ARGS__)
#define dev_err(dev, fmt, ...)    fprintf(stderr, KERN_ERR "[%s] " fmt, \
    (dev) ? "dev" : "null", ##__VA_ARGS__)
#define dev_warn(dev, fmt, ...)   fprintf(stderr, KERN_WARNING "[%s] " fmt, \
    (dev) ? "dev" : "null", ##__VA_ARGS__)
#define dev_notice(dev, fmt, ...) fprintf(stderr, KERN_NOTICE "[%s] " fmt, \
    (dev) ? "dev" : "null", ##__VA_ARGS__)
#define dev_info(dev, fmt, ...)   fprintf(stdout, KERN_INFO "[%s] " fmt, \
    (dev) ? "dev" : "null", ##__VA_ARGS__)
#define dev_dbg(dev, fmt, ...)    fprintf(stderr, KERN_DEBUG "[%s] " fmt, \
    (dev) ? "dev" : "null", ##__VA_ARGS__)

// WARN_ON系列宏
#define WARN_ON(condition) ({                                           \
    int __ret_warn_on = !!(condition);                                  \
    if (__ret_warn_on)                                                  \
        fprintf(stderr, "WARNING: at %s:%d\n", __FILE__, __LINE__);    \
    __ret_warn_on;                                                      \
})

#define WARN_ON_ONCE(condition) WARN_ON(condition)

#define BUG() do {                                                      \
    fprintf(stderr, "BUG: at %s:%d\n", __FILE__, __LINE__);            \
    abort();                                                            \
} while (0)

#define BUG_ON(condition) do {                                          \
    if (condition) BUG();                                               \
} while (0)

// WARN宏
#define WARN(condition, fmt, ...) ({                                    \
    int __ret_warn = !!(condition);                                     \
    if (__ret_warn)                                                     \
        fprintf(stderr, "WARNING: " fmt " at %s:%d\n",                 \
                ##__VA_ARGS__, __FILE__, __LINE__);                     \
    __ret_warn;                                                         \
})

// 断言宏
#define ASSERT(x) do {                                                  \
    if (!(x)) {                                                         \
        fprintf(stderr, "ASSERT failed: %s at %s:%d\n",                \
                #x, __FILE__, __LINE__);                                \
        abort();                                                        \
    }                                                                   \
} while (0)

// dump_stack（用户态为空操作）
static inline void dump_stack(void) {}
