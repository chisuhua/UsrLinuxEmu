add_library(kernel SHARED
    kernel/types.cpp
    kernel/module.cpp
    kernel/vfs.cpp
    kernel/file_ops.cpp
    kernel/wait_queue.cpp
    kernel/plugin_manager.cpp
    kernel/module_loader.cpp
    kernel/config_manager.cpp
    kernel/service_registry.cpp
    kernel/logger.cpp
    kernel/device.cpp
    kernel/poll_watcher.cpp
    kernel/device/serial_device.cpp
    kernel/device/memory_device.cpp
    kernel/pcie/pcie_emu.cpp
    kernel/pcie/config_space.cpp
    kernel/pcie/capability_walk.cpp
    kernel/pcie/msi_x.cpp
    # Stage 1.1 — IOMMU + ATS (see openspec/changes/stage-1-1-iommu-ats/)
    kernel/iommu/iommu_domain.cpp
    kernel/iommu/iommu_group.cpp
    kernel/iommu/ioasid.cpp
    kernel/iommu/iommu_emu_state.cpp
    kernel/iommu/dma_remap.cpp
    kernel/iommu/ats_protocol.cpp
    kernel/iommu/invalidate.cpp
    kernel/iommu/pcie_integration.cpp
    # Stage 1.2 — DRM GEM lifecycle (TDD)
    kernel/drm/drm_gem.cpp
    kernel/drm/drm_file.cpp
    kernel/drm/drm_prime.cpp
    kernel/drm/render_node.cpp

)

# 添加 UsrLinuxEmu 头文件路径
include_directories(${PROJECT_SOURCE_DIR}/include)

target_link_libraries(kernel PRIVATE dl pthread)
set_target_properties(kernel PROPERTIES POSITION_INDEPENDENT_CODE ON)