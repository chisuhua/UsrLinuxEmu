#pragma once

#include "kernel/device/gpgpu_device.h"
#include "gpu/buddy_allocator.h"

class SampleGpuDriver : public GpuDevice {
public:
    SampleGpuDriver();
    ~SampleGpuDriver();

    long ioctl(int fd, unsigned long request, void* argp) override;
    int allocate_memory(size_t size, GpuMemoryHandle* addr_out) override;
    int free_memory(GpuMemoryHandle addr) override;
    void submit_task(const GpuTask& task) override;
    
    // 添加缺失的纯虚函数实现
    ssize_t read(int fd, void* buf, size_t count) override;

    void submit_kernel(const GpuKernel& kernel);
    void fill_info(struct GpuDeviceInfo* info);
    void wait_for_tasks();

    // 从FileOperations继承的mmap方法
    void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset) override;

private:
    BuddyAllocator memory_pool_; // 512 MB 显存池
    WaitQueue wait_queue_;
};