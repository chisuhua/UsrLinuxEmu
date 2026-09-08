#include "cpptlm/bridge.h"
#include "cpptlm/endpoint.h"
#include "pcie/bypass.h"

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
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
  IntrDeliverCb msix_cb{nullptr};
  void* msix_ctx{nullptr};
  std::mutex mutex;
};

namespace {

// ── CppTLM ABI C signatures (from CppTLM/include/abi/cpptlm_emulator.h) ──

struct CpptlmDeviceInfo {
  uint16_t vendor_id;
  uint16_t device_id;
  uint8_t  revision;
  uint32_t subsys_vendor_id;
  uint32_t subsys_device_id;
  char     profile_path[256];
  uint64_t visible_vram_size;
  uint64_t invisible_vram_size;
  uint64_t va_region_size;
  uint32_t gpu_id;
  uint16_t gfx_version;
  uint16_t bdf;
  uint64_t bar_sizes[6];
};

using cpptlm_emulator_t = struct cpptlm_emulator_opaque;
using cpptlm_handle = uint64_t;

struct CpptlmSymbols {
  cpptlm_emulator_t* (*create)(const char* profile_path);
  cpptlm_emulator_t* (*create_by_id)(uint32_t dev_id);
  void (*destroy)(cpptlm_emulator_t* emu);
  int (*open)(uint32_t dev_id, cpptlm_handle* out_handle);
  int (*close)(cpptlm_handle handle);
  int (*get_adapter_info)(cpptlm_handle handle, CpptlmDeviceInfo* out_info);
  int (*mmio_read)(cpptlm_emulator_t* emu, uint8_t bar,
                   uint64_t offset, void* buf, size_t len);
  int (*mmio_write)(cpptlm_emulator_t* emu, uint8_t bar,
                    uint64_t offset, const void* buf, size_t len);
  int (*backdoor_read)(cpptlm_emulator_t* emu, uint8_t bar,
                       uint64_t offset, void* buf, size_t len);
  int (*backdoor_write)(cpptlm_emulator_t* emu, uint8_t bar,
                        uint64_t offset, const void* buf, size_t len);
  int (*pcie_config_read)(cpptlm_emulator_t* emu, uint16_t offset,
                          uint8_t width, uint32_t* out_value);
  int (*pcie_config_write)(cpptlm_emulator_t* emu, uint16_t offset,
                           uint8_t width, uint32_t value);
  int (*msix_init)(cpptlm_emulator_t* emu, uint32_t table_size, uint32_t mask);
  int (*msix_update_pending)(cpptlm_emulator_t* emu, uint32_t vector);
  int (*msix_clear_pending)(cpptlm_emulator_t* emu, uint32_t vector);
  int (*lookup_register)(cpptlm_emulator_t* emu, uint32_t offset,
                         char* out_name, size_t name_buf_size);
  const char* (*get_version)(void);
  int (*get_device_count)(void);
  int (*get_device_info)(uint32_t dev_id, CpptlmDeviceInfo* out_info);
  int (*register_callbacks)(cpptlm_emulator_t* emu, void* intr_cb,
                            void* error_cb, void* reset_complete_cb,
                            void* power_cb, void* user_ctx);
  int (*register_backdoor_cb)(cpptlm_emulator_t* emu, void* cb);
  int (*register_dma_translate_cb)(cpptlm_emulator_t* emu, void* cb);
};

// ── Bridge-level CppTLM state (separate from backdoor_endpoint's state) ──

struct CpptlmBridgeState {
  std::mutex mu;
  void* dl_handle = nullptr;
  CpptlmSymbols syms;
  bool resolved = false;
  bool load_failed = false;
};

CpptlmBridgeState& bridge_state() {
  static CpptlmBridgeState s;
  return s;
}

bool resolve_bridge_symbols(CpptlmSymbols& out_syms, void*& out_handle) {
  out_handle = dlopen("libcpptlm_emulator.so", RTLD_NOW | RTLD_LOCAL);
  if (!out_handle) {
    out_handle = dlopen("../CppTLM/build/lib/libcpptlm_emulator.so",
                        RTLD_NOW | RTLD_LOCAL);
  }
  if (!out_handle) {
    out_handle = dlopen("/workspace/project/CppTLM/build/lib/libcpptlm_emulator.so",
                        RTLD_NOW | RTLD_LOCAL);
  }
  if (!out_handle) return false;

  auto sym = [&](const char* name) -> void* {
    return dlsym(out_handle, name);
  };

  struct NamedSym { const char* name; void** slot; };
  NamedSym table[] = {
    {"cpptlm_emulator_create",             (void**)&out_syms.create},
    {"cpptlm_emulator_create_by_id",       (void**)&out_syms.create_by_id},
    {"cpptlm_emulator_destroy",            (void**)&out_syms.destroy},
    {"cpptlm_emulator_open",               (void**)&out_syms.open},
    {"cpptlm_emulator_close",              (void**)&out_syms.close},
    {"cpptlm_emulator_get_adapter_info",   (void**)&out_syms.get_adapter_info},
    {"cpptlm_emulator_mmio_read",          (void**)&out_syms.mmio_read},
    {"cpptlm_emulator_mmio_write",         (void**)&out_syms.mmio_write},
    {"cpptlm_emulator_backdoor_read",      (void**)&out_syms.backdoor_read},
    {"cpptlm_emulator_backdoor_write",     (void**)&out_syms.backdoor_write},
    {"cpptlm_emulator_pcie_config_read",   (void**)&out_syms.pcie_config_read},
    {"cpptlm_emulator_pcie_config_write",  (void**)&out_syms.pcie_config_write},
    {"cpptlm_emulator_msix_init",          (void**)&out_syms.msix_init},
    {"cpptlm_emulator_msix_update_pending",(void**)&out_syms.msix_update_pending},
    {"cpptlm_emulator_msix_clear_pending", (void**)&out_syms.msix_clear_pending},
    {"cpptlm_emulator_lookup_register",    (void**)&out_syms.lookup_register},
    {"cpptlm_emulator_get_version",        (void**)&out_syms.get_version},
    {"cpptlm_emulator_get_device_count",   (void**)&out_syms.get_device_count},
    {"cpptlm_emulator_get_device_info",    (void**)&out_syms.get_device_info},
    {"cpptlm_emulator_register_callbacks", (void**)&out_syms.register_callbacks},
    {"cpptlm_emulator_register_backdoor_cb",(void**)&out_syms.register_backdoor_cb},
    {"cpptlm_emulator_register_dma_translate_cb",
                                            (void**)&out_syms.register_dma_translate_cb},
  };

  for (auto& ns : table) {
    void* p = sym(ns.name);
    if (!p) {
      std::fprintf(stderr,
                   "[bridge] WARN: cpptlm ABI missing: %s — %s\n",
                   ns.name, dlerror());
      dlclose(out_handle);
      out_handle = nullptr;
      return false;
    }
    *(ns.slot) = p;
  }
  return true;
}

bool ensure_bridge_loaded() {
  std::lock_guard<std::mutex> lock(bridge_state().mu);
  if (bridge_state().resolved) return true;
  if (bridge_state().load_failed) return false;

  void* h = nullptr;
  if (resolve_bridge_symbols(bridge_state().syms, h)) {
    bridge_state().dl_handle = h;
    bridge_state().resolved = true;
    std::fprintf(stderr,
                 "[bridge] OK: resolved 22 CppTLM ABI symbols "
                 "(libcpptlm_emulator.so loaded for bridge.cpp)\n");
    return true;
  }
  bridge_state().load_failed = true;
  std::fprintf(stderr,
               "[bridge] WARN: libcpptlm_emulator.so not found; "
               "CpptlmBridge kCpptlm backend falling back to -ENOSYS\n");
  return false;
}

// ── Bridge-level helpers (need backdoor endpoint to integrate later) ──
// For bridge.cpp we delegate to backdoor_endpoint's CppTLM handles via the
// active CpptlmBridge_set_active / CpptlmBridge_get pattern. PcieEndpointIP
// is bound to the bridge via attach_endpoint, and its dispatch to CppTLM ABI
// is via the symbol table below.

CpptlmBridge* g_active_bridge = nullptr;
std::mutex g_active_bridge_mutex;

struct TlpGuard {
  TlpGuard() { bypass_enter_tlp(); }
  ~TlpGuard() { bypass_exit_tlp(); }
};

bool valid_mmio(uint8_t bar, uint64_t offset, size_t len) {
  if (bar >= 6) return false;
  if (len == 0) return false;
  if (len > 4096) return false;
  if (offset > 4096) return false;
  if (len > 4096 - offset) return false;
  if (len != 1 && len != 2 && len != 4 && len != 8) return false;
  if (offset % len != 0) return false;
  if (offset + len < offset) return false;
  return true;
}
}  // namespace

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
    // Try to load libcpptlm_emulator.so; fall back to -ENOSYS on failure
    if (!ensure_bridge_loaded()) {
      return -ENOSYS;
    }
  }

  impl_->backend = params.backend;
  impl_->flags = params.flags;
  std::memset(impl_->topology_path, 0, sizeof(impl_->topology_path));
  if (params.topology_path) {
    std::strncpy(impl_->topology_path, params.topology_path,
                 sizeof(impl_->topology_path) - 1);
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

  if (impl_->backend == CpptlmBackendKind::kCpptlm) {
    if (!bridge_state().resolved) return -ENOSYS;
    // TODO(P4.NEW-D): plumb emu through and call syms.mmio_read
    return -ENOSYS;
  }

  TlpGuard tlp;
  std::memcpy(buf, impl_->bars[bar].data() + offset, len);
  return 0;
}

