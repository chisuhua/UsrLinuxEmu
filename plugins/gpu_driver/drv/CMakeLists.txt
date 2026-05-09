# drv/CMakeLists.txt — 可移植驱动代码

add_library(gpu_drv STATIC
    gpu_drm_driver.cpp
)

set_target_properties(gpu_drv PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

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
)
