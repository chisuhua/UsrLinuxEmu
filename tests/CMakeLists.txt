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

# Define Catch2 sim test function (links kernel + gpu_sim)
function(add_catch_sim_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE} ${CATCH_INCLUDE_DIR}/catch_amalgamated.cpp)
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
    test_gpu_plugin.cpp
    test_module_loader_isolation.cpp
    test_pcie_emu_standalone.cpp
    test_config_space_standalone.cpp
    test_msi_x_inject_standalone.cpp
    # Stage 1.1 — IOMMU + ATS (see openspec/changes/stage-1-1-iommu-ats/)
    test_iommu_emu_standalone.cpp
    test_dma_remap_standalone.cpp
    test_ats_protocol_standalone.cpp
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

# Catch2 sim tests (Catch2 + link kernel + gpu_sim)
set(CATCH2_SIM_TESTS
    # Stage 1.3 — UVM/HMM sim layer (tasks.md §4)
    test_page_fault_handler_standalone.cpp
    test_page_migration_standalone.cpp
)
foreach(src ${CATCH2_SIM_TESTS})
    get_filename_component(testname ${src} NAME_WE)
    add_catch_sim_test(${testname} ${src})
endforeach()

# 为每个 sim 测试源文件创建测试
foreach(src ${SIM_TESTS})
    get_filename_component(testname ${src} NAME_WE)
    string(APPEND testname "_standalone")
    add_sim_test(${testname} ${src})
endforeach()