// sim_hardware/include/cpptlm/bridge.h — CpptlmBridge 统一入口
// 封装 cpptlm_emulator.h C ABI 为 C++ 接口
// per ADR-088 §D5 + ADR-091 v0.2 §D3.3
#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>

namespace usr_linux_emu::sim_hardware::cpptlm {

/// MSI-X 中断投递回调（usr_linux_emu host 端）
using IntrDeliverCb = std::function<void(uint32_t vector, void* ctx)>;

/// CpptlmBridge：封装 cpptlm_emulator.h C ABI 的统一入口
class CpptlmBridge {
 public:
    CpptlmBridge();
    ~CpptlmBridge();

    /// 初始化（调 cpptlm_emulator_create + 加载 profile）
    int init(const char* profile_path);

    /// 注册 MSI-X 投递回调
    int register_msix_callback(IntrDeliverCb cb, void* ctx);

    /// Tier 1 直调 C ABI（无枚举）
    int mmio_read(uint8_t bar, uint64_t offset, void* buf, size_t len);
    int mmio_write(uint8_t bar, uint64_t offset, const void* buf, size_t len);
    int config_read(uint16_t offset, uint32_t* val);
    int config_write(uint16_t offset, uint32_t val);

    /// Tier 2: C++ composition（attach endpoint）
    int attach_endpoint(void* endpoint_handle);
};

}  // namespace usr_linux_emu::sim_hardware::cpptlm