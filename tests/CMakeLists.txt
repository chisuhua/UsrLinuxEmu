# 查找所有 test/test_*.cpp 文件
enable_testing()
set(CATCH_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB TEST_SOURCES "test_*.cpp")

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
    
    # 添加测试
    add_test(NAME ${TEST_NAME} COMMAND ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIGURATION>/${TEST_NAME})

    # 设置包含目录
    target_include_directories(${TEST_NAME} PRIVATE 
        ${PROJECT_SOURCE_DIR}/include 
        ${PROJECT_SOURCE_DIR}/include/kernel
        ${PROJECT_SOURCE_DIR}/include/linux_compat)
endfunction()

# 为每个测试源文件创建测试
foreach(src ${TEST_SOURCES})
    get_filename_component(testname ${src} NAME_WE)
    add_catch_test(${testname} ${src})
endforeach()
