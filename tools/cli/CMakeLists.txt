# CLI工具可执行文件
add_executable(cli 
    main.cpp
)

target_link_libraries(cli PRIVATE kernel)
target_include_directories(cli PRIVATE 
    ${PROJECT_SOURCE_DIR}/include 
    ${PROJECT_SOURCE_DIR}/include/kernel
    ${PROJECT_SOURCE_DIR}/include/linux_compat)