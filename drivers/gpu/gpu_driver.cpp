#include "gpu_driver.h"
#include "gpu/gpu_command_packet.h"
#include "gpu/ioctl_gpgpu.h"
#include <iostream>
#include <cstring>
#include <unistd.h>

GpuDriver::GpuDriver() {
    // 从 GPU 模拟器获取显存信息
    gpu_phys_base_ = 0x10000000ULL;  // 示例基地址
    gpu_phys_size_ = 0x10000000ULL;  // 256MB 显存

    // RingBuffer 分配在显存末尾
    ring_buffer_phys_base_ = gpu_phys_base_ + gpu_phys_size_ - 0x100000; // 1MB ringbuffer
    ring_buffer_size_ = 0x100000;

    // 初始化内存池
    fb_public_pool_.reset(new BuddyAllocator(gpu_phys_base_, gpu_phys_size_/2));
    system_uncached_pool_.reset(new BuddyAllocator(gpu_phys_base_ + gpu_phys_size_/2, gpu_phys_size_/4));
    system_cached_pool_.reset(new BuddyAllocator(gpu_phys_base_ + (gpu_phys_size_*3)/4, gpu_phys_size_/4));

    // 初始化 GPU 模拟器
    gpu_sim_.reset(new BasicGpuSimulator());
}

GpuDriver::~GpuDriver() {
}

std::unique_ptr<BuddyAllocator> GpuDriver::get_allocator(AddressSpaceType type) {
    switch (type) {
        case AddressSpaceType::FB_PUBLIC:
            return std::move(fb_public_pool_);
        case AddressSpaceType::SYSTEM_UNCACHED:
            return std::move(system_uncached_pool_);
        case AddressSpaceType::SYSTEM_CACHED:
            return std::move(system_cached_pool_);
        default:
            std::cerr << "[GpuDriver] Unsupported memory space: " << static_cast<int>(type) << std::endl;
            return nullptr;
    }
}

long GpuDriver::ioctl(int fd, unsigned long request, void* argp) {
    switch (request) {
        case GPGPU_GET_DEVICE_INFO: {
            auto info = static_cast<struct GpuDeviceInfo*>(argp);
            fill_info(info);
            break;
        }
        case GPGPU_ALLOC_MEM: {
            auto req = static_cast<GpuMemoryRequest*>(argp);
            GpuMemoryHandle handle{};
            if(allocate_memory(req->size, &handle) == 0) {
                // 实际应用中需要将handle信息复制到argp指向的位置
                // 简化处理
            }
            break;
        }
        case GPGPU_FREE_MEM: {
            uint64_t addr = *static_cast<uint64_t*>(argp);
            // 创建一个临时handle来释放内存
            GpuMemoryHandle handle;
            handle.phys_addr = addr;
            handle.size = 0; // 不知道大小，简化处理
            free_memory(handle);
            break;
        }
        case GPGPU_SUBMIT_PACKET: {
            auto req = static_cast<const GpuCommandRequest*>(argp);
            // 暂时忽略ring buffer相关的提交
            break;
        }
        default:
            std::cerr << "[GpuDriver] Unknown ioctl command: 0x" << std::hex << request << std::dec << std::endl;
            return -1;
    }

    return 0;
}

int GpuDriver::allocate_memory(size_t size, GpuMemoryHandle* out) {
    // 简单实现：分配固定地址范围内的内存
    static uint64_t current_addr = gpu_phys_base_;
    
    out->phys_addr = current_addr;
    out->user_ptr = reinterpret_cast<void*>(out->phys_addr);
    out->size = size;
    
    current_addr += size;
    if(current_addr > gpu_phys_base_ + gpu_phys_size_) {
        return -1; // 内存不足
    }
    
    return 0;
}

int GpuDriver::free_memory(GpuMemoryHandle handle) {
    // 简单实现：暂不释放内存
    return 0;
}

void GpuDriver::submit_task(const GpuTask& task) {
    std::cout << "[GpuDriver] Submitting task with ID: " << task.task_id << std::endl;
    // 这里可以添加任务处理逻辑
}

void GpuDriver::wait_for_tasks() {
    // 等待任务完成
}

void GpuDriver::submit_kernel(const GpuKernel& kernel) {
    std::cout << "[GpuDriver] Submitting kernel" << std::endl;
    // 这里可以添加内核处理逻辑
}

void GpuDriver::fill_info(struct GpuDeviceInfo* info) {
    info->name = "Sample GPU";
    info->memory_size = gpu_phys_size_;
    info->max_queues = 4;
    info->compute_units = 8;
}

// 实现PciDevice接口的方法
uint32_t GpuDriver::read_config_dword(uint8_t offset) {
    // 模拟PCI配置空间读取
    return 0;
}

void GpuDriver::write_config_dword(uint8_t offset, uint32_t value) {
    // 模拟PCI配置空间写入
}

void GpuDriver::enable_bus_master() {
    // 启用总线主控
}

void GpuDriver::disable_bus_master() {
    // 禁用总线主控
}

uint32_t GpuDriver::get_vendor_id() const {
    return 0x10DE; // NVIDIA vendor ID
}

uint32_t GpuDriver::get_device_id() const {
    return 0xFFFF; // 示例设备ID
}

ssize_t GpuDriver::read(int fd, void* buf, size_t count) {
    // 实现读取功能
    return 0;
}
