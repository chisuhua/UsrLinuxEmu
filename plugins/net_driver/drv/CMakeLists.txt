# drv/CMakeLists.txt — ② portable network driver code

add_library(net_drv STATIC
    net_driver.cpp
)

set_target_properties(net_drv PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(net_drv PRIVATE _GNU_SOURCE)

target_include_directories(net_drv PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/kernel
)

target_link_libraries(net_drv PUBLIC kernel)