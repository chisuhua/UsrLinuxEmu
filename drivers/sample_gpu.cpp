#include "sample_gpu.h"
#include "gpu/ioctl_gpgpu.h"


#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/mman.h>
#include "kernel/vfs.h"
#include "gpu/buddy_allocator.h"

SampleGpuDriver::SampleGpuDriver() : memory_pool_(0x100000000UL, 512 * 1024 * 1024) {  // 从0x100000000开始的512MB内存池
    std::cout << "[SampleGpu] GPU device initialized." << std::endl;
}

SampleGpuDriver::~SampleGpuDriver() {
    std::cout << "[SampleGpu] GPU device destroyed." << std::endl;
}

long SampleGpuDriver::ioctl(int fd, unsigned long request, void* argp) {
    switch (request) {
        case GPGPU_GET_DEVICE_INFO: {
            auto info = static_cast<struct GpuDeviceInfo*>(argp);
            fill_info(info);
            break;
        }
        case GPGPU_ALLOC_MEM: {
            auto req = static_cast<struct GpuMemoryRequest*>(argp);
            GpuMemoryHandle handle;
            if (allocate_memory(req->size, &handle) == 0) {
                memcpy(argp, &handle, sizeof(handle));
            }
            break;
        }
        case GPGPU_FREE_MEM: {
            auto handle = *static_cast<GpuMemoryHandle*>(argp);
            free_memory(handle);
            break;
        }
        case GPGPU_SUBMIT_PACKET: {  // 使用正确的ioctl命令
            auto task = static_cast<struct GpuCommandRequest*>(argp);
            // 暂时忽略这个命令，因为可能没有定义GpuCommandRequest
            std::cout << "[SampleGpu] Submit packet command received" << std::endl;
            break;
        }
        default:
            std::cerr << "[SampleGpu] Unknown ioctl command" << std::endl;
            return -1;
    }
    return 0;
}

int SampleGpuDriver::allocate_memory(size_t size, GpuMemoryHandle* addr_out) {
    uint64_t addr = 0;
    int ret = memory_pool_.allocate(size, &addr);
    if (ret == 0) {
        addr_out->phys_addr = addr;
        addr_out->size = size;
        addr_out->user_ptr = nullptr;  // 实际使用时需要mmap映射
    }
    return ret;
}

int SampleGpuDriver::free_memory(GpuMemoryHandle addr) {
    return memory_pool_.free(addr.phys_addr);
}

void SampleGpuDriver::submit_task(const GpuTask& task) {
    std::cout << "[SampleGpu] Submitting task..." << std::endl;
    // 模拟执行
    usleep(500000); // 500ms
    wait_queue_.wake_up();
}

void SampleGpuDriver::submit_kernel(const GpuKernel& kernel) {
    std::cout << "[SampleGpu] Submitting kernel at: 0x" << std::hex << kernel.kernel_addr
              << " with args at: 0x" << kernel.args_addr << std::dec << std::endl;

    // 模拟执行时间
    usleep(800000); // 800ms
    wait_queue_.wake_up();
}

void SampleGpuDriver::fill_info(struct GpuDeviceInfo* info) {
    info->name = "sample_gpu";
    info->memory_size = 512ULL * 1024 * 1024; // 512MB
    info->max_queues = 4;
    info->compute_units = 16;
    std::cout << "[SampleGpu] Device info requested." << std::endl;
}

void SampleGpuDriver::wait_for_tasks() {
    std::cout << "[SampleGpu] Waiting for tasks to complete..." << std::endl;
    wait_queue_.wait();
}

void* SampleGpuDriver::mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset) {
    return FileOperations::mmap(addr, length, prot, flags, fd, offset);
}