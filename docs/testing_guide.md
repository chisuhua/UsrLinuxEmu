# 测试指南

## 测试概述

UsrLinuxEmu 项目包含全面的测试套件，用于验证各个组件的功能和性能。测试主要分为以下几类：

- 单元测试：测试单个组件的功能
- 集成测试：测试多个组件之间的交互
- 系统测试：测试整个系统的功能
- 性能测试：评估系统性能

## 测试结构

### 测试目录结构

```
tests/
├── CMakeLists.txt          # 测试构建配置
├── test_gpu_ioctl.cpp      # GPU IOCTL 操作测试
├── test_gpu_memory.cpp     # GPU 内存管理测试
├── test_gpu_mmap.cpp       # GPU 内存映射测试
├── test_gpu_mmap_and_submit.cpp  # GPU 内存映射和命令提交测试
├── test_gpu_register.cpp   # GPU 寄存器操作测试
├── test_gpu_regs.cpp       # GPU 寄存器测试
├── test_gpu_ringbuffer.cpp # GPU 环形缓冲区测试
├── test_gpu_submit.cpp     # GPU 命令提交测试
├── test_ioctl.cpp          # IOCTL 操作测试
├── test_logger.cpp         # 日志系统测试
├── test_module_load_and_vfs.cpp  # 模块加载和 VFS 测试
├── test_module_loader.cpp  # 模块加载器测试
├── test_pcie_gpu.cpp       # PCIe GPU 设备测试
├── test_plugin.cpp         # 插件系统测试
├── test_poll.cpp           # 轮询功能测试
├── test_serial.cpp         # 串口设备测试
├── test_serial_device.cpp  # 串口设备测试
├── test_serial_ioctl.cpp   # 串口 IOCTL 测试
└── testno_vfs.cpp          # 非 VFS 相关测试
```

## 编译和运行测试

### 构建测试

```bash
# 创建构建目录
mkdir build && cd build

# 配置构建系统（启用测试）
cmake .. -DBUILD_TESTS=ON

# 编译所有测试
make -j$(nproc)
```

### 运行所有测试

```bash
# 运行所有测试
make test

# 或运行特定测试可执行文件
./bin/test_gpu_submit
./bin/test_plugin
```

### 运行特定测试

```bash
# 使用ctest运行特定测试
ctest -R gpu_submit    # 运行包含"gpu_submit"的测试
ctest -R gpu          # 运行包含"gpu"的所有测试
ctest -V              # 详细输出
```

## 测试类型详解

### GPU 相关测试

#### GPU IOCTL 测试 ([test_gpu_ioctl.cpp](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/tests/test_gpu_ioctl.cpp))

验证 GPU 设备的 IOCTL 操作，包括内存分配、释放等操作。

```cpp
// 示例：测试 GPU 内存分配
TEST(GpuIoctlTest, TestAllocMemory) {
    int fd = open("/dev/gpgpu0", O_RDWR);
    ASSERT_GT(fd, 0);
    
    struct gpgpu_mem_alloc_args args = {0};
    args.size = 1024;
    
    int ret = ioctl(fd, GPGPU_ALLOC_MEM, &args);
    ASSERT_EQ(ret, 0);
    ASSERT_GT(args.addr, 0);
    
    close(fd);
}
```

#### GPU 内存管理测试 ([test_gpu_memory.cpp](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/tests/test_gpu_memory.cpp))

测试 GPU 内存分配器的功能，包括分配、释放和内存碎片管理。

#### GPU 内存映射测试 ([test_gpu_mmap.cpp](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/tests/test_gpu_mmap.cpp))

验证 GPU 内存的 mmap 操作，确保用户空间可以正确访问 GPU 内存。

#### GPU 命令提交测试 ([test_gpu_submit.cpp](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/tests/test_gpu_submit.cpp))

测试 GPU 命令提交机制，验证命令是否能正确传递给模拟器执行。

### 框架功能测试

#### 插件系统测试 ([test_plugin.cpp](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/tests/test_plugin.cpp))

验证插件的加载、卸载和使用功能。

