#add_library(sample_memory STATIC sample_memory.cpp)
#add_library(sample_serial STATIC sample_serial.cpp)


add_library(plugin_sample_memory MODULE sample_memory.cpp
                sample_memory_plugin.cpp
)
add_library(plugin_sample_serial MODULE sample_serial.cpp
                ../src/kernel/device/serial_device.cpp
)

set_target_properties(plugin_sample_memory PROPERTIES PREFIX "" SUFFIX ".so")
set_target_properties(plugin_sample_serial PROPERTIES PREFIX "" SUFFIX ".so")
set_target_properties(plugin_sample_memory PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(plugin_sample_serial PROPERTIES POSITION_INDEPENDENT_CODE ON)

#target_include_directories(sample_memory PRIVATE ${PROJECT_SOURCE_DIR}/include/kernel)
#arget_include_directories(sample_serial PRIVATE ${PROJECT_SOURCE_DIR}/include/kernel)

#arget_link_libraries(sample_memory PRIVATE kernel)
#arget_link_libraries(sample_serial PRIVATE kernel)
target_link_libraries(plugin_sample_memory PRIVATE kernel)
target_link_libraries(plugin_sample_serial PRIVATE kernel)


add_library(plugin_gpu MODULE
    sample_gpu.cpp
    plugin_sample_gpu.cpp
    ../src/kernel/device/buddy_allocator.cpp
)

set_target_properties(plugin_gpu PROPERTIES PREFIX "" SUFFIX ".so")
target_link_libraries(plugin_gpu PRIVATE kernel)
add_subdirectory(gpu)
