# hal/CMakeLists.txt — 硬件抽象层接口

add_library(gpu_hal STATIC
    hal_user.cpp
)

set_target_properties(gpu_hal PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(gpu_hal PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}  # 让 hal/*.h 可通过 "gpu_hal.h" 引用
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/kernel
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/shared
    ${PROJECT_SOURCE_DIR}/libgpu_core/include
)

target_link_libraries(gpu_hal PUBLIC
    gpu_core
)

# HAL mock 库（仅测试用）
add_library(gpu_hal_mock STATIC
    hal_mock.cpp
)

set_target_properties(gpu_hal_mock PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(gpu_hal_mock PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/include
)

# HAL 接口测试（仅在影子编译启用时）
if(ENABLE_GPU_SHADOW)
  add_executable(test_hal
      test_hal.cpp
  )

  target_link_libraries(test_hal PRIVATE
      gpu_hal_mock
      gpu_hal
      gpu_drv
  )

  target_include_directories(test_hal PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}
      ${PROJECT_SOURCE_DIR}/include
      ${PROJECT_SOURCE_DIR}/plugins/gpu_driver
      ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/shared
  )

  add_test(NAME test_hal COMMAND $<TARGET_FILE:test_hal>)
endif()
