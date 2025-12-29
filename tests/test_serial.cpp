#include <iostream>
#include <thread>
#include <unistd.h>
#include "kernel/vfs.h"
#include "kernel/device/serial_device.h"

void reader_thread() {
    auto dev = VFS::instance().open("/dev/ttyS0", 0);
    char buf[100];
    std::cout << "[Reader] Waiting for data..." << std::endl;
    dev->fops->read(0, buf, sizeof(buf));
    std::cout << "[Reader] Got data!" << std::endl;
}

int main() {
    auto serial_dev = std::make_shared<Device>(
        "ttyS0", 12346,
        std::make_shared<SerialDevice>(),
        nullptr);

    VFS::instance().register_device(serial_dev);

    std::thread t(reader_thread);
    sleep(2);

    auto dev = VFS::instance().lookup_device("ttyS0");
    if (dev) {
        static_cast<SerialDevice*>(dev->fops.get())->push_data("Hello Serial!");
    }

    t.join();
    return 0;
}
