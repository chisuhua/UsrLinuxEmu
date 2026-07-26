#pragma once

// linux_compat/io.h - I/O memory access compatibility layer
//
// Provides Linux kernel ioremap/iounmap/readl/writel/ioread32/iowrite32
// APIs in user-space. Per ADR-069 D2, these are classified as ① layer
// kernel API (NOT through HAL). The inline volatile accessors match
// Linux 6.12 LTS include/linux/io.h signatures.
//
// See: openspec/changes/stage4-1-bar-ioremap/design.md §1.1

#include <cstdint>
#include <cstddef>
#include <sys/types.h>

// __iomem is a sparse annotation in Linux; empty in user-space
#ifndef __iomem
#define __iomem
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Physical address type (matches Linux phys_addr_t from linux/types.h)
typedef uint64_t phys_addr_t;

// I/O memory mapping - maps a physical BAR address to a virtual address
// Per ADR-069: sim_proxy pattern delegates to sim_bar_ioremap (③ layer)
void* ioremap(phys_addr_t phys_addr, unsigned long size);

// Unmap previously mapped I/O memory
void iounmap(volatile void __iomem* addr);

// Inline volatile accessors - NOT through HAL (ADR-069 D2)
// These are direct volatile memory accesses, identical to Linux kernel

static inline uint32_t readl(const volatile void __iomem* addr) {
  return *(const volatile uint32_t*)addr;
}

static inline void writel(uint32_t value, volatile void __iomem* addr) {
  *(volatile uint32_t*)addr = value;
}

static inline uint32_t ioread32(const volatile void __iomem* addr) {
  return readl(addr);
}

static inline void iowrite32(uint32_t value, void __iomem* addr) {
  writel(value, addr);
}

#ifdef __cplusplus
}
#endif
