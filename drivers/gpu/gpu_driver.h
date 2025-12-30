#pragma once

#include "kernel/device/gpgpu_device.h"
#include "gpu/buddy_allocator.h"
#include "gpu/basic_gpu_simulator.h"  // 修改为正确的相对路径
#include "kernel/pcie_device.h"  // 包含正确的PciDevice定义

class GpuDriver : public GpuDevice, public PciDevice {  // 修复继承问题
public:
    GpuDriver();
    ~GpuDriver();

    long ioctl(int fd, unsigned long request, void* argp) override;

    int allocate_memory(size_t size, GpuMemoryHandle* out) override;  // 添加override
    int free_memory(GpuMemoryHandle handle) override;  // 添加override

    void submit_kernel(const GpuKernel& kernel);  // 移除override，因为基类中没有此函数
    void submit_task(const GpuTask& task) override;  // 修复类型名

    // PciDevice 接口实现
    uint32_t read_config_dword(uint8_t offset) override;
    void write_config_dword(uint8_t offset, uint32_t value) override;
    void enable_bus_master() override;
    void disable_bus_master() override;
    uint32_t get_vendor_id() const override;
    uint32_t get_device_id() const override;

    // 从GpuDevice继承的纯虚函数实现
    ssize_t read(int fd, void* buf, size_t count) override;

private:
    void fill_info(struct GpuDeviceInfo* info);  // 移除override，因为基类中是纯虚函数
    void wait_for_tasks();  // 移除override，因为基类中是纯虚函数

    std::unique_ptr<BuddyAllocator> get_allocator(AddressSpaceType type);

    std::unique_ptr<BuddyAllocator> fb_public_pool_;
    std::unique_ptr<BuddyAllocator> system_uncached_pool_;
    std::unique_ptr<BuddyAllocator> system_cached_pool_;

    std::unique_ptr<BasicGpuSimulator> gpu_sim_;  // 修复类型名
    // 移除RingBuffer，因为可能未定义
    // std::unique_ptr<RingBuffer> command_queue_;

    uint64_t gpu_phys_base_ = 0;
    size_t gpu_phys_size_ = 0;

    uint64_t ring_buffer_phys_base_ = 0;
    size_t ring_buffer_size_ = 0x100000; // 1MB
};