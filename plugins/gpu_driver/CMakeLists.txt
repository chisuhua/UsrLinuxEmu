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
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/sim
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/sim/hardware
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/sim/scheduler
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

# sim/ 总是编译（gpu_sim 提供 doorbell/puller/scheduler 仿真）
add_subdirectory(sim)

target_link_libraries(gpu_driver_plugin PRIVATE kernel gpu_hal gpu_core gpu_drv gpu_sim)

set_target_properties(gpu_driver_plugin PROPERTIES
    PREFIX ""
    SUFFIX ".so"
    POSITION_INDEPENDENT_CODE ON
)

# ── Post-build: 同步插件到 plugins/ 目录 ─────────────────────────
# 解决 build/ vs plugins/ 两份 .so 的问题，确保 ModuleLoader 加载最新插件
add_custom_command(TARGET gpu_driver_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:gpu_driver_plugin>
        ${PROJECT_SOURCE_DIR}/plugins/plugin_gpu_driver.so
    COMMENT "Copying gpu_driver_plugin.so to plugins/ directory"
)

# ── P1.1a 影子编译目标 ─────────────────────────
option(ENABLE_GPU_SHADOW "启用 GPU 驱动影子编译（drv/sim/hal）" OFF)