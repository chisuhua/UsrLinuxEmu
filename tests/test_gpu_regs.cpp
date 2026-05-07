#include <iostream>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <cstring>

#include "kernel/vfs.h"
#include "kernel/module_loader.h"
#include "kernel/file_ops.h"
#include "gpu_driver/shared/gpu_ioctl.h"
#include "gpu_driver/shared/gpu_types.h"

int main() {
    ModuleLoader::load_plugins("plugins");

    auto dev = VFS::instance().open("/dev/gpgpu0", 0);
    if (!dev || !dev->fops) {
        std::cerr << "[TestGPU] Failed to open GPU device." << std::endl;
        return -1;
    }

    int fd = 0;

    struct gpu_device_info info{};
    long ret = dev->fops->ioctl(fd, GPU_IOCTL_GET_DEVICE_INFO, &info);
    if (ret == 0) {
        std::cout << "[TestGPU] Device vendor=0x" << std::hex << info.vendor_id
                  << " device=0x" << info.device_id << std::dec << std::endl;
        std::cout << "[TestGPU] VRAM: " << (info.vram_size / (1024 * 1024)) << "MB" << std::endl;
    }

    struct gpu_alloc_bo_args alloc_args = {
        .size = 0x1000,
        .domain = GPU_MEM_DOMAIN_VRAM,
        .flags = GPU_BO_DEVICE_LOCAL,
        .handle = 0,
        .gpu_va = 0
    };

    ret = dev->fops->ioctl(fd, GPU_IOCTL_ALLOC_BO, &alloc_args);
    if (ret == 0) {
        std::cout << "[TestGPU] Allocated BO: handle=" << alloc_args.handle << std::endl;
        u32 handle = alloc_args.handle;
        dev->fops->ioctl(fd, GPU_IOCTL_FREE_BO, &handle);
    }

    dev.reset();
    ModuleLoader::unload_plugins();
    return 0;
}