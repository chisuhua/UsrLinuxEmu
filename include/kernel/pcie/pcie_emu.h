#pragma once

#include <cstdint>

class PcieEmu {
public:
    virtual ~PcieEmu() = default;

    // 获取 VendorID / DeviceID
    virtual uint32_t get_vendor_id() const = 0;
    virtual uint32_t get_device_id() const = 0;

    // 获取 BAR 信息
    virtual uint64_t get_bar_address(int index) const = 0;
    virtual uint64_t get_bar_size(int index) const = 0;

    // 分配 BAR 地址空间
    virtual void assign_bar(int index, uint64_t base, size_t size, bool is_mmio) = 0;

    // MMIO 和 RAM 访问接口
    virtual int read_mmio(uint64_t bar_offset, void* buffer, size_t size) = 0;
    virtual int write_mmio(uint64_t bar_offset, const void* buffer, size_t size) = 0;

    virtual int read_ram(uint64_t bar_offset, void* buffer, size_t size) = 0;
    virtual int write_ram(uint64_t bar_offset, const void* buffer, size_t size) = 0;

    // 启用总线主控
    virtual void enable_bus_master() = 0;
    virtual void disable_bus_master() = 0;
};
