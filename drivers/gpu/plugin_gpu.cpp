#include "sample_gpu_driver.h"

extern "C" module mod = {
    .name = "gpu",
    .depends = nullptr,
    .init = []() -> int {
        auto dev0 = std::make_shared<Device>("gpgpu0", 12347,
                                            std::make_shared<GpuDriver>(), nullptr);
        VFS::instance().register_device(dev0);

        auto dev1 = std::make_shared<Device>("gpgpu1", 12348,
                                            std::make_shared<GpuDriver>(), nullptr);
        VFS::instance().register_device(dev1);

        std::cout << "[Gpu] Module initialized." << std::endl;
        return 0;
    },
    .exit = []() {
        std::cout << "[Gpu] Module exited." << std::endl;
    }
};
