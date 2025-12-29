add_library(gpu_driver STATIC
    gpu_driver.cpp
)

add_library(plugin_gpu MODULE
    plugin_gpu.cpp
)

target_link_libraries(plugin_gpu PRIVATE kernel gpu_driver)
set_target_properties(plugin_gpu PROPERTIES PREFIX "" SUFFIX ".so")
set_target_properties(plugin_gpu PROPERTIES POSITION_INDEPENDENT_CODE ON)
