# Linux 兼容层开发详细计划

**文档状态**: 活跃  
**创建日期**: 2026-03-24  
**优先级**: 高  
**预计工期**: 8-10 周  
**对应总体计划**: 第二阶段

---

## 概述

Linux 兼容层是 UsrLinuxEmu 项目的核心组件，目标是提供 Linux 内核 API 的用户态实现，使开发者能够使用熟悉的 Linux 驱动编程风格编写设备驱动。

### 当前状态

| 模块 | 文件 | 完成度 | 状态 |
|------|------|--------|------|
| 基础类型 | `types.h` | 60% | 🔄 进行中 |
| 内存管理 | `memory.h` | 30% | 🔄 进行中 |
| IOCTL 宏 | `ioctl.h` | 80% | 🔄 进行中 |
| 字符串函数 | - | 0% | ⏸️ 未开始 |
| 字符设备 | `cdev.h` | 0% | ⏸️ 未开始 |
| 设备模型 | `device.h` | 0% | ⏸️ 未开始 |
| 同步机制 | `sync.h` | 20% | 🔄 进行中 |
| PCI 设备 | `pci.h` | 0% | ⏸️ 未开始 |

**总体完成度**: 约 20%

### 目标

- **短期 (v0.2)**: 完成基础 API，达到 50%
- **中期 (v0.5)**: 完成设备模型和同步机制，达到 80%
- **长期 (v1.0)**: 完整兼容，达到 95%

---

## 第一阶段：基础 API (3 周)

### 周 1-2: 类型和内存管理

#### 任务 1.1: 完善类型定义

**文件**: `include/linux_compat/types.h`

**需要实现**:
```cpp
// 基础整数类型
typedef __u8 u8;
typedef __u16 u16;
typedef __u32 u32;
typedef __u64 u64;
typedef __s8 s8;
typedef __s16 s16;
typedef __s32 s32;
typedef __s64 s64;

// 内核专用类型
typedef unsigned long phys_addr_t;
typedef unsigned long dma_addr_t;
typedef long ssize_t;
typedef loff_t loff_t;

// 设备号
typedef u32 dev_t;
#define MAJOR(dev) ((dev) >> 8)
#define MINOR(dev) ((dev) & 0xFF)
#define MKDEV(major, minor) (((major) << 8) | (minor))

// 时间类型
typedef long ktime_t;
typedef unsigned long jiffies_t;

// 位域类型
typedef u32 bitmap_t;
```

**测试**: `tests/test_compat_types.cpp`

**验收标准**:
- 所有类型定义与 Linux 内核一致
- 类型大小在 32/64 位系统上正确
- 编译无警告

---

#### 任务 1.2: 内存管理 API

**文件**: `include/linux_compat/memory.h`

**需要实现**:
```cpp
// 基础内存分配
void* kmalloc(size_t size, gfp_t flags);
void kfree(void* ptr);

void* kzalloc(size_t size, gfp_t flags);
void* kmalloc_array(size_t n, size_t size, gfp_t flags);
void* kcalloc(size_t n, size_t size, gfp_t flags);

// 虚拟连续内存
void* vmalloc(size_t size);
void* vzalloc(size_t size);
void vfree(void* addr);

// DMA 一致内存
void* dma_alloc_coherent(struct device* dev, size_t size,
                         dma_addr_t* dma_handle, gfp_t flag);
void dma_free_coherent(struct device* dev, size_t size,
                       void* vaddr, dma_addr_t dma_handle);

// 内存操作
void* memcpy(void* dest, const void* src, size_t count);
void* memset(void* s, int c, size_t count);
void* memmove(void* dest, const void* src, size_t count);
int memcmp(const void* cs, const void* ct, size_t count);

// 内存屏障
#define mb()   asm volatile("mfence" ::: "memory")
#define rmb()  asm volatile("lfence" ::: "memory")
#define wmb()  asm volatile("sfence" ::: "memory")
```

