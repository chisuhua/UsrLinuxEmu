# 插件开发指南

本文档介绍如何为 UsrLinuxEmu 开发和加载设备插件。

**最后更新**: 2026-03-24  
**作者**: UsrLinuxEmu Team

---

## 概述

UsrLinuxEmu 采用插件化架构，允许开发者动态加载和卸载设备驱动。插件系统具有以下特性：

- **热插拔**: 运行时加载/卸载，无需重新编译
- **隔离性**: 每个插件独立运行，互不干扰
- **统一管理**: 通过 `PluginManager` 统一管理
- **自动清理**: 卸载时自动释放资源

---

## 插件架构

### 核心组件

```
PluginManager (单例)
    ├── load_plugin()     # 加载插件
    ├── unload_plugin()   # 卸载插件
    └── list_plugins()    # 列出已加载插件
          ↓
    PluginInfo
    ├── path              # 插件路径
    ├── handle            # dlopen 句柄
    ├── mod               # 模块结构
    └── ref_count         # 引用计数
```

### 插件结构

每个插件必须导出以下符号：

```cpp
extern "C" {
    module mod;  // 模块描述结构
}

// 模块初始化函数（自动调用）
__attribute__((constructor))
void init_module();
```

---

## 步骤 1: 定义模块结构

### module 结构体

```cpp
// kernel/module.h
struct module {
    const char* name;                          // 模块名称
    const char* depends;                       // 依赖模块（nullptr 表示无依赖）
    std::function<int()> init;                 // 初始化函数
    std::function<void()> exit;                // 清理函数
    bool loaded;                               // 加载状态
};
```

### 示例：GPU 插件

```cpp
// drivers/gpu/plugin_gpu.cpp
#include "gpu_driver.h"
#include "kernel/device/device.h"
#include "kernel/vfs.h"
#include "kernel/module.h"
#include <iostream>
#include <memory>

extern "C" {
    // 声明模块结构体
    module mod;
}

// 初始化模块结构体
__attribute__((constructor))
void init_module() {
    mod.name = "gpu";
    mod.depends = nullptr;
    
    mod.init = []() -> int {
        // 创建并注册设备
        auto dev0 = std::make_shared<Device>(
            "gpgpu0", 
            12347,
            std::make_shared<GpuDriver>(), 
            nullptr
        );
        VFS::instance().register_device(dev0);

        auto dev1 = std::make_shared<Device>(
            "gpgpu1", 
            12348,
            std::make_shared<GpuDriver>(), 
            nullptr
        );
        VFS::instance().register_device(dev1);

        std::cout << "[Gpu] Module initialized." << std::endl;
        return 0;
    };
    
    mod.exit = []() {
        std::cout << "[Gpu] Module exited." << std::endl;
    };
    
    mod.loaded = false;
}
```

---

## 步骤 2: 实现设备驱动

### 继承 Device 类

```cpp
// drivers/sample_plugin.h
#pragma once

#include "kernel/device/device.h"
#include "kernel/file_ops.h"
#include <memory>
#include <mutex>

/**
 * @brief 示例插件设备
 */
class SamplePluginDevice : public Device {
public:
    SamplePluginDevice();
    ~SamplePluginDevice() override;

    // FileOperations 接口
    int open(int fd, int flags) override;
    int close(int fd) override;
    ssize_t read(int fd, void* buf, size_t count) override;
    ssize_t write(int fd, const void* buf, size_t count) override;
    long ioctl(int fd, unsigned long request, void* argp) override;
    void* mmap(void* addr, size_t length, int prot, int flags,
               int fd, off_t offset) override;

private:
    std::mutex mutex_;
    bool initialized_ = false;
};
```

### 实现设备方法

```cpp
// drivers/sample_plugin.cpp
#include "sample_plugin.h"
#include "kernel/logger.h"
#include <cstring>

SamplePluginDevice::SamplePluginDevice()
    : Device("sample_plugin0", 0x200, nullptr, nullptr) {
    Logger::info << "SamplePluginDevice created";
}

SamplePluginDevice::~SamplePluginDevice() {
    close(0);
}

int SamplePluginDevice::open(int fd, int flags) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        Logger::warn << "Device already open";
        return -EBUSY;
    }
    
    initialized_ = true;
    Logger::info << "Device opened, fd=" << fd;
    return 0;
}

int SamplePluginDevice::close(int fd) {
    std::lock_guard<std::mutex> lock(mutex_);
    initialized_ = false;
    Logger::info << "Device closed";
    return 0;
}

ssize_t SamplePluginDevice::read(int fd, void* buf, size_t count) {
    if (!buf || count == 0) {
        return -EINVAL;
    }
    
    const char* data = "Hello from plugin!";
    size_t len = strlen(data);
    size_t to_copy = std::min(count, len);
    
    memcpy(buf, data, to_copy);
    return to_copy;
}

ssize_t SamplePluginDevice::write(int fd, const void* buf, size_t count) {
    if (!buf || count == 0) {
        return -EINVAL;
    }
    
    Logger::info << "Received " << count << " bytes";
    return count;
}

long SamplePluginDevice::ioctl(int fd, unsigned long request, void* argp) {
    // 处理 IOCTL 命令
    return -ENOTTY;
}

void* SamplePluginDevice::mmap(void* addr, size_t length, int prot, int flags,
                               int fd, off_t offset) {
    return MAP_FAILED;
}
```

