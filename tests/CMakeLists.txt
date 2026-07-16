# 查找所有 test/test_*.cpp 文件
enable_testing()
set(CATCH_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# Stage 3.2 - perf benchmark framework (C-10, see openspec/changes/2026-07-22-*)
# OFF by default；opt-in build via:
#   cmake -DUSR_LINUX_EMU_PERF_TESTS=ON -B build
option(USR_LINUX_EMU_PERF_TESTS "Build performance benchmarks (Stage 3.2 baseline)" OFF)

# ADR-063 D4: kernel (SHARED) intentionally references sim_pm_invalidate
# from gpu_sim (STATIC, in plugins/).  Tests that don't link gpu_sim must
# ignore undefined symbols in shared libs; the runtime guard
# (if flushed_entries && state->sim_pm) prevents calling the unresolved
# symbol when no sim_pm is attached.  Tests that DO link gpu_sim (via
# add_catch_sim_test / add_sim_test) get the real implementation.
add_link_options("-Wl,--unresolved-symbols=ignore-in-shared-libs")


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
    set_tests_properties(${TEST_NAME} PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

    # 设置包含目录
    target_include_directories(${TEST_NAME} PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/kernel
        ${PROJECT_SOURCE_DIR}/include/linux_compat
        ${PROJECT_SOURCE_DIR}/drivers
        ${PROJECT_SOURCE_DIR}/plugins
        ${CMAKE_CURRENT_SOURCE_DIR})
endfunction()

# 定义创建独立测试目标的函数
function(add_standalone_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME} PRIVATE kernel)
    target_include_directories(${TEST_NAME} PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/kernel
        ${PROJECT_SOURCE_DIR}/include/linux_compat
        ${PROJECT_SOURCE_DIR}/drivers
        ${PROJECT_SOURCE_DIR}/plugins)
    # 添加测试，这样ctest可以运行它
    add_test(NAME ${TEST_NAME} COMMAND $<TARGET_FILE:${TEST_NAME}>)
    set_tests_properties(${TEST_NAME} PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
endfunction()

# 定义创建 sim 相关测试的函数 (链接 gpu_sim)
function(add_sim_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME} PRIVATE kernel gpu_sim)
    target_include_directories(${TEST_NAME} PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/kernel
        ${PROJECT_SOURCE_DIR}/include/linux_compat
        ${PROJECT_SOURCE_DIR}/drivers
        ${PROJECT_SOURCE_DIR}/plugins
        ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/sim
        ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/sim/hardware
        ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/sim/scheduler
        ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/shared
        ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/hal)
    add_test(NAME ${TEST_NAME} COMMAND $<TARGET_FILE:${TEST_NAME}>)
    set_tests_properties(${TEST_NAME} PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
endfunction()

# Stage 2.2.4 — net driver test (links kernel + net_drv)
function(add_catch_net_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE} ${CATCH_INCLUDE_DIR}/catch_amalgamated.cpp)
    target_link_libraries(${TEST_NAME} PRIVATE kernel net_drv)
    target_include_directories(${TEST_NAME} PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/kernel
        ${PROJECT_SOURCE_DIR}/include/linux_compat
        ${PROJECT_SOURCE_DIR}/drivers
        ${PROJECT_SOURCE_DIR}/plugins
        ${PROJECT_SOURCE_DIR}/plugins/net_driver/drv
        ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME ${TEST_NAME} COMMAND $<TARGET_FILE:${TEST_NAME}>)
    set_tests_properties(${TEST_NAME} PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
endfunction()

# Define Catch2 sim test function (links kernel + gpu_sim + gpu_drv)
function(add_catch_sim_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE} ${CATCH_INCLUDE_DIR}/catch_amalgamated.cpp)
    target_link_libraries(${TEST_NAME} PRIVATE kernel gpu_sim gpu_drv)
    target_include_directories(${TEST_NAME} PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/kernel
        ${PROJECT_SOURCE_DIR}/include/linux_compat
        ${PROJECT_SOURCE_DIR}/drivers
        ${PROJECT_SOURCE_DIR}/plugins
        ${PROJECT_SOURCE_DIR}/plugins/gpu_driver
        ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/shared
        ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv
        ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/sim
        ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/sim/hardware
        ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/sim/scheduler
        ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/shared
        ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/hal
        ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME ${TEST_NAME} COMMAND $<TARGET_FILE:${TEST_NAME}>)
    set_tests_properties(${TEST_NAME} PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
endfunction()

# 定义独立测试的列表（包含自定义main函数的测试）
set(STANDALONE_TESTS
    test_compat_memory.cpp
    test_compat_types.cpp
    test_doorbell_emu.cpp
    test_gpu_ioctl.cpp
    test_gpu_ioctl_number.cpp
    test_gpu_fence_return.cpp
    test_gpu_mempool_export.cpp
    test_gpu_mmap.cpp
    test_gpu_mmap_and_submit.cpp
    test_gpu_pushbuffer_validation.cpp
    test_gpu_register.cpp
    test_gpu_regs.cpp
    test_gpu_submit.cpp
    test_ioctl.cpp
    test_logger.cpp
    test_pcie_gpu.cpp
    test_module_loader.cpp
    test_module_load_and_vfs.cpp
    test_plugin.cpp
    test_va_space.cpp
    test_serial.cpp
    test_serial_device.cpp
    test_poll.cpp
    test_serial_ioctl.cpp
)

# 定义Catch2测试的列表（使用Catch2框架的测试）
set(CATCH2_TESTS
    test_gpu_memory.cpp
    test_gpu_mmap_bar.cpp
    test_module_loader_isolation.cpp
    test_pcie_emu_standalone.cpp
    test_config_space_standalone.cpp
    test_msi_x_inject_standalone.cpp
    # Stage 1.1 — IOMMU + ATS (see openspec/changes/stage-1-1-iommu-ats/)
    # All IOMMU tests moved to CATCH2_SIM_TESTS: ADR-063 D4 ①->③ bridge
    # (default_flush_iotlb -> sim_pm_invalidate) requires gpu_sim link.
    # Stage 1.2 — VFS path resolution (see openspec/changes/stage-1-2-drm-subset/)
    test_vfs_path_standalone.cpp
    # Stage 1.2 — DRM GEM lifecycle (TDD) — see openspec/changes/stage-1-2-drm-subset/spec.md §"1.2/1.3 边界契约 G1"
    test_drm_gem_standalone.cpp
    test_drm_ioctl_dispatch_standalone.cpp
    test_render_node_standalone.cpp
    test_uvm_drm_lifecycle_standalone.cpp
    # Stage 1.2 — DRM prime (TDD)
    test_drm_prime_standalone.cpp
    # Stage 1.2 — DRM file + mode + ioctl (TDD)
    test_drm_file_standalone.cpp
    test_drm_file_lifecycle_standalone.cpp
    test_drm_prime_lifecycle_standalone.cpp
    test_drm_mode_config_standalone.cpp
    test_drm_ioctl_standalone.cpp
    # Stage 1.2 — KFD-compat handler dispatch tests
    test_drm_kfd_handlers_standalone.cpp
    # Stage 1.3 — UVM/HMM PoC (Launch Condition LC2: userfaultfd + mmap)
    poc/test_userfaultfd_poc.cpp
    # Stage 1.3 — UVM/HMM header validation (tasks.md §2)
    test_uvm_headers_standalone.cpp
    # Stage 1.3 — UVM/HMM mmu_notifier (tasks.md §3.1 + §8.1 + §10.1)
    test_mmu_notifier_standalone.cpp
    # H4 + M5 + M6 — mm_shim init/vma/find/foreach unit tests
    test_mm_shim_standalone.cpp
    # Stage 1.3 — UVM/HMM hmm_range (tasks.md §3.2 + §10.2)
    test_hmm_range_standalone.cpp
    # Stage 1.3 — UVM/HMM migrate + fault_inject (tasks.md §3.3 + §3.4)
    test_migrate_standalone.cpp
    test_fault_inject_standalone.cpp
    # Stage 1.3 — UVM/HMM zone_device + page_state_machine
    test_page_state_machine_standalone.cpp
    test_zone_device_standalone.cpp
    # Stage 1.3 — UVM/HMM SVM ioctl integration test (tasks.md §10.3)
    test_svm_ioctl_standalone.cpp
    # C-12 B.1.10 — Thread infrastructure PoC (per ADR-060)
    test_kfd_threading_standalone.cpp
)

# 定义 sim 相关测试的列表（链接 gpu_sim）
set(SIM_TESTS
    test_gpu_ringbuffer.cpp
    test_queue_puller_integration.cpp
    test_hardware_puller_emu.cpp
    test_global_scheduler.cpp
    test_gpfifo_translator.cpp
    test_gpu_callback_integration.cpp
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

# Catch2 sim tests (Catch2 + link kernel + gpu_sim + gpu_drv)
set(CATCH2_SIM_TESTS
    # Stage 1.1 IOMMU tests moved here from CATCH2_TESTS: ADR-063 D4
    # default_flush_iotlb -> sim_pm_invalidate 1->3 bridge requires
    # gpu_sim link for sim_pm_invalidate symbol resolution.
    test_iommu_emu_standalone.cpp
    test_dma_remap_standalone.cpp
    test_ats_protocol_standalone.cpp
    # Stage 1.3 — UVM/HMM sim layer (tasks.md §4)
    test_page_fault_handler_standalone.cpp
    test_page_migration_standalone.cpp
    # Stage 1.4 — Tier-1 runtime penetration tests (B.2/B.3)
    # Stage 1.4 — Tier-2 runtime penetration tests (STUB_HANDLER upgrades)
    test_register_mmu_cb_runtime_standalone.cpp
    test_register_firmware_cb_runtime_standalone.cpp
    test_stub_handlers_tier2_standalone.cpp
    test_mmu_notifier_callback_runtime_standalone.cpp
    test_iommu_invalidate_runtime_standalone.cpp
    test_map_memory_runtime_standalone.cpp
    test_unmap_memory_runtime_standalone.cpp
    test_update_queue_runtime_standalone.cpp
    test_get_process_aperture_runtime_standalone.cpp
    test_migration_e2e_standalone.cpp
    # Stage 2.2.1 — Network socket + sk_buff compat (per ADR-038 D1/D2)
    test_socket_skbuff_standalone.cpp
    # sim-stream-primitive-support — §5.1 Phase 3.1/3.2 sim primitives
    test_sim_stream_capture_standalone.cpp
    test_sim_graph_standalone.cpp
    test_sim_mem_pool_standalone.cpp
    # sim-stream-primitive-support — §5.6 fence_id lifecycle
    test_fence_id_lifecycle_standalone.cpp
    # sim-stream-primitive-support — §5.3 KFD IOCTL dispatch verification (compile-time)
    test_kfd_portability_phase31_standalone.cpp
    # Issue #21 regression lock-in tests
    test_iommu_priv_contract_regression_standalone.cpp
    test_hardware_puller_emu_concurrent_regression_standalone.cpp
    # Phase 3.1+3.2 — test_gpu_plugin extended with 18 new IOCTL dispatch cases
    test_gpu_plugin.cpp
    # Phase 3.1+3.2 — cross-repo integration test (GpuDriverClient-style ioctl chain)
    test_gpu_driver_client_phase31_standalone.cpp
    # Phase 3.3 — error injection via HAL mock (links gpu_hal_mock for controlled failures)
    test_error_inject_standalone.cpp
    # R-3 — HAL thread-safety smoke test via TSan
    test_hal_thread_safety_standalone.cpp
)

# Stage 2.2.4 — net driver plugin test (per ADR-038 D1)
set(CATCH2_NET_TESTS
    test_net_driver_standalone.cpp
)
foreach(src ${CATCH2_NET_TESTS})
    get_filename_component(testname ${src} NAME_WE)
    add_catch_net_test(${testname} ${src})
endforeach()

# Stage 2.3.4 — storage driver test (links kernel + storage_drv)
function(add_catch_storage_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE} ${CATCH_INCLUDE_DIR}/catch_amalgamated.cpp)
    target_link_libraries(${TEST_NAME} PRIVATE kernel storage_drv)
    target_include_directories(${TEST_NAME} PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/kernel
        ${PROJECT_SOURCE_DIR}/include/linux_compat
        ${PROJECT_SOURCE_DIR}/drivers
        ${PROJECT_SOURCE_DIR}/plugins
        ${PROJECT_SOURCE_DIR}/plugins/storage_driver/drv
        ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME ${TEST_NAME} COMMAND $<TARGET_FILE:${TEST_NAME}>)
    set_tests_properties(${TEST_NAME} PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
endfunction()

set(CATCH2_STORAGE_TESTS
    test_storage_driver_standalone.cpp
)
foreach(src ${CATCH2_STORAGE_TESTS})
    get_filename_component(testname ${src} NAME_WE)
    add_catch_storage_test(${testname} ${src})
endforeach()
foreach(src ${CATCH2_SIM_TESTS})
    get_filename_component(testname ${src} NAME_WE)
    add_catch_sim_test(${testname} ${src})
endforeach()

# test_error_inject_standalone needs gpu_hal_mock for HAL failure injection
target_link_libraries(test_error_inject_standalone PRIVATE gpu_hal_mock)

# R-3 — test_hal_thread_safety_standalone links gpu_hal + gpu_hal_mock
# TSan instrumentation is applied globally via add_compile_options(-fsanitize=thread) below
target_link_libraries(test_hal_thread_safety_standalone PRIVATE gpu_hal gpu_hal_mock)
find_package(Threads REQUIRED)
target_link_libraries(test_hal_thread_safety_standalone PRIVATE ${CMAKE_THREAD_LIBS_INIT})

# 为每个 sim 测试源文件创建测试
foreach(src ${SIM_TESTS})
    get_filename_component(testname ${src} NAME_WE)
    string(APPEND testname "_standalone")
    add_sim_test(${testname} ${src})
endforeach()

# Stage 3.2 — performance benchmarks (opt-in via -DUSR_LINUX_EMU_PERF_TESTS=ON)
# 不通过 ctest 运行，手动跑（夜测 CI 走 .github/workflows/perf-nightly.yml）。
if(USR_LINUX_EMU_PERF_TESTS)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/perf)
endif()

# C-12 B.1.10.7 — Apply TSan to kernel threading test when ENABLE_TSAN=ON
# 仅作用于 test_kfd_threading_standalone（其他测试不需要 TSan 监控线程）
if(ENABLE_TSAN)
    # Apply TSan globally to all test targets (simple, single-point control)
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer -g -O1)
    add_link_options(-fsanitize=thread)
    message(STATUS "[TSan] ThreadSanitizer enabled globally (ENABLE_TSAN=ON)")

    # C-12 B.1.10.7 — kernel threading test (originally per-test, now covered by global flags)
    message(STATUS "[C-12 B.1.10.7] TSan active for test_kfd_threading_standalone")
    # R-3 — HAL thread-safety smoke test (originally per-test, now covered by global flags)
    message(STATUS "[R-3] TSan active for test_hal_thread_safety_standalone")
endif()

# C-12 B.1.5 — kfd_process standalone test (pure C impl, isolated)
add_executable(test_kfd_process_standalone
    test_kfd_process_standalone.cpp
    ${CATCH_INCLUDE_DIR}/catch_amalgamated.cpp
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd/kfd_process.c
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd/kfd_pasid.c
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd/kfd_svm.c
)
target_link_libraries(test_kfd_process_standalone PRIVATE kernel)
target_include_directories(test_kfd_process_standalone PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/linux_compat
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_options(test_kfd_process_standalone PRIVATE -Wno-implicit-function-declaration)
add_test(NAME test_kfd_process_standalone COMMAND test_kfd_process_standalone)
set_tests_properties(test_kfd_process_standalone PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

# C-12 C.2.3 — kfd_concurrent_processes standalone test (multi-thread PID isolation via mm_shim)
add_executable(test_kfd_concurrent_processes_standalone
    test_kfd_concurrent_processes_standalone.cpp
    ${CATCH_INCLUDE_DIR}/catch_amalgamated.cpp
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd/kfd_process.c
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd/kfd_pasid.c
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd/kfd_svm.c
)
target_link_libraries(test_kfd_concurrent_processes_standalone PRIVATE kernel ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(test_kfd_concurrent_processes_standalone PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/linux_compat
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_options(test_kfd_concurrent_processes_standalone PRIVATE -Wno-implicit-function-declaration)
add_test(NAME test_kfd_concurrent_processes_standalone COMMAND test_kfd_concurrent_processes_standalone)
set_tests_properties(test_kfd_concurrent_processes_standalone PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

# C-12 B.1.3 — PASID allocator standalone test (links kfd_pasid.c directly)
add_executable(test_kfd_pasid_standalone
    test_kfd_pasid_standalone.cpp
    ${CATCH_INCLUDE_DIR}/catch_amalgamated.cpp
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd/kfd_pasid.c
)
target_link_libraries(test_kfd_pasid_standalone PRIVATE kernel)
target_include_directories(test_kfd_pasid_standalone PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/linux_compat
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_options(test_kfd_pasid_standalone PRIVATE -Wno-implicit-function-declaration)
add_test(NAME test_kfd_pasid_standalone COMMAND test_kfd_pasid_standalone)
set_tests_properties(test_kfd_pasid_standalone PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

# C-12 B.4.4 — sim_event standalone test (links sim_event.c + kfd_types)
add_executable(test_sim_event_standalone
    test_sim_event_standalone.cpp
    ${CATCH_INCLUDE_DIR}/catch_amalgamated.cpp
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/sim/sim_event.c
)
set_source_files_properties(
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/sim/sim_event.c
    PROPERTIES LANGUAGE C
)
target_link_libraries(test_sim_event_standalone PRIVATE kernel)
target_include_directories(test_sim_event_standalone PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_options(test_sim_event_standalone PRIVATE -Wno-implicit-function-declaration)
add_test(NAME test_sim_event_standalone COMMAND test_sim_event_standalone)
set_tests_properties(test_sim_event_standalone PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

# C-12 B.4.4 + B.3.4 — HAL event_signal async integration test
# Links: kfd_events.c (Agent A), sim_event.c, hal_mock.cpp
add_executable(test_hal_event_signal_standalone
    test_hal_event_signal_standalone.cpp
    ${CATCH_INCLUDE_DIR}/catch_amalgamated.cpp
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/sim/sim_event.c
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd/kfd_events.c
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/hal/hal_mock.cpp
)
set_source_files_properties(
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/sim/sim_event.c
    PROPERTIES LANGUAGE C
)
target_link_libraries(test_hal_event_signal_standalone PRIVATE kernel gpu_sim)
target_include_directories(test_hal_event_signal_standalone PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/kernel
    ${PROJECT_SOURCE_DIR}/include/kernel/thread
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/hal
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_options(test_hal_event_signal_standalone PRIVATE -Wno-implicit-function-declaration)
add_test(NAME test_hal_event_signal_standalone COMMAND test_hal_event_signal_standalone)
set_tests_properties(test_hal_event_signal_standalone PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

# C-12 B.2.1 — KFD dispatch table standalone test (links kfd_dispatch.c directly)
add_executable(test_kfd_dispatch_standalone
    test_kfd_dispatch_standalone.cpp
    ${CATCH_INCLUDE_DIR}/catch_amalgamated.cpp
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd/kfd_dispatch.c
)
target_link_libraries(test_kfd_dispatch_standalone PRIVATE kernel)
target_include_directories(test_kfd_dispatch_standalone PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/linux_compat
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_options(test_kfd_dispatch_standalone PRIVATE -Wno-implicit-function-declaration)
add_test(NAME test_kfd_dispatch_standalone COMMAND test_kfd_dispatch_standalone)
set_tests_properties(test_kfd_dispatch_standalone PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

# C-12 E.0.1 — KFD end-to-end: 5 ioctls (dispatch + bridge)
add_executable(test_kfd_end_to_end_standalone
    test_kfd_end_to_end_standalone.cpp
    ${CATCH_INCLUDE_DIR}/catch_amalgamated.cpp
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd/kfd_dispatch.c
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd_sim_bridge.cpp
)
target_link_libraries(test_kfd_end_to_end_standalone PRIVATE kernel gpu_sim)
target_include_directories(test_kfd_end_to_end_standalone PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/linux_compat
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/sim
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/hal
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/shared
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_options(test_kfd_end_to_end_standalone PRIVATE -Wno-implicit-function-declaration)
add_test(NAME test_kfd_end_to_end_standalone COMMAND test_kfd_end_to_end_standalone)
set_tests_properties(test_kfd_end_to_end_standalone PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

# C-12 E.0.2 — KFD page fault → event signal → sim chain
add_executable(test_kfd_fault_handling_standalone
    test_kfd_fault_handling_standalone.cpp
    ${CATCH_INCLUDE_DIR}/catch_amalgamated.cpp
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd/kfd_events.c
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/sim/sim_event.c
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/sim/page_fault_handler.cpp
)
target_link_libraries(test_kfd_fault_handling_standalone PRIVATE kernel gpu_sim)
target_include_directories(test_kfd_fault_handling_standalone PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/linux_compat
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/sim
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/hal
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/shared
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_options(test_kfd_fault_handling_standalone PRIVATE -Wno-implicit-function-declaration)
add_test(NAME test_kfd_fault_handling_standalone COMMAND test_kfd_fault_handling_standalone)
set_tests_properties(test_kfd_fault_handling_standalone PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

# C-12 E.2.4.1 — KFD L1↔L2 bridge skeleton (cross-repo E2E; full sync per ADR-035 §Rule 5.1)
add_executable(test_kfd_l1_l2_bridge_standalone
    test_kfd_l1_l2_bridge_standalone.cpp
    ${CATCH_INCLUDE_DIR}/catch_amalgamated.cpp
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd/kfd_dispatch.c
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd_sim_bridge.cpp
)
target_link_libraries(test_kfd_l1_l2_bridge_standalone PRIVATE kernel gpu_sim)
target_include_directories(test_kfd_l1_l2_bridge_standalone PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/linux_compat
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/hal
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/shared
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_options(test_kfd_l1_l2_bridge_standalone PRIVATE -Wno-implicit-function-declaration)
add_test(NAME test_kfd_l1_l2_bridge_standalone COMMAND test_kfd_l1_l2_bridge_standalone)
set_tests_properties(test_kfd_l1_l2_bridge_standalone PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

# C-12 B.1.11 — kfd_module standalone test (lifecycle + idempotency)
add_executable(test_kfd_module_standalone
    test_kfd_module_standalone.cpp
    ${CATCH_INCLUDE_DIR}/catch_amalgamated.cpp
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd/kfd_module.c
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd/kfd_topology.c
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd/kfd_svm.c
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd/kfd_pasid.c
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd/kfd_process.c
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd/kfd_dispatch.c
)
target_link_libraries(test_kfd_module_standalone PRIVATE kernel)
target_include_directories(test_kfd_module_standalone PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/linux_compat
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_options(test_kfd_module_standalone PRIVATE -Wno-implicit-function-declaration)
add_test(NAME test_kfd_module_standalone COMMAND test_kfd_module_standalone)
set_tests_properties(test_kfd_module_standalone PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

# C-12 B.3.6 — kfd_mmu standalone test (init/exit/map/unmap/get_workqueue)
add_executable(test_kfd_mmu_standalone
    test_kfd_mmu_standalone.cpp
    ${CATCH_INCLUDE_DIR}/catch_amalgamated.cpp
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd/kfd_mmu.c
    # NO hal_mock.cpp yet (B.3.4 is Agent B)
)
# kfd_mmu.c uses kernel_workqueue (C++ class) → compile as C++
set_source_files_properties(
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd/kfd_mmu.c
    PROPERTIES LANGUAGE CXX
)
target_link_libraries(test_kfd_mmu_standalone PRIVATE kernel)
target_include_directories(test_kfd_mmu_standalone PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/linux_compat
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/hal
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/shared
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_options(test_kfd_mmu_standalone PRIVATE -Wno-implicit-function-declaration)
add_test(NAME test_kfd_mmu_standalone COMMAND test_kfd_mmu_standalone)
set_tests_properties(test_kfd_mmu_standalone PROPERTIES
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    ENVIRONMENT "TSAN_OPTIONS=report_signal_unsafe=0"
)

# C-12 B.4.5 — kfd_events standalone test (init/exit/signal/get_workqueue)
add_executable(test_kfd_events_standalone
    test_kfd_events_standalone.cpp
    ${CATCH_INCLUDE_DIR}/catch_amalgamated.cpp
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd/kfd_events.c
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/sim/sim_event.c  # B.4.3: kfd_events.c lambda calls sim_signal_event()
)
# kfd_events.c uses kernel_workqueue (C++ class) → compile as C++
set_source_files_properties(
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd/kfd_events.c
    PROPERTIES LANGUAGE CXX
)
target_link_libraries(test_kfd_events_standalone PRIVATE kernel)
target_include_directories(test_kfd_events_standalone PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/linux_compat
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/hal
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/shared
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/sim  # B.4.3: sim_signal_event() location
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_options(test_kfd_events_standalone PRIVATE -Wno-implicit-function-declaration)
add_test(NAME test_kfd_events_standalone COMMAND test_kfd_events_standalone)
set_tests_properties(test_kfd_events_standalone PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

# C-12 B.3.3 + B.3.5 — kfd_sim_bridge audit + set_hal + sim_pm_* integration test
add_executable(test_kfd_sim_bridge_audit_standalone
    test_kfd_sim_bridge_audit_standalone.cpp
    ${CATCH_INCLUDE_DIR}/catch_amalgamated.cpp
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd_sim_bridge.cpp
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/sim/page_migration.cpp
)
target_link_libraries(test_kfd_sim_bridge_audit_standalone PRIVATE kernel gpu_sim)
target_include_directories(test_kfd_sim_bridge_audit_standalone PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/linux_compat
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/sim
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/shared
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_options(test_kfd_sim_bridge_audit_standalone PRIVATE -Wno-implicit-function-declaration)
target_compile_definitions(test_kfd_sim_bridge_audit_standalone PRIVATE
    PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
add_test(NAME test_kfd_sim_bridge_audit_standalone COMMAND test_kfd_sim_bridge_audit_standalone)
set_tests_properties(test_kfd_sim_bridge_audit_standalone PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

# C-12 B.4.6.6 — TSan hardening for kfd_events async path
# Exercises concurrent producers, drain contract, init/exit ordering, atomic counters.
# TSan instrumentation applied globally via add_compile_options(-fsanitize=thread) above.
add_executable(test_kfd_events_tsan_standalone
    test_kfd_events_tsan_standalone.cpp
    ${CATCH_INCLUDE_DIR}/catch_amalgamated.cpp
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/sim/sim_event.c
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/sim/page_migration.cpp
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd/kfd_events.c
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/hal/hal_mock.cpp
)
set_source_files_properties(
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/sim/sim_event.c
    PROPERTIES LANGUAGE C
)
# kfd_events.c uses kernel_workqueue (C++ class) → compile as C++
set_source_files_properties(
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd/kfd_events.c
    PROPERTIES LANGUAGE CXX
)
target_link_libraries(test_kfd_events_tsan_standalone PRIVATE kernel)
target_include_directories(test_kfd_events_tsan_standalone PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/kernel
    ${PROJECT_SOURCE_DIR}/include/kernel/thread
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/hal
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_options(test_kfd_events_tsan_standalone PRIVATE -Wno-implicit-function-declaration)
add_test(NAME test_kfd_events_tsan_standalone COMMAND test_kfd_events_tsan_standalone)
set_tests_properties(test_kfd_events_tsan_standalone PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

# C-12 Phase D — kfd_queue FIXME cleanup verification
# Verifies: kfd_queue_unref_bo_vas_locked exists, kfd_queue_buffer_put is removed.
add_executable(test_kfd_queue_standalone
    test_kfd_queue_standalone.cpp
    ${CATCH_INCLUDE_DIR}/catch_amalgamated.cpp
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd/kfd_queue.c
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd/kfd_topology.c
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd/kfd_svm.c
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd/kfd_pasid.c
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd/kfd_process.c
)
target_link_libraries(test_kfd_queue_standalone PRIVATE kernel)
target_include_directories(test_kfd_queue_standalone PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/linux_compat
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_options(test_kfd_queue_standalone PRIVATE -Wno-implicit-function-declaration)
# TSan: when ENABLE_TSAN=ON, C files compiled with -fsanitize=thread need TSan runtime at link
if(ENABLE_TSAN)
    target_link_libraries(test_kfd_queue_standalone PRIVATE -fsanitize=thread)
endif()
add_test(NAME test_kfd_queue_standalone COMMAND test_kfd_queue_standalone)
set_tests_properties(test_kfd_queue_standalone PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})