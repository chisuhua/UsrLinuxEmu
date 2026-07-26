#pragma once

#include <cstdint>
#include <cstddef>
#include <map>

#define DMA_COHERENT_BASE  0x100000000ULL
#define DMA_COHERENT_SIZE  0x10000000ULL

namespace usr_linux_emu {

struct DmaCoherentPool {
    void*              cpu_pool;
    uint64_t           next_dma_addr;
    std::map<uint64_t, size_t> allocations;
    bool               initialized;

    DmaCoherentPool();
    ~DmaCoherentPool();

    bool init();
    void* allocate(size_t size, uint64_t* out_dma_addr);
    void  free(uint64_t dma_addr);
};

extern DmaCoherentPool g_dma_pool;

} // namespace usr_linux_emu
