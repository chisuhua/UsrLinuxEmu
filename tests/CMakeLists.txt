# 查找所有 test/test_*.cpp 文件
file(GLOB TEST_SOURCES "test_*.cpp")

# 遍历每个测试源文件，创建可执行文件和链接 kernel 库
foreach(src ${TEST_SOURCES})
    # 获取文件名（不带路径和扩展名）
    get_filename_component(testname ${src} NAME_WE)
    
    # 添加可执行目标
    add_executable(${testname} ${src})
    
    # 链接 kernel 模块
    target_link_libraries(${testname} 
        PRIVATE kernel)
    #            kernel sample_memory sample_serial)
endforeach()

# 创建一键运行所有测试的目标
add_custom_target(run_all_tests
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target all_tests_run
)

# 创建一个伪目标，用于运行所有测试
add_custom_target(all_tests_run ALL)

# 为每个测试添加运行命令
foreach(src ${TEST_SOURCES})
    get_filename_component(testname ${src} NAME_WE)
    add_custom_command(
        TARGET all_tests_run
        POST_BUILD
        COMMAND $<TARGET_FILE:${testname}>
        COMMENT "Running test: ${testname}"
    )
endforeach()

#enable_testing()
#find_package(GTest REQUIRED)

#foreach(src ${TEST_SOURCES})
#    get_filename_component(testname ${src} NAME_WE)
#    add_executable(${testname} ${src})
#    target_link_libraries(${testname} PRIVATE kernel GTest::GTest GTest::Main)
#    add_test(NAME ${testname} COMMAND ${testname})
#endforeach()
