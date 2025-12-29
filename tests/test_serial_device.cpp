#include <iostream>
#include "kernel/vfs.h"
#include "kernel/device/serial_device.h"

int main() {
    // 创建串口设备
    auto serial_dev = std::make_shared<Device>(
        "ttyS0", 12346,
        std::make_shared<SerialDevice>(),
        nullptr
    );

    // 注册设备
    VFS::instance().register_device(serial_dev);

    // 打开设备
    auto dev = VFS::instance().open("/dev/ttyS0", 0);
    if (dev) {
        std::cout << "[Main] Successfully opened device." << std::endl;
    }

    return 0;
}
