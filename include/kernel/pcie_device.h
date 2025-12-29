#pragma once

#include <cstdint>

class PciDevice {
public:
    virtual ~PciDevice() = default;

    virtual uint32_t read_config_dword(uint8_t offset) = 0;
    virtual void write_config_dword(uint8_t offset, uint32_t value) = 0;

    virtual void enable_bus_master() = 0;
    virtual void disable_bus_master() = 0;

    // 获取设备信息
    virtual uint32_t get_vendor_id() const = 0;
    virtual uint32_t get_device_id() const = 0;
};

class PcieRootComplex {
public:
    virtual ~PcieRootComplex() = default;

    virtual PciDevice* find_pci_device(uint32_t bus, uint32_t dev, uint32_t func) = 0;
    virtual void assign_bar(PciDevice* dev, int bar_index, uint64_t base, size_t size) = 0;
};
