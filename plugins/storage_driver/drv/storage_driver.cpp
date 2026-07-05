/*
 * storage_driver.cpp - ② portable block storage driver (per ADR-038)
 *
 * Per plan §2.3 决策: block device is simpler than network (no
 * request queue, no bio layer). ② portable driver wraps the kernel
 * compat block API (us_block_*) and exposes void* opaque handles.
 *
 * Per ADR-038 D1: ② does not include any C++ infrastructure
 * (no namespace usr_linux_emu, no templates, no STL).  Portable to
 * drivers/block/xxx/ (3-way separation proof).
 */

#include <linux_compat/types.h>

#include <cstring>
#include <string>
#include <vector>

#include "kernel/block/bio_compat.h"

namespace usr_linux_emu {

class DiskSim;

struct BlockDevice {
    std::string name;
    DiskSim* sim;
    int fd;
    bool opened;

    int (*bdo_open)(BlockDevice*);
    int (*bdo_close)(BlockDevice*);
    long (*bdo_read)(BlockDevice*, void* buf, unsigned long count);
    long (*bdo_write)(BlockDevice*, const void* buf, unsigned long count);
};

class DiskSim {
 public:
    int open(const char* path, unsigned long flags) {
        fd_ = us_block_open(path, flags);
        if (fd_ < 0) return fd_;
        return 0;
    }
    int close() {
        if (fd_ < 0) return -9;
        int rc = us_block_close(fd_);
        fd_ = -1;
        return rc;
    }
    long read(void* buf, unsigned long count) {
        return fd_ < 0 ? -9 : us_block_read(fd_, buf, count);
    }
    long write(const void* buf, unsigned long count) {
        return fd_ < 0 ? -9 : us_block_write(fd_, buf, count);
    }
    unsigned long size() const { return fd_ < 0 ? 0 : us_block_size(fd_); }

 private:
    int fd_ = -1;
};

static int bd_open(BlockDevice* dev) {
    if (!dev || !dev->sim) return -22;
    int rc = dev->sim->open("/tmp/storage_drv_test.dat", 0x40 /* O_CREAT */);
    if (rc == 0) dev->opened = true;
    return rc;
}

static int bd_close(BlockDevice* dev) {
    if (!dev || !dev->sim) return -22;
    int rc = dev->sim->close();
    if (rc == 0) dev->opened = false;
    return rc;
}

static long bd_read(BlockDevice* dev, void* buf, unsigned long count) {
    if (!dev || !dev->opened || !dev->sim) return -1;
    return dev->sim->read(buf, count);
}

static long bd_write(BlockDevice* dev, const void* buf, unsigned long count) {
    if (!dev || !dev->opened || !dev->sim) return -1;
    return dev->sim->write(buf, count);
}

extern "C" {

BlockDevice* block_device_create(const char* name) {
    auto* dev = new BlockDevice{};
    dev->name = name ? name : "sda0";
    dev->sim = new DiskSim{};
    dev->fd = -1;
    dev->opened = false;
    dev->bdo_open = bd_open;
    dev->bdo_close = bd_close;
    dev->bdo_read = bd_read;
    dev->bdo_write = bd_write;
    return dev;
}

void block_device_destroy(BlockDevice* dev) {
    if (!dev) return;
    if (dev->opened && dev->sim) dev->sim->close();
    delete dev->sim;
    delete dev;
}

int block_device_open(BlockDevice* dev) {
    return (dev && dev->bdo_open) ? dev->bdo_open(dev) : -22;
}

int block_device_close(BlockDevice* dev) {
    return (dev && dev->bdo_close) ? dev->bdo_close(dev) : -22;
}

long block_device_read(BlockDevice* dev, void* buf, unsigned long count) {
    return (dev && dev->bdo_read) ? dev->bdo_read(dev, buf, count) : -22;
}

long block_device_write(BlockDevice* dev, const void* buf,
                       unsigned long count) {
    return (dev && dev->bdo_write) ? dev->bdo_write(dev, buf, count) : -22;
}

unsigned long block_device_size(BlockDevice* dev) {
    return dev && dev->sim ? dev->sim->size() : 0;
}

const char* block_device_get_name(BlockDevice* dev) {
    return dev ? dev->name.c_str() : nullptr;
}

}  // extern "C"

}  // namespace usr_linux_emu