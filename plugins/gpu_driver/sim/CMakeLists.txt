# sim/CMakeLists.txt — 仿真代码（仅在用户态运行）

add_library(gpu_sim STATIC
    hardware/hardware_puller_emu.cpp
    gpu_queue_emu.cpp
    scheduler/global_scheduler.cpp
    scheduler/translator/gpfifo_translator.cpp
    # Stage 1.3 — UVM/HMM sim layer (see openspec/changes/stage-1-3-uvm-hmm/)
    page_fault_handler.cpp
    page_migration.cpp
    # sim-stream-primitive-support — Phase A.1 fence_id (Fix-1 / Oracle H4)
    fence_id.cpp
    # sim-stream-primitive-support — §2.1 Stream Capture (Phase 3.1)
    stream_capture.cpp
    # sim-stream-primitive-support — §2.2 Graph (Phase 3.1)
    graph.cpp
    # sim-stream-primitive-support — §2.3 Memory Pool (Phase 3.2, Fix-2 Option B)
    mem_pool.cpp
    # phase4-cu-mempool-alloc-real-va — per-device VA sub-range allocator (ADR-058 D1)
    sim_device_va_allocator.cpp
    # C-12 B.4.4 — KFD event signal sim-layer stub (per ADR-062 §D3)
    sim_event.c
)

set_source_files_properties(
    sim_event.c
    PROPERTIES LANGUAGE C
)

set_target_properties(gpu_sim PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(gpu_sim PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/kernel
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/shared
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd
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

# Propagate CMake-configurable VA base to the header's #ifndef guard.
# Consumers (incl. test binaries linking gpu_sim) see the same value.
target_compile_definitions(gpu_sim PUBLIC SIM_DEVICE_VA_BASE=${SIM_DEVICE_VA_BASE})
