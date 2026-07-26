// linux_compat dma-mapping.cpp - DMA coherent memory implementation
//
// Per ADR-073 D4: dma_alloc_coherent delegates to DmaCoherentPool (③ sim layer).
// The g_dma_pool symbol is resolved at runtime from gpu_sim when linked into
// test executables. When no sim is attached, allocation returns nullptr.
//
// Streaming DMA (dma_map_single) is a stub per ADR-073 D6 (conditions 2/3
// not triggered).

#include "linux_compat/dma-mapping.h"
#include "kernel/sim_dma_proxy.h"  // Per ADR-063: proxy, NOT plugins/gpu_driver/sim/dma_coherent_pool.h

#include <cstddef>

// Access the global DMA pool from gpu_sim (runtime resolved)
// Must match the mangled name: usr_linux_emu::g_dma_pool
namespace usr_linux_emu {
  extern DmaCoherentPool g_dma_pool;
}

void* dma_alloc_coherent(struct device* dev, size_t size,
                          dma_addr_t* dma_handle, unsigned int gfp) {
  (void)dev;
  (void)gfp;

  uint64_t dma_addr = 0;
  void* cpu_addr = usr_linux_emu::g_dma_pool.allocate(size, &dma_addr);
  if (cpu_addr && dma_handle) {
    *dma_handle = static_cast<dma_addr_t>(dma_addr);
  }
  return cpu_addr;
}

void dma_free_coherent(struct device* dev, size_t size,
                        void* cpu_addr, dma_addr_t dma_handle) {
  (void)dev;
  (void)size;
  (void)cpu_addr;
  usr_linux_emu::g_dma_pool.free(static_cast<uint64_t>(dma_handle));
}

dma_addr_t dma_map_single(struct device* dev, void* cpu_addr,
                           size_t size, enum dma_data_direction dir) {
  // Stub - streaming deferred per ADR-073 D6 (conditions 2/3 not triggered)
  (void)dev;
  (void)cpu_addr;
  (void)size;
  (void)dir;
  return 0;
}
