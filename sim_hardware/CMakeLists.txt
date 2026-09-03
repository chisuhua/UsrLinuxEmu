cmake_minimum_required(VERSION 3.14)

# sim_hardware: PC 系统硬件仿真（Q3 象限，per ADR-091 v0.2）
# Tier 1: Software Bypass（HostBypassTLM）
# Tier 2: Root Complex Mirror（PcieRootComplexTLM）
# Tier 3-8: Link Layer / PHY / SR-IOV / Completion / AXI 在 Change-3..5 实施

# Wave 1C: 创建 INTERFACE 库骨架。Tier 1+2 实际实现在 Change-2 完成。
add_library(sim_hardware INTERFACE)

target_include_directories(sim_hardware INTERFACE
    ${PROJECT_SOURCE_DIR}/sim_hardware/include
    ${PROJECT_SOURCE_DIR}/sim_hardware/include/pcie
    ${PROJECT_SOURCE_DIR}/sim_hardware/include/cpptlm
    ${PROJECT_SOURCE_DIR}/sim_hardware/include/chipset
    ${PROJECT_SOURCE_DIR}/sim_hardware/include/topology
)

target_compile_features(sim_hardware INTERFACE cxx_std_17)

# CppTLM 静态库依赖（从 Change-2 开始链接；Wave 1C 占位不实际链接）
# target_link_libraries(sim_hardware INTERFACE cpptlm_core)
# 注：cpptlm_core target 由 Change-2 创建；当前 Wave 1C 不链接