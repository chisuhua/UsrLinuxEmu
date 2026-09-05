/*
 * plugin.cpp - GPU 驱动仿真插件入口
 */
#include <iostream>
#include <memory>
#include <vector>
#include "kernel/device/device.h"
#include "kernel/module_loader.h"
#include "kernel/vfs.h"
#include "drv/gpgpu_device.h"
#include "hal/hal_user.h"
#include "sim/hardware/doorbell_emu.h"
#include "sim/scheduler/global_scheduler.h"
#include "sim/vram_store.h"
#include "sim/dma_coherent_pool.h"
#include "drv/kfd/kfd_module.h"

#include <kernel/uvm/mm_shim.h>
#include "drv/kfd_sim_bridge.h"

#include "pci_probe.h"

namespace {
struct HalHolder {
  struct gpu_hal_ops hal;
  struct hal_user_context ctx;
  DoorbellEmu doorbell;
  GlobalScheduler scheduler;
  hal_puller_handle_t puller_handle = 0;
};

// One HalHolder per DiscoveredDevice; loop populates this in plugin_init.
static std::vector<std::unique_ptr<HalHolder>> hal_holders;

/* Phase C.2.1: single-process mm_shim fallback. Real C-12 uses
 * kfd_process_create()->mm_shim, but Tier-1 plugin init runs before any
 * process is created. The bridge holds the singleton mm_shim and
 * initializes it to a process-lifetime instance so MAP/UNMAP handlers
 * have something to register VMAs against. */
static struct us_mm_shim g_plugin_mm_shim;
static bool g_mm_shim_inited = false;
} // anonymous namespace

using namespace usr_linux_emu;
using usr_linux_emu::pci::pci_probe_enumerate_from_sim_hardware;
using usr_linux_emu::sim_hardware::pcie::DiscoveredDevice;

