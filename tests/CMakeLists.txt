# 查找所有 test/test_*.cpp 文件
enable_testing()
set(CATCH_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})


# 遍历每个测试源文件，创建可执行文件和链接 kernel 库
function(add_catch_test TEST_NAME)
    # 获取文件名（不带路径和扩展名）
    set(SOURCES ${ARGN})

    list(APPEND SOURCES ${CATCH_INCLUDE_DIR}/catch_amalgamated.cpp)
    
    # 添加可执行目标
    add_executable(${TEST_NAME} ${SOURCES})
    
    # 链接 kernel 模块
    target_link_libraries(${TEST_NAME} 
        PRIVATE kernel)
    
    # 添加测试，指定完整路径到可执行文件
    add_test(NAME ${TEST_NAME} COMMAND $<TARGET_FILE:${TEST_NAME}>)

    # 设置包含目录
    target_include_directories(${TEST_NAME} PRIVATE 
        ${PROJECT_SOURCE_DIR}/include 
        ${PROJECT_SOURCE_DIR}/include/kernel
        ${PROJECT_SOURCE_DIR}/include/linux_compat
        ${PROJECT_SOURCE_DIR}/drivers)
endfunction()

# 定义创建独立测试目标的函数
function(add_standalone_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME} PRIVATE kernel)
    target_include_directories(${TEST_NAME} PRIVATE 
        ${PROJECT_SOURCE_DIR}/include 
        ${PROJECT_SOURCE_DIR}/include/kernel
        ${PROJECT_SOURCE_DIR}/include/linux_compat
        ${PROJECT_SOURCE_DIR}/drivers)
    # 添加测试，这样ctest可以运行它
    add_test(NAME ${TEST_NAME} COMMAND $<TARGET_FILE:${TEST_NAME}>)
endfunction()

# 定义独立测试的列表（包含自定义main函数的测试）
set(STANDALONE_TESTS
    test_compat_memory.cpp
    test_compat_types.cpp
    test_gpu_ioctl.cpp
    test_gpu_mmap.cpp
    test_gpu_mmap_and_submit.cpp
    test_gpu_register.cpp
    test_gpu_regs.cpp
    test_gpu_ringbuffer.cpp
    test_gpu_submit.cpp
    test_ioctl.cpp
    test_logger.cpp
    test_pcie_gpu.cpp
    test_module_loader.cpp
    test_module_load_and_vfs.cpp
    test_plugin.cpp
)

# 定义Catch2测试的列表（使用Catch2框架的测试）
set(CATCH2_TESTS
    test_gpu_memory.cpp
    test_gpu_mmap_bar.cpp
)

foreach(src ${STANDALONE_TESTS})
    get_filename_component(testname ${src} NAME_WE)
    string(APPEND testname "_standalone")
    add_standalone_test(${testname} ${src})
endforeach()

# 为每个Catch2测试源文件创建测试
foreach(src ${CATCH2_TESTS})
    get_filename_component(testname ${src} NAME_WE)
    add_catch_test(${testname} ${src})
endforeach()