#include <iostream>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "kernel/vfs.h"
#include "kernel/module_loader.h"
#include "kernel/device/gpgpu_device.h"

void main() {
    ModuleLoader::load_plugins("plugins");

    auto dev = VFS::instance().open("/dev/gpgpu0", O_RDWR);
    if (!dev || !dev->fops) return;

    GpuCommandPacket packet{};
    packet.type = CommandType::KERNEL;
    packet.size = sizeof(KernelCommand);
    packet.kernel.kernel_addr = 0xdeadbeef;
    packet.kernel.args_addr = 0xcafebabe;
    packet.kernel.grid[0] = 1;
    packet.kernel.block[0] = 128;

    // 构造 CommandRequest
    GpuCommandRequest req{};
    req.packet_ptr = &packet;
    req.packet_size = packet.size;

    dev->fops->ioctl(0, GPGPU_SUBMIT_PACKET, &req);

    ModuleLoader::unload_plugins();
}
