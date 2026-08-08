# kernel/process/ — Per-process GPU resource isolation (Phase 1)
target_sources(kernel PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/isolated_device.cpp
)
target_include_directories(kernel PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
