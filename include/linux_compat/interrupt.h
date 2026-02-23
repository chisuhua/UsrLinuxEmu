#pragma once

#include <cstring>
#include "types.h"

// Linux中断处理兼容层（用户态模拟）

// 中断处理函数返回值
typedef int irqreturn_t;
#define IRQ_NONE        0
#define IRQ_HANDLED     1
#define IRQ_WAKE_THREAD 2

// 中断处理函数类型
typedef irqreturn_t (*irq_handler_t)(int irq, void *dev_id);

// 中断标志
#define IRQF_SHARED         0x00000080  // 共享中断
#define IRQF_TRIGGER_RISING  0x00000001
#define IRQF_TRIGGER_FALLING 0x00000002
#define IRQF_TRIGGER_HIGH    0x00000004
#define IRQF_TRIGGER_LOW     0x00000008
#define IRQF_DISABLED       0x00000020  // 已废弃，兼容性保留

// IRQ注册表（简单模拟）
struct irq_entry {
    int             irq;
    irq_handler_t   handler;
    void           *dev_id;
    char            devname[64];
};

// 简单的IRQ注册表（静态存储）
#define MAX_IRQ_HANDLERS 64
static struct {
    struct irq_entry entries[MAX_IRQ_HANDLERS];
    int count;
} irq_registry = { {}, 0 };

// 注册中断处理程序
static inline int request_irq(unsigned int irq, irq_handler_t handler,
                               unsigned long flags, const char *name, void *dev) {
    (void)flags;
    if (!handler) return -1;
    if (irq_registry.count >= MAX_IRQ_HANDLERS) return -1;

    struct irq_entry *entry = &irq_registry.entries[irq_registry.count++];
    entry->irq = (int)irq;
    entry->handler = handler;
    entry->dev_id = dev;
    strncpy(entry->devname, name ? name : "", sizeof(entry->devname) - 1);
    entry->devname[sizeof(entry->devname) - 1] = '\0';
    return 0;
}

// 释放中断处理程序
static inline void free_irq(unsigned int irq, void *dev_id) {
    for (int i = 0; i < irq_registry.count; i++) {
        if (irq_registry.entries[i].irq == (int)irq &&
            irq_registry.entries[i].dev_id == dev_id) {
            // 移除条目
            for (int j = i; j < irq_registry.count - 1; j++) {
                irq_registry.entries[j] = irq_registry.entries[j + 1];
            }
            irq_registry.count--;
            return;
        }
    }
}

// 触发模拟中断（用于测试）
static inline irqreturn_t simulate_irq(unsigned int irq, void *dev_id) {
    for (int i = 0; i < irq_registry.count; i++) {
        if (irq_registry.entries[i].irq == (int)irq) {
            if (dev_id == nullptr || irq_registry.entries[i].dev_id == dev_id) {
                return irq_registry.entries[i].handler((int)irq,
                           irq_registry.entries[i].dev_id);
            }
        }
    }
    return IRQ_NONE;
}

// 禁用/启用中断（用户态中为空操作）
static inline void disable_irq(unsigned int irq) {
    (void)irq;
}

static inline void enable_irq(unsigned int irq) {
    (void)irq;
}

static inline void disable_irq_nosync(unsigned int irq) {
    (void)irq;
}

// 同步等待中断完成（用户态为空操作）
static inline void synchronize_irq(unsigned int irq) {
    (void)irq;
}

// 本地中断控制（用户态为空操作）
#define local_irq_save(flags)       do { (void)(flags); } while (0)
#define local_irq_restore(flags)    do { (void)(flags); } while (0)
#define local_irq_disable()         do { } while (0)
#define local_irq_enable()          do { } while (0)
