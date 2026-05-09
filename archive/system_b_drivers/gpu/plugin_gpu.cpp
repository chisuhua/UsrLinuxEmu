#include <iostream>
#include <memory>
#include "gpu_driver.h"
#include "kernel/device/device.h"
#include "kernel/module.h"
#include "kernel/vfs.h"

using namespace usr_linux_emu;

extern "C" {
module mod;
}

__attribute__((constructor)) void init_module() {
  mod.name = "gpu";
  mod.depends = nullptr;
  mod.init = []() -> int {
    auto dev0 = std::make_shared<Device>("gpgpu0", 12347, std::make_shared<GpuDriver>(), nullptr);
    VFS::instance().register_device(dev0);

    auto dev1 = std::make_shared<Device>("gpgpu1", 12348, std::make_shared<GpuDriver>(), nullptr);
    VFS::instance().register_device(dev1);

    std::cout << "[Gpu] Module initialized." << std::endl;
    return 0;
  };
  mod.exit = []() {
    std::cout << "[Gpu] Module exited." << std::endl;
  };
  mod.loaded = false;
}