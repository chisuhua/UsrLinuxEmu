# CLI工具可执行文件
add_executable(cli
    main.cpp
)

# ADR-063 D4: kernel (SHARED) intentionally references sim_pm_invalidate
# from gpu_sim (STATIC, in plugins/).  cli does not load gpu_sim at link
# time, but runtime guards (flushed_entries && state->sim_pm) in
# default_flush_iotlb prevent calling the unresolved symbol when no
# sim_pm has been attached via iommu_domain_attach_sim_pm.
target_link_options(cli PRIVATE "-Wl,--unresolved-symbols=ignore-in-shared-libs")

target_link_libraries(cli PRIVATE kernel)
target_include_directories(cli PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/kernel
    ${PROJECT_SOURCE_DIR}/include/linux_compat)