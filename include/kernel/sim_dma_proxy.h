/**
 * @file sim_dma_proxy.h
 * @brief µ-thin forward-prototype header for ①→③ DMA coherent sim bridging
 *
 * Per ADR-063 D4 Path X: ① (kernel env) code includes THIS header, NOT
 * ③'s dma_coherent_pool.h. Only a forward declaration of DmaCoherentPool
 * + extern declaration of g_dma_pool are exposed.
 * Linker resolves g_dma_pool from plugins/gpu_driver/sim/ at link time.
 *
 * See: openspec/changes/stage4-1-bar-ioremap/design.md §D2
 */

#ifndef KERNEL_SIM_DMA_PROXY_H
#define KERNEL_SIM_DMA_PROXY_H

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

#endif /* KERNEL_SIM_DMA_PROXY_H */