#pragma once

#include "kernel/pcie/pcie_emu.h"
#include "gpu/gpu_command_packet.h"
#include <cstddef>
#include <memory>
#include <vector>

namespace usr_linux_emu {

class CommandParser;

class BasicGpuSimulator : public PcieEmu {
 public:
    BasicGpuSimulator();
    ~BasicGpuSimulator();

    uint64_t get_bar_address(int index) const override;
    uint64_t get_bar_size(int index) const override;
    void assign_bar(int index, uint64_t base, size_t size, bool is_mmio) override;

    int read_mmio(uint64_t bar_offset, void* buffer, size_t size) override;
    int write_mmio(uint64_t bar_offset, const void* buffer, size_t size) override;

    int read_ram(uint64_t bar_offset, void* buffer, size_t size) override;
    int write_ram(uint64_t bar_offset, const void* buffer, size_t size) override;

    int submit_command_packet(const GpuCommandPacket& packet);
    int execute_command();

    uint32_t get_vendor_id() const override;
    uint32_t get_device_id() const override;
    void enable_bus_master() override;
    void disable_bus_master() override;

 private:
    uint64_t bar0_base_ = 0;
    uint64_t bar0_size_ = 0x1000000;
    uint64_t bar1_base_ = 0;
    uint64_t bar1_size_ = 0x10000000;

    char* gpu_memory_ = nullptr;

    std::unique_ptr<CommandParser> parser_;
};

}  // namespace usr_linux_emu