**GFP 标志**:
```cpp
#define GFP_KERNEL  0x00u
#define GFP_ATOMIC  0x01u
#define GFP_DMA     0x02u
#define GFP_ZERO    0x04u
```

**测试**: `tests/test_compat_memory.cpp`

**验收标准**:
- 所有分配函数返回正确对齐的内存
- 内存内容正确（特别是 kzalloc/vzalloc）
- 无内存泄漏
- 性能开销 ≤ 20%

---

#### 任务 1.3: 字符串函数

**文件**: `include/linux_compat/string.h`

**需要实现**:
```cpp
size_t strlen(const char* s);
size_t strnlen(const char* s, size_t count);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t count);
char* strcat(char* dest, const char* src);
char* strncat(char* dest, const char* src, size_t count);
int strcmp(const char* cs, const char* ct);
int strncmp(const char* cs, const char* ct, size_t count);
char* strchr(const char* s, int c);
char* strrchr(const char* s, int c);
char* strstr(const char* cs, const char* ct);

// 内核扩展
char* kasprintf(gfp_t gfp, const char* fmt, ...);
char* kvasprintf(gfp_t gfp, const char* fmt, va_list va);
int scnprintf(char* buf, size_t size, const char* fmt, ...);
```

**测试**: `tests/test_compat_string.cpp`

---

#### 任务 1.4: 打印和日志函数

**文件**: `include/linux_compat/printk.h`

**需要实现**:
```cpp
// 打印级别
#define KERN_EMERG  "<0>"
#define KERN_ALERT  "<1>"
#define KERN_CRIT   "<2>"
#define KERN_ERR    "<3>"
#define KERN_WARN   "<4>"
#define KERN_NOTICE "<5>"
#define KERN_INFO   "<6>"
#define KERN_DEBUG  "<7>"

// 打印宏
#define printk(fmt, ...) \
    Logger::info(format_string(fmt, ##__VA_ARGS__))

#define pr_emerg(fmt, ...) \
    printk(KERN_EMERG fmt, ##__VA_ARGS__)
#define pr_alert(fmt, ...) \
    printk(KERN_ALERT fmt, ##__VA_ARGS__)
#define pr_err(fmt, ...) \
    printk(KERN_ERR fmt, ##__VA_ARGS__)
#define pr_warn(fmt, ...) \
    printk(KERN_WARN fmt, ##__VA_ARGS__)
#define pr_info(fmt, ...) \
    printk(KERN_INFO fmt, ##__VA_ARGS__)
#define pr_debug(fmt, ...) \
    printk(KERN_DEBUG fmt, ##__VA_ARGS__)

// dev_级别打印
#define dev_err(dev, fmt, ...) \
    pr_err("%s: " fmt, dev_name(dev), ##__VA_ARGS__)
#define dev_warn(dev, fmt, ...) \
    pr_warn("%s: " fmt, dev_name(dev), ##__VA_ARGS__)
#define dev_info(dev, fmt, ...) \
    pr_info("%s: " fmt, dev_name(dev), ##__VA_ARGS__)

// printf 家族
int sprintf(char* buf, const char* fmt, ...);
int snprintf(char* buf, size_t size, const char* fmt, ...);
int vsprintf(char* buf, const char* fmt, va_list va);
int vsnprintf(char* buf, size_t size, const char* fmt, va_list va);
```

**测试**: `tests/test_compat_printk.cpp`

---

### 周 3: 集成测试和文档

#### 任务 1.5: 基础 API 集成测试

**文件**: `tests/test_compat_basic_api.cpp`

**测试场景**:
1. 分配各种大小的内存并验证
2. 字符串操作正确性
3. 打印函数输出验证
4. 内存泄漏检测

**验收标准**:
- 所有测试通过
- 测试覆盖率 ≥ 80%
- 无内存泄漏（Valgrind 验证）

---

