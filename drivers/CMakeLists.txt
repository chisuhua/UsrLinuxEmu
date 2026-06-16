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

target_include_directories(plugin_sample_memory PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_include_directories(plugin_sample_serial PRIVATE ${PROJECT_SOURCE_DIR}/include)

#target_include_directories(sample_memory PRIVATE ${PROJECT_SOURCE_DIR}/include/kernel)
#arget_include_directories(sample_serial PRIVATE ${PROJECT_SOURCE_DIR}/include/kernel)

#arget_link_libraries(sample_memory PRIVATE kernel)
#arget_link_libraries(sample_serial PRIVATE kernel)
target_link_libraries(plugin_sample_memory PRIVATE kernel)
target_link_libraries(plugin_sample_serial PRIVATE kernel)

# 强制构建 sample 插件,即使没有其他 target 链接它们
# (这些是 MODULE 库,通过 dlopen 加载,不会被自动构建)
add_custom_target(sample_plugins ALL
    DEPENDS plugin_sample_memory plugin_sample_serial)