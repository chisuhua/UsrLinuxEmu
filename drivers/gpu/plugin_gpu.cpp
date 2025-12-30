#include "gpu_driver.h"
#include "kernel/device/device.h"
#include "kernel/vfs.h"
#include "kernel/module.h"
#include <iostream>
#include <memory>

extern "C" {
    // 声明模块结构体
    module mod;
}

// 初始化模块结构体
__attribute__((constructor))
void init_module() {
    mod.name = "gpu";
    mod.depends = nullptr;
    mod.init = []() -> int {
        auto dev0 = std::make_shared<Device>("gpgpu0", 12347,
                                            std::make_shared<GpuDriver>(), nullptr);
        VFS::instance().register_device(dev0);

        auto dev1 = std::make_shared<Device>("gpgpu1", 12348,
                                            std::make_shared<GpuDriver>(), nullptr);
        VFS::instance().register_device(dev1);

        std::cout << "[Gpu] Module initialized." << std::endl;
        return 0;
    };
    mod.exit = []() {
        std::cout << "[Gpu] Module exited." << std::endl;
    };
    mod.loaded = false;
}