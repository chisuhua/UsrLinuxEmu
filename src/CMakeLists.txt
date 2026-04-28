add_library(kernel 
    kernel/types.cpp
    kernel/module.cpp
    kernel/vfs.cpp
    kernel/file_ops.cpp
    kernel/wait_queue.cpp
    kernel/plugin_manager.cpp
    kernel/module_loader.cpp
    kernel/config_manager.cpp
    kernel/service_registry.cpp
    kernel/logger.cpp
    kernel/device.cpp
    kernel/poll_watcher.cpp
    kernel/device/serial_device.cpp
    kernel/device/memory_device.cpp
    kernel/device/gpgpu_device.cpp
    kernel/device/cuda_compat_ioctl.cpp
)

# 添加 TaskRunner 头文件路径（用于 CudaStub）
include_directories(${PROJECT_SOURCE_DIR}/../TaskRunner/include)
include_directories(${PROJECT_SOURCE_DIR}/include)

target_link_libraries(kernel PRIVATE dl pthread)
set_target_properties(kernel PROPERTIES POSITION_INDEPENDENT_CODE ON)