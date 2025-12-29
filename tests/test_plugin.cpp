#include <iostream>
#include <thread>
#include <unistd.h>
#include "kernel/module_loader.h"
#include "kernel/vfs.h"
#include "kernel/plugin_manager.h"
#include "../drivers/sample_memory.h"

using namespace std;

void test_ioctl() {
    auto dev = VFS::instance().open("/dev/sample", 0);
    if (!dev) return;

    int mode = 1;
    dev->fops->ioctl(0, SAMPLE_SET_MODE, &mode);

    int status;
    dev->fops->ioctl(0, SAMPLE_GET_STATUS, &status);
}

int main() {
    cout << "[Main] Starting plugin manager demo..." << endl;

    // 加载初始插件
    ModuleLoader::load_plugins("plugins");

    // 列出当前插件
    ModuleLoader::list_plugins();

    // 卸载 sample 插件
    cout << "\n[Main] Unloading sample plugin..." << endl;
    ModuleLoader::unload_plugin("sample");

    // 等待几秒后重新加载
    sleep(2);
    cout << "\n[Main] Reloading sample plugin at runtime..." << endl;
    ModuleLoader::load_plugin("drivers/sample_memory_plugin.so");

    // 再次列出插件
    ModuleLoader::list_plugins();

    // 启动多线程测试
    thread t1(test_ioctl);
    thread t2(test_ioctl);

    t1.join();
    t2.join();

    // 最终卸载插件
    ModuleLoader::unload_plugins();

    return 0;
}
