# 添加新设备指南

本文档介绍如何在 UsrLinuxEmu 框架中添加新的设备类型。

**最后更新**: 2026-03-24  
**作者**: UsrLinuxEmu Team

---

## 概述

UsrLinuxEmu 采用插件化设备架构，允许开发者通过以下步骤添加新设备：

1. 定义设备接口（头文件）
2. 实现设备驱动（源文件）
3. 实现 IOCTL 命令处理
4. 注册设备到 VFS
5. （可选）创建插件动态加载

---

## 设备架构

### 继承层次

```
Device (基类)
    ├── GpgpuDevice (GPU 设备)
    │   └── SampleGpuDriver (示例 GPU 驱动)
    ├── SerialDevice (串口设备)
    └── MemoryDevice (内存设备)
```

### 核心组件

| 组件 | 位置 | 说明 |
|------|------|------|
| `Device` | `include/kernel/device/device.h` | 设备基类 |
| `FileOperations` | `include/kernel/file_ops.h` | 文件操作接口 |
| `VFS` | `include/kernel/vfs.h` | 虚拟文件系统 |
| `PluginManager` | `include/kernel/plugin_manager.h` | 插件管理器 |

---

## 步骤 1: 定义设备类

### 创建头文件

在 `drivers/` 目录创建新设备头文件，例如 `sample_device.h`：

```cpp
#pragma once

#include "kernel/device/device.h"
#include "kernel/file_ops.h"
#include <memory>

/**
 * @brief 示例设备驱动
 * @details 演示如何添加新的设备类型
 */
class SampleDevice : public Device {
public:
    /**
     * @brief 构造函数
     */
    SampleDevice();
    
    /**
     * @brief 析构函数
     */
    ~SampleDevice() override;

    // FileOperations 接口实现
    int open(int fd, int flags) override;
    int close(int fd) override;
    ssize_t read(int fd, void* buf, size_t count) override;
    ssize_t write(int fd, const void* buf, size_t count) override;
    long ioctl(int fd, unsigned long request, void* argp) override;
    void* mmap(void* addr, size_t length, int prot, int flags, 
               int fd, off_t offset) override;
    int poll(int fd, int events) override;

private:
    // 设备私有成员
    std::mutex device_mutex_;  // 线程安全保护
    bool initialized_ = false; // 初始化状态
};
```

### 定义 IOCTL 命令

在 `include/linux_compat/ioctl.h` 或设备专用头文件中定义 IOCTL 命令：

```cpp
#pragma once

#include <linux/ioctl.h>

// 定义 IOCTL 魔术数（设备唯一标识）
#define SAMPLE_DEVICE_MAGIC 0xAA

// 定义 IOCTL 命令
#define SAMPLE_IOCTL_INIT      _IO(SAMPLE_DEVICE_MAGIC, 0x01)
#define SAMPLE_IOCTL_READ_DATA _IOR(SAMPLE_DEVICE_MAGIC, 0x02, struct sample_data)
#define SAMPLE_IOCTL_WRITE_DATA _IOW(SAMPLE_DEVICE_MAGIC, 0x03, struct sample_data)
#define SAMPLE_IOCTL_RESET     _IO(SAMPLE_DEVICE_MAGIC, 0x04)

// 定义数据结构
struct sample_data {
    uint32_t size;
    uint64_t buffer_addr;
    uint32_t flags;
};
```

---

## 步骤 2: 实现设备驱动

### 创建源文件

创建 `drivers/sample_device.cpp`：

```cpp
#include "sample_device.h"
#include "linux_compat/compat.h"
#include <cstring>
#include <iostream>

SampleDevice::SampleDevice() 
    : Device("sample0", 0x100, nullptr, nullptr) {
    // 初始化设备
    std::cout << "SampleDevice initialized" << std::endl;
}

SampleDevice::~SampleDevice() {
    close(0);
}

int SampleDevice::open(int fd, int flags) {
    if (initialized_) {
        return -EBUSY;  // 设备已打开
    }
    
    // 执行初始化逻辑
    initialized_ = true;
    return 0;
}

int SampleDevice::close(int fd) {
    initialized_ = false;
    return 0;
}

ssize_t SampleDevice::read(int fd, void* buf, size_t count) {
    if (!buf || count == 0) {
        return -EINVAL;
    }
    
    // 实现读取逻辑
    // 示例：返回固定数据
    const char* data = "Sample Device Data";
    size_t len = strlen(data);
    size_t to_copy = std::min(count, len);
    
    memcpy(buf, data, to_copy);
    return to_copy;
}

ssize_t SampleDevice::write(int fd, const void* buf, size_t count) {
    if (!buf || count == 0) {
        return -EINVAL;
    }
    
    // 实现写入逻辑
    // 示例：打印接收的数据
    std::cout << "Received " << count << " bytes" << std::endl;
    return count;
}

long SampleDevice::ioctl(int fd, unsigned long request, void* argp) {
    switch (request) {
        case SAMPLE_IOCTL_INIT:
            // 初始化设备
            std::cout << "IOCTL: Initialize device" << std::endl;
            return 0;
        
        case SAMPLE_IOCTL_READ_DATA: {
            struct sample_data* data = static_cast<struct sample_data*>(argp);
            if (!data) {
                return -EINVAL;
            }
            // 读取数据逻辑
            data->size = 1024;
            return 0;
        }
        
        case SAMPLE_IOCTL_WRITE_DATA: {
            const struct sample_data* data = 
                static_cast<const struct sample_data*>(argp);
            if (!data) {
                return -EINVAL;
            }
            // 写入数据逻辑
            std::cout << "Writing " << data->size << " bytes" << std::endl;
            return 0;
        }
        
        case SAMPLE_IOCTL_RESET:
            // 重置设备
            initialized_ = false;
            std::cout << "IOCTL: Reset device" << std::endl;
            return 0;
        
        default:
            return -ENOTTY;  // 不支持的 IOCTL 命令
    }
}

void* SampleDevice::mmap(void* addr, size_t length, int prot, int flags, 
                         int fd, off_t offset) {
    // 实现内存映射逻辑（如果需要）
    // 示例：返回 MAP_FAILED 表示不支持
    return MAP_FAILED;
}

int SampleDevice::poll(int fd, int events) {
    // 实现轮询逻辑（如果需要）
    return 0;
}
```

