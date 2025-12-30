#include <iostream>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <cstring>

#include "kernel/vfs.h"
#include "kernel/module_loader.h"
#include "kernel/device/gpgpu_device.h"
#include "gpu/ioctl_gpgpu.h"
#include "gpu/gpu_command_packet.h"

int cudaMalloc(void** devPtr, size_t size) {
    int fd = 0;
    size_t alloc_size = size;

    auto dev = VFS::instance().open("/dev/gpgpu0", 0);
    if (!dev || !dev->fops) return -1;

    if (dev->fops->ioctl(fd, GPGPU_ALLOC_MEM, &alloc_size) != 0) {
        return -1;
    }

    *devPtr = reinterpret_cast<void*>(alloc_size);  // 这里应该返回设备指针
    return 0;
}

int cudaMemcpy(void* dst, const void* src, size_t size, int direction) {
    (void)direction;
    memcpy(dst, src, size);
    return 0;
}

int cudaFree(void* devPtr) {
    int fd = 0;
    uint64_t addr_to_free = reinterpret_cast<uint64_t>(devPtr);

    auto dev = VFS::instance().open("/dev/gpgpu0", 0);
    return dev->fops->ioctl(fd, GPGPU_FREE_MEM, &addr_to_free);
}

int cudaSubmitKernel(uint64_t kernel_addr, uint64_t args_addr, size_t shared_mem,
                     unsigned int* grid, unsigned int* block) {
    // 使用正确的结构体
    KernelCommand kernel_cmd{};
    kernel_cmd.kernel_addr = kernel_addr;
    kernel_cmd.args_addr = args_addr;
    kernel_cmd.shared_mem = shared_mem;
    if (grid) memcpy(kernel_cmd.grid, grid, sizeof(kernel_cmd.grid));
    if (block) memcpy(kernel_cmd.block, block, sizeof(kernel_cmd.block));

    GpuCommandRequest cmd_request{};
    cmd_request.packet_ptr = &kernel_cmd;
    cmd_request.packet_size = sizeof(kernel_cmd);

    int fd = 0;
    auto dev = VFS::instance().open("/dev/gpgpu0", 0);
    return dev->fops->ioctl(fd, GPGPU_SUBMIT_PACKET, &cmd_request);
}