cmake_minimum_required(VERSION 3.14)
project(user_kernel_emu VERSION 1.0.0)

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

# ===== Sanitizer options (must be before add_subdirectory for global flag propagation) =====
# Phase A (change 2026-07-16-three-sanitizer-infra) — root-level sanitizer infra.
option(ENABLE_ASAN  "Enable AddressSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(ENABLE_TSAN  "Enable ThreadSanitizer for kernel threading tests (C-12 B.1.10, Clang only)" OFF)

# --- Sanitizer validation ---
# TSan is mutually exclusive with ASan and UBSan (linker/runtime conflict).
if(ENABLE_TSAN AND (ENABLE_ASAN OR ENABLE_UBSAN))
    message(FATAL_ERROR "TSan is mutually exclusive with ASan and UBSan "
        "(ENABLE_TSAN=${ENABLE_TSAN}, ENABLE_ASAN=${ENABLE_ASAN}, "
        "ENABLE_UBSAN=${ENABLE_UBSAN})")
endif()

# ASan: GCC or Clang family.
if(ENABLE_ASAN)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang" OR
       NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        message(FATAL_ERROR "ASan requires GCC or Clang (AppleClang included) "
            "(C compiler: ${CMAKE_C_COMPILER_ID}, "
            "C++ compiler: ${CMAKE_CXX_COMPILER_ID})")
    endif()
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address)
    message(STATUS "[ASan] AddressSanitizer enabled globally (ENABLE_ASAN=ON)")
endif()

# UBSan: GCC or Clang family; -fno-sanitize-recover=all aborts on first UB.
if(ENABLE_UBSAN)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang" OR
       NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        message(FATAL_ERROR "UBSan requires GCC or Clang (AppleClang included) "
            "(C compiler: ${CMAKE_C_COMPILER_ID}, "
            "C++ compiler: ${CMAKE_CXX_COMPILER_ID})")
    endif()
    add_compile_options(-fsanitize=undefined -fno-sanitize-recover=all
        -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=undefined)
    message(STATUS "[UBSan] UndefinedBehaviorSanitizer enabled globally, fatal mode (ENABLE_UBSAN=ON)")
endif()

# TSan: Clang only (per C-12 B.1.10.7); preserve -O1 baseline.
if(ENABLE_TSAN)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang" OR
       NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "TSan requires Clang "
            "(C compiler: ${CMAKE_C_COMPILER_ID}, "
            "C++ compiler: ${CMAKE_CXX_COMPILER_ID})")
    endif()
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer -g -O1)
    add_link_options(-fsanitize=thread)
    message(STATUS "[TSan] ThreadSanitizer enabled globally (ENABLE_TSAN=ON)")
endif()

# ===== Device VA base address (configurable; see plugins/gpu_driver/sim/sim_device_va_allocator.h) =====
# Override with -DSIM_DEVICE_VA_BASE=0x<value> when the default 86 TiB base
# collides with a sanitizer build or other user-process mmap region. See the
# header comment for probing methodology (ASan/TSan regions on Linux x86-64).
set(SIM_DEVICE_VA_BASE 0x560000000000ULL CACHE STRING "Device VA base address (bytes, hex)")

# 添加子模块
add_subdirectory(src)
add_subdirectory(drivers)
add_subdirectory(plugins)
add_subdirectory(tests)
add_subdirectory(tools/cli)
add_subdirectory(libgpu_core)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ===== Doxygen (optional; Stage 3.4) =====
find_package(Doxygen QUIET)
if(Doxygen_FOUND)
    add_custom_target(doxygen
        COMMAND ${DOXYGEN_EXECUTABLE} ${PROJECT_SOURCE_DIR}/docs/Doxyfile
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen → docs/api/"
    )
    message(STATUS "Doxygen found — 'make doxygen' target available")
else()
    message(STATUS "Doxygen not found — 'make doxygen' target unavailable (install: apt install doxygen)")
endif()

