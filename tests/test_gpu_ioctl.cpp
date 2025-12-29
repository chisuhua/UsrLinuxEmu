#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <cstring>

#include "kernel/vfs.h"
#include "kernel/module_loader.h"
#include "kernel/ioctl_gpgpu.h"
#include "kernel/device/gpgpu_device.h"

int main() {
    ModuleLoader::load_plugins("plugins");

    auto dev = VFS::instance().open("/dev/gpgpu0", 0);
    if (!dev) {
        std::cerr << "[TestGPU] Failed to open GPGPU device." << std::endl;
        return -1;
    }

    int fd = 0;

    // 获取设备信息
    GpuDeviceInfo info{};
    dev->fops->ioctl(fd, GPGPU_GET_DEVICE_INFO, &info);
    std::cout << "[TestGPU] Device Name: " << info.name << std::endl;
    std::cout << "[TestGPU] Memory Size: " << info.memory_size / (1024 * 1024) << "MB" << std::endl;

    // 分配显存
    size_t alloc_size = 128 * 1024; // 128KB
    uint64_t gpu_addr = 0;
    dev->fops->ioctl(fd, GPGPU_ALLOC_MEM, &alloc_size);
    memcpy(&gpu_addr, &alloc_size, sizeof(gpu_addr));
    std::cout << "[TestGPU] Allocated at: 0x" << std::hex << gpu_addr << std::dec << std::endl;

    // 提交任务
    GpuTask task{};
    task.kernel_addr = 0xdeadbeef;
    task.args_addr = 0xcafebabe;
    task.shared_mem = 1024;
    task.grid[0] = 1;
    task.block[0] = 128;

    dev->fops->ioctl(fd, GPGPU_SUBMIT_TASK, &task);

    // 等待任务完成
    dev->fops->ioctl(fd, GPGPU_WAIT_TASK, nullptr);

    // 释放显存
    dev->fops->ioctl(fd, GPGPU_FREE_MEM, &gpu_addr);

    ModuleLoader::unload_plugins();
    return 0;
}

