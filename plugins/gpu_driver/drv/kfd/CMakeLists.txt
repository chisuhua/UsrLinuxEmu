# kfd/CMakeLists.txt — KFD Tier-1 single-file PoC (Stage 1.4)
# Source: linux/drivers/gpu/drm/amd/amdkfd/kfd_queue.c (Linux 6.12 LTS)
# Per kfd-portability-boundary.md §3.4: only kfd_queue.c compiles cleanly.
# Multi-file KFD integration (kfd_module/device/doorbell/process) is
# Tier-2, out of Stage 1.4 scope (53+ amdgpu_* headers dependency gap).
#
# NOTE: Compiled as C++17 (not C11) because linux_compat/* headers are
# C++ namespace-scoped implementations. Linux kernel C code is valid C++.

add_library(gpu_kfd STATIC
    kfd_queue.c
)

set_source_files_properties(
    kfd_queue.c
    PROPERTIES LANGUAGE C
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
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/shared
)

target_link_libraries(gpu_kfd PUBLIC kernel)
