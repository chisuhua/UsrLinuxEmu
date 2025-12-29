#include <iostream>
#include <thread>
#include <unistd.h>
#include "kernel/module_loader.h"
#include "kernel/vfs.h"
#include "../drivers/sample_memory.h"

void test_ioctl() {
    auto dev = VFS::instance().open("/dev/sample", 0);
    if (!dev) return;

    int mode = 1;
    dev->fops->ioctl(0, SAMPLE_SET_MODE, &mode);

    int status;
    dev->fops->ioctl(0, SAMPLE_GET_STATUS, &status);
}

int main() {
    ModuleLoader::load_plugins("plugins");

    std::thread t1(test_ioctl);
    std::thread t2(test_ioctl);

    t1.join();
    t2.join();

    ModuleLoader::unload_plugins();
    return 0;
}