---

## 步骤 3: 编写插件入口

### 完整插件示例

```cpp
// drivers/sample_plugin_main.cpp
#include "sample_plugin.h"
#include "kernel/vfs.h"
#include "kernel/module.h"
#include <iostream>
#include <memory>

extern "C" {
    module mod;
}

__attribute__((constructor))
void init_module() {
    mod.name = "sample_plugin";
    mod.depends = nullptr;
    
    mod.init = []() -> int {
        std::cout << "Loading sample plugin..." << std::endl;
        
        // 创建并注册设备
        auto device = std::make_shared<SamplePluginDevice>();
        
        int ret = VFS::instance().register_device(
            "/dev/sample_plugin0", 
            device
        );
        
        if (ret < 0) {
            std::cerr << "Failed to register device: " << ret << std::endl;
            return ret;
        }
        
        std::cout << "Sample plugin loaded successfully" << std::endl;
        mod.loaded = true;
        return 0;
    };
    
    mod.exit = []() {
        std::cout << "Unloading sample plugin..." << std::endl;
        
        // 清理资源
        VFS::instance().unregister_device("/dev/sample_plugin0");
        
        mod.loaded = false;
        std::cout << "Sample plugin unloaded" << std::endl;
    };
    
    mod.loaded = false;
}
```

---

## 步骤 4: 构建配置

### CMakeLists.txt

```cmake
# drivers/CMakeLists.txt

# Sample Plugin
add_library(sample_plugin SHARED
    sample_plugin.cpp
    sample_plugin_main.cpp
)

target_include_directories(sample_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/drivers
)

target_link_libraries(sample_plugin PRIVATE
    kernel_lib
)

# 设置输出目录到 plugins 文件夹
set_target_properties(sample_plugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/plugins
    PREFIX ""  # 移除 lib 前缀（可选）
)
```

### 构建插件

```bash
# 构建项目
mkdir -p build && cd build
cmake ..
make -j$(nproc)

# 插件将输出到 build/bin/plugins/
ls -la bin/plugins/
# sample_plugin.so
```

---

## 步骤 5: 加载插件

### 程序化加载

```cpp
// main.cpp
#include "kernel/plugin_manager.h"
#include <iostream>

int main() {
    // 加载插件
    int ret = PluginManager::instance().load_plugin("./bin/plugins/sample_plugin.so");
    if (ret < 0) {
        std::cerr << "Failed to load plugin: " << ret << std::endl;
        return ret;
    }
    
    std::cout << "Plugin loaded successfully" << std::endl;
    
    // 列出已加载插件
    PluginManager::instance().list_plugins();
    
    // 使用设备
    // ...
    
    // 卸载插件
    ret = PluginManager::instance().unload_plugin("sample_plugin");
    if (ret < 0) {
        std::cerr << "Failed to unload plugin: " << ret << std::endl;
        return ret;
    }
    
    return 0;
}
```

### CLI 工具加载

```bash
# 使用 CLI 工具加载插件
./build/bin/cli_tool --load-plugin ./bin/plugins/sample_plugin.so

# 列出已加载插件
./build/bin/cli_tool --list-plugins

# 卸载插件
./build/bin/cli_tool --unload-plugin sample_plugin
```

---

## 插件管理

### 加载插件

```cpp
int PluginManager::load_plugin(const std::string& path) {
    // 1. 打开动态库
    void* handle = dlopen(path.c_str(), RTLD_NOW);
    if (!handle) {
        std::cerr << "dlopen failed: " << dlerror() << std::endl;
        return -1;
    }
    
    // 2. 获取 module 符号
    module* mod = reinterpret_cast<module*>(dlsym(handle, "mod"));
    if (!mod) {
        std::cerr << "dlsym failed: " << dlerror() << std::endl;
        dlclose(handle);
        return -1;
    }
    
    // 3. 调用初始化函数
    if (mod->init) {
        int ret = mod->init();
        if (ret < 0) {
            dlclose(handle);
            return ret;
        }
    }
    
    // 4. 记录插件信息
    auto info = std::make_shared<PluginInfo>();
    info->path = path;
    info->handle = handle;
    info->mod = mod;
    info->ref_count = 1;
    
    plugins_[mod->name] = info;
    mod->loaded = true;
    
    return 0;
}
```

### 卸载插件

```cpp
int PluginManager::unload_plugin(const std::string& name) {
    auto it = plugins_.find(name);
    if (it == plugins_.end()) {
        std::cerr << "Plugin not found: " << name << std::endl;
        return -ENOENT;
    }
    
    auto info = it->second;
    
    // 1. 调用清理函数
    if (info->mod->exit) {
        info->mod->exit();
    }
    
    // 2. 关闭动态库
    dlclose(info->handle);
    
    // 3. 移除记录
    info->mod->loaded = false;
    plugins_.erase(it);
    
    return 0;
}
```

