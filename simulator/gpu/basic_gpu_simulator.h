#pragma once

#include "kernel/pcie/pcie_emu.h"
#include "kernel/device/gpu_command_packet.h"

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

private:
    void process_kernel(const KernelCommand& kernel);
    void process_dma(const DmaCommand& dma);

    uint64_t ram_phys_base_;
    size_t ram_phys_size_;
    uint64_t mmio_phys_base_;
    size_t mmio_phys_size_;

    std::vector<uint32_t> reg_space_; // 寄存器空间
    std::unique_ptr<CommandParser> parser_;
};

