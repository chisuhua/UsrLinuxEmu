#include <iostream>
#include "sample_gpu.h"
#include "kernel/device/device.h"
#include "gpu/gpu_driver.h"
#include "kernel/module.h"
#include "kernel/vfs.h"  // 添加VFS包含
#include <memory>

// 定义初始化和退出函数
int gpu_module_init() {
    // 注册多个 GPU 设备
    auto dev0 = std::make_shared<Device>("gpgpu0", 12347,
                                        std::make_shared<SampleGpuDriver>(), nullptr);
    auto dev1 = std::make_shared<Device>("gpgpu1", 12348,
                                         std::make_shared<SampleGpuDriver>(), nullptr);

    VFS::instance().register_device(dev0);
    VFS::instance().register_device(dev1);

    std::cout << "[SampleGpu] Module initialized." << std::endl;
    return 0;
}

void gpu_module_exit() {
    std::cout << "[SampleGpu] Module exited." << std::endl;
}

extern "C" {
    // 声明模块结构体
    module mod;
}

// 初始化模块结构体
__attribute__((constructor))
void init_module() {
    mod.name = "gpu";
    mod.depends = nullptr;
    mod.init = gpu_module_init;
    mod.exit = gpu_module_exit;
    mod.loaded = false;
}