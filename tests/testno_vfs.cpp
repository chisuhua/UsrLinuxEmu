#include <iostream>
#include "kernel/vfs.h"
#include "kernel/device/memory_device.h"
#include "../drivers/sample_memory.h"

int main() {
    auto mem_dev = std::make_shared<Device>(
        "mem0", 12344,
        std::make_shared<SampleMemory>(4096),
        nullptr);

    VFS::instance().register_device(mem_dev);

    auto dev = VFS::instance().open("/dev/mem0", 0);
    if (dev) {
        char buf[16] = {0};
        dev->fops->read(0, buf, sizeof(buf));
        std::cout << "[TestVFS] Read complete." << std::endl;
    }

    return 0;
}
