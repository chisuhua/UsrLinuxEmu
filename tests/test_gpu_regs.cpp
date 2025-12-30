#include <iostream>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <cstring>

#include "kernel/vfs.h"
#include "kernel/module_loader.h"
#include "kernel/device/gpgpu_device.h"
#include "gpu/ioctl_gpgpu.h"

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

    // 分配显存
    size_t alloc_size = 0x1000;
    dev->fops->ioctl(fd, GPGPU_ALLOC_MEM, &alloc_size);
    uint64_t gpu_addr = 0;
    memcpy(&gpu_addr, &alloc_size, sizeof(gpu_addr));

    // 申请系统内存
    size_t sys_mem_size = 0x1000;
    void* sys_mem = mmap(nullptr, sys_mem_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    strcpy((char*)sys_mem, "Ring buffer data");

    // 清理资源
    munmap(sys_mem, sys_mem_size);
    ModuleLoader::unload_plugins();
    return 0;
}
