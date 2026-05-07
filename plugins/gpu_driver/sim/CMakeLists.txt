# sim/CMakeLists.txt — 仿真代码（仅在用户态运行）

add_library(gpu_sim STATIC
    buddy_allocator.cpp
    fence_sim.cpp
    hardware_puller_emu.cpp
    doorbell_emu.cpp
)

target_include_directories(gpu_sim PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/kernel
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/shared
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/hal
    ${PROJECT_SOURCE_DIR}/libgpu_core/include
)

target_link_libraries(gpu_sim PUBLIC
    gpu_core
)
