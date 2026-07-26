#include "bar_sim.h"
#include "vram_store.h"

namespace usr_linux_emu {
    extern GpuVramStore g_vram_store;
}

void* sim_bar_ioremap(uint64_t phys_addr, uint64_t size) {
    return usr_linux_emu::g_vram_store.bar_ioremap(phys_addr, size);
}

void sim_bar_iounmap(void* addr, uint64_t size) {
    usr_linux_emu::g_vram_store.bar_iounmap(addr, size);
}
