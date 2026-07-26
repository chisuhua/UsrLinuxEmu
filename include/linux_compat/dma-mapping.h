#pragma once

// linux_compat/dma-mapping.h - DMA coherent memory allocation compatibility
//
// Provides Linux kernel dma_alloc_coherent/dma_free_coherent/dma_map_single
// APIs in user-space. Per ADR-073 D4, DMA coherent memory uses an independent
// mmap pool (DmaCoherentPool), physically isolated from VRAM backing store.
//
// Streaming DMA (dma_map_page/dma_unmap_page/dma_map_sg) is deferred per
// ADR-073 D6 (conditions 2/3 not triggered).
//
// See: openspec/changes/stage4-1-bar-ioremap/design.md §1.2

#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

// DMA bus address type (matches Linux dma_addr_t)
typedef uint64_t dma_addr_t;

// DMA data direction enum (matches Linux enum dma_data_direction)
enum dma_data_direction {
  DMA_BIDIRECTIONAL = 0,
  DMA_TO_DEVICE     = 1,
  DMA_FROM_DEVICE   = 2,
  DMA_NONE          = 3,
};

// GFP flags - already defined in memory.h, guard for standalone use
#ifndef GFP_KERNEL
#define GFP_KERNEL 0
#endif

// struct device forward declaration (from existing compat layer)
struct device;

// Allocate coherent DMA memory - returns cpu_addr, sets dma_handle
// Per ADR-073 D4: backed by DmaCoherentPool (independent mmap)
void* dma_alloc_coherent(struct device* dev, size_t size,
                          dma_addr_t* dma_handle, unsigned int gfp);

// Free previously allocated coherent DMA memory
void dma_free_coherent(struct device* dev, size_t size,
                        void* cpu_addr, dma_addr_t dma_handle);

// Map a single buffer for streaming DMA
// Stub: streaming deferred per ADR-073 D6 (conditions 2/3 not triggered)
dma_addr_t dma_map_single(struct device* dev, void* cpu_addr,
                           size_t size, enum dma_data_direction dir);

#ifdef __cplusplus
}
#endif
