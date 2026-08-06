#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <sys/mman.h>

namespace usr_linux_emu {

struct PciBarSim {
    uint64_t phys_base;
    uint64_t size;
    void*    backing;
    int      fd;
};

struct GpuVramStore {
    size_t    vram_size;
    void*     pool_backing;
    PciBarSim bars[6];
    bool      initialized;
    std::mutex vram_lock_;

    GpuVramStore();
    ~GpuVramStore();

    bool init(size_t vram_size_mb = 256);
    PciBarSim* get_bar(int bar_index);
    void* bar_ioremap(uint64_t phys_addr, uint64_t size);
    void  bar_iounmap(void* addr, uint64_t size);
};

extern GpuVramStore g_vram_store;

} // namespace usr_linux_emu
