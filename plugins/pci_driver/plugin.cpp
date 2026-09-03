// plugins/pci_driver/plugin.cpp
// PCI driver 插件入口（Stage 5.5.1 迁移自 src/kernel/pcie/）
// 见 openspec/changes/2026-09-03-pci-driver-refactor/

#include "kernel/module_loader.h"

extern "C" {

// 模块加载钩子
static int pci_driver_init(void) {
    return 0;
}

static void pci_driver_exit(void) {
    // cleanup
}

module mod = {
    .name = "pci_driver",
    // .load_priority = 100  (Wave 1D 添加，struct module 当前无此字段)
    .depends = (const char*[]){nullptr},  // 无 plugin 依赖
    .init = pci_driver_init,
    .exit = pci_driver_exit,
};

}  // extern "C"