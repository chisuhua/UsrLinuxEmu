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
#include "sim/hardware/doorbell_emu.h"
#include "sim/hardware/hardware_puller_emu.h"
#include "sim/scheduler/global_scheduler.h"

namespace {
struct HalHolder {
  struct gpu_hal_ops hal;
  struct hal_user_context ctx;
  DoorbellEmu doorbell;
  GlobalScheduler scheduler;
  std::shared_ptr<HardwarePullerEmu> puller;
};
static HalHolder* g_hal = nullptr;
}

using namespace usr_linux_emu;

extern "C" {

static int plugin_init_internal() {
  std::cout << "[GpuPlugin] Initializing...\n";

  static HalHolder hal_holder;
  hal_user_init(&hal_holder.hal, &hal_holder.ctx);
  g_hal = &hal_holder;

  hal_holder.puller = std::make_shared<HardwarePullerEmu>(&hal_holder.hal,
                                                          &hal_holder.doorbell,
                                                          &hal_holder.scheduler);

  // 注册内核到 GlobalScheduler (与 GpgpuDevice 保持一致)
  hal_holder.scheduler.registerKernel(0, "simple_kernel");
  hal_holder.scheduler.registerKernel(1, "matmul_kernel");

  // 设置 launch callback - 当 GPFIFO translator 解析出 kernel 启动参数时调用
  hal_holder.scheduler.setLaunchCallback(
      [](const char* kernel_name, uint32_t grid_x, uint32_t grid_y, uint32_t grid_z,
         uint32_t block_x, uint32_t block_y, uint32_t block_z, uint32_t shared_mem) {
        std::cout << "[GpuPlugin] LaunchCallback: kernel=" << kernel_name
                  << " grid=(" << grid_x << "," << grid_y << "," << grid_z << ")"
                  << " block=(" << block_x << "," << block_y << "," << block_z << ")"
                  << std::endl;
        (void)shared_mem;
      });

  int ret = hal_user_set_doorbell_cb(&hal_holder.ctx,
      [](void* cb_ctx, uint32_t queue_id) {
        auto* dh = static_cast<HalHolder*>(cb_ctx);
        dh->doorbell.write(queue_id);
      },
      &hal_holder);
  if (ret != 0) {
    std::cerr << "[GpuPlugin] Failed to set doorbell callback: " << ret << "\n";
    return ret;
  }

  hal_holder.puller->start();

  auto device = std::make_shared<GpgpuDevice>(&hal_holder.hal);
  device->setPuller(hal_holder.puller);

  VFS& vfs = VFS::instance();
  auto dev = std::make_shared<Device>(device->name, 0, device, nullptr);
  vfs.register_device(dev);

  std::cout << "[GpuPlugin] Registered /dev/gpgpu0\n";
  return 0;
}

static void plugin_fini_internal() {
  std::cout << "[GpuPlugin] Shutting down...\n";
  if (g_hal) {
    if (g_hal->puller) {
      g_hal->puller->stop();
    }
    g_hal->puller.reset();
    hal_user_destroy(&g_hal->ctx);
    g_hal = nullptr;
  }
  VFS::instance().unregister_device("gpgpu0");
}

module mod = {
    .name = "gpu_driver",
    .depends = nullptr,
    .init = plugin_init_internal,
    .exit = plugin_fini_internal,
};

}  // extern "C"