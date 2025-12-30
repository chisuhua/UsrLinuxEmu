cmake_minimum_required(VERSION 3.14)
project(user_kernel_emu)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 输出路径
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

# 启用测试
enable_testing()

# 包含头文件目录
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/include/kernel)
include_directories(${PROJECT_SOURCE_DIR}/include/kernel/device)
include_directories(${PROJECT_SOURCE_DIR}/drivers)
include_directories(${PROJECT_SOURCE_DIR}/external/json)
include_directories(${PROJECT_SOURCE_DIR}/simulator)

# 添加子模块
add_subdirectory(src)
add_subdirectory(drivers)
add_subdirectory(tests)
add_subdirectory(tools/cli)