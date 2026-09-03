cmake_minimum_required(VERSION 3.14)

# PCI driver 插件（Stage 5.5.1 迁移自 src/kernel/pcie/）
# 见 openspec/changes/2026-09-03-pci-driver-refactor/
# 注意：必须用 SHARED 而不是 MODULE，因为 test_*.standalone 二进制需要静态链入
# create_pcie_emu 等符号。运行时 ModuleLoader::load_plugins("plugins") 仍会 dlopen 它。
add_library(pci_driver_plugin SHARED
    plugin.cpp
    probe.cpp
    pci_access.cpp
    pci_msi.cpp
    pci_cap.cpp
    pci_iommu_integration.cpp
)

target_include_directories(pci_driver_plugin PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/kernel
    ${PROJECT_SOURCE_DIR}/include/linux_compat
    ${PROJECT_SOURCE_DIR}/include/linux_compat/pci
    ${PROJECT_SOURCE_DIR}/plugins/pci_driver/include
    # Stage 5.5.1 — pci_iommu_integration.cpp 引用 iommu_driver 的 iommu_internal.h
    ${PROJECT_SOURCE_DIR}/plugins/iommu_driver/include
)

target_link_libraries(pci_driver_plugin PRIVATE kernel ${CMAKE_DL_LIBS})

set_target_properties(pci_driver_plugin PROPERTIES
    PREFIX ""
    SUFFIX ".so"
    POSITION_INDEPENDENT_CODE ON
)

# Post-build: copy plugin to plugins/ 目录供 ModuleLoader 加载
add_custom_command(TARGET pci_driver_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:pci_driver_plugin>
        ${PROJECT_SOURCE_DIR}/plugins/plugin_pci_driver.so
    COMMENT "Copying pci_driver_plugin.so to plugins/ directory"
)