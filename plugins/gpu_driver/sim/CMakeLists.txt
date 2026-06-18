# sim/CMakeLists.txt — 仿真代码（仅在用户态运行）

add_library(gpu_sim STATIC
    buddy_allocator.cpp
    fence_sim.cpp
    hardware/hardware_puller_emu.cpp
    hardware/doorbell_emu.cpp
    gpu_queue_emu.cpp
    scheduler/global_scheduler.cpp
    scheduler/translator/gpfifo_translator.cpp
)

set_target_properties(gpu_sim PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(gpu_sim PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/kernel
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/shared
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/hal
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/sim
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/sim/hardware
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/sim/scheduler
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/sim/scheduler/translator
    ${PROJECT_SOURCE_DIR}/libgpu_core/include
)

target_link_libraries(gpu_sim PUBLIC
    gpu_core
)