int CpptlmBridge::mmio_write(uint8_t bar, uint64_t offset, const void* buf,
                            size_t len) {
  if (!impl_) return -EINVAL;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (!impl_->initialized) return -ENODEV;
  if (!buf || !valid_mmio(bar, offset, len)) return -EINVAL;

  TlpGuard tlp;
  std::memcpy(impl_->bars[bar].data() + offset, buf, len);
  return 0;
}

int CpptlmBridge::backdoor_read(uint8_t bar, uint64_t offset, void* buf,
                                size_t len) {
  if (!impl_) return -EINVAL;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (!impl_->initialized) return -ENODEV;
  if (!buf || !valid_mmio(bar, offset, len)) return -EINVAL;

  if (impl_->backend == CpptlmBackendKind::kCpptlm) {
    if (!bridge_state().resolved) return -ENOSYS;
    return -ENOSYS;  // TODO(P4.NEW-D): call syms.backdoor_read(emu, ...)
  }
  return -ENOSYS;
}

int CpptlmBridge::backdoor_write(uint8_t bar, uint64_t offset, const void* buf,
                                 size_t len) {
  if (!impl_) return -EINVAL;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (!impl_->initialized) return -ENODEV;
  if (!buf || !valid_mmio(bar, offset, len)) return -EINVAL;

  if (impl_->backend == CpptlmBackendKind::kCpptlm) {
    return -ENOSYS;  // see backdoor_read note
  }
  return -ENOSYS;
}

