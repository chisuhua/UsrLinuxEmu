/*
 * sim_hardware/src/cpptlm/backdoor_endpoint.cpp
 * 5.5.6-cpptlm-ep-binding — P4.NEW-A: real CppTLM BackdoorEndpoint implementation
 *
 * Design (see openspec/changes/2026-09-08-5-5-6-cpptlm-ep-binding/design.md §2):
 *   - 5 functions (acquire / release / get_adapter_info / read / write)
 *   - dlopen libcpptlm_emulator.so from dynamic linker path or sibling build path
 *   - dlsym 22 CppTLM ABI symbols (cpptlm_emulator_*)
 *   - dlopen fails → fall back to -ENOSYS (preserves 164/164 ctest baseline)
 */
#include "cpptlm/backdoor_endpoint.h"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <future>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace usr_linux_emu::sim_hardware::cpptlm::detail {

// ── CppTLM ABI C signatures (from CppTLM/include/abi/cpptlm_emulator.h) ──

struct cpptlm_device_info_s_abi {
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

using cpptlm_emulator_t_abi = struct cpptlm_emulator_opaque;
using cpptlm_emulator_handle_t_abi = uint64_t;

// ── Symbol table (22 ABI) ──

struct CpptlmSymbols {
  // Lifecycle
  cpptlm_emulator_t_abi* (*create)(const char* profile_path);
  cpptlm_emulator_t_abi* (*create_by_id)(uint32_t dev_id);
  void (*destroy)(cpptlm_emulator_t_abi* emu);

  // Adapter
  int (*open)(uint32_t dev_id, cpptlm_emulator_handle_t_abi* out_handle);
  int (*close)(cpptlm_emulator_handle_t_abi handle);
  int (*get_adapter_info)(cpptlm_emulator_handle_t_abi handle,
                          cpptlm_device_info_s_abi* out_info);

  // BAR MMIO
  int (*mmio_read)(cpptlm_emulator_t_abi* emu, uint8_t bar,
                   uint64_t offset, void* buf, size_t len);
  int (*mmio_write)(cpptlm_emulator_t_abi* emu, uint8_t bar,
                    uint64_t offset, const void* buf, size_t len);

  // Backdoor
  int (*backdoor_read)(cpptlm_emulator_t_abi* emu, uint8_t bar,
                       uint64_t offset, void* buf, size_t len);
  int (*backdoor_write)(cpptlm_emulator_t_abi* emu, uint8_t bar,
                        uint64_t offset, const void* buf, size_t len);

  // Config Space
  int (*pcie_config_read)(cpptlm_emulator_t_abi* emu, uint16_t offset,
                          uint8_t width, uint32_t* out_value);
  int (*pcie_config_write)(cpptlm_emulator_t_abi* emu, uint16_t offset,
                           uint8_t width, uint32_t value);

  // MSI-X
  int (*msix_init)(cpptlm_emulator_t_abi* emu, uint32_t table_size, uint32_t mask);
  int (*msix_update_pending)(cpptlm_emulator_t_abi* emu, uint32_t vector);
  int (*msix_clear_pending)(cpptlm_emulator_t_abi* emu, uint32_t vector);

  // Misc
  int (*lookup_register)(cpptlm_emulator_t_abi* emu, uint32_t offset,
                         char* out_name, size_t name_buf_size);
  const char* (*get_version)(void);
  int (*get_device_count)(void);
  int (*get_device_info)(uint32_t dev_id, cpptlm_device_info_s_abi* out_info);

