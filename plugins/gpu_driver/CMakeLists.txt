cmake_minimum_required(VERSION 3.14)

# GPU 驱动仿真插件
add_library(gpu_driver_plugin MODULE
    plugin.cpp
)

# 添加 shared 目录（canonical 接口）
target_include_directories(gpu_driver_plugin PRIVATE
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/shared
)

# 添加 kernel 框架头文件
target_include_directories(gpu_driver_plugin PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/kernel
)

target_link_libraries(gpu_driver_plugin PRIVATE kernel)

set_target_properties(gpu_driver_plugin PROPERTIES
    PREFIX ""
    SUFFIX ".so"
    POSITION_INDEPENDENT_CODE ON
)