```cpp
// 示例：测试插件加载
TEST(PluginTest, TestLoadPlugin) {
    int result = PluginManager::load_plugin("path/to/plugin.so");
    ASSERT_EQ(result, 0);
    
    auto plugin = PluginManager::get_plugin("plugin_name");
    ASSERT_NE(plugin, nullptr);
}
```

#### VFS 测试 ([test_module_load_and_vfs.cpp](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/tests/test_module_load_and_vfs.cpp))

测试虚拟文件系统的设备注册和查找功能。

#### 日志系统测试 ([test_logger.cpp](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/tests/test_logger.cpp))

验证日志系统的功能，包括不同级别的日志输出。

## 编写新测试

### 单元测试模板

```cpp
#include <gtest/gtest.h>
#include "your_component.h"

// 测试夹具类（如果需要）
class YourComponentTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 测试前的初始化
    }
    
    void TearDown() override {
        // 测试后的清理
    }
};

// 简单测试
TEST(YourComponentTest, TestBasicFunctionality) {
    YourComponent comp;
    EXPECT_EQ(comp.do_something(), expected_result);
}

// 使用测试夹具的测试
TEST_F(YourComponentTest, TestWithFixture) {
    // 使用在 SetUp 中初始化的资源
    EXPECT_TRUE(some_condition);
}

// 参数化测试
INSTANTIATE_TEST_SUITE_P(VariousSizes, 
                         YourComponentTest,
                         ::testing::Values(10, 100, 1000));
```

### GPU 组件测试示例

```cpp
#include <gtest/gtest.h>
#include "drivers/gpu/buddy_allocator.h"

class BuddyAllocatorTest : public ::testing::Test {
protected:
    static const size_t TEST_MEMORY_SIZE = 1024 * 1024; // 1MB
    
    void SetUp() override {
        allocator = std::make_unique<BuddyAllocator>(TEST_MEMORY_SIZE);
    }
    
    void TearDown() override {
        allocator.reset();
    }
    
    std::unique_ptr<BuddyAllocator> allocator;
};

TEST_F(BuddyAllocatorTest, TestBasicAllocation) {
    uint64_t addr;
    size_t size = 4096;  // 4KB
    
    int result = allocator->allocate(size, &addr);
    EXPECT_EQ(result, 0);
    EXPECT_GT(addr, 0);
}

TEST_F(BuddyAllocatorTest, TestMultipleAllocations) {
    std::vector<uint64_t> addresses;
    
    // 分配多个块
    for (int i = 0; i < 10; ++i) {
        uint64_t addr;
        size_t size = 1024;
        ASSERT_EQ(allocator->allocate(size, &addr), 0);
        addresses.push_back(addr);
    }
    
    // 验证地址不重叠
    for (size_t i = 0; i < addresses.size(); ++i) {
        for (size_t j = i + 1; j < addresses.size(); ++j) {
            // 检查是否有重叠（简单检查地址是否相同）
            EXPECT_NE(addresses[i], addresses[j]);
        }
    }
    
    // 释放所有块
    for (auto addr : addresses) {
        EXPECT_EQ(allocator->deallocate(addr), 0);
    }
}

TEST_F(BuddyAllocatorTest, TestLargeAllocation) {
    uint64_t addr;
    // 尝试分配接近总大小的内存
    size_t large_size = TEST_MEMORY_SIZE - 1;
    
    int result = allocator->allocate(large_size, &addr);
    // 根据分配器策略，这可能成功或失败，但不应崩溃
    EXPECT_TRUE(result == 0 || result == -1);
}
```

### 集成测试示例

