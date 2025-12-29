#include <iostream>
#include <thread>
#include <unistd.h>
#include "kernel/module_loader.h"
#include "kernel/vfs.h"
#include "../drivers/sample_memory.h"

int main() {
    auto dev1 = VFS::instance().lookup_device("sample");
    ModuleLoader::load_plugins("../drivers");

    auto dev = VFS::instance().lookup_device("sample");
    if (!dev) {
        std::cerr << "[Main] Device not found!" << std::endl;
        return -1;
    }

    // 启动一个线程尝试读取（阻塞）
    std::thread t([&]() {
        char buf[100];
        dev->fops->read(0, buf, sizeof(buf)); // 应该阻塞
    });

    sleep(2); // 主线程休眠2秒后写入数据

    // 写入数据，唤醒等待线程
    dev->fops->write(0, "data", 4);

    t.join();

    ModuleLoader::unload_plugins();
    return 0;
}
