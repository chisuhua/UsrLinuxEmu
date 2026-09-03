// tests/test_plugin_loader_helper.h
// Stage 5.5.1: 在测试启动时 dlopen pci_driver / iommu_driver plugin，
// 否则 MODULE 库中符号（create_pcie_emu、iommu_emu_init 等）不会被解析。
//
// 用法：在每个 TEST_CASE 开头加 LOAD_PLUGINS_FOR_TESTS();

#pragma once

#include "kernel/module_loader.h"

#include <cstdlib>
#include <cstdio>

namespace usr_linux_emu {
namespace test_helpers {

// 进程级一次性 load_plugins。多次调用安全（ModuleLoader 内部已去重）。
inline void ensure_plugins_loaded_once() {
    static bool loaded = false;
    if (loaded) return;
    loaded = true;
    const char* dir = std::getenv("USR_LINUX_EMU_PLUGIN_DIR");
    std::string plugin_dir = dir ? dir : "plugins";
    int rc = ModuleLoader::load_plugins(plugin_dir);
    if (rc != 0) {
        std::fprintf(stderr,
            "[test_plugin_loader_helper] WARN: load_plugins(%s) returned %d "
            "(missing plugin .so may cause subsequent undefined-symbol failures)\n",
            plugin_dir.c_str(), rc);
    }
}

}  // namespace test_helpers
}  // namespace usr_linux_emu

// TEST_CASE 入口宏。线程安全由 C++11 静态局部变量初始化保证。
#define LOAD_PLUGINS_FOR_TESTS() \
    ::usr_linux_emu::test_helpers::ensure_plugins_loaded_once()