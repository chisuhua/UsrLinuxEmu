# drv/CMakeLists.txt — 可移植驱动代码

add_library(gpu_drv STATIC
    gpu_drm_driver.cpp
    gpgpu_device.cpp
    kfd_sim_bridge.cpp
)

set_target_properties(gpu_drv PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

# GCC 13 + glibc pre-existing build issue: <thread> pulls in pthread/gthr
# headers which need _GNU_SOURCE for cpu_set_t/sched_yield declarations.
# Pre-existing issue documented; full verification blocked until upstream fix.
target_compile_definitions(gpu_drv PRIVATE _GNU_SOURCE)

target_include_directories(gpu_drv PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/kernel
    ${PROJECT_SOURCE_DIR}/include/linux_compat
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/shared
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver
    ${PROJECT_SOURCE_DIR}/libgpu_core/include
)

# drv 不直接依赖 sim（仅通过 hal 接口）
target_link_libraries(gpu_drv PUBLIC
    kernel
    gpu_hal
    gpu_sim
    gpu_kfd
)

# KFD PoC C library (Stage 1.2)
add_subdirectory(kfd)
