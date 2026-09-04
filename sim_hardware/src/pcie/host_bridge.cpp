// sim_hardware/src/pcie/host_bridge.cpp — 占位实现（Wave 1C）
// 真实实现见 Change-2 (Stage 5.5.2)
#include "pcie/host_bridge.h"

#include <cerrno>

namespace usr_linux_emu::sim_hardware::pcie {

int host_bridge_enumerate(DiscoveredDevice* devs, std::size_t max_devs,
                          std::size_t* out_count) {
    if (!out_count) return -EINVAL;
    *out_count = 0;
    (void)devs;
    (void)max_devs;
    return -ENOSYS;  // Wave 1C 占位；Change-2 真实调用 PcieRootComplexTLM
}

int host_bridge_bypass_write(uint8_t bar, uint64_t offset,
                             const void* src, std::size_t len) {
    (void)bar; (void)offset; (void)src; (void)len;
    return -ENOSYS;
}

int host_bridge_bypass_read(uint8_t bar, uint64_t offset,
                            void* dst, std::size_t len) {
    (void)bar; (void)offset; (void)dst; (void)len;
    return -ENOSYS;
}

}  // namespace usr_linux_emu::sim_hardware::pcie