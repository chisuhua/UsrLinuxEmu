#include <iostream>
#include "kernel/vfs.h"
#include "kernel/module_loader.h"
#include "kernel/device/gpgpu_device.h"
#include "kernel/pcie_device.h"

int main() {
    ModuleLoader::load_plugins("plugins");

    auto dev = VFS::instance().open("/dev/gpgpu0", 0);
    if (!dev) {
        std::cerr << "[TestPCIE] Failed to open GPU device." << std::endl;
        return -1;
    }

    PciDevice* pci_dev = dynamic_cast<PciDevice*>(dev->fops.get());
    if (!pci_dev) {
        std::cerr << "[TestPCIE] Device does not support PCIe interface." << std::endl;
        return -1;
    }

    std::cout << "[TestPCIE] Vendor ID: 0x" << std::hex << pci_dev->get_vendor_id() << std::dec << std::endl;
    std::cout << "[TestPCIE] Device ID: 0x" << std::hex << pci_dev->get_device_id() << std::dec << std::endl;

    // PCIe 配置空间读写
    std::cout << "[TestPCIE] Reading config dword at offset 0x00: 0x" << std::hex
              << pci_dev->read_config_dword(0x00) << std::dec << std::endl;

    // 启用总线主控
    pci_dev->enable_bus_master();

    // 用户态 IOCTL 测试（略）

    ModuleLoader::unload_plugins();
    return 0;
}
