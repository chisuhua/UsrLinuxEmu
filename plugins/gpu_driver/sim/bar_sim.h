#pragma once

#include <cstdint>
#include <cstddef>

#define BAR0_HQD_BASE   0x4000
#define BAR0_HQD_STRIDE  64

#ifdef __cplusplus
extern "C" {
#endif

void* sim_bar_ioremap(uint64_t phys_addr, uint64_t size);
void  sim_bar_iounmap(void* addr, uint64_t size);

void     sim_bar0_writel(uint64_t offset, uint32_t value);
uint32_t sim_bar0_readl(uint64_t offset);

#ifdef __cplusplus
}
#endif
