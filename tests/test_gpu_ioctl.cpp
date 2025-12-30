#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
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

    // 提交任务 - 使用正确的命令包结构
    // 为了解决union成员的析构问题，我们创建一个简单的结构体来模拟命令
    struct SimpleKernelCommand {
        uint64_t kernel_addr;
        uint64_t args_addr;
        size_t shared_mem;
        unsigned int grid[3];
        unsigned int block[3];
    };

    SimpleKernelCommand simple_cmd{};
    simple_cmd.kernel_addr = 0xdeadbeef;
    simple_cmd.args_addr = 0xcafebabe;
    simple_cmd.shared_mem = 1024;
    simple_cmd.grid[0] = 1;
    simple_cmd.grid[1] = 1;
    simple_cmd.grid[2] = 1;
    simple_cmd.block[0] = 128;
    simple_cmd.block[1] = 1;
    simple_cmd.block[2] = 1;

    // 创建一个GpuCommandRequest结构体
    GpuCommandRequest cmd_request{};
    cmd_request.packet_ptr = &simple_cmd;
    cmd_request.packet_size = sizeof(simple_cmd);

    dev->fops->ioctl(fd, GPGPU_SUBMIT_PACKET, &cmd_request);

    // 释放显存
    dev->fops->ioctl(fd, GPGPU_FREE_MEM, &gpu_addr);

    ModuleLoader::unload_plugins();
    return 0;
}