int CpptlmBridge::config_read(uint16_t offset, uint32_t* value) {
  if (!impl_) return -EINVAL;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (!impl_->initialized) return -ENODEV;
  if (!value || offset > 4092) return -EINVAL;
  TlpGuard tlp;
  std::memcpy(value, impl_->config_space.data() + offset, sizeof(*value));
  return 0;
}

int CpptlmBridge::config_write(uint16_t offset, uint32_t value) {
  if (!impl_) return -EINVAL;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (!impl_->initialized) return -ENODEV;
  if (offset > 4092) return -EINVAL;
  TlpGuard tlp;
  std::memcpy(impl_->config_space.data() + offset, &value, sizeof(value));
  return 0;
}

int CpptlmBridge::register_msix_callback(IntrDeliverCb cb, void* ctx) {
  if (!impl_) return -EINVAL;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (!impl_->initialized) return -ENODEV;
  impl_->msix_cb = cb;
  impl_->msix_ctx = ctx;
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

extern "C" void bridge_inject_msix_shim(uint32_t vector) {
  std::lock_guard<std::mutex> g_lock(g_active_bridge_mutex);
  CpptlmBridge* b = g_active_bridge;
  if (!b) return;
  b->inject_msix_for_test(vector);
}

void CpptlmBridge::inject_msix_for_test(uint32_t vector) {
  if (!impl_) return;
  IntrDeliverCb cb;
  void* ctx;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->initialized) return;
    cb = impl_->msix_cb;
    ctx = impl_->msix_ctx;
  }
  if (cb) cb(vector, ctx);
}

}  // namespace usr_linux_emu::sim_hardware