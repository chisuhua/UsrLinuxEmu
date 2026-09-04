#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

namespace usr_linux_emu::sim_hardware {

enum class CpptlmBackendKind {
  kMock = 0,
  kCpptlm = 1,
};

using IntrDeliverCb = std::function<void(uint32_t vector, void* ctx)>;

struct CpptlmBridgeInitParams {
  CpptlmBackendKind backend = CpptlmBackendKind::kMock;
  const char* topology_path = nullptr;
  uint32_t flags = 0;
};

class CpptlmBridge {
 public:
  CpptlmBridge();
  ~CpptlmBridge();

  int init(const CpptlmBridgeInitParams& params);
  void destroy();

  int mmio_read(uint8_t bar, uint64_t offset, void* buf, size_t len);
  int mmio_write(uint8_t bar, uint64_t offset, const void* buf, size_t len);

  int config_read(uint16_t offset, uint32_t* value);
  int config_write(uint16_t offset, uint32_t value);

  int register_msix_callback(IntrDeliverCb cb, void* ctx);
  int attach_endpoint(void* endpoint_handle);

 private:
  struct Impl;
  Impl* impl_;
};

CpptlmBridge* CpptlmBridge_get();
int CpptlmBridge_set_active(CpptlmBridge* bridge);

}  // namespace usr_linux_emu::sim_hardware

namespace usr_linux_emu::sim_hardware::cpptlm {
using CpptlmBridge = ::usr_linux_emu::sim_hardware::CpptlmBridge;
using IntrDeliverCb = ::usr_linux_emu::sim_hardware::IntrDeliverCb;
using CpptlmBackendKind = ::usr_linux_emu::sim_hardware::CpptlmBackendKind;
using CpptlmBridgeInitParams = ::usr_linux_emu::sim_hardware::CpptlmBridgeInitParams;
}  // namespace usr_linux_emu::sim_hardware::cpptlm
