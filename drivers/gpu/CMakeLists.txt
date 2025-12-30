# GPU驱动模块
add_library(driver_gpu MODULE
    gpu_driver.cpp
    address_space.cpp
    buddy_allocator.cpp
    ring_buffer.cpp
    plugin_gpu.cpp
)

set_target_properties(driver_gpu PROPERTIES PREFIX "" SUFFIX ".so")
target_link_libraries(driver_gpu PRIVATE kernel)
target_include_directories(driver_gpu PRIVATE 
    ${PROJECT_SOURCE_DIR}/include 
    ${PROJECT_SOURCE_DIR}/include/kernel
    ${PROJECT_SOURCE_DIR}/include/linux_compat
    ${PROJECT_SOURCE_DIR}/simulator)