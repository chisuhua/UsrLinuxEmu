#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <cstring>

#include "kernel/vfs.h"
#include "kernel/module_loader.h"
#include "gpu/ioctl_gpgpu.h"
#include "kernel/device/gpgpu_device.h"

int main() {
    ModuleLoader::load_plugins("plugins");

    auto dev = VFS::instance().open("/dev/gpgpu0", 0);
    if (!dev) {
        std::cerr << "[TestGPU] Failed to open GPU device." << std::endl;
        return -1;
    }

    int fd = 0;

    size_t alloc_size = 128 * 1024;  // 移除const，因为ioctl需要修改它
    uint64_t gpu_addr = 0;

    // 分配显存
    dev->fops->ioctl(fd, GPGPU_ALLOC_MEM, &alloc_size);
    memcpy(&gpu_addr, &alloc_size, sizeof(gpu_addr));
    std::cout << "[TestGPU] GPU Memory allocated at: 0x" << std::hex << gpu_addr << std::dec << std::endl;

    // mmap 显存
    void* user_ptr = dev->fops->mmap(nullptr, alloc_size, PROT_READ | PROT_WRITE, MAP_SHARED, 0, 0);
    if (user_ptr == MAP_FAILED) {
        std::cerr << "[TestGPU] mmap failed!" << std::endl;
        return -1;
    }

    std::cout << "[TestGPU] Mapped to user space at: " << user_ptr << std::endl;

    // 写入数据
    char* ptr = static_cast<char*>(user_ptr);
    strcpy(ptr, "Hello from CPU to GPU!");

    // 打印写入内容
    std::cout << "[TestGPU] Data written: " << ptr << std::endl;

    // 释放资源
    munmap(user_ptr, alloc_size);
    dev->fops->ioctl(fd, GPGPU_FREE_MEM, &gpu_addr);

    ModuleLoader::unload_plugins();
    return 0;
}