## 第二阶段：设备模型 (3 周)

### 周 4-5: 字符设备

#### 任务 2.1: 字符设备 API

**文件**: `include/linux_compat/cdev.h`

**需要实现**:
```cpp
// file_operations 结构
struct file_operations {
    struct module* owner;
    int (*open)(struct inode* inode, struct file* filp);
    int (*release)(struct inode* inode, struct file* filp);
    ssize_t (*read)(struct file* filp, char __user* buf,
                    size_t count, loff_t* ppos);
    ssize_t (*write)(struct file* filp, const char __user* buf,
                     size_t count, loff_t* ppos);
    long (*unlocked_ioctl)(struct file* filp, unsigned int cmd,
                           unsigned long arg);
    long (*compat_ioctl)(struct file* filp, unsigned int cmd,
                         unsigned long arg);
    int (*mmap)(struct file* filp, struct vm_area_struct* vma);
    unsigned int (*poll)(struct file* filp, struct poll_table_struct* wait);
    int (*fasync)(int fd, struct file* filp, int on);
};

// cdev 结构
struct cdev {
    struct kobject kobj;
    struct module* owner;
    const struct file_operations* ops;
    struct list_head list;
    dev_t dev;
    unsigned int count;
};

// 函数
int register_chrdev_region(dev_t from, unsigned count, const char* name);
void unregister_chrdev_region(dev_t from, unsigned count);
int alloc_chrdev_region(dev_t* dev, unsigned baseminor, unsigned count,
                        const char* name);

void cdev_init(struct cdev* cdev, const struct file_operations* fops);
int cdev_add(struct cdev* cdev, dev_t dev, unsigned count);
void cdev_del(struct cdev* cdev);
```

**测试**: `tests/test_compat_cdev.cpp`

---

#### 任务 2.2: 设备类 API

**文件**: `include/linux_compat/device.h`

**需要实现**:
```cpp
// device 结构
struct device {
    const char* init_name;
    struct device_type* type;
    struct mutex mutex;
    atomic_t knode_class->n_ref;
    struct device* parent;
    void* driver_data;
    // ...
};

// class 结构
struct class {
    const char* name;
    struct module* owner;
    struct device* (*devnode)(struct device* dev, umode_t* mode);
    // ...
};

// 函数
struct class* __class_create(struct module* owner, const char* name,
                             struct lock_class_key* key);
void class_destroy(struct class* cls);

struct device* __must_check device_create(struct class* cls,
                                          struct device* parent,
                                          dev_t devt, void* drvdata,
                                          const char* fmt, ...);
void device_destroy(struct class* cls, dev_t devt);

const char* dev_name(const struct device* dev);
void* dev_get_drvdata(const struct device* dev);
void dev_set_drvdata(struct device* dev, void* data);
```

**测试**: `tests/test_compat_device_class.cpp`

---

### 周 6: 设备模型集成

#### 任务 2.3: 设备模型集成测试

**文件**: `tests/test_compat_device_model.cpp`

**测试场景**:
1. 注册字符设备
2. 创建设备类
3. 创建设备节点
4. 打开/读写/关闭设备
5. IOCTL 调用

**示例代码**:
```cpp
static struct cdev my_cdev;
static struct class* my_class;
static dev_t devno;

// 设备注册
devno = MKDEV(200, 0);
register_chrdev_region(devno, 1, "my_device");
cdev_init(&my_cdev, &my_fops);
cdev_add(&my_cdev, devno, 1);
my_class = class_create(THIS_MODULE, "my_class");
device_create(my_class, NULL, devno, NULL, "my_device");

// 使用
int fd = open("/dev/my_device", O_RDWR);
ioctl(fd, MY_IOCTL_CMD, &arg);
close(fd);
```

---

## 第三阶段：同步机制 (2 周)

### 周 7: 锁机制

#### 任务 3.1: 自旋锁

**文件**: `include/linux_compat/spinlock.h`

