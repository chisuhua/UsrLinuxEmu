# API 参考文档

## 核心框架 API

### Device 抽象类

#### 概述
[device.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/device/device.h) 定义了所有设备的基类，提供统一的设备接口。

#### 接口定义

```cpp
class Device {
public:
    virtual ~Device() = default;
    
    virtual int open(int flags);
    virtual int close();
    virtual int ioctl(unsigned long cmd, void *arg);
    virtual void *mmap(void *addr, size_t length, int prot, int flags, off_t offset);
    virtual ssize_t write(const void *buf, size_t count);
    virtual ssize_t read(void *buf, size_t count);
    
    // 获取设备文件描述符
    int get_fd() const;
    
    // 设置设备文件描述符
    void set_fd(int fd);
};
```

#### 方法说明

- `open(int flags)`: 打开设备，初始化设备状态
- `close()`: 关闭设备，释放相关资源
- `ioctl(unsigned long cmd, void *arg)`: 设备控制操作，处理设备特定命令
- `mmap(void *addr, size_t length, int prot, int flags, off_t offset)`: 内存映射操作
- `write(const void *buf, size_t count)`: 向设备写入数据
- `read(void *buf, size_t count)`: 从设备读取数据

### File Operations 抽象

#### 概述
[file_ops.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/file_ops.h) 定义了文件操作的抽象接口。

#### 接口定义

```cpp
struct file_operations {
    int (*open)(struct inode *, struct file *);
    int (*release)(struct inode *, struct file *);
    long (*unlocked_ioctl)(struct file *, unsigned int, unsigned long);
    int (*mmap)(struct file *filp, struct vm_area_struct *vma);
    ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *);
    ssize_t (*read)(struct file *, char __user *, size_t, loff_t *);
};
```

### VFS (Virtual File System)

#### 概述
[vfs.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/vfs.h) 提供设备节点的注册与查找机制。

#### 接口定义

```cpp
class VFS {
public:
    // 注册设备到虚拟文件系统
    static int register_device(const std::string& path, std::shared_ptr<Device> device);
    
    // 查找设备
    static std::shared_ptr<Device> lookup_device(const std::string& path);
    
    // 注销设备
    static int unregister_device(const std::string& path);
};
```

### Plugin Manager

#### 概述
[plugin_manager.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/plugin_manager.h) 提供插件的动态加载和管理功能。

#### 接口定义

```cpp
class PluginManager {
public:
    // 加载插件
    static int load_plugin(const std::string& plugin_path);
    
    // 卸载插件
    static int unload_plugin(const std::string& plugin_name);
    
    // 获取插件
    static std::shared_ptr<Module> get_plugin(const std::string& plugin_name);
};
```

## GPU 驱动 API

### GPU Driver

#### 概述
[gpu_driver.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/drivers/gpu/gpu_driver.h) 实现 GPU 设备的驱动逻辑。

#### 接口定义

```cpp
class GpuDriver {
public:
    GpuDriver(size_t gpu_memory_size);
    ~GpuDriver();
    
    // 初始化驱动
    int initialize();
    
    // 分配 GPU 内存
    int allocate_gpu_memory(size_t size, uint64_t *gpu_addr);
    
    // 释放 GPU 内存
    int free_gpu_memory(uint64_t gpu_addr);
    
    // 提交命令到 GPU
    int submit_command(const GpuCommandPacket& cmd);
    
    // 写入命令队列
    ssize_t write_command_queue(const void *buf, size_t count);
    
    // 获取内存分配器
    BuddyAllocator* get_memory_allocator();
    
    // 获取环形缓冲区
    RingBuffer* get_command_buffer();
};
```

### Buddy Allocator

#### 概述
[buddy_allocator.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/drivers/gpu/buddy_allocator.h) 实现高效的内存分配算法。

#### 接口定义

```cpp
class BuddyAllocator {
public:
    BuddyAllocator(size_t total_size);
    ~BuddyAllocator();
    
    // 分配内存块
    int allocate(size_t size, uint64_t *addr);
    
    // 释放内存块
    int deallocate(uint64_t addr);
    
    // 获取总内存大小
    size_t get_total_size() const;
    
    // 获取可用内存大小
    size_t get_free_size() const;
    
    // 获取已分配内存大小
    size_t get_used_size() const;
};
```

### Ring Buffer

#### 概述
[ring_buffer.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/drivers/gpu/ring_buffer.h) 实现 GPU 命令队列的环形缓冲区。

#### 接口定义

```cpp
class RingBuffer {
public:
    RingBuffer(size_t size);
    ~RingBuffer();
    
    // 写入数据到缓冲区
    ssize_t write(const void *data, size_t size);
    
    // 读取数据从缓冲区
    ssize_t read(void *data, size_t size);
    
    // 获取可用写入空间大小
    size_t get_available_write_space() const;
    
    // 获取可读数据大小
    size_t get_available_read_size() const;
    
    // 重置缓冲区
    void reset();
};
```

## IOCTL 命令

### GPU IOCTL 命令

