#include "basic_gpu_simulator.h"
#include <iostream>
#include <sys/mman.h>

BasicGpuSimulator::BasicGpuSimulator()
    : ram_phys_base_(reinterpret_cast<uint64_t>(
          mmap(nullptr, ram_phys_size_, PROT_READ | PROT_WRITE,
               MAP_SHARED | MAP_ANONYMOUS, -1, 0))),
      mmio_phys_base_(0xbeefcafe00000000ULL),
      mmio_phys_size_(0x1000) {
    if (ram_phys_base_ == reinterpret_cast<uint64_t>(MAP_FAILED)) {
        std::cerr << "[BasicGpuSimulator] Failed to allocate GPU RAM!" << std::endl;
        ram_phys_base_ = 0;
        ram_phys_size_ = 0x80000000;
    }

    reg_space_.resize(0x1000 / 4, 0); // 初始化寄存器空间

    // 设置显存基地址寄存器
    reg_space_[static_cast<int>(GpuRegisterOffsets::GPU_RAM_BASE_ADDR) / 4] = ram_phys_base_;
    reg_space_[static_cast<int>(GpuRegisterOffsets::GPU_RAM_SIZE) / 4] = ram_phys_size_;

    // BAR 分配
    assign_bar(0, ram_phys_base_, ram_phys_size_, false);
    assign_bar(1, mmio_phys_base_, mmio_phys_size_, true);

    parser_.reset(new CommandParser(this, this, ram_phys_base_, ram_phys_size_));
}

uint64_t BasicGpuSimulator::get_bar_address(int index) const {
    if (index == 0) {
        return bar0_base_;
    } else if (index == 1) {
        return bar1_base_;
    }
    return 0;
}

uint64_t BasicGpuSimulator::get_bar_size(int index) const {
    if (index == 0) {
        return bar0_size_;
    } else if (index == 1) {
        return bar1_size_;
    }
    return 0;
}

void BasicGpuSimulator::assign_bar(int index, uint64_t base, size_t size, bool is_mmio) {
    if (index == 0) {
        bar0_base_ = base;
        bar0_size_ = size;
    } else if (index == 1) {
        bar1_base_ = base;
        bar1_size_ = size;
    }
}

int BasicGpuSimulator::read_mmio(uint64_t bar_offset, void* buffer, size_t size) {
    // 模拟 MMIO 读取
    if (bar_offset + size > mmio_phys_size_) {
        return -1; // 超出范围
    }
    
    // 从寄存器空间读取数据
    uint64_t reg_idx = bar_offset / 4;
    if (reg_idx < reg_space_.size()) {
        *(static_cast<uint32_t*>(buffer)) = reg_space_[reg_idx];
    }
    
    return 0;
}

int BasicGpuSimulator::write_mmio(uint64_t bar_offset, const void* buffer, size_t size) {
    // 模拟 MMIO 写入
    if (bar_offset + size > mmio_phys_size_) {
        return -1; // 超出范围
    }
    
    // 写入寄存器空间
    uint64_t reg_idx = bar_offset / 4;
    if (reg_idx < reg_space_.size()) {
        reg_space_[reg_idx] = *(static_cast<const uint32_t*>(buffer));
    }
    
    return 0;
}

int BasicGpuSimulator::read_ram(uint64_t bar_offset, void* buffer, size_t size) {
    // 模拟显存读取
    if (bar_offset + size > bar0_size_) {
        return -1; // 超出范围
    }
    
    if (gpu_memory_) {
        memcpy(buffer, gpu_memory_ + bar_offset, size);
    }
    
    return 0;
}

int BasicGpuSimulator::write_ram(uint64_t bar_offset, const void* buffer, size_t size) {
    // 模拟显存写入
    if (bar_offset + size > bar0_size_) {
        return -1; // 超出范围
    }
    
    if (gpu_memory_) {
        memcpy(gpu_memory_ + bar_offset, buffer, size);
    }
    
    return 0;
}

uint32_t BasicGpuSimulator::get_vendor_id() const {
    return 0x10DE; // NVIDIA vendor ID
}

uint32_t BasicGpuSimulator::get_device_id() const {
    return 0x1234; // 示例设备ID
}

void BasicGpuSimulator::enable_bus_master() {
    // 启用总线主控
}

void BasicGpuSimulator::disable_bus_master() {
    // 禁用总线主控
}

int BasicGpuSimulator::submit_command_packet(const GpuCommandPacket& packet) {
    // 提交命令包
    return 0;
}

int BasicGpuSimulator::execute_command() {
    // 执行命令
    return 0;
}