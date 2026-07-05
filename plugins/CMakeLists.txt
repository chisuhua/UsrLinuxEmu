cmake_minimum_required(VERSION 3.14)

add_subdirectory(gpu_driver)
# Stage 2.2 — Network device plugin (per ADR-038)
add_subdirectory(net_driver)
# Stage 2.3 — Storage device plugin (per plan §2.3.2)
add_subdirectory(storage_driver)