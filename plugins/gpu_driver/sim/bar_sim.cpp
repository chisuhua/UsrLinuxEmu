#include "bar_sim.h"
#include "vram_store.h"
#include "hardware/mqd_state.h"

#include <cstring>

namespace usr_linux_emu {
    extern GpuVramStore g_vram_store;
}

static const uint64_t BAR0_HQD_WINDOW_SIZE = 0x4000;
static const int      BAR0_HQD_MAX_CHANNELS = 32;

static MQD g_hqd_pool[BAR0_HQD_MAX_CHANNELS];

static inline bool is_hqd_offset(uint64_t offset) {
    return offset >= BAR0_HQD_BASE
        && offset < BAR0_HQD_BASE + BAR0_HQD_WINDOW_SIZE;
}

static MQD* hqd_for_offset(uint64_t offset, uint32_t* reg_out) {
    uint64_t rel = offset - BAR0_HQD_BASE;
    uint32_t channel = static_cast<uint32_t>(rel / BAR0_HQD_STRIDE);
    uint32_t reg      = static_cast<uint32_t>(rel % BAR0_HQD_STRIDE);
    if (channel >= BAR0_HQD_MAX_CHANNELS) {
        return nullptr;
    }
    if (reg_out) *reg_out = reg;
    return &g_hqd_pool[channel];
}

void* sim_bar_ioremap(uint64_t phys_addr, uint64_t size) {
    return usr_linux_emu::g_vram_store.bar_ioremap(phys_addr, size);
}

void sim_bar_iounmap(void* addr, uint64_t size) {
    usr_linux_emu::g_vram_store.bar_iounmap(addr, size);
}

void sim_bar0_writel(uint64_t offset, uint32_t value) {
    if (is_hqd_offset(offset)) {
        uint32_t reg = 0;
        MQD* mqd = hqd_for_offset(offset, &reg);
        if (!mqd) return;

        if (reg == 0x00) {
            if (value & 0x00000001) {
                mqd_state_activate(mqd);
            }
            if (value & 0x00000002) {
                mqd_state_preempt(mqd);
            }
            if ((value & 0x00000003) == 0) {
                mqd_state_deactivate(mqd);
            }
        } else {
            volatile uint32_t* dst =
                reinterpret_cast<volatile uint32_t*>(
                    reinterpret_cast<char*>(mqd) + reg);
            *dst = value;
        }
        return;
    }

    void* backing = sim_bar_ioremap(0x0000, 0x8000);
    if (!backing) return;
    volatile uint32_t* dst =
        reinterpret_cast<volatile uint32_t*>(
            reinterpret_cast<char*>(backing) + offset);
    *dst = value;
}

uint32_t sim_bar0_readl(uint64_t offset) {
    if (is_hqd_offset(offset)) {
        uint32_t reg = 0;
        MQD* mqd = hqd_for_offset(offset, &reg);
        if (!mqd) return 0;

        if (reg == 0x04) {
            return mqd->state;
        }

        volatile uint32_t* src =
            reinterpret_cast<volatile uint32_t*>(
                reinterpret_cast<char*>(mqd) + reg);
        return *src;
    }

    void* backing = sim_bar_ioremap(0x0000, 0x8000);
    if (!backing) return 0;
    volatile uint32_t* src =
        reinterpret_cast<volatile uint32_t*>(
            reinterpret_cast<char*>(backing) + offset);
    return *src;
}
