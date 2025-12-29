#pragma once

#include "kernel/device/gpgpu_device.h"
#include "../simulator/gpu/basic_gpu_simulator.h"

class GpuDriver : public GpuDevice, public PciDevice {
public:
    GpuDriver();
    ~GpuDriver();

    long ioctl(int fd, unsigned long request, void* argp) override;

    int allocate_memory(size_t size, GpuMemoryHandle* out) ;
    int free_memory(GpuMemoryHandle handle) ;

    void submit_kernel(const GpuKerneltask& kernel) override;
    void submit_task(const GpuDmaTask& task) override;

private:
    void fill_info(struct GpuDeviceInfo* info) override;
    void wait_for_tasks() override;

    std::unique_ptr<BuddyAllocator> get_allocator(AddressSpaceType type);

    std::unique_ptr<BuddyAllocator> fb_public_pool_;
    std::unique_ptr<BuddyAllocator> system_uncached_pool_;
    std::unique_ptr<BuddyAllocator> system_cached_pool_;

    std::unique_ptr<GpuSimulator> gpu_sim_;
    std::unique_ptr<RingBuffer> command_queue_;

    uint64_t gpu_phys_base_ = 0;
    size_t gpu_phys_size_ = 0;

    uint64_t ring_buffer_phys_base_ = 0;
    size_t ring_buffer_size_ = 0x100000; // 1MB
};
