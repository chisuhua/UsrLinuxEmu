cmake_minimum_required(VERSION 3.14)

# IOMMU driver 插件（Stage 5.5.1 迁移自 src/kernel/iommu/）
# 见 openspec/changes/2026-09-03-pci-driver-refactor/
# 注意：必须用 SHARED 而不是 MODULE，因为 test_*.standalone 二进制需要静态链入
# iommu_emu_init 等符号。运行时 ModuleLoader::load_plugins("plugins") 仍会 dlopen 它。
add_library(iommu_driver_plugin SHARED
    plugin.cpp
    iommu.cpp
    iommu_group.cpp
    iommu_domain.cpp
    ats_protocol.cpp
    dma_remap.cpp
    ioasid.cpp
    invalidate.cpp
    vfio_bridge.cpp
)

target_include_directories(iommu_driver_plugin PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/kernel
    ${PROJECT_SOURCE_DIR}/include/linux_compat
    ${PROJECT_SOURCE_DIR}/include/linux_compat/iommu
    ${PROJECT_SOURCE_DIR}/plugins/iommu_driver/include
    ${PROJECT_SOURCE_DIR}/plugins/pci_driver/include
)

target_link_libraries(iommu_driver_plugin PRIVATE kernel ${CMAKE_DL_LIBS})

set_target_properties(iommu_driver_plugin PROPERTIES
    PREFIX ""
    SUFFIX ".so"
    POSITION_INDEPENDENT_CODE ON
)

add_custom_command(TARGET iommu_driver_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:iommu_driver_plugin>
        ${PROJECT_SOURCE_DIR}/plugins/plugin_iommu_driver.so
    COMMENT "Copying iommu_driver_plugin.so to plugins/ directory"
)