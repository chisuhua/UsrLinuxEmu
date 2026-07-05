cmake_minimum_required(VERSION 3.14)

# Stage 2.2 network driver plugin (per ADR-038)
add_library(net_driver_plugin MODULE
    drv/plugin.cpp
)

target_include_directories(net_driver_plugin PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/kernel
    ${PROJECT_SOURCE_DIR}/plugins/net_driver/drv
)

target_link_libraries(net_driver_plugin PRIVATE net_drv kernel)

# drv/ always compiled (net_drv static lib)
add_subdirectory(drv)
# sim/ header-only loopback NIC (no .cpp target needed for Stage 2.2.3)
add_subdirectory(sim)