#### 概述
GPU 设备支持以下 IOCTL 命令，定义在 [ioctl_gpgpu.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/drivers/gpu/ioctl_gpgpu.h) 中。

#### 命令列表

```cpp
// 分配 GPU 内存
#define GPGPU_ALLOC_MEM _IOW('g', 1, struct gpgpu_mem_alloc_args)

// 释放 GPU 内存
#define GPGPU_FREE_MEM _IOW('g', 2, struct gpgpu_mem_free_args)

// 提交 GPU 命令
#define GPGPU_SUBMIT_CMD _IOW('g', 3, struct gpgpu_command_args)

// 获取 GPU 信息
#define GPGPU_GET_INFO _IOR('g', 4, struct gpgpu_info)
```

#### 参数结构

```cpp
// 内存分配参数
struct gpgpu_mem_alloc_args {
    size_t size;        // 要分配的大小
    uint64_t addr;      // 分配的地址（输出）
};

// 内存释放参数
struct gpgpu_mem_free_args {
    uint64_t addr;      // 要释放的地址
};

// 命令提交参数
struct gpgpu_command_args {
    void *command_buffer;   // 命令缓冲区指针
    size_t size;            // 命令缓冲区大小
    uint64_t gpu_addr;      // GPU 地址
};

// GPU 信息
struct gpgpu_info {
    uint64_t total_memory;      // 总内存大小
    uint64_t free_memory;       // 可用内存大小
    uint32_t compute_units;     // 计算单元数
    uint32_t gpu_id;            // GPU ID
};
```

## 模拟器 API

### Basic GPU Simulator

#### 概述
[basic_gpu_simulator.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/simulator/gpu/basic_gpu_simulator.h) 提供基本的 GPU 模拟功能。

#### 接口定义

```cpp
class BasicGpuSimulator {
public:
    BasicGpuSimulator();
    ~BasicGpuSimulator();
    
    // 初始化模拟器
    int initialize();
    
    // 执行命令
    int execute_command(const GpuCommandPacket& cmd);
    
    // 内存拷贝（从设备）
    int copy_from_device(uint64_t gpu_addr, void *dst, size_t size);
    
    // 内存拷贝（到设备）
    int copy_to_device(void *src, uint64_t gpu_addr, size_t size);
    
    // 同步操作
    int synchronize();
    
    // 设置完成回调
    void set_completion_callback(std::function<void()> callback);
};
```

## 日志系统 API

### Logger

#### 概述
[logger.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/logger.h) 提供统一的日志输出接口。

#### 接口定义

```cpp
// 日志级别
enum LogLevel {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
};

// 设置日志级别
void set_log_level(LogLevel level);

// 输出调试日志
void log_debug(const char *fmt, ...);

// 输出信息日志
void log_info(const char *fmt, ...);

// 输出警告日志
void log_warn(const char *fmt, ...);

// 输出错误日志
void log_error(const char *fmt, ...);
```

#### 宏定义

```cpp
#define LOG_DEBUG(fmt, ...) log_debug(fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  log_info(fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  log_warn(fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) log_error(fmt, ##__VA_ARGS__)
```

## 服务注册 API

### Service Registry

#### 概述
[service_registry.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/service_registry.h) 提供全局服务注册与获取功能。

#### 接口定义

```cpp
class ServiceRegistry {
public:
    // 注册服务
    template<typename T>
    static void register_service(const std::string& name, std::shared_ptr<T> service);
    
    // 获取服务
    template<typename T>
    static std::shared_ptr<T> get_service(const std::string& name);
    
    // 注销服务
    static void unregister_service(const std::string& name);
};
```

## 使用示例

### 设备使用示例

```cpp
// 注册设备
auto gpu_device = std::make_shared<GpuDevice>();
VFS::register_device("/dev/gpgpu0", gpu_device);

// 在用户程序中使用
int fd = open("/dev/gpgpu0", O_RDWR);
if (fd < 0) {
    LOG_ERROR("Failed to open GPU device");
    return -1;
}

// 分配 GPU 内存
struct gpgpu_mem_alloc_args alloc_args = {0};
alloc_args.size = 1024 * 1024; // 1MB
if (ioctl(fd, GPGPU_ALLOC_MEM, &alloc_args) < 0) {
    LOG_ERROR("Failed to allocate GPU memory");
    close(fd);
    return -1;
}

// 将 GPU 内存映射到用户空间
void *user_ptr = mmap(NULL, alloc_args.size, PROT_READ | PROT_WRITE, 
                      MAP_SHARED, fd, alloc_args.addr);
if (user_ptr == MAP_FAILED) {
    LOG_ERROR("Failed to mmap GPU memory");
    close(fd);
    return -1;
}

// 使用映射的内存...

// 释放资源
munmap(user_ptr, alloc_args.size);
close(fd);
```

### 插件开发示例

```cpp
// plugin_gpu.cpp
#include "gpu_driver.h"
#include "kernel/module_loader.h"

// 创建 GPU 设备实例
static Device* create_gpu_device() {
    return new GpuDevice();
}

// 注册插件
REGISTER_DEVICE_PLUGIN("gpgpu", create_gpu_device);
```