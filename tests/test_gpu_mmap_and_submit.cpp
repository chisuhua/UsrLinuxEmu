#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "kernel/vfs.h"
#include "kernel/module_loader.h"
#include "kernel/ioctl_gpgpu.h"

int main() {
    ModuleLoader::load_plugins("plugins");

    auto dev = VFS::instance().open("/dev/gpgpu0", 0);
    if (!dev) {
        std::cerr << "[TestGPU] Failed to open GPU device." << std::endl;
        return -1;
    }

    int fd = 0;

    // 获取设备信息
    GpuDeviceInfo info{};
    dev->fops->ioctl(fd, GPGPU_GET_DEVICE_INFO, &info);
    std::cout << "[TestGPU] Device Name: " << info.name << std::endl;
    std::cout << "[TestGPU] Memory Size: " << (info.memory_size / (1024 * 1024)) << "MB" << std::endl;

    // 分配显存
    const size_t alloc_size = 1024 * 1024; // 1MB
    uint64_t gpu_addr = 0;
    dev->fops->ioctl(fd, GPGPU_ALLOC_MEM, &alloc_size);
    memcpy(&gpu_addr, &alloc_size, sizeof(gpu_addr));
    std::cout << "[TestGPU] Allocated GPU memory at: 0x" << std::hex << gpu_addr << std::dec << std::endl;

    // mmap 到用户空间
    void* user_ptr = dev->fops->mmap(nullptr, alloc_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (user_ptr == MAP_FAILED) {
        std::cerr << "[TestGPU] mmap failed!" << std::endl;
        return -1;
    }
    std::cout << "[TestGPU] Mapped to user space at: " << user_ptr << std::endl;

    // 写入 GPU 内核代码（模拟）
    char* ptr = static_cast<char*>(user_ptr);
    strcpy(ptr, "__device__ void my_kernel(...) { ... }");
    std::cout << "[TestGPU] Wrote kernel code to mapped memory." << std::endl;

    // 提交内核
    GpuKernel kernel{};
    kernel.kernel_addr = reinterpret_cast<uint64_t>(user_ptr);
    kernel.args_addr = reinterpret_cast<uint64_t>(ptr + 0x100);
    kernel.code_size = strlen(ptr) + 1;

    dev->fops->ioctl(fd, GPGPU_SUBMIT_KERNEL, &kernel);

    // 等待执行完成
    dev->fops->ioctl(fd, GPGPU_WAIT_TASK, nullptr);

    // 释放资源
    munmap(user_ptr, alloc_size);
    dev->fops->ioctl(fd, GPGPU_FREE_MEM, &gpu_addr);

    ModuleLoader::unload_plugins();
    return 0;
}
