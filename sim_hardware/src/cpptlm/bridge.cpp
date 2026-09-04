#include "cpptlm/bridge.h"
#include "cpptlm/endpoint.h"

#include <array>
#include <cerrno>
#include <cstring>
#include <mutex>

namespace usr_linux_emu::sim_hardware {

struct CpptlmBridge::Impl {
  bool initialized{false};
  CpptlmBackendKind backend{CpptlmBackendKind::kMock};
  char topology_path[256]{0};
  uint32_t flags{0};
  cpptlm::PcieEndpointIP* endpoint{nullptr};
  std::array<uint8_t, 4096> config_space{};
  std::array<std::array<uint8_t, 4096>, 6> bars{};
  std::mutex mutex;
};

namespace {
CpptlmBridge* g_active_bridge = nullptr;
std::mutex g_active_bridge_mutex;

bool valid_mmio(uint8_t bar, uint64_t offset, size_t len) {
  if (bar >= 6) return false;
  if (len == 0) return false;
  if (len > 4096) return false;
  if (offset > 4096) return false;
  if (len > 4096 - offset) return false;  // would overflow buffer
  // Width must be power-of-2 in {1,2,4,8}
  if (len != 1 && len != 2 && len != 4 && len != 8) return false;
  // Alignment: offset must be a multiple of len
  if (offset % len != 0) return false;
  // 64-bit offset+len overflow check
  if (offset + len < offset) return false;  // wrap-around guard
  return true;
}
}

CpptlmBridge::CpptlmBridge() : impl_(new Impl()) {}

CpptlmBridge::~CpptlmBridge() {
  destroy();
  delete impl_;
  impl_ = nullptr;
}

int CpptlmBridge::init(const CpptlmBridgeInitParams& params) {
  if (!impl_) return -EINVAL;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (impl_->initialized) {
    return -EBUSY;
  }
  if (params.backend == CpptlmBackendKind::kCpptlm) {
    return -ENOSYS;
  }
  impl_->backend = params.backend;
  impl_->flags = params.flags;
  std::memset(impl_->topology_path, 0, sizeof(impl_->topology_path));
  if (params.topology_path) {
    std::strncpy(impl_->topology_path, params.topology_path, sizeof(impl_->topology_path) - 1);
  }
  impl_->config_space.fill(0);
  for (auto& bar : impl_->bars) bar.fill(0);
  impl_->initialized = true;

  std::lock_guard<std::mutex> g_lock(g_active_bridge_mutex);
  if (!g_active_bridge) {
    g_active_bridge = this;
  }
  return 0;
}

void CpptlmBridge::destroy() {
  if (!impl_) return;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->initialized = false;
  std::lock_guard<std::mutex> g_lock(g_active_bridge_mutex);
  if (g_active_bridge == this) {
    g_active_bridge = nullptr;
  }
}

int CpptlmBridge::mmio_read(uint8_t bar, uint64_t offset, void* buf, size_t len) {
  if (!impl_) return -EINVAL;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (!impl_->initialized) return -ENODEV;
  if (!buf || !valid_mmio(bar, offset, len)) return -EINVAL;
  std::memcpy(buf, impl_->bars[bar].data() + offset, len);
  return 0;
}

int CpptlmBridge::mmio_write(uint8_t bar, uint64_t offset, const void* buf, size_t len) {
  if (!impl_) return -EINVAL;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (!impl_->initialized) return -ENODEV;
  if (!buf || !valid_mmio(bar, offset, len)) return -EINVAL;
  std::memcpy(impl_->bars[bar].data() + offset, buf, len);
  return 0;
}

int CpptlmBridge::config_read(uint16_t offset, uint32_t* value) {
  if (!impl_) return -EINVAL;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (!impl_->initialized) return -ENODEV;
  if (!value || offset > 4092) return -EINVAL;
  std::memcpy(value, impl_->config_space.data() + offset, sizeof(*value));
  return 0;
}

int CpptlmBridge::config_write(uint16_t offset, uint32_t value) {
  if (!impl_) return -EINVAL;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (!impl_->initialized) return -ENODEV;
  if (offset > 4092) return -EINVAL;
  std::memcpy(impl_->config_space.data() + offset, &value, sizeof(value));
  return 0;
}

int CpptlmBridge::register_msix_callback(IntrDeliverCb cb, void* ctx) {
  if (!impl_) return -EINVAL;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (!impl_->initialized) return -ENODEV;
  (void)cb; (void)ctx;
  return 0;
}

int CpptlmBridge::attach_endpoint(void* endpoint_handle) {
  if (!impl_ || !endpoint_handle) return -EINVAL;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (!impl_->initialized) return -ENODEV;
  impl_->endpoint = static_cast<cpptlm::PcieEndpointIP*>(endpoint_handle);
  return 0;
}

CpptlmBridge* CpptlmBridge_get() {
  std::lock_guard<std::mutex> lock(g_active_bridge_mutex);
  return g_active_bridge;
}

int CpptlmBridge_set_active(CpptlmBridge* bridge) {
  std::lock_guard<std::mutex> lock(g_active_bridge_mutex);
  g_active_bridge = bridge;
  return 0;
}

}  // namespace usr_linux_emu::sim_hardware
