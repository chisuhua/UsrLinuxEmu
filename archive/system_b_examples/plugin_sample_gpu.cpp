#include <iostream>
#include <memory>
#include "gpu/gpu_driver.h"
#include "kernel/device/device.h"
#include "kernel/module.h"
#include "kernel/vfs.h"
#include "sample_gpu.h"

using namespace usr_linux_emu;

int gpu_module_init() {
  auto dev0 =
      std::make_shared<Device>("gpgpu0", 12347, std::make_shared<SampleGpuDriver>(), nullptr);
  auto dev1 =
      std::make_shared<Device>("gpgpu1", 12348, std::make_shared<SampleGpuDriver>(), nullptr);

  VFS::instance().register_device(dev0);
  VFS::instance().register_device(dev1);

  std::cout << "[SampleGpu] Module initialized." << std::endl;
  return 0;
}

void gpu_module_exit() {
  std::cout << "[SampleGpu] Module exited." << std::endl;
}

extern "C" {
module mod;
}

__attribute__((constructor)) void init_module() {
  mod.name = "gpu";
  mod.depends = nullptr;
  mod.init = gpu_module_init;
  mod.exit = gpu_module_exit;
  mod.loaded = false;
}