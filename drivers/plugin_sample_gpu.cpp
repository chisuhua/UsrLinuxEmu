#include <iostream>
#include "sample_gpu.h"
#include "kernel/device.h"
#include "kernel/vfs.h"
#include "kernel/module_loader.h"

module mod = {
    .name = "gpu",
    .depends = nullptr,
    .init = []() -> int {
        // 注册多个 GPU 设备
        auto dev0 = std::make_shared<Device>("gpgpu0", 12347,
                                            std::make_shared<SampleGpuDriver>(), nullptr);
        auto dev1 = std::make_shared<Device>("gpgpu1", 12348,
                                             std::make_shared<SampleGpuDriver>(), nullptr);

        VFS::instance().register_device(dev0);
        VFS::instance().register_device(dev1);

        std::cout << "[SampleGpu] Module initialized." << std::endl;
        return 0;
    },
    .exit = []() {
        std::cout << "[SampleGpu] Module exited." << std::endl;
    }
};
