# kfd/CMakeLists.txt — KFD PoC C library
# Stage 1.2 PoC: compile kfd_queue.c from Linux 6.12 LTS
# (zero logic modifications, only #include path adjustments)

add_library(gpu_kfd STATIC
    kfd_queue.c
)

set_target_properties(gpu_kfd PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    C_STANDARD 11
)

target_include_directories(gpu_kfd PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/linux_compat
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/shared
)

target_link_libraries(gpu_kfd PUBLIC kernel)
