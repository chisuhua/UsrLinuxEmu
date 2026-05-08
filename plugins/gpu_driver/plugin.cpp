/*
 * plugin.cpp - GPU 驱动仿真插件入口
 */
#include <iostream>
#include <memory>
#include "kernel/device/device.h"
#include "kernel/module_loader.h"
#include "kernel/vfs.h"
#include "drv/gpgpu_device.h"
#include "hal/hal_user.h"

namespace {
struct HalHolder {
  struct gpu_hal_ops hal;
  struct hal_user_context ctx;
};
static HalHolder* g_hal = nullptr;
}

extern "C" {

static int plugin_init_internal() {
  std::cout << "[GpuPlugin] Initializing...\n";

  static HalHolder hal_holder;
  hal_user_init(&hal_holder.hal, &hal_holder.ctx);
  g_hal = &hal_holder;

  auto device = std::make_shared<GpgpuDevice>(&hal_holder.hal);

  VFS& vfs = VFS::instance();
  auto dev = std::make_shared<Device>(device->name, 0, device, nullptr);
  vfs.register_device(dev);

  std::cout << "[GpuPlugin] Registered /dev/gpgpu0\n";
  return 0;
}

static void plugin_fini_internal() {
  std::cout << "[GpuPlugin] Shutting down...\n";
  VFS::instance().unregister_device("gpgpu0");
  if (g_hal) {
    hal_user_destroy(&g_hal->ctx);
    g_hal = nullptr;
  }
}

module mod = {
    .name = "gpu_driver",
    .depends = nullptr,
    .init = plugin_init_internal,
    .exit = plugin_fini_internal,
};

}  // extern "C"
