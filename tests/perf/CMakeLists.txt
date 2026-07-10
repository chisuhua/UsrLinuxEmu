# Stage 3.2 — performance benchmarks (C-10)
# 包含由父 CMakeLists.txt 的 `option(USR_LINUX_EMU_PERF_TESTS "..." OFF)` 守护；
# 仅在 -DUSR_LINUX_EMU_PERF_TESTS=ON 时此子目录才被 add_subdirectory 引入。
#
# 模式对齐 tests/CMakeLists.txt 的 add_catch_sim_test：
#   - 链 kernel + gpu_sim + gpu_drv + gpu_hal_mock
#   - 输出 build/bin/<name>_bench_standalone
#   - 不调 add_test（手工跑，不参与 ctest）

function(add_perf_bench BENCH_NAME BENCH_SOURCE)
    add_executable(${BENCH_NAME} ${BENCH_SOURCE} ${CMAKE_CURRENT_SOURCE_DIR}/../catch_amalgamated.cpp)
    target_link_libraries(${BENCH_NAME} PRIVATE kernel gpu_sim gpu_drv gpu_hal_mock)
    target_include_directories(${BENCH_NAME} PRIVATE
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
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..)
    set_target_properties(${BENCH_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
endfunction()

# Build all benchmark binaries (gather list locally)
set(PERF_BENCHMARKS
    ioctl_dispatch_bench.cpp
    pushbuffer_bench.cpp
    mmap_overhead_bench.cpp
)

foreach(src ${PERF_BENCHMARKS})
    get_filename_component(benchname ${src} NAME_WE)
    string(APPEND benchname "_standalone")
    add_perf_bench(${benchname} ${src})
endforeach()

# 提供 `make perf-tests` 目标：构建所有 benchmark 二进制
add_custom_target(perf-tests
    DEPENDS
        ioctl_dispatch_bench_standalone
        pushbuffer_bench_standalone
        mmap_overhead_bench_standalone
    COMMENT "Built all perf benchmarks; run from project root: ./build/bin/<bench>_standalone")

# 提供 `make run-perf` 便捷目标：依次串行跑三个 bench 并实时输出
add_custom_target(run-perf
    COMMAND ${PROJECT_SOURCE_DIR}/build/bin/ioctl_dispatch_bench_standalone
    COMMAND ${PROJECT_SOURCE_DIR}/build/bin/pushbuffer_bench_standalone
    COMMAND ${PROJECT_SOURCE_DIR}/build/bin/mmap_overhead_bench_standalone
    COMMENT "Running all three perf benchmarks in sequence"
    USES_TERMINAL)
