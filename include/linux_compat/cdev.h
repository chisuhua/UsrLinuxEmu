#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include "types.h"

// Linux字符设备API兼容层

// 设备号操作宏（dev_t由sys/types.h提供）
#define MAJOR(dev)          ((unsigned int)((dev) >> 20))
#define MINOR(dev)          ((unsigned int)((dev) & 0xfffff))
#define MKDEV(ma, mi)       (((dev_t)(ma) << 20) | (mi))

// 前向声明
struct inode;
struct file;
struct vm_area_struct;

// 虚拟内存区域（简化版）
struct vm_area_struct {
    unsigned long vm_start;
    unsigned long vm_end;
    unsigned long vm_flags;
    void *vm_private_data;
};

// inode结构（简化版）
struct inode {
    dev_t       i_rdev;
    void       *i_private;
};

// loff_t类型（由sys/types.h提供，此处仅作注释说明）
// loff_t is provided by <sys/types.h>

// 文件结构（简化版）
struct file {
    unsigned int        f_flags;
    loff_t              f_pos;
    void               *private_data;
    struct inode       *f_inode;
    const struct file_operations *f_op;
};

// 文件操作结构
struct file_operations {
    int     (*open)(struct inode *, struct file *);
    int     (*release)(struct inode *, struct file *);
    long    (*unlocked_ioctl)(struct file *, unsigned int, unsigned long);
    int     (*mmap)(struct file *, struct vm_area_struct *);
    ssize_t (*write)(struct file *, const char *, size_t, loff_t *);
    ssize_t (*read)(struct file *, char *, size_t, loff_t *);
    unsigned int (*poll)(struct file *, void *);
};

// 字符设备结构
struct cdev {
    dev_t                       dev;
    unsigned int                count;
    const struct file_operations *ops;
    void                       *owner;
};

// 初始化字符设备
static inline void cdev_init(struct cdev *cdev, const struct file_operations *fops) {
    memset(cdev, 0, sizeof(struct cdev));
    cdev->ops = fops;
}

// 分配字符设备号
static inline int register_chrdev_region(dev_t from, unsigned count, const char *name) {
    (void)from; (void)count; (void)name;
    return 0;
}

static inline int alloc_chrdev_region(dev_t *dev, unsigned baseminor, unsigned count, const char *name) {
    (void)baseminor; (void)count; (void)name;
    if (!dev) return -1;
    // 分配一个模拟的主设备号
    static unsigned int next_major = 200;
    *dev = MKDEV(next_major++, 0);
    return 0;
}

static inline void unregister_chrdev_region(dev_t from, unsigned count) {
    (void)from; (void)count;
}

// 添加字符设备到系统
static inline int cdev_add(struct cdev *p, dev_t dev, unsigned count) {
    if (!p) return -1;
    p->dev = dev;
    p->count = count;
    return 0;
}

// 从系统删除字符设备
static inline void cdev_del(struct cdev *p) {
    if (!p) return;
    memset(p, 0, sizeof(struct cdev));
}

// 文件标志定义
#define O_RDONLY    0
#define O_WRONLY    1
#define O_RDWR      2
#define O_ACCMODE   3
