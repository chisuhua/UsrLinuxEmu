# kfd/CMakeLists.txt — KFD multi-file integration (C-12)
# Source: linux/drivers/gpu/drm/amd/amdkfd/ (Linux 6.12 LTS)
# Per kfd-multi-file.md §3-5 + ADR-059: 6-module split.
# Phase B.1.1: kfd_module.c stub + kfd_queue.c (Tier-1 PoC)
# Phase B.1.3+: kfd_pasid.c, kfd_process.c, etc.
#
# NOTE: Compiled as C++17 (not C11) because linux_compat/* headers are
# C++ namespace-scoped implementations. Linux kernel C code is valid C++.

add_library(gpu_kfd STATIC
    kfd_queue.c
    kfd_module.c  # C-12 B.1.1 stub: kfd_module_init/exit bridge
    kfd_dispatch.c  # C-12 B.2.1: ioctl dispatch table scaffolding
    kfd_topology.c  # C-12 B.1.8 stub: kfd_topology_device_by_id + kfd_topology_init
    kfd_svm.c  # C-12 B.1.9 stub: svm_range_from_addr + svm_range_list_init
    kfd_pasid.c  # C-12 B.1.3: PASID bitmap allocator (standalone, no cross-deps)
    kfd_process.c  # C-12 B.1.5: process management (depends on pasid, svm)
    kfd_mmu.c  # C-12 B.3.1: IOMMU map/unmap (depends on kernel_workqueue C++ class)
    kfd_events.c  # C-12 B.4.1: event signal (depends on kernel_workqueue C++ class)
)

set_source_files_properties(
    kfd_queue.c
    PROPERTIES LANGUAGE C
)

set_source_files_properties(
    kfd_module.c
    PROPERTIES LANGUAGE C
)

set_source_files_properties(
    kfd_dispatch.c
    PROPERTIES LANGUAGE C
)

set_source_files_properties(
    kfd_topology.c
    PROPERTIES LANGUAGE C
)

set_source_files_properties(
    kfd_svm.c
    PROPERTIES LANGUAGE C
)

set_source_files_properties(
    kfd_pasid.c
    PROPERTIES LANGUAGE C
)

set_source_files_properties(
    kfd_process.c
    PROPERTIES LANGUAGE C
)

# kfd_mmu.c and kfd_events.c need C++ (use kernel_workqueue class, new/delete).
# Do NOT set LANGUAGE C for these files.
set_source_files_properties(
    kfd_mmu.c
    PROPERTIES LANGUAGE CXX
)

set_source_files_properties(
    kfd_events.c
    PROPERTIES LANGUAGE CXX
)

set_target_properties(gpu_kfd PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    C_STANDARD 11
)

target_compile_options(gpu_kfd PRIVATE -Wno-implicit-function-declaration)

target_include_directories(gpu_kfd PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/linux_compat
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/hal
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/shared
)

target_link_libraries(gpu_kfd PRIVATE kernel)