### 列出插件

```cpp
void PluginManager::list_plugins() const {
    std::cout << "Loaded plugins:" << std::endl;
    for (const auto& [name, info] : plugins_) {
        std::cout << "  - " << name 
                  << " (path: " << info->path 
                  << ", ref_count: " << info->ref_count 
                  << ")" << std::endl;
    }
}
```

---

## 插件开发最佳实践

### 1. 错误处理

```cpp
mod.init = []() -> int {
    // 检查资源
    if (!resource_available()) {
        std::cerr << "Resource not available" << std::endl;
        return -ENODEV;
    }
    
    // 初始化设备
    int ret = initialize_device();
    if (ret < 0) {
        std::cerr << "Initialization failed: " << ret << std::endl;
        return ret;
    }
    
    return 0;
};
```

### 2. 资源管理

```cpp
mod.exit = []() {
    // 1. 注销设备
    VFS::instance().unregister_device("/dev/sample_plugin0");
    
    // 2. 释放内存
    // (智能指针会自动释放)
    
    // 3. 关闭文件描述符
    // close(fd);
    
    // 4. 记录日志
    std::cout << "Plugin cleanup completed" << std::endl;
};
```

### 3. 线程安全

```cpp
class ThreadSafePluginDevice : public Device {
private:
    std::mutex mutex_;
    std::atomic<bool> running_{false};
    
public:
    int open(int fd, int flags) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (running_) {
            return -EBUSY;
        }
        running_ = true;
        return 0;
    }
    
    int close(int fd) override {
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
        return 0;
    }
};
```

### 4. 日志记录

```cpp
#include "kernel/logger.h"

mod.init = []() -> int {
    Logger::info << "Loading plugin: " << mod.name;
    
    // ... 初始化逻辑
    
    Logger::info << "Plugin loaded successfully";
    return 0;
};

mod.exit = []() {
    Logger::info << "Unloading plugin: " << mod.name;
    
    // ... 清理逻辑
    
    Logger::info << "Plugin unloaded";
};
```

---

## 调试插件

### 检查符号导出

```bash
# 检查插件是否正确导出 mod 符号
nm -D ./bin/plugins/sample_plugin.so | grep mod

# 输出示例：
# 0000000000004010 B mod
```

### 检查依赖

```bash
# 检查插件的动态库依赖
ldd ./bin/plugins/sample_plugin.so

# 输出示例：
# linux-vdso.so.1 (0x00007fff...)
# libkernel_lib.so => ./build/bin/libkernel_lib.so
# libstdc++.so.6 => /usr/lib/x86_64-linux-gnu/libstdc++.so.6
```

### 使用 lldb 调试

```bash
# 启动 lldb
lldb ./bin/cli_tool

# 设置断点
(lldb) break set -n init_module

# 运行
(lldb) run

# 当插件加载时会中断
```

---

## 常见问题

### Q: 插件加载失败，报错 "dlopen failed"

**A**: 检查以下几点：

1. **路径是否正确**
   ```bash
   ls -la ./bin/plugins/sample_plugin.so
   ```

2. **依赖是否满足**
   ```bash
   ldd ./bin/plugins/sample_plugin.so
   ```

3. **权限问题**
   ```bash
   chmod +x ./bin/plugins/sample_plugin.so
   ```

### Q: 插件加载后找不到设备

**A**: 检查：

1. **设备是否注册**
   ```cpp
   // 在 init 中添加日志
   std::cout << "Registering device: /dev/sample_plugin0" << std::endl;
   ```

2. **设备名称是否正确**
   ```cpp
   // 确保名称一致
   VFS::instance().register_device("/dev/sample_plugin0", device);
   ```

### Q: 插件卸载后程序崩溃

**A**: 可能是资源未正确清理：

1. **确保注销设备**
   ```cpp
   mod.exit = []() {
       VFS::instance().unregister_device("/dev/sample_plugin0");
   };
   ```

2. **检查悬空指针**
   - 确保智能指针正确管理
   - 避免裸指针

---

## 完整示例

### 目录结构

```
drivers/
├── sample_plugin/
│   ├── CMakeLists.txt
│   ├── sample_plugin.h
│   ├── sample_plugin.cpp
│   └── sample_plugin_main.cpp
```

### CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.14)
project(sample_plugin)

add_library(sample_plugin SHARED
    sample_plugin.cpp
    sample_plugin_main.cpp
)

target_include_directories(sample_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(sample_plugin PRIVATE
    kernel_lib
)

set_target_properties(sample_plugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/plugins
    PREFIX ""
)
```

---

## 相关文档

- [添加新设备](../03-development/adding-devices.md) - 设备开发基础
- [调试指南](../03-development/debugging.md) - 调试工具
- [API 参考](../06-reference/api-reference.md) - API 文档

---

**最后更新**: 2026-03-24
