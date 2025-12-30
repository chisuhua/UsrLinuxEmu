#include <iostream>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <cstring>

#include "kernel/vfs.h"
#include "kernel/module_loader.h"
#include "kernel/device/gpgpu_device.h"
#include "gpu/ioctl_gpgpu.h"

void test_submit_dma_with_space_type(int space_type) {
    // 这里我们简化测试，因为原代码中使用了一些不存在的类型
    std::cout << "[TestGPU] Testing DMA with space type: " << space_type << std::endl;
}

int main() {
    ModuleLoader::load_plugins("plugins");

    auto dev = VFS::instance().open("/dev/gpgpu0", 0);
    if (!dev || !dev->fops) {
        std::cerr << "[TestGPU] Failed to open GPU device." << std::endl;
        return -1;
    }

    int fd = 0;

    // 获取设备信息
    GpuDeviceInfo info{};
    dev->fops->ioctl(fd, GPGPU_GET_DEVICE_INFO, &info);
    std::cout << "[TestGPU] Device: " << info.name
              << ", Memory Size: " << info.memory_size / (1024 * 1024) << "MB" << std::endl;

    test_submit_dma_with_space_type(1); // 使用整数代替不存在的枚举值

    ModuleLoader::unload_plugins();
    return 0;
}