**需要实现**:
```cpp
typedef struct {
    std::atomic_flag locked = ATOMIC_FLAG_INIT;
} spinlock_t;

#define __SPIN_LOCK_UNLOCKED(name) \
    { .locked = ATOMIC_FLAG_INIT }

#define DEFINE_SPINLOCK(name) \
    spinlock_t name = __SPIN_LOCK_UNLOCKED(name)

static inline void spin_lock_init(spinlock_t* lock) {
    atomic_flag_clear(&lock->locked);
}

static inline void spin_lock(spinlock_t* lock) {
    while (atomic_flag_test_and_set(&lock->locked))
        cpu_relax();
}

static inline void spin_unlock(spinlock_t* lock) {
    atomic_flag_clear(&lock->locked);
}

// 中断版本
static inline unsigned long spin_lock_irqsave(spinlock_t* lock,
                                               unsigned long flags) {
    local_irq_save(flags);
    spin_lock(lock);
    return flags;
}

static inline void spin_unlock_irqrestore(spinlock_t* lock,
                                           unsigned long flags) {
    spin_unlock(lock);
    local_irq_restore(flags);
}

#define cpu_relax() asm volatile("pause" ::: "memory")
```

**测试**: `tests/test_compat_spinlock.cpp`

---

#### 任务 3.2: 互斥锁

**文件**: `include/linux_compat/mutex.h`

**需要实现**:
```cpp
struct mutex {
    std::mutex mutex_;
    atomic_t count;  // 调试用
};

#define __MUTEX_INITIALIZER(name) \
    { .mutex_ = PTHREAD_MUTEX_INITIALIZER, .count = ATOMIC_VAR_INIT(1) }

#define DEFINE_MUTEX(name) \
    struct mutex name = __MUTEX_INITIALIZER(name)

static inline void mutex_init(struct mutex* lock) {
    // 初始化
}

static inline void mutex_lock(struct mutex* lock) {
    lock->mutex_.lock();
}

static inline int mutex_lock_interruptible(struct mutex* lock) {
    // 可中断版本
    lock->mutex_.lock();
    return 0;
}

static inline void mutex_unlock(struct mutex* lock) {
    lock->mutex_.unlock();
}
```

**测试**: `tests/test_compat_mutex.cpp`

---

#### 任务 3.3: 信号量

**文件**: `include/linux_compat/semaphore.h`

**需要实现**:
```cpp
struct semaphore {
    std::counting_semaphore<> sem_;
    atomic_t count;
};

#define __SEMAPHORE_INITIALIZER(name, n) \
    { .sem_ = std::counting_semaphore<>(n), .count = ATOMIC_VAR_INIT(n) }

#define DEFINE_SEMAPHORE(name) \
    struct semaphore name = __SEMAPHORE_INITIALIZER(name, 1)

static inline void sema_init(struct semaphore* sem, int val) {
    // 初始化
}

static inline void down(struct semaphore* sem) {
    sem->sem_.acquire();
}

static inline int down_interruptible(struct semaphore* sem) {
    // 可中断版本
    sem->sem_.acquire();
    return 0;
}

static inline int down_trylock(struct semaphore* sem) {
    // 尝试获取
    return sem->sem_.try_acquire() ? 0 : 1;
}

static inline void up(struct semaphore* sem) {
    sem->sem_.release();
}
```

**测试**: `tests/test_compat_semaphore.cpp`

---

### 周 8: 完成量和读写锁

#### 任务 3.4: 完成量

**文件**: `include/linux_compat/completion.h`

