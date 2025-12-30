# 开发指南

## 环境搭建

### 系统要求
- Linux 环境（推荐 Ubuntu 18.04 或更高版本）
- CMake ≥ 3.14
- GCC 或 Clang 支持 C++17

### 安装依赖
```bash
# Ubuntu 系统
sudo apt update
sudo apt install build-essential cmake

# 或使用其他包管理器
# CentOS/RHEL: sudo yum install gcc gcc-c++ cmake
# macOS: brew install cmake
```

### 获取源码
```bash
git clone <repository-url>
cd UsrLinuxEmu
```

### 编译项目
```bash
# 方法1: 使用构建脚本
./build.sh

# 方法2: 手动构建
mkdir build && cd build
cmake ..
make -j$(nproc)

# 构建插件
cd plugins
make
```

## 项目结构详解

### 核心框架 ([include/kernel](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel) 和 [src/kernel](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/src/kernel))

- [types.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/types.h): 基础类型定义
- [device.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/device/device.h): 设备抽象基类
- [file_ops.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/file_ops.h): 文件操作抽象
- [vfs.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/vfs.h): 虚拟文件系统接口
- [logger.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/logger.h): 日志系统
- [plugin_manager.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/plugin_manager.h): 插件管理
- [service_registry.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/service_registry.h): 服务注册中心

### GPU 驱动实现 ([drivers/gpu](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/drivers/gpu))

- [gpu_driver.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/drivers/gpu/gpu_driver.h): GPU 驱动主类
- [buddy_allocator.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/drivers/gpu/buddy_allocator.h): GPU 内存分配器
- [ring_buffer.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/drivers/gpu/ring_buffer.h): GPU 命令环形缓冲区
- [address_space.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/drivers/gpu/address_space.h): GPU 地址空间管理
- [plugin_gpu.cpp](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/drivers/gpu/plugin_gpu.cpp): GPU 设备插件入口

### GPU 模拟器 ([simulator/gpu](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/simulator/gpu))

- [basic_gpu_simulator.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/simulator/gpu/basic_gpu_simulator.h): 基础 GPU 模拟器
- [command_parser.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/simulator/gpu/command_parser.h): GPU 命令解析器

### 测试代码 ([tests](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/tests))

- [test_gpu_submit.cpp](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/tests/test_gpu_submit.cpp): GPU 命令提交测试
- [test_gpu_memory.cpp](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/tests/test_gpu_memory.cpp): GPU 内存管理测试
- [test_plugin.cpp](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/tests/test_plugin.cpp): 插件系统测试

## 添加新设备

### 1. 定义设备类

继承 [device.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/device/device.h) 中的 Device 基类:

```cpp
// example_device.h
#pragma once
#include "kernel/device/device.h"

class ExampleDevice : public Device {
public:
    ExampleDevice();
    virtual ~ExampleDevice() = default;
    
    // 实现设备接口
    int open(int flags) override;
    int close() override;
    int ioctl(unsigned long cmd, void *arg) override;
    void *mmap(void *addr, size_t length, int prot, int flags, off_t offset) override;
    ssize_t write(const void *buf, size_t count) override;
    ssize_t read(void *buf, size_t count) override;
    
private:
    // 设备特定的数据成员
};
```

### 2. 实现设备功能

```cpp
// example_device.cpp
#include "example_device.h"

ExampleDevice::ExampleDevice() {
    // 初始化设备
}

int ExampleDevice::open(int flags) {
    // 实现设备打开逻辑
    return 0;
}

int ExampleDevice::ioctl(unsigned long cmd, void *arg) {
    // 实现设备控制逻辑
    switch(cmd) {
        // 处理不同的命令
        default:
            return -1;
    }
}

// 实现其他接口...
```

### 3. 创建插件

```cpp
// plugin_example.cpp
#include "example_device.h"
#include "kernel/module_loader.h"

static Device* create_example_device() {
    return new ExampleDevice();
}

// 注册设备创建函数
REGISTER_DEVICE_PLUGIN("example", create_example_device);
```