  // Callbacks
  int (*register_callbacks)(cpptlm_emulator_t_abi* emu, void* intr_cb,
                            void* error_cb, void* reset_complete_cb,
                            void* power_cb, void* user_ctx);
  int (*register_backdoor_cb)(cpptlm_emulator_t_abi* emu, void* cb);
  int (*register_dma_translate_cb)(cpptlm_emulator_t_abi* emu, void* cb);
};

// ── Global state (singleton, lazy init) ──

struct LiveEmulator {
  cpptlm_emulator_t_abi* emu = nullptr;
  uint32_t dev_id = 0;
};

struct GlobalState {
  std::mutex mu;
  void* dl_handle = nullptr;
  CpptlmSymbols syms;
  bool syms_resolved = false;
  bool load_failed = false;
  std::unordered_map<uint64_t, LiveEmulator> live_emulators;
  uint64_t next_handle_id = 1;
};

GlobalState& global() {
  static GlobalState s;
  return s;
}

// ── Load CppTLM symbols ──

bool resolve_symbols_clean(CpptlmSymbols& out_syms) {
  auto load = [](const char* path) -> void* {
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
  };

  void* h = load("libcpptlm_emulator.so");
  if (!h) h = load("../CppTLM/build/lib/libcpptlm_emulator.so");
  if (!h) h = load("/workspace/project/CppTLM/build/lib/libcpptlm_emulator.so");
  if (!h) return false;

  auto sym = [h](const char* name) -> void* {
    return dlsym(h, name);
  };

  // Resolve all 24 symbols. Any failure aborts and returns false.
  struct NamedSym {
    const char* name;
    void** slot;
  };

  NamedSym table[] = {
    {"cpptlm_emulator_create",             reinterpret_cast<void**>(&out_syms.create)},
    {"cpptlm_emulator_create_by_id",       reinterpret_cast<void**>(&out_syms.create_by_id)},
    {"cpptlm_emulator_destroy",            reinterpret_cast<void**>(&out_syms.destroy)},
    {"cpptlm_emulator_open",               reinterpret_cast<void**>(&out_syms.open)},
    {"cpptlm_emulator_close",              reinterpret_cast<void**>(&out_syms.close)},
    {"cpptlm_emulator_get_adapter_info",   reinterpret_cast<void**>(&out_syms.get_adapter_info)},
    {"cpptlm_emulator_mmio_read",          reinterpret_cast<void**>(&out_syms.mmio_read)},
    {"cpptlm_emulator_mmio_write",         reinterpret_cast<void**>(&out_syms.mmio_write)},
    {"cpptlm_emulator_backdoor_read",      reinterpret_cast<void**>(&out_syms.backdoor_read)},
    {"cpptlm_emulator_backdoor_write",     reinterpret_cast<void**>(&out_syms.backdoor_write)},
    {"cpptlm_emulator_pcie_config_read",   reinterpret_cast<void**>(&out_syms.pcie_config_read)},
    {"cpptlm_emulator_pcie_config_write",  reinterpret_cast<void**>(&out_syms.pcie_config_write)},
    {"cpptlm_emulator_msix_init",          reinterpret_cast<void**>(&out_syms.msix_init)},
    {"cpptlm_emulator_msix_update_pending",reinterpret_cast<void**>(&out_syms.msix_update_pending)},
    {"cpptlm_emulator_msix_clear_pending", reinterpret_cast<void**>(&out_syms.msix_clear_pending)},
    {"cpptlm_emulator_lookup_register",    reinterpret_cast<void**>(&out_syms.lookup_register)},
    {"cpptlm_emulator_get_version",        reinterpret_cast<void**>(&out_syms.get_version)},
    {"cpptlm_emulator_get_device_count",   reinterpret_cast<void**>(&out_syms.get_device_count)},
    {"cpptlm_emulator_get_device_info",    reinterpret_cast<void**>(&out_syms.get_device_info)},
    {"cpptlm_emulator_register_callbacks", reinterpret_cast<void**>(&out_syms.register_callbacks)},
    {"cpptlm_emulator_register_backdoor_cb",reinterpret_cast<void**>(&out_syms.register_backdoor_cb)},
    {"cpptlm_emulator_register_dma_translate_cb",
                                            reinterpret_cast<void**>(&out_syms.register_dma_translate_cb)},
  };

  for (auto& ns : table) {
    void* p = sym(ns.name);
    if (!p) {
      std::fprintf(stderr,
                    "[backdoor_endpoint] WARN: cpptlm ABI missing: %s — %s\n",
                    ns.name, dlerror());
      // dlclose + return false → caller falls back to -ENOSYS
      dlclose(h);
      return false;
    }
    *(ns.slot) = p;
  }

  // Also resolve these two: they take a different signature (the symbols are
  // already in the table above, but we double-check for clarity).
  out_syms.open = reinterpret_cast<decltype(out_syms.open)>(sym("cpptlm_emulator_open"));
  out_syms.get_adapter_info = reinterpret_cast<decltype(out_syms.get_adapter_info)>(sym("cpptlm_emulator_get_adapter_info"));

  // Stash handle for cleanup (will be dlclose'd on shutdown, never per-test)
  global().dl_handle = h;
  return true;
}

bool ensure_loaded() {
  std::lock_guard<std::mutex> lock(global().mu);
  if (global().syms_resolved) return true;
  if (global().load_failed) return false;

  if (resolve_symbols_clean(global().syms)) {
    global().syms_resolved = true;
    std::fprintf(stderr,
                 "[backdoor_endpoint] OK: resolved 22 CppTLM ABI symbols "
                 "(libcpptlm_emulator.so loaded)\n");
    return true;
  }

  global().load_failed = true;
  std::fprintf(stderr,
               "[backdoor_endpoint] WARN: libcpptlm_emulator.so not found; "
               "backdoor_endpoint falling back to -ENOSYS\n");
  return false;
}

}  // namespace usr_linux_emu::sim_hardware::cpptlm::detail