**需要实现**:
```cpp
struct completion {
    std::binary_semaphore sem_;
    atomic_t done;
};

#define __COMPLETION_INITIALIZER(work) \
    { .sem_ = std::binary_semaphore(0), .done = ATOMIC_VAR_INIT(0) }

#define COMPLETION_INITIALIZER_ONSTACK(work) \
    __COMPLETION_INITIALIZER(work)

#define DECLARE_COMPLETION_ONSTACK(name) \
    struct completion name = COMPLETION_INITIALIZER_ONSTACK(name)

static inline void init_completion(struct completion* x) {
    // 初始化
}

static inline void reinit_completion(struct completion* x) {
    x->done = 0;
}

static inline void complete(struct completion* x) {
    x->done = 1;
    x->sem_.release();
}

static inline void complete_all(struct completion* x) {
    complete(x);
    // 唤醒所有等待者
}

static inline void wait_for_completion(struct completion* x) {
    x->sem_.acquire();
}

static inline long wait_for_completion_timeout(struct completion* x,
                                                unsigned long timeout) {
    // 超时版本
    return x->sem_.try_acquire_for(timeout) ? timeout : 0;
}
```

**测试**: `tests/test_compat_completion.cpp`

---

#### 任务 3.5: 读写锁

**文件**: `include/linux_compat/rwlock.h`

**需要实现**:
```cpp
struct rwlock {
    std::shared_mutex mutex_;
};

#define __RW_LOCK_UNLOCKED(name) \
    { .mutex_ = PTHREAD_RWLOCK_INITIALIZER }

#define DEFINE_RWLOCK(name) \
    struct rwlock name = __RW_LOCK_UNLOCKED(name)

static inline void rwlock_init(struct rwlock* lock) {
    // 初始化
}

// 读锁
static inline void read_lock(struct rwlock* lock) {
    lock->mutex_.lock_shared();
}

static inline int read_lock_irqsave(struct rwlock* lock,
                                     unsigned long flags) {
    local_irq_save(flags);
    read_lock(lock);
    return flags;
}

static inline void read_unlock(struct rwlock* lock) {
    lock->mutex_.unlock_shared();
}

// 写锁
static inline void write_lock(struct rwlock* lock) {
    lock->mutex_.lock();
}

static inline int write_lock_irqsave(struct rwlock* lock,
                                      unsigned long flags) {
    local_irq_save(flags);
    write_lock(lock);
    return flags;
}

static inline void write_unlock(struct rwlock* lock) {
    lock->mutex_.unlock();
}
```

**测试**: `tests/test_compat_rwlock.cpp`

---

## 第四阶段：PCI 设备 (2 周)

### 周 9-10: PCI 兼容层

#### 任务 4.1: PCI 配置空间

**文件**: `include/linux_compat/pci.h`

**需要实现**:
```cpp
// PCI 设备结构
struct pci_dev {
    unsigned int busnr;
    struct pci_bus* bus;
    struct resource* resource[DEVICE_COUNT_RESOURCE];
    struct resource* rom_base_reg;
    struct pci_vpd* vpd;
    dev_t dev;
    // ...
};

// PCI BAR
enum {
    PCI_STD_RESOURCES     = 6,
    PCI_ROM_RESOURCE      = PCI_STD_RESOURCES,
    PCI_STD_RESOURCE_END  = 5,
};

// 函数
int pci_register_driver(struct pci_driver* driver);
void pci_unregister_driver(struct pci_driver* driver);

int pci_enable_device(struct pci_dev* dev);
void pci_disable_device(struct pci_dev* dev);

int pci_request_regions(struct pci_dev* dev, const char* res_name);
void pci_release_regions(struct pci_dev* dev);

void* pci_iomap(struct pci_dev* dev, int bar, unsigned long maxlen);
void pci_iounmap(struct pci_dev* dev, void __iomem* addr);

// PCI 配置空间访问
int pci_read_config_byte(struct pci_dev* dev, int where, u8* val);
int pci_read_config_word(struct pci_dev* dev, int where, u16* val);
int pci_read_config_dword(struct pci_dev* dev, int where, u32* val);
int pci_write_config_byte(struct pci_dev* dev, int where, u8 val);
int pci_write_config_word(struct pci_dev* dev, int where, u16 val);
int pci_write_config_dword(struct pci_dev* dev, int where, u32 val);

// MSI/MSI-X
int pci_enable_msi(struct pci_dev* dev);
void pci_disable_msi(struct pci_dev* dev);
int pci_enable_msix(struct pci_dev* dev, struct msix_entry* entries, int nvec);
void pci_disable_msix(struct pci_dev* dev);
```