---

## 步骤 3: 注册设备到 VFS

### 手动注册

在应用程序中手动注册设备：

```cpp
#include "sample_device.h"
#include "kernel/vfs.h"
#include <memory>

int main() {
    // 创建设备实例
    auto sample_device = std::make_shared<SampleDevice>();
    
    // 注册到 VFS
    int ret = VFS::instance().register_device("/dev/sample0", sample_device);
    if (ret < 0) {
        std::cerr << "Failed to register device: " << ret << std::endl;
        return ret;
    }
    
    // 使用设备（类似 Linux 设备操作）
    int fd = VFS::instance().open("/dev/sample0", O_RDWR);
    if (fd < 0) {
        std::cerr << "Failed to open device" << std::endl;
        return fd;
    }
    
    // 读写设备
    char buf[256];
    ssize_t n = VFS::instance().read(fd, buf, sizeof(buf));
    if (n > 0) {
        std::cout << "Read: " << std::string(buf, n) << std::endl;
    }
    
    // 关闭设备
    VFS::instance().close(fd);
    
    return 0;
}
```

### 自动注册（插件方式）

实现插件导出函数，由 `PluginManager` 自动加载：

```cpp
// drivers/sample_device_plugin.cpp
#include "sample_device.h"
#include "kernel/plugin_manager.h"

extern "C" {

/**
 * @brief 插件初始化函数
 * @param manager 插件管理器实例
 * @return 0 成功，负数失败
 */
int plugin_init(PluginManager* manager) {
    auto device = std::make_shared<SampleDevice>();
    
    int ret = manager->register_device("/dev/sample0", device);
    if (ret < 0) {
        return ret;
    }
    
    std::cout << "SampleDevice plugin loaded" << std::endl;
    return 0;
}

/**
 * @brief 插件清理函数
 */
void plugin_exit() {
    std::cout << "SampleDevice plugin unloaded" << std::endl;
}

}
```

---

## 步骤 4: 构建配置

### 添加到 CMakeLists.txt

在 `drivers/CMakeLists.txt` 中添加：

```cmake
# Sample Device Driver
add_library(sample_device SHARED
    sample_device.cpp
    sample_device_plugin.cpp
)

target_include_directories(sample_device PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(sample_device PRIVATE
    kernel_lib
)

# 设置输出目录
set_target_properties(sample_device PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/plugins
)
```

---

## 步骤 5: 测试设备

### 单元测试

创建 `tests/test_sample_device.cpp`：

```cpp
#include <gtest/gtest.h>
#include "sample_device.h"
#include "kernel/vfs.h"

class SampleDeviceTest : public ::testing::Test {
protected:
    std::shared_ptr<SampleDevice> device;
    
    void SetUp() override {
        device = std::make_shared<SampleDevice>();
        VFS::instance().register_device("/dev/sample0", device);
    }
    
    void TearDown() override {
        VFS::instance().unregister_device("/dev/sample0");
    }
};

TEST_F(SampleDeviceTest, OpenAndClose) {
    int fd = VFS::instance().open("/dev/sample0", O_RDWR);
    ASSERT_GE(fd, 0);
    
    int ret = VFS::instance().close(fd);
    ASSERT_EQ(ret, 0);
}

TEST_F(SampleDeviceTest, ReadData) {
    int fd = VFS::instance().open("/dev/sample0", O_RDWR);
    ASSERT_GE(fd, 0);
    
    char buf[256];
    ssize_t n = VFS::instance().read(fd, buf, sizeof(buf));
    ASSERT_GT(n, 0);
    
    VFS::instance().close(fd);
}

TEST_F(SampleDeviceTest, IoctlInit) {
    int fd = VFS::instance().open("/dev/sample0", O_RDWR);
    ASSERT_GE(fd, 0);
    
    long ret = VFS::instance().ioctl(fd, SAMPLE_IOCTL_INIT, nullptr);
    ASSERT_EQ(ret, 0);
    
    VFS::instance().close(fd);
}
```

