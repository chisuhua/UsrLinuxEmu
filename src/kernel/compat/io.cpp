// linux_compat io.cpp - I/O memory mapping implementation
//
// Per ADR-069 D2: ioremap/iounmap are ① layer kernel API (NOT through HAL).
// Delegates to sim_bar_ioremap/sim_bar_iounmap (③ sim layer) via sim_proxy
// pattern (ADR-063).
//
// The sim symbols (sim_bar_ioremap, sim_bar_iounmap) are resolved at runtime
// from gpu_sim when linked into test executables. When no sim is attached,
// they return nullptr (graceful degradation).

#include "linux_compat/io.h"
#include "sim/bar_sim.h"

#include <cstddef>

void* ioremap(phys_addr_t phys_addr, unsigned long size) {
  return sim_bar_ioremap(static_cast<uint64_t>(phys_addr),
                         static_cast<uint64_t>(size));
}

void iounmap(volatile void __iomem* addr) {
  // Size is tracked internally by sim_bar_iounmap (matches BAR by pointer)
  // Cast away volatile since sim_bar_iounmap takes plain void*
  sim_bar_iounmap(const_cast<void*>(addr), 0);
}
