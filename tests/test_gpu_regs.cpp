#include <iostream>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "kernel/vfs.h"
#include "kernel/module_loader.h"
#include "kernel/device/gpgpu_device.h"
#include "kernel/pcie/gpu_register.h"

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

    // 写入寄存器
    GpuRegisterWrite reg_write{};
    reg_write.offset = GpuRegisterOffsets::NV_GPU_COMMAND_QUEUE;
    reg_write.value = 0x1; // 触发任务提交
    dev->fops->ioctl(fd, GPGPU_WRITE_REG, &reg_write);

    // 申请系统内存并注册给 GPU
    size_t sys_mem_size = 0x1000;
    void* sys_mem = mmap(nullptr, sys_mem_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    strcpy((char*)sys_mem, "Ring buffer data");

    SystemMemoryRegion region{};
    region.cpu_ptr = sys_mem;
    region.size = sys_mem_size;
    region.type = AddressSpaceType::SYSTEM_UNCACHED;

    dev->fops->ioctl(fd, GPGPU_REGISTER_SYS_MEM, &region);

    // GPU DMA 读取 ring buffer
    char buffer[256] = {0};
    gpu_sim_->copy_from_device(buffer, reinterpret_cast<uint64_t>(sys_mem), sizeof(buffer));
    std::cout << "[TestGPU] GPU read from system memory: " << buffer << std::endl;

    // 清理资源
    munmap(sys_mem, sys_mem_size);
    ModuleLoader::unload_plugins();
    return 0;
}