**测试**: `tests/test_compat_pci.cpp`

---

## 测试计划

### 单元测试

| 测试文件 | 覆盖模块 | 目标覆盖率 |
|----------|----------|------------|
| test_compat_types.cpp | 类型定义 | 90% |
| test_compat_memory.cpp | 内存管理 | 90% |
| test_compat_string.cpp | 字符串函数 | 85% |
| test_compat_printk.cpp | 打印函数 | 80% |
| test_compat_cdev.cpp | 字符设备 | 85% |
| test_compat_device_class.cpp | 设备类 | 85% |
| test_compat_spinlock.cpp | 自旋锁 | 90% |
| test_compat_mutex.cpp | 互斥锁 | 90% |
| test_compat_semaphore.cpp | 信号量 | 90% |
| test_compat_completion.cpp | 完成量 | 85% |
| test_compat_rwlock.cpp | 读写锁 | 85% |
| test_compat_pci.cpp | PCI 设备 | 80% |

### 集成测试

| 测试文件 | 场景 | 目标 |
|----------|------|------|
| test_compat_device_model.cpp | 完整设备注册流程 | 端到端通过 |
| test_compat_driver_sample.cpp | 示例驱动 | 可编译运行 |
| test_compat_concurrent.cpp | 并发访问 | 无数据竞争 |

---

## 里程碑和验收标准

### 里程碑 1: 基础 API (第 3 周末)

**验收标准**:
- [ ] 类型定义完成
- [ ] 内存管理完成
- [ ] 字符串函数完成
- [ ] 打印函数完成
- [ ] 所有单元测试通过
- [ ] 测试覆盖率 ≥ 80%

### 里程碑 2: 设备模型 (第 6 周末)

**验收标准**:
- [ ] 字符设备 API 完成
- [ ] 设备类 API 完成
- [ ] 集成测试通过
- [ ] 示例驱动可编译运行

### 里程碑 3: 同步机制 (第 8 周末)

**验收标准**:
- [ ] 自旋锁完成
- [ ] 互斥锁完成
- [ ] 信号量完成
- [ ] 完成量完成
- [ ] 读写锁完成
- [ ] 并发测试通过

### 里程碑 4: PCI 兼容 (第 10 周末)

**验收标准**:
- [ ] PCI 配置空间访问完成
- [ ] PCI 资源管理完成
- [ ] MSI/MSI-X 支持完成
- [ ] PCI 测试通过

---

## 风险和缓解

### 技术风险

| 风险 | 影响 | 概率 | 缓解措施 |
|------|------|------|----------|
| API 行为不一致 | 高 | 中 | 建立详细的行为对比测试 |
| 性能开销过大 | 中 | 中 | 持续性能分析和优化 |
| 线程安全问题 | 高 | 低 | 使用 TSan 检测数据竞争 |

### 进度风险

| 风险 | 影响 | 概率 | 缓解措施 |
|------|------|------|----------|
| API 数量超出预期 | 中 | 中 | 优先实现常用 API |
| 测试编写耗时 | 中 | 中 | 使用测试模板和工具 |

---

## 参考文档

- [Linux 内核文档](https://www.kernel.org/doc/html/latest/)
- [Linux 驱动开发](https://lwn.net/Kernel/LDD3/)
- [UsrLinuxEmu 架构设计](../02-core/architecture.md)
- [总体开发计划](../plans/master_plan_2026.md)

---

**维护者**: UsrLinuxEmu Team  
**审查周期**: 每两周审查一次
