# 构建系统文档

## 构建系统概述

UsrLinuxEmu 项目使用 CMake 作为主要的构建系统，支持跨平台构建和模块化管理。CMake 最低要求版本为 3.14。

## 项目构建结构

### 主 CMakeLists.txt

主构建配置文件位于项目根目录，定义了整个项目的构建规则：

```cmake
cmake_minimum_required(VERSION 3.14)
project(user_kernel_emu)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 输出路径
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

# 包含头文件目录
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/include/kernel)
include_directories(${PROJECT_SOURCE_DIR}/include/kernel/device)
include_directories(${PROJECT_SOURCE_DIR}/drivers)

# 添加子模块
add_subdirectory(src)
add_subdirectory(drivers)
add_subdirectory(test)
add_subdirectory(tools/cli)
```

### 子模块 CMakeLists.txt

项目被划分为多个子模块，每个模块都有自己的 CMakeLists.txt：

- `src/`: 核心框架源码
- `drivers/`: 设备驱动实现
- `test/`: 测试代码
- `tools/cli`: 命令行工具

## 构建选项

### 标准构建选项

- `CMAKE_BUILD_TYPE`: 构建类型（Debug/Release/RelWithDebInfo/MinSizeRel）
- `CMAKE_CXX_STANDARD`: C++ 标准（默认为 17）
- `CMAKE_RUNTIME_OUTPUT_DIRECTORY`: 可执行文件输出目录

### 项目特定选项

- `BUILD_TESTS`: 是否构建测试（默认为 OFF）
- `ENABLE_LOGGING`: 是否启用详细日志（默认为 ON）
- `COVERAGE`: 是否启用代码覆盖率（默认为 OFF）

## 构建步骤

### 基本构建

```bash
# 创建构建目录
mkdir build && cd build

# 配置项目
cmake ..

# 编译项目
make -j$(nproc)

# 或使用 ninja（如果可用）
ninja
```

### 构建测试

```bash
# 配置项目并启用测试
cmake .. -DBUILD_TESTS=ON

# 编译所有内容（包括测试）
make -j$(nproc)

# 运行所有测试
make test
```

### 调试版本构建

```bash
# 构建调试版本
cmake .. -DCMAKE_BUILD_TYPE=Debug

# 或同时启用调试和测试
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
```

### 启用代码覆盖率

```bash
# 配置项目启用覆盖率
cmake .. -DCOVERAGE=ON -DBUILD_TESTS=ON

# 编译
make -j$(nproc)

# 运行测试以收集覆盖率数据
make test

# 生成覆盖率报告（需要 lcov）
lcov --directory . --capture --output-file coverage.info
genhtml coverage.info --output-directory coverage_report
```

## 依赖管理

### 编译依赖

- C++17 兼容的编译器（GCC 7+ 或 Clang 5+）
- CMake 3.14 或更高版本
- 标准 C++ 库
- pthread 库（隐式链接）

### 构建脚本

项目提供了便捷的构建脚本：

```bash
# 使用项目提供的构建脚本
./build.sh
```

脚本内容：
```bash
#!/bin/bash
mkdir -p build
cd build
cmake ..
make -j$(nproc)
```

## 驱动程序构建

### GPU 驱动构建

GPU 驱动模块在 [drivers/gpu/CMakeLists.txt](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/drivers/gpu/CMakeLists.txt) 中定义：

```cmake
# drivers/gpu/CMakeLists.txt
set(GPU_DRIVER_SOURCES
    gpu_driver.cpp
    buddy_allocator.cpp
    ring_buffer.cpp
    address_space.cpp
    plugin_gpu.cpp
)

add_library(gpu_driver ${GPU_DRIVER_SOURCES})

target_include_directories(gpu_driver 
    PUBLIC 
    ${PROJECT_SOURCE_DIR}/drivers/gpu
    PRIVATE 
    ${PROJECT_SOURCE_DIR}/include
)
```

### 插件构建

插件系统支持动态加载，插件构建在 [plugins/](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/plugins) 目录下：

```makefile
# plugins/Makefile 示例
CXX=g++
CXXFLAGS=-std=c++17 -fPIC -shared
INCLUDES=-I../include -I../include/kernel

PLUGINS = gpu_plugin.so

all: $(PLUGINS)

gpu_plugin.so: ../drivers/gpu/plugin_gpu.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $<

clean:
	rm -f $(PLUGINS)
```

## 测试构建系统

### 测试配置

测试构建在 [tests/CMakeLists.txt](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/tests/CMakeLists.txt) 中定义：

```cmake
if(BUILD_TESTS)
    enable_testing()
    
    # 包含测试依赖
    find_package(GTest REQUIRED)
    
    # 定义测试源文件
    set(TEST_SOURCES
        test_gpu_submit.cpp
        test_gpu_memory.cpp
        # ... 其他测试文件
    )
    
    # 为每个测试源文件创建测试
    foreach(test_src ${TEST_SOURCES})
        get_filename_component(test_name ${test_src} NAME_WE)
        add_executable(${test_name} ${test_src})
        target_link_libraries(${test_name} 
            ${PROJECT_LIBRARIES} 
            gtest gtest_main pthread
        )
        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()
endif()
```

## 工具构建

### CLI 工具

命令行工具构建在 [tools/cli/CMakeLists.txt](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/tools/cli/CMakeLists.txt) 中：

```cmake
add_executable(cli_tool main.cpp)

target_link_libraries(cli_tool 
    ${PROJECT_LIBRARIES}
)

set_target_properties(cli_tool PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/tools
)
```

## 构建输出

### 输出目录结构

构建完成后，输出文件位于 `build/bin/` 目录：

```
build/bin/
├── cli_tool              # CLI 工具
├── test_gpu_submit       # GPU 提交测试
├── test_plugin           # 插件测试
├── test_gpu_memory       # GPU 内存测试
└── ...                   # 其他测试和工具
```

### 运行脚本

项目提供运行脚本：

```bash
# 运行 CLI 工具
./run_cli.sh
```

脚本内容：
```bash
#!/bin/bash
./build/bin/cli_tool
```

## 自定义构建配置

### 添加新模块

要添加新模块，需要：

1. 在项目目录创建新模块
2. 在新模块目录创建 CMakeLists.txt
3. 在主 CMakeLists.txt 中添加 `add_subdirectory(新模块名)`

### 添加新驱动

添加新驱动需要：

1. 在 [drivers/](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/drivers) 目录创建新驱动目录
2. 实现驱动代码和头文件
3. 创建对应的 CMakeLists.txt
4. 修改主 CMakeLists.txt 或 [drivers/CMakeLists.txt](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/drivers/CMakeLists.txt) 包含新驱动

### 构建变体

#### Debug 构建
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
```

特点：
- 启用调试符号
- 禁用优化
- 启用额外的运行时检查

#### Release 构建
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release
```

特点：
- 启用优化
- 不包含调试符号
- 最小化大小或最大化性能

#### RelWithDebInfo 构建
```bash
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo
```

特点：
- 启用优化
- 包含调试符号
- 平衡性能和调试能力