// ── 5 public functions (match header linkage) ──

namespace usr_linux_emu::sim_hardware::cpptlm::detail {

// Look up emu by our internal handle_id
static cpptlm_emulator_t_abi* lookup_emu(uint64_t handle_id) {
  std::lock_guard<std::mutex> lock(global().mu);
  auto it = global().live_emulators.find(handle_id);
  if (it == global().live_emulators.end()) return nullptr;
  return it->second.emu;
}

static uint32_t lookup_dev_id(uint64_t handle_id) {
  std::lock_guard<std::mutex> lock(global().mu);
  auto it = global().live_emulators.find(handle_id);
  if (it == global().live_emulators.end()) return 0;
  return it->second.dev_id;
}

}  // namespace usr_linux_emu::sim_hardware::cpptlm::detail

int ule_dgpu_acquire(uint32_t dev_id, ule_dgpu_handle_t* out_handle) {
  using namespace usr_linux_emu::sim_hardware::cpptlm::detail;

  if (!out_handle) return -EINVAL;
  if (!ensure_loaded()) return -ENOSYS;

  // CppTLM assigns dev_id from 1 upward (dev_id=0 means "auto-assign").
  // We can't read back the assigned dev_id from the opaque emu struct, so
  // we require explicit non-zero dev_id to keep a stable handle→dev_id mapping.
  if (dev_id == 0) {
    std::fprintf(stderr,
                 "[backdoor_endpoint] WARN: dev_id=0 not supported "
                 "(CppTLM auto-assigns and we can't read it back); "
                 "use dev_id >= 1\n");
    return -EINVAL;
  }

  cpptlm_emulator_t_abi* emu = global().syms.create_by_id(dev_id);
  if (!emu) return -ENOSYS;

  uint64_t handle_id = 0;
  {
    std::lock_guard<std::mutex> lock(global().mu);
    handle_id = global().next_handle_id++;
    global().live_emulators[handle_id] = {emu, dev_id};
  }

  *out_handle = static_cast<ule_dgpu_handle_t>(handle_id);
  return 0;
}

int ule_dgpu_release(ule_dgpu_handle_t handle) {
  using namespace usr_linux_emu::sim_hardware::cpptlm::detail;

  if (handle == 0) return -EINVAL;
  if (!global().syms_resolved) return -ENOSYS;

  cpptlm_emulator_t_abi* emu = nullptr;
  {
    std::lock_guard<std::mutex> lock(global().mu);
    auto it = global().live_emulators.find(static_cast<uint64_t>(handle));
    if (it != global().live_emulators.end()) {
      emu = it->second.emu;
      global().live_emulators.erase(it);
    }
  }

  if (emu != nullptr) {
    global().syms.destroy(emu);
  }
  return 0;
}

