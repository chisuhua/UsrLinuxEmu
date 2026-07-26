#include "dma_coherent_pool.h"
#include <cstring>
#include <unistd.h>
#include <sys/mman.h>

namespace usr_linux_emu {

DmaCoherentPool g_dma_pool;

DmaCoherentPool::DmaCoherentPool()
    : cpu_pool(nullptr), next_dma_addr(DMA_COHERENT_BASE), initialized(false) {}

DmaCoherentPool::~DmaCoherentPool() {
    if (cpu_pool) {
        munmap(cpu_pool, DMA_COHERENT_SIZE);
    }
}

bool DmaCoherentPool::init() {
    cpu_pool = mmap(nullptr, DMA_COHERENT_SIZE, PROT_READ | PROT_WRITE,
                    MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if (cpu_pool == MAP_FAILED) {
        cpu_pool = nullptr;
        return false;
    }
    initialized = true;
    return true;
}

void* DmaCoherentPool::allocate(size_t size, uint64_t* out_dma_addr) {
    if (!initialized) return nullptr;
    if (next_dma_addr + size > DMA_COHERENT_BASE + DMA_COHERENT_SIZE) return nullptr;

    *out_dma_addr = next_dma_addr;
    allocations[*out_dma_addr] = size;

    uint8_t* base = static_cast<uint8_t*>(cpu_pool);
    void* result = base + (next_dma_addr - DMA_COHERENT_BASE);
    next_dma_addr += size;
    return result;
}

void DmaCoherentPool::free(uint64_t dma_addr) {
    allocations.erase(dma_addr);
}

} // namespace usr_linux_emu
