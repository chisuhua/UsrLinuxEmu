#pragma once

#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

void* sim_bar_ioremap(uint64_t phys_addr, uint64_t size);
void  sim_bar_iounmap(void* addr, uint64_t size);

#ifdef __cplusplus
}
#endif