int ule_dgpu_get_adapter_info(ule_dgpu_handle_t handle,
                               ule_dgpu_adapter_info* out_info) {
  using namespace usr_linux_emu::sim_hardware::cpptlm::detail;

  if (!out_info) return -EINVAL;
  if (handle == 0) return -EINVAL;
  if (!global().syms_resolved) return -ENOSYS;

  cpptlm_emulator_t_abi* emu = lookup_emu(static_cast<uint64_t>(handle));
  if (!emu) return -EINVAL;

  uint32_t dev_id = lookup_dev_id(static_cast<uint64_t>(handle));
  if (dev_id == 0) return -EINVAL;

  cpptlm_device_info_s_abi cpptlm_info{};
  int ret = global().syms.get_device_info(dev_id, &cpptlm_info);
  if (ret != 0) return ret;

  out_info->vendor_id          = cpptlm_info.vendor_id;
  out_info->device_id          = cpptlm_info.device_id;
  out_info->gpu_id             = cpptlm_info.gpu_id;
  out_info->gfx_version        = cpptlm_info.gfx_version;
  out_info->bdf                = cpptlm_info.bdf;
  out_info->visible_vram_size  = cpptlm_info.visible_vram_size;
  out_info->invisible_vram_size = cpptlm_info.invisible_vram_size;
  out_info->va_region_size     = cpptlm_info.va_region_size;
  for (int i = 0; i < 6; ++i) {
    out_info->bar_sizes[i] = cpptlm_info.bar_sizes[i];
  }
  return 0;
}

int ule_dgpu_read(ule_dgpu_handle_t handle, ule_dgpu_space space,
                  uint64_t offset, void* buf, size_t len) {
  using namespace usr_linux_emu::sim_hardware::cpptlm::detail;

  if (!buf || len == 0) return -EINVAL;
  if (handle == 0) return -EINVAL;
  if (!global().syms_resolved) return -ENOSYS;

  cpptlm_emulator_t_abi* emu = lookup_emu(static_cast<uint64_t>(handle));
  if (!emu) return -EINVAL;

  switch (space) {
    case ule_dgpu_space::kConfig:
      if (offset > 4096 || len > 4096 ||
          (len != 1 && len != 2 && len != 4)) return -EINVAL;
      {
        uint32_t val = 0;
        uint8_t width = (len == 4) ? 4 : (len == 2) ? 2 : 1;
        int ret = global().syms.pcie_config_read(emu,
            static_cast<uint16_t>(offset), width, &val);
        if (ret != 0) return ret;
        std::memcpy(buf, &val, len);
      }
      return 0;

    case ule_dgpu_space::kBarMmio:
      return global().syms.mmio_read(emu, /*bar=*/0, offset, buf, len);

    case ule_dgpu_space::kBarVram:
      return global().syms.backdoor_read(emu, /*bar=*/0, offset, buf, len);

    case ule_dgpu_space::kAxiDirect:
      return -EOPNOTSUPP;
  }
  return -EINVAL;
}

int ule_dgpu_write(ule_dgpu_handle_t handle, ule_dgpu_space space,
                   uint64_t offset, const void* buf, size_t len) {
  using namespace usr_linux_emu::sim_hardware::cpptlm::detail;

  if (!buf || len == 0) return -EINVAL;
  if (handle == 0) return -EINVAL;
  if (!global().syms_resolved) return -ENOSYS;

  cpptlm_emulator_t_abi* emu = lookup_emu(static_cast<uint64_t>(handle));
  if (!emu) return -EINVAL;

  switch (space) {
    case ule_dgpu_space::kConfig:
      if (offset > 4096 || len > 4096 ||
          (len != 1 && len != 2 && len != 4)) return -EINVAL;
      {
        uint32_t val = 0;
        std::memcpy(&val, buf, len);
        uint8_t width = (len == 4) ? 4 : (len == 2) ? 2 : 1;
        return global().syms.pcie_config_write(emu,
            static_cast<uint16_t>(offset), width, val);
      }

    case ule_dgpu_space::kBarMmio:
      return global().syms.mmio_write(emu, /*bar=*/0, offset, buf, len);

    case ule_dgpu_space::kBarVram:
      return global().syms.backdoor_write(emu, /*bar=*/0, offset, buf, len);

    case ule_dgpu_space::kAxiDirect:
      return -EOPNOTSUPP;
  }
  return -EINVAL;
}