### 4. 注册设备到 VFS

在系统初始化时注册设备:

```cpp
// 在初始化代码中
auto example_dev = std::make_shared<ExampleDevice>();
VFS::register_device("/dev/example", example_dev);
```

## 添加新 IOCTL 命令

### 1. 定义命令码

在 [ioctl.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/ioctl.h) 或设备特定的头文件中定义:

```cpp
#define EXAMPLE_IOCTL_CMD _IOW('x', 1, struct example_cmd_args)
```

### 2. 定义参数结构

```cpp
struct example_cmd_args {
    uint64_t param1;
    uint64_t param2;
    // 其他参数...
};
```

### 3. 在设备的 ioctl 方法中处理

```cpp
int ExampleDevice::ioctl(unsigned long cmd, void *arg) {
    switch(cmd) {
        case EXAMPLE_IOCTL_CMD:
            return handle_example_cmd(arg);
        default:
            return Device::ioctl(cmd, arg);  // 调用父类处理
    }
}
```

## 内存管理

### GPU 内存分配

GPU 使用 Buddy 分配器管理内存，主要接口:

```cpp
// 分配 GPU 内存
uint64_t gpu_addr;
size_t size = 1024 * 1024; // 1MB
if (buddy_allocator->allocate(size, &gpu_addr) == 0) {
    // 分配成功，gpu_addr 是 GPU 物理地址
}
```

### 地址映射

用户空间通过 mmap 将 GPU 内存映射到自己的地址空间:

```cpp
// 用户程序
void *user_ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, gpu_addr);
```

## 测试开发

### 编写单元测试

在 [tests](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/tests) 目录下创建测试文件:

```cpp
// test_example_device.cpp
#include <gtest/gtest.h>
#include "example_device.h"

class ExampleDeviceTest : public ::testing::Test {
protected:
    void SetUp() override {
        device = std::make_unique<ExampleDevice>();
    }
    
    void TearDown() override {
        device.reset();
    }
    
    std::unique_ptr<ExampleDevice> device;
};

TEST_F(ExampleDeviceTest, OpenDevice) {
    ASSERT_EQ(device->open(0), 0);
}

// 添加更多测试...
```

### 运行测试

```bash
# 构建测试
cd build
cmake .. -DBUILD_TESTS=ON
make

# 运行所有测试
make test

# 运行特定测试
./bin/test_gpu_submit
```

## 调试技巧

### 使用日志

```cpp
#include "logger.h"

// 记录调试信息
LOG_DEBUG("Processing command: %d", cmd_id);
LOG_INFO("Device initialized successfully");
LOG_ERROR("Failed to allocate memory: size=%zu", size);
```

### 使用 CLI 工具

```bash
# 运行 CLI 工具
./run_cli.sh

# 或直接运行
./build/bin/cli_tool
```

### GDB 调试

```bash
# 编译时保留调试信息
cmake .. -DCMAKE_BUILD_TYPE=Debug

# 使用 GDB 调试
gdb ./bin/test_gpu_submit
(gdb) run
```

## 最佳实践

### 代码规范

1. 使用有意义的变量和函数名
2. 添加适当的注释，特别是复杂逻辑
3. 遵循 RAII 原则管理资源
4. 正确处理错误情况

### 性能优化

1. 避免不必要的内存分配
2. 合理使用锁，减少竞争
3. 使用对象池缓存常用对象
4. 考虑内存局部性

### 安全性

1. 验证所有用户输入
2. 检查指针是否为空
3. 防止缓冲区溢出
4. 实现适当的访问控制

## 常见问题

### 构建问题

1. "C++17 not supported": 确保使用支持 C++17 的编译器
2. "Missing dependencies": 安装 CMake、GCC 等必要工具

### 运行时问题

1. "Permission denied": 检查设备文件权限
2. "Invalid argument": 验证 ioctl 参数格式
3. "Cannot allocate memory": 检查内存管理实现

### 调试建议

1. 使用日志输出中间状态
2. 编写单元测试验证组件功能
3. 逐步集成，避免一次性添加过多功能