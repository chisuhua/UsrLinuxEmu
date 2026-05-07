cmake_minimum_required(VERSION 3.14)

project(libgpu_core C)

# 纯 C 静态库
add_library(gpu_core STATIC
    src/buddy.c
)

target_include_directories(gpu_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 要求 C99 标准
set_target_properties(gpu_core PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

# 测试
option(BUILD_TESTS "Build libgpu_core tests" ON)

if(BUILD_TESTS)
    add_executable(test_buddy
        test/test_buddy.c
    )
    target_link_libraries(test_buddy PRIVATE gpu_core)
    target_include_directories(test_buddy PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    add_test(NAME test_buddy COMMAND test_buddy)
endif()