```cpp
#include <gtest/gtest.h>
#include "kernel/device/gpgpu_device.h"
#include "drivers/gpu/gpu_driver.h"

TEST(GpuIntegrationTest, TestFullWorkflow) {
    // 创建 GPU 设备
    auto gpu_device = std::make_shared<GpuDevice>();
    ASSERT_NE(gpu_device, nullptr);
    
    // 打开设备
    int fd = gpu_device->open(0);
    EXPECT_GT(fd, 0);
    
    // 分配 GPU 内存
    struct gpgpu_mem_alloc_args alloc_args = {};
    alloc_args.size = 1024;
    int ioctl_result = gpu_device->ioctl(GPGPU_ALLOC_MEM, &alloc_args);
    EXPECT_EQ(ioctl_result, 0);
    EXPECT_GT(alloc_args.addr, 0);
    
    // 测试内存映射
    void *mapped_addr = gpu_device->mmap(
        nullptr, alloc_args.size, 
        PROT_READ | PROT_WRITE, 
        MAP_SHARED, 
        alloc_args.addr
    );
    EXPECT_NE(mapped_addr, MAP_FAILED);
    
    // 测试写入
    const char test_data[] = "Hello GPU";
    memcpy(mapped_addr, test_data, sizeof(test_data));
    
    // 验证写入的数据
    EXPECT_STREQ(static_cast<char*>(mapped_addr), test_data);
    
    // 清理
    munmap(mapped_addr, alloc_args.size);
    gpu_device->close();
}
```

## 测试最佳实践

### 断言使用

- 使用 `ASSERT_*` 进行关键条件检查，失败时停止测试
- 使用 `EXPECT_*` 进行非关键条件检查，失败时继续测试
- 选择合适的断言类型：
  - `EQ/NE` 用于相等性检查
  - `TRUE/FALSE` 用于布尔值检查
  - `NULL/NOTNULL` 用于指针检查
  - `STREQ/STRNE` 用于字符串比较

### 资源管理

```cpp
// 使用 RAII 确保资源清理
class ResourceGuard {
public:
    ResourceGuard(int fd) : fd_(fd) {}
    ~ResourceGuard() { if (fd_ >= 0) close(fd_); }
    
private:
    int fd_;
};

TEST(ResourceManagementTest, TestWithGuard) {
    int fd = open("/dev/gpgpu0", O_RDWR);
    ResourceGuard guard(fd);
    
    // 测试代码...
    // 离开作用域时会自动关闭文件描述符
}
```

### 错误处理测试

```cpp
TEST(ErrorHandlingTest, TestInvalidArguments) {
    BuddyAllocator allocator(1024 * 1024);
    uint64_t addr;
    
    // 测试无效参数
    EXPECT_NE(allocator.allocate(0, &addr), 0);  // 0 大小应失败
    EXPECT_NE(allocator.allocate(1024*1024*1024, &addr), 0);  // 超大分配应失败
}
```

### 性能测试

```cpp
TEST(PerformanceTest, TestAllocationSpeed) {
    BuddyAllocator allocator(1024 * 1024);
    const int iterations = 10000;
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        uint64_t addr;
        allocator.allocate(1024, &addr);
        if (addr != 0) {
            allocator.deallocate(addr);
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // 验证性能在可接受范围内（例如：10000次操作不超过1秒）
    EXPECT_LT(duration.count(), 1000000);  // 1秒 = 1,000,000微秒
}
```

## 测试覆盖范围

### 重要测试场景

1. **边界条件测试**：
   - 最小/最大值
   - 空值/零值
   - 溢出情况

2. **错误路径测试**：
   - 无效输入
   - 资源不足
   - 系统调用失败

3. **并发测试**：
   - 多线程访问
   - 竞态条件
   - 死锁情况

4. **资源泄漏测试**：
   - 内存泄漏
   - 文件描述符泄漏
   - 锁未释放

### 代码覆盖率

```bash
# 使用 lcov 生成覆盖率报告
sudo apt-get install lcov

# 重新配置项目以启用覆盖率
cmake .. -DCOVERAGE=ON -DBUILD_TESTS=ON
make

# 运行测试
make test

# 生成覆盖率报告
lcov --directory . --capture --output-file coverage.info
lcov --remove coverage.info '/usr/*' 'tests/*' --output-file coverage.info
genhtml coverage.info --output-directory coverage_report

# 查看报告
xdg-open coverage_report/index.html
```

## 持续集成

### 测试执行策略

在 CI/CD 环境中：

1. 每次提交运行快速单元测试
2. 定期运行完整测试套件
3. 性能回归测试
4. 代码覆盖率检查

### 测试报告

确保测试输出清晰的报告，包括：

- 通过/失败的测试数量
- 执行时间
- 失败测试的详细信息
- 性能指标