### 运行测试

```bash
cd build
ctest -R sample_device --output-on-failure
```

---

## 完整示例参考

### GPU 设备

参考现有 GPU 驱动实现：

- 头文件：`drivers/sample_gpu.h`
- 实现：`drivers/sample_gpu.cpp`
- IOCTL: `drivers/gpu/ioctl_gpgpu.h`
- 插件：`drivers/gpu/plugin_gpu.cpp`

### 串口设备

参考串口设备实现：

- 头文件：`drivers/sample_serial.h`
- 实现：`drivers/sample_serial.cpp`

### 内存设备

参考内存设备实现：

- 头文件：`drivers/sample_memory.h`
- 实现：`drivers/sample_memory.cpp`

---

## 最佳实践

### 线程安全

设备操作可能被多线程并发访问，确保：

1. 使用 `std::mutex` 保护共享状态
2. 避免在锁内执行耗时操作
3. 考虑使用 `std::atomic` 处理简单计数器

```cpp
class ThreadSafeDevice : public Device {
private:
    std::mutex mutex_;
    std::atomic<int> ref_count_{0};
    
public:
    int open(int fd, int flags) override {
        std::lock_guard<std::mutex> lock(mutex_);
        ref_count_++;
        return 0;
    }
};
```

### 错误处理

遵循 Linux 错误码规范：

| 错误码 | 含义 | 使用场景 |
|--------|------|----------|
| `-EINVAL` | 无效参数 | 参数检查失败 |
| `-ENOMEM` | 内存不足 | 内存分配失败 |
| `-EBUSY` | 设备忙 | 设备已打开/忙 |
| `-ENOTTY` | 不支持 | IOCTL 命令未知 |
| `-EFAULT` | 地址错误 | 用户空间指针无效 |
| `-EIO` | I/O 错误 | 硬件操作失败 |

### 内存管理

1. 优先使用智能指针（`std::shared_ptr`, `std::unique_ptr`）
2. 避免裸指针 `new/delete`
3. 使用 RAII 管理资源

---

## 调试技巧

### 启用日志

在设备中使用 `Logger` 类：

```cpp
#include "kernel/logger.h"

int SampleDevice::ioctl(int fd, unsigned long request, void* argp) {
    LOG_INFO << "SampleDevice ioctl: request=" << std::hex << request;
    
    // ... 处理逻辑
    
    LOG_DEBUG << "ioctl completed";
    return 0;
}
```

### 使用 GDB 调试

```bash
# 启动 GDB
gdb --args ./build/bin/your_test

# 设置断点
(gdb) break SampleDevice::ioctl

# 运行
(gdb) run
```

---

## 常见问题

### Q: 设备号如何选择？

A: 设备号（`dev_t`）由主设备号和次设备号组成：

```cpp
// 主设备号 << 8 | 次设备号
dev_t dev_id = (0x100 << 8) | 0x01;  // 主设备号 0x100, 次设备号 0x01
```

建议：
- 主设备号按设备类型分配（GPU: 0x10, 串口：0x20, 内存：0x30）
- 次设备号按实例编号（0, 1, 2...）

### Q: 如何支持多个设备实例？

A: 创建多个设备对象并注册不同路径：

```cpp
auto device0 = std::make_shared<SampleDevice>();
auto device1 = std::make_shared<SampleDevice>();

VFS::instance().register_device("/dev/sample0", device0);
VFS::instance().register_device("/dev/sample1", device1);
```

### Q: 插件加载失败怎么办？

A: 检查以下几点：

1. 插件路径是否正确
2. 是否导出 `plugin_init` 和 `plugin_exit`
3. 使用 `extern "C"` 避免 C++ name mangling
4. 检查依赖库是否链接

```cpp
// 正确导出
extern "C" {
    int plugin_init(PluginManager* manager);
    void plugin_exit();
}
```

---

## 下一步

完成设备开发后：

1. ✅ 编写单元测试（覆盖率 > 80%）
2. ✅ 更新 API 参考文档
3. ✅ 添加到 CI/CD 测试矩阵
4. ✅ 编写用户使用示例

---

## 相关文档

- [开发指南](guide.md) - 开发环境和流程
- [代码风格](coding-style.md) - 编码规范
- [API 参考](../06-reference/api-reference.md) - API 接口文档
- [GPU 驱动架构](../05-advanced/gpu-driver-architecture.md) - GPU 驱动详细设计

---

**最后更新**: 2026-03-24
