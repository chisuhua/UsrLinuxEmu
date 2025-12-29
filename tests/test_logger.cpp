#include "kernel/logger.h"
#include "kernel/module_loader.h"

int main() {
    Logger::set_level(Logger::INFO);
    Logger::info("User kernel emulator started.");

    // 可调用 CLI 工具进行插件管理
    // 或者在这里直接测试
    ModuleLoader::load_plugin("../drivers/plugin_sample_memory.so");
    //ModuleLoader::unload_plugin("sample");

    Logger::info("User kernel emulator exited.");
    return 0;
}
