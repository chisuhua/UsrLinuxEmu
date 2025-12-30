#include <iostream>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <cstring>

#include "kernel/vfs.h"
#include "kernel/module_loader.h"
#include "kernel/device/gpgpu_device.h"
#include "gpu/ioctl_gpgpu.h"
#include "gpu/gpu_command_packet.h"

int main() {
    ModuleLoader::load_plugins("plugins");

    auto dev = VFS::instance().open("/dev/gpgpu0", O_RDWR);
    if (!dev || !dev->fops) return -1;

    // 构造一个简单的命令
    KernelCommand kernel_cmd{};
    kernel_cmd.kernel_addr = 0xdeadbeef;
    kernel_cmd.args_addr = 0xcafebabe;
    kernel_cmd.shared_mem = 1024;
    kernel_cmd.grid[0] = 1;
    kernel_cmd.grid[1] = 1;
    kernel_cmd.grid[2] = 1;
    kernel_cmd.block[0] = 128;
    kernel_cmd.block[1] = 1;
    kernel_cmd.block[2] = 1;

    GpuCommandRequest req{};
    req.packet_ptr = &kernel_cmd;
    req.packet_size = sizeof(kernel_cmd);

    dev->fops->ioctl(0, GPGPU_SUBMIT_PACKET, &req);

    ModuleLoader::unload_plugins();
    return 0;
}
