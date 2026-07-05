# drv/CMakeLists.txt — ② portable block storage driver code

add_library(storage_drv STATIC
    storage_driver.cpp
)

set_target_properties(storage_drv PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(storage_drv PRIVATE _GNU_SOURCE)

target_include_directories(storage_drv PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/kernel
)

target_link_libraries(storage_drv PUBLIC kernel)