extern "C" {

static int plugin_init_internal() {
  std::cout << "[GpuPlugin] Initializing...\n";

  // C-12 B.1.1: KFD subsystem init (per kfd_module.h bridge contract)
  int kfd_ret = kfd_module_init();
  if (kfd_ret != 0) {
    std::cerr << "[GpuPlugin] Failed to init KFD subsystem: " << kfd_ret << "\n";
    return kfd_ret;
  }

  // Stage 4.1: process-global sim singletons. Composition root owns these
  // (not GpgpuDevice ctor) because they are process-lifetime, not per-device.
  if (!g_vram_store.init(256)) {
    std::cerr << "[GpuPlugin] WARN: g_vram_store.init(256) failed "
              << "(BAR support disabled)\n";
  }
  if (!g_dma_pool.init()) {
    std::cerr << "[GpuPlugin] WARN: g_dma_pool.init() failed\n";
  }

  // Phase C.2.1: bind mm_shim to bridge + GpgpuDevice before VFS registration.
  // PID 0 = "kernel/driver-internal" host process (no real client yet).
  if (!g_mm_shim_inited) {
    us_mm_shim_init(&g_plugin_mm_shim, 0);
    g_mm_shim_inited = true;
  }
  kfd_sim_set_mm_shim(&g_plugin_mm_shim);

  // E.2.4 L1↔L2 bridge: initialize sim bridge state (sim_pm_create + internal maps)
  kfd_sim_reset();

  constexpr size_t kMaxDiscoveredDevices = 16;
  DiscoveredDevice discovered[kMaxDiscoveredDevices] = {};
  size_t out_count = 0;
  int rc = pci_probe_enumerate_from_sim_hardware(
      discovered, kMaxDiscoveredDevices, &out_count,
      "sim_hardware/topology/default_topology.json");
  if (rc != 0) {
    std::cerr << "[GpuPlugin] pci_probe_enumerate_from_sim_hardware failed: "
              << rc << "\n";
    return rc;
  }

  if (out_count > 1) {
    std::cerr << "[GpuPlugin] sim_hardware topology has N=" << out_count
              << " devices; multi-device shared singletons not yet supported. "
              << "Bridging device[0] only.\n";
  }

  size_t devices_to_bridge = (out_count > 1) ? 1 : out_count;
  for (size_t i = 0; i < devices_to_bridge; ++i) {
    auto& h = hal_holders.emplace_back(std::make_unique<HalHolder>());

    hal_user_init(&h->hal, &h->ctx);

    // Create the puller through the HAL opaque-handle API.
    // Stage 4.7.3: drv/ must not hold std::shared_ptr<HardwarePullerEmu>.
    int puller_ret = hal_puller_create(&h->hal,
                                       static_cast<void*>(&h->doorbell),
                                       static_cast<void*>(&h->scheduler),
                                       &h->puller_handle);
    if (puller_ret != 0) {
      std::cerr << "[GpuPlugin] Failed to create puller for device " << i
                << ": " << puller_ret << "\n";
      return puller_ret;
    }

    h->scheduler.registerKernel(0, "simple_kernel");
    h->scheduler.registerKernel(1, "matmul_kernel");
    h->scheduler.setLaunchCallback(
        [](const char* kernel_name, uint32_t grid_x, uint32_t grid_y, uint32_t grid_z,
           uint32_t block_x, uint32_t block_y, uint32_t block_z, uint32_t shared_mem) {
          std::cout << "[GpuPlugin] LaunchCallback: kernel=" << kernel_name
                    << " grid=(" << grid_x << "," << grid_y << "," << grid_z << ")"
                    << " block=(" << block_x << "," << block_y << "," << block_z << ")"
                    << std::endl;
          (void)shared_mem;
        });

    int db_ret = hal_user_set_doorbell_cb(&h->ctx,
        [](void* cb_ctx, uint32_t queue_id) {
          static_cast<HalHolder*>(cb_ctx)->doorbell.write(queue_id);
        },
        h.get());
    if (db_ret != 0) {
      std::cerr << "[GpuPlugin] Failed to set doorbell callback for device "
                << i << ": " << db_ret << "\n";
      return db_ret;
    }

    auto dev = std::make_shared<GpgpuDevice>(&h->hal);
    dev->setPuller(h->puller_handle);
    dev->set_mm_shim(&g_plugin_mm_shim);
    dev->setHalContext(&h->ctx);

    std::string dev_name = "gpgpu" + std::to_string(i);
    auto vfs_dev = std::make_shared<Device>(dev_name, 0, dev, nullptr);
    if (int reg_rc = VFS::instance().register_device(vfs_dev); reg_rc != 0) {
      std::cerr << "[GpuPlugin] register_device(" << dev_name << ") failed: "
                << reg_rc << "\n";
      return reg_rc;
    }
  }

  std::cout << "[GpuPlugin] Registered " << devices_to_bridge
            << " device(s) (topology out_count=" << out_count << ")\n";
  return 0;
}

static void plugin_fini_internal() {
  std::cout << "[GpuPlugin] Shutting down...\n";
  // VFS unregister first (vtable still valid before HAL destroy).
  for (size_t i = 0; i < hal_holders.size(); ++i) {
    std::string dev_name = "gpgpu" + std::to_string(i);
    VFS::instance().unregister_device(dev_name);
  }
  // C-12 B.1.1: KFD subsystem exit (must precede HAL destroy)
  kfd_module_exit();
  for (auto& h : hal_holders) {
    if (h->puller_handle != 0) {
      hal_puller_destroy(&h->hal, h->puller_handle);
      h->puller_handle = 0;
    }
    hal_user_destroy(&h->ctx);
  }
  hal_holders.clear();
}

module mod = {
    .name = "gpu_driver",
    .load_priority = 50,
    .depends = nullptr,
    .init = plugin_init_internal,
    .exit = plugin_fini_internal,
};

}  // extern "C"