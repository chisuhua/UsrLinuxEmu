// sim_hardware/src/cpptlm/bridge.cpp — 占位实现（Wave 1C）
#include "cpptlm/bridge.h"

namespace usr_linux_emu::sim_hardware::cpptlm {

CpptlmBridge::CpptlmBridge() = default;
CpptlmBridge::~CpptlmBridge() = default;

int CpptlmBridge::init(const char* profile_path) {
    (void)profile_path;
    return -ENOSYS;  // Change-2 真实调 cpptlm_emulator_create
}

int CpptlmBridge::register_msix_callback(IntrDeliverCb cb, void* ctx) {
    (void)cb; (void)ctx;
    return -ENOSYS;
}

int CpptlmBridge::mmio_read(uint8_t bar, uint64_t offset, void* buf, std::size_t len) {
    (void)bar; (void)offset; (void)buf; (void)len;
    return -ENOSYS;
}

int CpptlmBridge::mmio_write(uint8_t bar, uint64_t offset, const void* buf, std::size_t len) {
    (void)bar; (void)offset; (void)buf; (void)len;
    return -ENOSYS;
}

int CpptlmBridge::config_read(uint16_t offset, uint32_t* val) {
    (void)offset; (void)val;
    return -ENOSYS;
}

int CpptlmBridge::config_write(uint16_t offset, uint32_t val) {
    (void)offset; (void)val;
    return -ENOSYS;
}

int CpptlmBridge::attach_endpoint(void* endpoint_handle) {
    (void)endpoint_handle;
    return -ENOSYS;
}

}  // namespace usr_linux_emu::sim_hardware::cpptlm