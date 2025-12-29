#include <iostream>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "kernel/vfs.h"
#include "kernel/module_loader.h"
#include "kernel/device/gpgpu_device.h"

void test_submit_dma_with_space_type(AddressSpaceType alloc_type) {
    GpuMemoryRequest req{};
    req.size = 0x1000;
    req.space_type = alloc_type;

    GpuMemoryHandle handle{};
    dev->fops->ioctl(fd, GPGPU_ALLOC_MEM, &req);
    memcpy(&handle, &req, sizeof(handle));

    // 构造 DMA 任务
    DmaCommand dma_cmd{};
    dma_cmd.src_phys = handle.phys_addr;
    dma_cmd.dst_phys = handle.phys_addr + 0x80000000ULL;
    dma_cmd.size = 0x1000;
    dma_cmd.direction = DmaDirection::H2D;

    // 构造 packet
    struct {
        CommandType type = CommandType::DMA_COPY;
        uint32_t size = sizeof(DmaCommand) + sizeof(CommandHeader);
        DmaCommand cmd;
    } packet;

    packet.type = CommandType::DMA_COPY;
    packet.size = sizeof(packet);
    packet.cmd = dma_cmd;

    dev->fops->ioctl(fd, GPGPU_SUBMIT_DMA, &packet);

    // 清理资源
    dev->fops->ioctl(fd, GPGPU_FREE_MEM, &src_handle);
    dev->fops->ioctl(fd, GPGPU_FREE_MEM, &dst_handle);
}

int main() {
    ModuleLoader::load_plugins("plugins");

    auto dev = VFS::instance().open("/dev/gpgpu0", 0);
    if (!dev || !dev->fops) {
        std::cerr << "[TestGPU] Failed to open GPU device." << std::endl;
        return -1;
    }

    int fd = 0;

    test_submit_dma_with_space_type(GPGPU_ALLOC_MEM);


    ModuleLoader::unload_plugins();
    return 0;
}
