# kfd/CMakeLists.txt — KFD 5-file integration (Stage 1.4)
# Source: linux/drivers/gpu/drm/amd/amdkfd/ (Linux 6.12 LTS)
# Per tasks.md §3: ZERO logic modifications, only #include path adjustments.
#
# NOTE: Compiled as C++17 (not C11) because linux_compat/* headers are
# C++ namespace-scoped implementations. Linux kernel C code is valid C++
# (no C-only constructs in the 5 KFD files), so this preserves the
# "logic zero modification" constraint — .c file contents unchanged,
# only the compiler interpretation shifts from C to C++.

add_library(gpu_kfd STATIC
    kfd_module.c
    kfd_queue.c
    kfd_device.c
    kfd_doorbell.c
    kfd_process.c
)

set_source_files_properties(
    kfd_module.c kfd_queue.c kfd_device.c kfd_doorbell.c kfd_process.c
    PROPERTIES LANGUAGE CXX
)

set_target_properties(gpu_kfd PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_STANDARD 17
)

target_include_directories(gpu_kfd PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/linux_compat
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/shared
)

target_link_libraries(gpu_kfd PUBLIC kernel)
