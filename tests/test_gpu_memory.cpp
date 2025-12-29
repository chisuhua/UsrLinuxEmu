#include <iostream>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "kernel/vfs.h"
#include "kernel/module_loader.h"
#include "kernel/device/gpgpu_device.h"
#include "kernel/ioctl_gpgpu.h"

int cudaMalloc(void** devPtr, size_t size) {
    int fd = 0;
    GpuMemoryHandle handle{};

    auto dev = VFS::instance().open("/dev/gpgpu0", 0);
    if (!dev || !dev->fops) return -1;

    if (dev->fops->ioctl(fd, GPGPU_ALLOC_MEM, &size) != 0) {
        return -1;
    }

    memcpy(&handle, &size, sizeof(handle));
    *devPtr = handle.user_ptr;
    return 0;
}

int cudaMemcpy(void* dst, const void* src, size_t size, int direction) {
    (void)direction;
    memcpy(dst, src, size);
    return 0;
}

int cudaFree(void* devPtr) {
    int fd = 0;
    GpuMemoryHandle handle{};
    handle.user_ptr = devPtr;
    handle.size = 0x1000;

    auto dev = VFS::instance().open("/dev/gpgpu0", 0);
    return dev->fops->ioctl(fd, GPGPU_FREE_MEM, &handle);
}

int cudaSubmitKernel(uint64_t kernel_addr, uint64_t args_addr, size_t shared_mem,
                     unsigned int* grid, unsigned int* block) {
    GpuTask task{};
    task.kernel_addr = kernel_addr;
    task.args_addr = args_addr;
    task.shared_mem = shared_mem;
    if (grid) memcpy(task.grid, grid, sizeof(task.grid));
    if (block) memcpy(task.block, block, sizeof(task.block));

    int fd = 0;
    auto dev = VFS::instance().open("/dev/gpgpu0", 0);
    return dev->fops->ioctl(fd, GPGPU_SUBMIT_KERNEL, &task);
}
