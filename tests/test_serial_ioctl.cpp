#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "kernel/vfs.h"
#include "kernel/module_loader.h"

// 必须包含自定义命令
#include "kernel/ioctl.h"
#include "drivers/sample_serial.h"

int main() {
    ModuleLoader::load_plugins("plugins");

    auto dev = VFS::instance().open("/dev/ttyS0", 0);
    if (!dev) {
        std::cerr << "[Test] Failed to open serial device." << std::endl;
        return -1;
    }

    int fd = 0;

    // 获取当前波特率
    int current_baud = 0;
    dev->fops->ioctl(fd, SERIAL_GET_BAUDRATE, &current_baud);
    std::cout << "[Test] Current baud rate: " << current_baud << std::endl;

    // 设置新波特率
    int new_baud = 115200;
    dev->fops->ioctl(fd, SERIAL_SET_BAUDRATE, &new_baud);
    std::cout << "[Test] Set baud rate to: " << new_baud << std::endl;

    // 再次获取波特率验证
    dev->fops->ioctl(fd, SERIAL_GET_BAUDRATE, &current_baud);
    std::cout << "[Test] Updated baud rate: " << current_baud << std::endl;

    // 清空缓冲区
    dev->fops->ioctl(fd, SERIAL_FLUSH, nullptr);

    ModuleLoader::unload_plugins();
    return 0;
}
