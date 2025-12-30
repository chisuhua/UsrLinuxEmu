#pragma once

#include "kernel/pcie/pcie_emu.h"
#include "gpu/gpu_command_packet.h"
#include <vector>

// 前向声明或添加缺失的类型定义
class CommandParser;  // 添加前向声明

class BasicGpuSimulator : public PcieEmu {
public:
    BasicGpuSimulator();
    ~BasicGpuSimulator();

    // PCIe 接口实现
    uint64_t get_bar_address(int index) const override;
    uint64_t get_bar_size(int index) const override;
    void assign_bar(int index, uint64_t base, size_t size, bool is_mmio) override;

    // MMIO 访问接口
    int read_mmio(uint64_t bar_offset, void* buffer, size_t size) override;
    int write_mmio(uint64_t bar_offset, const void* buffer, size_t size) override;

    // 显存访问接口
    int read_ram(uint64_t bar_offset, void* buffer, size_t size) override;
    int write_ram(uint64_t bar_offset, const void* buffer, size_t size) override;

    // GPU 任务接口
    int submit_command_packet(const GpuCommandPacket& packet);
    int execute_command();

    // 实现PcieEmu基类的纯虚函数
    uint32_t get_vendor_id() const override;
    uint32_t get_device_id() const override;
    void enable_bus_master() override;
    void disable_bus_master() override;

private:
    uint64_t bar0_base_ = 0;
    uint64_t bar0_size_ = 0x1000000;  // 16MB 显存
    uint64_t bar1_base_ = 0;
    uint64_t bar1_size_ = 0x10000000; // 256MB 显存

    // 模拟显存
    char* gpu_memory_ = nullptr;
    
    // 添加缺失的成员变量
    std::unique_ptr<CommandParser> parser_;  // 使用前向声明的类型
};
