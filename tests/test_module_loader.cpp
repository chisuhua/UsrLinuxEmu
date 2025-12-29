#include <iostream>
#include "kernel/module_loader.h"

int main() {
    std::cout << "[KernelEmu] Starting user-space kernel emulator..." << std::endl;

    // 加载插件
    ModuleLoader::load_plugins("plugins");

    // 模拟运行一段时间
    std::cout << "[KernelEmu] Running... Press Enter to exit." << std::endl;
    std::cin.get();

    // 卸载插件
    ModuleLoader::unload_plugins();

    std::cout << "[KernelEmu] Exiting." << std::endl;
    return 0;
}
