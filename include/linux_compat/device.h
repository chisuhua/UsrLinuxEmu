#pragma once

#include <cstdlib>
#include <cstring>
#include "types.h"

// Linux设备模型API兼容层

// 前向声明
struct device;
struct device_driver;
struct bus_type;
struct device_class;

// 设备类型
struct device_type {
    const char *name;
};

// 设备驱动结构
struct device_driver {
    const char          *name;
    struct bus_type     *bus;
    int (*probe)(struct device *dev);
    int (*remove)(struct device *dev);
    void (*shutdown)(struct device *dev);
};

// 设备结构
struct device {
    struct device           *parent;
    const char              *init_name;
    const struct device_type *type;
    struct device_driver    *driver;
    void                    *driver_data;
    struct device_class     *class_dev;
    char                     bus_id[32];
};

// 设备类结构（Linux中的struct class，用device_class避免与C++关键字冲突）
struct device_class {
    const char *name;
    int (*dev_uevent)(struct device *dev, void *env);
    void (*dev_release)(struct device *dev);
};

// 总线类型结构
struct bus_type {
    const char *name;
    int (*match)(struct device *dev, struct device_driver *drv);
    int (*probe)(struct device *dev);
    int (*remove)(struct device *dev);
};

// 设备注册/注销
static inline int device_register(struct device *dev) {
    (void)dev;
    return 0;
}

static inline void device_unregister(struct device *dev) {
    (void)dev;
}

// 驱动注册/注销
static inline int driver_register(struct device_driver *drv) {
    (void)drv;
    return 0;
}

static inline void driver_unregister(struct device_driver *drv) {
    (void)drv;
}

// 设备私有数据访问
static inline void dev_set_drvdata(struct device *dev, void *data) {
    if (dev) dev->driver_data = data;
}

static inline void *dev_get_drvdata(const struct device *dev) {
    if (!dev) return nullptr;
    return dev->driver_data;
}

// 设备名称
static inline const char *dev_name(const struct device *dev) {
    if (!dev) return "(null)";
    return dev->init_name ? dev->init_name : dev->bus_id;
}

// 设备类创建/销毁
static inline struct device_class *class_create(void *owner, const char *name) {
    (void)owner;
    struct device_class *cls = (struct device_class *)malloc(sizeof(struct device_class));
    if (!cls) return nullptr;
    memset(cls, 0, sizeof(struct device_class));
    cls->name = name;
    return cls;
}

static inline void class_destroy(struct device_class *cls) {
    free(cls);
}

// 设备创建/销毁
static inline struct device *device_create(struct device_class *cls, struct device *parent,
                                           unsigned int devt, void *drvdata,
                                           const char *fmt, ...) {
    (void)cls; (void)parent; (void)devt; (void)drvdata; (void)fmt;
    struct device *dev = (struct device *)malloc(sizeof(struct device));
    if (!dev) return nullptr;
    memset(dev, 0, sizeof(struct device));
    dev->driver_data = drvdata;
    dev->class_dev = cls;
    return dev;
}

static inline void device_destroy(struct device_class *cls, unsigned int devt) {
    (void)cls; (void)devt;
}

// 总线注册/注销
static inline int bus_register(struct bus_type *bus) {
    (void)bus;
    return 0;
}

static inline void bus_unregister(struct bus_type *bus) {
    (void)bus;
}

// 设备初始化
static inline void device_initialize(struct device *dev) {
    if (!dev) return;
    memset(dev, 0, sizeof(struct device));
}
