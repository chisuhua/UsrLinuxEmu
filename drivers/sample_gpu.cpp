#include "sample_gpu.h"
#include "kernel/ioctl_gpgpu.h"


#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/mman.h>
#include "kernel/vfs.h"
#include "kernel/device/buddy_allocator.h"

SampleGpuDriver::SampleGpuDriver() {
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
            size_t size = *static_cast<size_t*>(argp);
            uint64_t addr = 0;
            if (allocate_memory(size, &addr) == 0) {
                memcpy(argp, &addr, sizeof(addr));
            }
            break;
        }
        case GPGPU_FREE_MEM: {
            uint64_t addr = *static_cast<uint64_t*>(argp);
            free_memory(addr);
            break;
        }
        case GPGPU_SUBMIT_TASK: {
            auto task = static_cast<struct GpuTask*>(argp);
            submit_task(*task);
            break;
        }
        case GPGPU_WAIT_TASK: {
            wait_for_tasks();
            break;
        }
        case GPGPU_SUBMIT_KERNEL: {
            auto kernel = static_cast<struct GpuKernel*>(argp);
            submit_kernel(*kernel);
            break;
        }
        default:
            std::cerr << "[SampleGpu] Unknown ioctl command" << std::endl;
            return -1;
    }
    return 0;
}

int SampleGpuDriver::allocate_memory(size_t size, uint64_t* addr_out) {
    int ret = memory_pool_.allocate(size, addr_out);
    return ret;
}

void SampleGpuDriver::free_memory(uint64_t addr) {
    memory_pool_.free(addr);
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
    info->memory_size = 1ULL * 1024 * 1024 * 1024; // 1GB
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
