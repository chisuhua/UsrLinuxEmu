#include "vram_store.h"
#include <cstring>
#include <unistd.h>
#include <sys/mman.h>

namespace usr_linux_emu {

GpuVramStore g_vram_store;

GpuVramStore::GpuVramStore() : vram_size(0), pool_backing(nullptr), initialized(false) {
    std::memset(bars, 0, sizeof(bars));
    for (int i = 0; i < 6; i++) {
        bars[i].fd = -1;
    }
}

GpuVramStore::~GpuVramStore() {
    for (int i = 0; i < 6; i++) {
        if (bars[i].backing) {
            munmap(bars[i].backing, bars[i].size);
        }
        if (bars[i].fd >= 0) {
            close(bars[i].fd);
        }
    }
    if (pool_backing) {
        munmap(pool_backing, vram_size);
    }
}

bool GpuVramStore::init(size_t vram_size_mb) {
    std::lock_guard<std::mutex> lock(vram_lock_);
    vram_size = vram_size_mb * 1024 * 1024;
    pool_backing = mmap(nullptr, vram_size, PROT_READ | PROT_WRITE,
                        MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if (pool_backing == MAP_FAILED) {
        pool_backing = nullptr;
        return false;
    }
    initialized = true;
    return true;
}

PciBarSim* GpuVramStore::get_bar(int bar_index) {
    if (bar_index < 0 || bar_index >= 6) return nullptr;
    return &bars[bar_index];
}

void* GpuVramStore::bar_ioremap(uint64_t phys_addr, uint64_t size) {
    for (int i = 0; i < 6; i++) {
        if (bars[i].phys_base == phys_addr) {
            if (!bars[i].backing) {
                bars[i].size = size;
                bars[i].backing = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                                       MAP_ANONYMOUS | MAP_SHARED, -1, 0);
                if (bars[i].backing == MAP_FAILED) {
                    bars[i].backing = nullptr;
                    return nullptr;
                }
            }
            return bars[i].backing;
        }
    }
    return nullptr;
}

void GpuVramStore::bar_iounmap(void* addr, uint64_t size) {
    for (int i = 0; i < 6; i++) {
        if (bars[i].backing == addr) {
            munmap(addr, size);
            bars[i].backing = nullptr;
            bars[i].size = 0;
            return;
        }
    }
}

} // namespace usr_linux_emu
