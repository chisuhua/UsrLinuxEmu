#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <cstring>

#include "kernel/vfs.h"
#include "kernel/module_loader.h"
#include "gpu/ioctl_gpgpu.h"
#include "kernel/device/gpgpu_device.h"
#include "gpu/gpu_command_packet.h"

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
    size_t alloc_size = 1024 * 1024; // 1MB，移除const
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

    // 写入一些数据到GPU内存
    char* ptr = static_cast<char*>(user_ptr);
    strcpy(ptr, "Hello from user space!");
    std::cout << "[TestGPU] Wrote data to mapped memory." << std::endl;

    // 提交内核 - 使用正确的结构体
    KernelCommand kernel_cmd{};
    kernel_cmd.kernel_addr = reinterpret_cast<uint64_t>(user_ptr);
    kernel_cmd.args_addr = reinterpret_cast<uint64_t>(ptr + 0x100);
    kernel_cmd.shared_mem = 1024;
    kernel_cmd.grid[0] = 1;
    kernel_cmd.grid[1] = 1;
    kernel_cmd.grid[2] = 1;
    kernel_cmd.block[0] = 128;
    kernel_cmd.block[1] = 1;
    kernel_cmd.block[2] = 1;

    GpuCommandRequest cmd_request{};
    cmd_request.packet_ptr = &kernel_cmd;
    cmd_request.packet_size = sizeof(kernel_cmd);

    dev->fops->ioctl(fd, GPGPU_SUBMIT_PACKET, &cmd_request);

    // 释放资源
    munmap(user_ptr, alloc_size);
    dev->fops->ioctl(fd, GPGPU_FREE_MEM, &gpu_addr);

    ModuleLoader::unload_plugins();
    return 0;
}