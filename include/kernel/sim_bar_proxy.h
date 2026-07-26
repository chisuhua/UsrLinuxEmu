/**
 * @file sim_bar_proxy.h
 * @brief µ-thin forward-prototype header for ①→③ BAR/ioremap sim bridging
 *
 * Per ADR-063 D4 Path X: ① (kernel env) code includes THIS header, NOT
 * ③'s bar_sim.h. Only extern "C" function prototypes are exposed.
 * Linker resolves sim_bar_ioremap/sim_bar_iounmap from plugins/gpu_driver/sim/
 * at link time.
 *
 * See: openspec/changes/stage4-1-bar-ioremap/design.md §D1
 */

#ifndef KERNEL_SIM_BAR_PROXY_H
#define KERNEL_SIM_BAR_PROXY_H

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

#endif /* KERNEL_SIM_BAR_PROXY_H */