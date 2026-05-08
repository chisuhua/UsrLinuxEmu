cmake_minimum_required(VERSION 3.14)

# GPU 驱动仿真插件（现有活跃入口）
add_library(gpu_driver_plugin MODULE
    plugin.cpp
)

# 添加 shared 目录（canonical 接口）
target_include_directories(gpu_driver_plugin PRIVATE
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/shared
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/hal
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver
    ${PROJECT_SOURCE_DIR}/libgpu_core/include
)

# 添加 kernel 框架头文件
target_include_directories(gpu_driver_plugin PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/kernel
)

# drv/ 和 hal/ 总是编译（plugin.cpp 依赖它们）
add_subdirectory(drv)
add_subdirectory(hal)

target_link_libraries(gpu_driver_plugin PRIVATE kernel gpu_hal gpu_core gpu_drv)

set_target_properties(gpu_driver_plugin PROPERTIES
    PREFIX ""
    SUFFIX ".so"
    POSITION_INDEPENDENT_CODE ON
)

# ── P1.1a 影子编译目标 ──────────────────────────
# 这些目标编译但不链接到 gpu_driver_plugin.so。
# 启用 ENABLE_GPU_SHADOW=ON 来激活它们（默认关闭）。
option(ENABLE_GPU_SHADOW "启用 GPU 驱动影子编译（drv/sim/hal）" OFF)

if(ENABLE_GPU_SHADOW)
  add_subdirectory(sim)
endif()