// plugins/iommu_driver/plugin.cpp
// IOMMU driver 插件入口（Stage 5.5.1 迁移自 src/kernel/iommu/）

#include "kernel/module_loader.h"

extern "C" {

static int iommu_driver_init(void) { return 0; }
static void iommu_driver_exit(void) {}

module mod = {
    .name = "iommu_driver",
    .load_priority = 200,  // Wave 1D：iommu_driver 依赖 pci_driver 先加载
    .depends = (const char*[]){"pci_driver", nullptr},
    .init = iommu_driver_init,
    .exit = iommu_driver_exit,
};

}