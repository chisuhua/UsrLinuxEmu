#pragma once

#include "../kernel/device/gpgpu_device.h"
#include "../kernel/device/buddy_allocator.h"

class SampleGpuDriver : public GpuDevice {
public:
    SampleGpuDriver();
    ~SampleGpuDriver();

    long ioctl(int fd, unsigned long request, void* argp) override;

    void submit_task(const GpuTask& task) override;
    int allocate_memory(size_t size, uint64_t* addr_out) override;
    void free_memory(uint64_t addr) override;

    void submit_kernel(const GpuKernel& kernel);

private:
    void fill_info(struct GpuDeviceInfo* info);
    void wait_for_tasks();

    BuddyAllocator memory_pool_{512 * 1024 * 1024}; // 512 MB 显存池
};

