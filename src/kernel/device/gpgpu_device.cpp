#include "gpgpu_device.h"
#include <iostream>
#include <sys/mman.h>
#include <unordered_map>
#include <cstring>
#include <iostream>


GpuDevice::GpuDevice() {
    bar0_start_ = 0xcafebabe00000000ULL;
    bar0_size_ = 0x80000000; // 2GB 显存
    bar1_start_ = bar0_start_ + bar0_size_;
    bar1_size_ = 0x1000; // 4KB MMIO
}

bool GpuDevice::is_in_bar0(uint64_t offset) const {
    return offset >= bar0_start_ && offset < bar0_start_ + bar0_size_;
}

bool GpuDevice::is_in_bar1(uint64_t offset) const {
    return offset >= bar1_start_ && offset < bar1_start_ + bar1_size_;
}

ssize_t GpuDevice::read(int fd, void* buf, size_t count) {
    off_t offset = lseek(fd, 0, SEEK_CUR);
    if (offset == -1) return -1;

    PciDevice* pci_dev = dynamic_cast<PciDevice*>(this);
    if (!pci_dev) return -1;

    if (is_in_bar0(offset)) {
        return pci_dev->read_ram(offset - bar0_start_, buf, count);
    } else if (is_in_bar1(offset)) {
        return pci_dev->read_mmio(offset - bar1_start_, buf, count);
    }

    return -1;
}

