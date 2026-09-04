cmake_minimum_required(VERSION 3.14)

# sim_hardware: PC 系统硬件仿真（Q3 象限，per ADR-091 v0.2）
# Tier 1: Software Bypass（HostBypassTLM）
# Tier 2: Root Complex Mirror（PcieRootComplexTLM）
# Tier 3-8: Link Layer / PHY / SR-IOV / Completion / AXI 在 Change-3..5 实施

# Wave 1: API contract remains INTERFACE; mock backend is a self-contained STATIC library.
add_library(sim_hardware INTERFACE)

target_include_directories(sim_hardware INTERFACE
    ${PROJECT_SOURCE_DIR}/sim_hardware/include
    ${PROJECT_SOURCE_DIR}/sim_hardware/include/pcie
    ${PROJECT_SOURCE_DIR}/sim_hardware/include/cpptlm
    ${PROJECT_SOURCE_DIR}/sim_hardware/include/chipset
    ${PROJECT_SOURCE_DIR}/sim_hardware/include/topology
)

target_compile_features(sim_hardware INTERFACE cxx_std_17)

add_library(sim_hardware_mock STATIC
    src/cpptlm/bridge.cpp
    src/cpptlm/endpoint.cpp
    src/pcie/bypass.cpp
    src/pcie/host_bridge.cpp
    src/topology.cpp
    src/platform.cpp
)

target_link_libraries(sim_hardware_mock PUBLIC sim_hardware)
target_link_options(sim_hardware_mock PRIVATE "-Wl,--no-undefined")
set_target_properties(sim_hardware_mock PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(sim_hardware_mock PUBLIC
    ${PROJECT_SOURCE_DIR}/sim_hardware/include
    ${PROJECT_SOURCE_DIR}/sim_hardware/include/pcie
    ${PROJECT_SOURCE_DIR}/sim_hardware/include/cpptlm
    ${PROJECT_SOURCE_DIR}/sim_hardware/include/chipset
    ${PROJECT_SOURCE_DIR}/sim_hardware/include/topology
    ${PROJECT_SOURCE_DIR}/external/json  # Wave 2: vendored nlohmann/json (header-only)
)
target_compile_features(sim_hardware_mock PUBLIC cxx_std_17)

# The mock backend intentionally has no CppTLM dependency.
