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
# Note: simulator/ is empty (legacy; simulation moved to plugins/gpu_driver/sim/ in Phase 1.5)
# The audit script (tools/docs-audit.sh §5.3) enforces this invariant.

# 添加子模块
add_subdirectory(src)
add_subdirectory(drivers)
add_subdirectory(plugins)
add_subdirectory(tests)
add_subdirectory(tools/cli)
add_subdirectory(libgpu_core)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# C-12 B.1.10.7 — ThreadSanitizer opt-in (Clang only)
# 启用方法：cmake -DENABLE_TSAN=ON
# 默认 OFF；GCC 构建不受影响
option(ENABLE_TSAN "Enable ThreadSanitizer for kernel threading tests (C-12 B.1.10, Clang only)" OFF)

