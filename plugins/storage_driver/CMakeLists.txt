cmake_minimum_required(VERSION 3.14)

# Stage 2.3 — Storage device plugin (per plan §2.3.2)
add_library(storage_driver_plugin MODULE
    drv/plugin.cpp
)

target_include_directories(storage_driver_plugin PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/kernel
    ${PROJECT_SOURCE_DIR}/plugins/storage_driver/drv
)

target_link_libraries(storage_driver_plugin PRIVATE storage_drv kernel)

add_subdirectory(drv)
add_subdirectory(sim)