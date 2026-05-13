#pragma once

#include <cstdlib>
#include <cstring>
#include "types.h"
#include "resource.h"

// Linux PCI设备兼容层

// PCI设备ID结构
struct pci_device_id {
    uint32_t vendor;
    uint32_t device;
    uint32_t subvendor;
    uint32_t subdevice;
    uint32_t class_id;
    uint32_t class_mask;
    unsigned long driver_data;
};

// PCI BAR资源数量
#define PCI_NUM_RESOURCES   7
#define PCI_ROM_RESOURCE    6

// PCI设备结构
struct pci_dev {
    uint16_t        vendor;
    uint16_t        device;
    uint16_t        subsystem_vendor;
    uint16_t        subsystem_device;
    uint8_t         revision;
    uint32_t        class_id;
    uint8_t         irq;
    struct resource resource[PCI_NUM_RESOURCES];
    void           *driver_data;
    bool            is_enabled;
    void           *mapped_bars[PCI_NUM_RESOURCES];
};

// PCI驱动结构
struct pci_driver {
    const char             *name;
    const struct pci_device_id *id_table;
    int  (*probe)(struct pci_dev *dev, const struct pci_device_id *id);
    void (*remove)(struct pci_dev *dev);
    void (*shutdown)(struct pci_dev *dev);
};

// PCI ID通配符
#define PCI_ANY_ID  (~0U)

// 定义PCI设备表条目
#define PCI_DEVICE(vend, dev) \
    .vendor = (vend), .device = (dev), \
    .subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID

// PCI驱动注册/注销
static inline int pci_register_driver(struct pci_driver *drv) {
    (void)drv;
    return 0;
}

static inline void pci_unregister_driver(struct pci_driver *drv) {
    (void)drv;
}

// PCI设备使能/禁用
static inline int pci_enable_device(struct pci_dev *dev) {
    if (!dev) return -1;
    dev->is_enabled = true;
    return 0;
}

static inline void pci_disable_device(struct pci_dev *dev) {
    if (!dev) return;
    dev->is_enabled = false;
}

static inline int pci_enable_device_mem(struct pci_dev *dev) {
    return pci_enable_device(dev);
}

// PCI BAR映射/取消映射
static inline void *pci_iomap(struct pci_dev *dev, int bar, unsigned long maxlen) {
    (void)maxlen;
    if (!dev || bar < 0 || bar >= PCI_NUM_RESOURCES) return nullptr;
    // 分配模拟I/O内存
    resource_size_t size = dev->resource[bar].end - dev->resource[bar].start + 1;
    if (size == 0) size = 4096;
    void *addr = malloc((size_t)size);
    if (!addr) return nullptr;
    memset(addr, 0, (size_t)size);
    dev->mapped_bars[bar] = addr;
    return addr;
}

static inline void pci_iounmap(struct pci_dev *dev, void *addr) {
    if (!dev || !addr) return;
    free(addr);
    for (int i = 0; i < PCI_NUM_RESOURCES; i++) {
        if (dev->mapped_bars[i] == addr) {
            dev->mapped_bars[i] = nullptr;
            break;
        }
    }
}

// PCI资源区域请求/释放
static inline int pci_request_regions(struct pci_dev *pdev, const char *res_name) {
    (void)pdev; (void)res_name;
    return 0;
}

static inline void pci_release_regions(struct pci_dev *pdev) {
    (void)pdev;
}

static inline int pci_request_region(struct pci_dev *pdev, int bar, const char *res_name) {
    (void)pdev; (void)bar; (void)res_name;
    return 0;
}

static inline void pci_release_region(struct pci_dev *pdev, int bar) {
    (void)pdev; (void)bar;
}

// PCI配置空间读写（简化模拟）
static inline int pci_read_config_byte(const struct pci_dev *dev, int where, uint8_t *val) {
    (void)dev; (void)where;
    *val = 0;
    return 0;
}

static inline int pci_read_config_word(const struct pci_dev *dev, int where, uint16_t *val) {
    (void)dev; (void)where;
    *val = 0;
    return 0;
}

static inline int pci_read_config_dword(const struct pci_dev *dev, int where, uint32_t *val) {
    (void)dev; (void)where;
    *val = 0;
    return 0;
}

static inline int pci_write_config_byte(const struct pci_dev *dev, int where, uint8_t val) {
    (void)dev; (void)where; (void)val;
    return 0;
}

static inline int pci_write_config_word(const struct pci_dev *dev, int where, uint16_t val) {
    (void)dev; (void)where; (void)val;
    return 0;
}

static inline int pci_write_config_dword(const struct pci_dev *dev, int where, uint32_t val) {
    (void)dev; (void)where; (void)val;
    return 0;
}

// PCI总线掌控
static inline int pci_set_master(struct pci_dev *dev) {
    (void)dev;
    return 0;
}

// DMA掩码设置
static inline int pci_set_dma_mask(struct pci_dev *dev, uint64_t mask) {
    (void)dev; (void)mask;
    return 0;
}

static inline int pci_set_consistent_dma_mask(struct pci_dev *dev, uint64_t mask) {
    (void)dev; (void)mask;
    return 0;
}

// 驱动私有数据访问
static inline void pci_set_drvdata(struct pci_dev *pdev, void *data) {
    if (pdev) pdev->driver_data = data;
}

static inline void *pci_get_drvdata(const struct pci_dev *pdev) {
    if (!pdev) return nullptr;
    return pdev->driver_data;
}

// BAR资源大小
static inline unsigned long pci_resource_start(struct pci_dev *dev, int bar) {
    if (!dev || bar < 0 || bar >= PCI_NUM_RESOURCES) return 0;
    return (unsigned long)dev->resource[bar].start;
}

static inline unsigned long pci_resource_end(struct pci_dev *dev, int bar) {
    if (!dev || bar < 0 || bar >= PCI_NUM_RESOURCES) return 0;
    return (unsigned long)dev->resource[bar].end;
}

static inline unsigned long pci_resource_len(struct pci_dev *dev, int bar) {
    if (!dev || bar < 0 || bar >= PCI_NUM_RESOURCES) return 0;
    return (unsigned long)(dev->resource[bar].end - dev->resource[bar].start + 1);
}

static inline unsigned long pci_resource_flags(struct pci_dev *dev, int bar) {
    if (!dev || bar < 0 || bar >= PCI_NUM_RESOURCES) return 0;
    return dev->resource[bar].flags;
}

// IORESOURCE标志（定义在resource.h中）
// 此处保持与Linux内核头文件的兼容性
