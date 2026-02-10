# 架构设计文档

## 系统概述

UsrLinuxEmu 是一个用户态 Linux 内核模拟环境，旨在提供无需 root 权限、无需内核编译的设备驱动开发和测试环境。系统采用分层架构设计，具有良好的扩展性和模块化特性。

## 整体架构

### 架构分层

系统采用四层架构设计：

```
┌─────────────────────────────────────────────────────────────┐
│                     用户应用层                                 │
│  (User Applications: CUDA Apps, Test Programs, CLI Tools)   │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│                   内核模拟框架层                               │
│  • VFS (Virtual File System)                                │
│  • File Operations Dispatcher                               │
│  • Plugin Manager                                           │
│  • Service Registry                                         │
│  • Config Manager                                           │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│                    设备驱动层                                  │
│  • GPGPU Driver        • Serial Driver                      │
│  • Memory Driver       • PCIe Emulator                      │
│  (Device Abstraction, File Ops Implementation)              │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│                   硬件模拟层                                   │
│  • GPU Simulator       • Memory Simulator                   │
│  • Command Parser      • Register Interface                 │
│  (Hardware Behavior Simulation)                             │
└─────────────────────────────────────────────────────────────┘
```

## 核心组件架构

### 1. 内核模拟框架层

#### 核心框架组件

```
┌──────────────────────────────────────────────────────────┐
│              Kernel Emulation Framework                   │
│                                                           │
│  ┌────────────┐  ┌──────────────┐  ┌─────────────────┐ │
│  │    VFS     │  │  Plugin Mgr  │  │ Service Registry│ │
│  │ (设备注册)  │  │  (插件加载)   │  │   (服务管理)     │ │
│  └────────────┘  └──────────────┘  └─────────────────┘ │
│                                                           │
│  ┌────────────┐  ┌──────────────┐  ┌─────────────────┐ │
│  │ File Ops   │  │ Module Loader│  │  Config Manager │ │
│  │ (文件操作)  │  │  (模块加载)   │  │   (配置管理)     │ │
│  └────────────┘  └──────────────┘  └─────────────────┘ │
│                                                           │
│  ┌────────────┐  ┌──────────────┐  ┌─────────────────┐ │
│  │Wait Queue  │  │ Poll Watcher │  │     Logger      │ │
│  │ (等待队列)  │  │  (事件监听)   │  │   (日志系统)     │ │
│  └────────────┘  └──────────────┘  └─────────────────┘ │
└──────────────────────────────────────────────────────────┘
```

**主要职责**：
- **VFS**: 设备节点的注册、查找和管理
- **Plugin Manager**: 动态加载和管理设备插件
- **Service Registry**: 全局服务的注册和获取（单例模式）
- **File Operations**: 文件操作的抽象和分发
- **Module Loader**: 模块的加载和初始化
- **Config Manager**: 配置文件的解析和管理
- **Wait Queue**: 等待队列的实现（类似内核等待队列）
- **Poll Watcher**: 事件监听和通知（观察者模式）
- **Logger**: 统一的日志输出接口

### 2. 设备抽象层

#### 设备类层次结构

```
                    ┌───────────────┐
                    │    Device     │
                    │   (抽象基类)   │
                    │  ───────────  │
                    │ + open()      │
                    │ + close()     │
                    │ + ioctl()     │
                    │ + mmap()      │
                    │ + read()      │
                    │ + write()     │
                    └───────┬───────┘
                            │
            ┌───────────────┼───────────────┐
            │               │               │
    ┌───────▼──────┐ ┌─────▼──────┐ ┌─────▼────────┐
    │SerialDevice  │ │MemoryDevice│ │ GpgpuDevice  │
    │  (串口设备)   │ │ (内存设备)  │ │  (GPU设备)   │
    └──────────────┘ └────────────┘ └──────────────┘
```

**设计模式**：
- **抽象工厂模式**: Device 基类定义统一接口
- **策略模式**: 不同设备实现不同的操作策略
- **模板方法模式**: 基类定义算法框架，子类实现具体步骤

### 3. GPU 驱动架构

#### GPGPU 驱动组件

```
┌─────────────────────────────────────────────────────────┐
│                   GpgpuDevice                            │
│  ┌──────────────────────────────────────────────────┐  │
│  │              GPU Driver Core                      │  │
│  │  • Command submission                             │  │
│  │  • Memory management interface                    │  │
│  │  • Register access                                │  │
│  └─────────┬────────────────────────────────────────┘  │
└────────────┼───────────────────────────────────────────┘
             │
    ┌────────┼────────┬─────────────┬──────────────┐
    │        │        │             │              │
┌───▼───┐ ┌─▼──────┐ ┌▼────────┐ ┌─▼──────────┐ ┌▼──────┐
│ Buddy │ │  Ring  │ │ Address │ │  Command   │ │  Mem  │
│Alloctr│ │ Buffer │ │  Space  │ │  Packet    │ │ Mapper│
└───────┘ └────────┘ └─────────┘ └────────────┘ └───────┘
```

**核心组件**：
- **Buddy Allocator**: GPU 物理内存分配器，支持高效的内存分配和回收
- **Ring Buffer**: 命令队列管理，生产者-消费者模式
- **Address Space**: GPU 虚拟地址空间管理
- **Command Packet**: GPU 命令包定义和解析
- **Memory Mapper**: 内存映射管理

### 4. 硬件模拟层

#### GPU 模拟器架构

```
┌──────────────────────────────────────────────────────┐
│              GPU Simulator                            │
│                                                       │
│  ┌──────────────┐         ┌──────────────────────┐  │
│  │   Command    │────────▶│   GPU Execution      │  │
│  │   Parser     │         │     Engine           │  │
│  └──────────────┘         └──────────────────────┘  │
│                                     │                │
│                                     ▼                │
│  ┌──────────────┐         ┌──────────────────────┐  │
│  │   Register   │◀───────│   Memory Access      │  │
│  │   Interface  │         │     Interface        │  │
│  └──────────────┘         └──────────────────────┘  │
└──────────────────────────────────────────────────────┘
```

## 数据流设计

### GPU 内存分配流程

```
┌──────────────┐
│ User Program │
└──────┬───────┘
       │ cudaMalloc(size)
       ▼
┌──────────────────┐
│  UsrLinuxEmu VFS │
└──────┬───────────┘
       │ lookup("/dev/gpu")
       ▼
┌──────────────────┐
│  GpgpuDevice     │
└──────┬───────────┘
       │ ioctl(GPGPU_ALLOC_MEM)
       ▼
┌──────────────────┐
│  GPU Driver      │
└──────┬───────────┘
       │ allocate()
       ▼
┌──────────────────┐
│ Buddy Allocator  │
└──────┬───────────┘
       │ return physical_addr
       ▼
┌──────────────────┐
│  mmap() mapping  │
└──────┬───────────┘
       │ user_ptr
       ▼
┌──────────────────┐
│  User Program    │
└──────────────────┘
```

### GPU 命令执行流程

```
┌──────────────┐
│ User Program │
└──────┬───────┘
       │ submit_kernel(cmd)
       ▼
┌──────────────────┐
│  GpgpuDevice     │
└──────┬───────────┘
       │ write(cmd)
       ▼
┌──────────────────┐
│  GPU Driver      │
└──────┬───────────┘
       │ enqueue(cmd)
       ▼
┌──────────────────┐
│   Ring Buffer    │
└──────┬───────────┘
       │ notify simulator
       ▼
┌──────────────────┐
│  GPU Simulator   │
└──────┬───────────┘
       │ parse command
       ▼
┌──────────────────┐
│ Command Parser   │
└──────┬───────────┘
       │ execute
       ▼
┌──────────────────┐
│ GPU Execution    │
└──────┬───────────┘
       │ access memory
       ▼
┌──────────────────┐
│ System Memory    │
└──────────────────┘
```

## 接口设计

### 核心框架层

#### 核心框架组件
- **设备抽象层**: `include/kernel/device/device.h` - 定义了统一的设备接口
- **文件操作层**: `include/kernel/file_ops.h` - 提供文件操作的抽象接口
- **虚拟文件系统层**: `include/kernel/vfs.h` - 实现设备节点的注册与查找机制
- **插件管理器**: `include/kernel/plugin_manager.h` - 负责动态加载和管理设备插件
- **服务注册器**: `include/kernel/service_registry.h` - 提供全局服务注册与获取

#### 设备实现层
- **串口设备**: `include/kernel/device/serial_device.h`
- **内存设备**: `include/kernel/device/memory_device.h`
- **GPGPU 设备**: `include/kernel/device/gpgpu_device.h`

#### 驱动实现层
- **GPU 驱动**: 实现 GPU 设备的驱动逻辑，包括内存管理、命令提交等
- **内存管理器**: 使用 Buddy Allocator 算法管理 GPU 本地物理内存
- **Ring Buffer**: 管理命令队列，实现异步命令处理
- **地址空间管理**: 管理 GPU 虚拟地址空间

#### 模拟器层
- **基础 GPU 模拟器**: 模拟 GPU 执行行为
- **命令解析器**: 解析 GPU 命令包

### Device 接口定义

```cpp
class Device {
public:
    virtual int open(int flags) = 0;
    virtual int close() = 0;
    virtual int ioctl(unsigned long cmd, void *arg) = 0;
    virtual void *mmap(void *addr, size_t length, int prot, 
                      int flags, off_t offset) = 0;
    virtual ssize_t read(void *buf, size_t count) = 0;
    virtual ssize_t write(const void *buf, size_t count) = 0;
};
```

### VFS 接口

```cpp
// 设备注册
int VFS::register_device(const std::string& path, 
                        std::shared_ptr<Device> device);

// 设备查找
std::shared_ptr<Device> VFS::lookup_device(const std::string& path);

// 设备注销
int VFS::unregister_device(const std::string& path);
```

### IOCTL 命令定义

```cpp
// GPU 内存管理
#define GPGPU_ALLOC_MEM      _IOWR('G', 1, struct alloc_mem_args)
#define GPGPU_FREE_MEM       _IOW('G', 2, uint64_t)
#define GPGPU_MAP_MEM        _IOWR('G', 3, struct map_mem_args)

// GPU 命令提交
#define GPGPU_SUBMIT_COMMAND _IOW('G', 4, struct command_packet)
#define GPGPU_WAIT_COMMAND   _IOW('G', 5, uint64_t)

// GPU 状态查询
#define GPGPU_GET_STATUS     _IOR('G', 6, struct gpu_status)
```

## 模块详细设计

### 1. 设备抽象模块

#### Device 基类
**文件**: `include/kernel/device/device.h`

**功能**:
- 定义所有设备的通用接口
- 提供设备生命周期管理
- 实现设备基础功能

**关键方法**:
- `open()`: 设备打开操作，初始化设备状态
- `close()`: 设备关闭操作，释放资源
- `ioctl()`: 设备控制操作，处理各种设备命令
- `mmap()`: 内存映射操作，将设备内存映射到用户空间
- `read()`: 数据读取操作
- `write()`: 数据写入操作

#### 设备实现类

**SerialDevice**: 串口设备
- 模拟串行通信设备
- 支持基本的读写操作
- 实现流式数据传输

**MemoryDevice**: 内存设备
- 模拟内存访问设备
- 提供直接内存访问接口
- 支持内存映射

**GpgpuDevice**: GPU 设备
- 完整的 GPU 功能模拟
- 内存管理和命令提交
- 与 GPU 模拟器集成

### 2. 文件操作模块

**文件**: `include/kernel/file_ops.h`

**功能**:
- 封装不同设备的文件操作
- 提供统一的接口调度机制
- 管理文件描述符和设备的映射

### 3. 虚拟文件系统模块

**文件**: `include/kernel/vfs.h`

**功能**:
- 设备节点的注册和查找
- 路径解析和管理
- 设备生命周期管理

**核心实现**:
```cpp
class VFS {
private:
    std::map<std::string, std::shared_ptr<Device>> devices_;
    std::mutex lock_;
    
public:
    static VFS& instance();
    int register_device(const std::string& path, 
                       std::shared_ptr<Device> device);
    std::shared_ptr<Device> lookup_device(const std::string& path);
};
```

### 4. GPU 驱动模块

#### Buddy Allocator (伙伴分配器)
**文件**: `drivers/gpu/buddy_allocator.h/cpp`

**功能**:
- 高效的内存分配和回收
- 支持 2 的幂次大小分配
- 减少内存碎片
- 快速合并相邻空闲块

**算法特点**:
- 时间复杂度: O(log n)
- 空间利用率高
- 支持快速分配和释放

#### Ring Buffer (环形缓冲区)
**文件**: `drivers/gpu/ring_buffer.h/cpp`

**功能**:
- 管理 GPU 命令队列
- 生产者-消费者模式
- 支持多线程并发访问
- 提供命令提交和消费接口

**实现特点**:
- 无锁设计（使用原子操作）
- 高效的命令传递
- 支持批量操作

#### Address Space Manager (地址空间管理)
**文件**: `drivers/gpu/address_space.h/cpp`

**功能**:
- 管理 GPU 虚拟地址空间
- 虚拟地址到物理地址映射
- 支持多进程地址隔离

#### GPU Driver (GPU 驱动)
**文件**: `drivers/gpu/gpu_driver.h/cpp`

**功能**:
- 处理 GPU 相关的 ioctl 命令
- 管理 GPU 命令提交
- 协调内存分配和命令执行
- 与 GPU 模拟器交互

### 5. GPU 模拟器模块

#### Basic GPU Simulator
**文件**: `simulator/gpu/basic_gpu_simulator.h/cpp`

**功能**:
- 模拟 GPU 指令执行
- 处理内存访问请求
- 模拟寄存器读写
- 生成执行结果

#### Command Parser
**文件**: `simulator/gpu/command_parser.h/cpp`

**功能**:
- 解析 GPU 命令包
- 验证命令格式
- 提取命令参数
- 分发到执行引擎

## 并发和同步设计

### 线程模型

```
┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│ User Thread  │     │ User Thread  │     │ User Thread  │
└──────┬───────┘     └──────┬───────┘     └──────┬───────┘
       │                    │                    │
       └────────────────────┼────────────────────┘
                            │
                   ┌────────▼────────┐
                   │  VFS + Devices  │
                   │   (Thread-Safe) │
                   └────────┬────────┘
                            │
                   ┌────────▼────────┐
                   │   Ring Buffer   │
                   │   (Lock-Free)   │
                   └────────┬────────┘
                            │
                   ┌────────▼────────┐
                   │ GPU Simulator   │
                   │    (Worker)     │
                   └─────────────────┘
```

### 同步机制

**文件**: `include/kernel/sync_utils.h`

**提供的同步原语**:
- Mutex (互斥锁)
- Spinlock (自旋锁)
- Semaphore (信号量)
- Condition Variable (条件变量)

**使用场景**:
- VFS 设备注册使用互斥锁
- Ring Buffer 使用原子操作
- Wait Queue 使用条件变量
- 内存分配器使用自旋锁（短临界区）

## 扩展性设计

### 插件化架构

#### 插件加载流程

```
┌────────────────┐
│  Plugin File   │
│  (.so / .dll)  │
└───────┬────────┘
        │
        ▼
┌────────────────┐
│ Plugin Manager │
└───────┬────────┘
        │ dlopen / LoadLibrary
        ▼
┌────────────────┐
│ Symbol Lookup  │
│ (create_device)│
└───────┬────────┘
        │
        ▼
┌────────────────┐
│ Device Factory │
│   Function     │
└───────┬────────┘
        │
        ▼
┌────────────────┐
│ Device Instance│
└───────┬────────┘
        │
        ▼
┌────────────────┐
│ VFS Register   │
└────────────────┘
```

#### 添加新设备类型

1. **定义设备类** (继承 Device)
2. **实现设备接口** (open, close, ioctl, etc.)
3. **创建插件入口** (REGISTER_DEVICE_PLUGIN)
4. **编译为动态库** (.so)
5. **配置插件加载** (config.json)

### Linux 兼容层扩展

**目录**: `include/linux_compat/`

**目标**: 提供 Linux 内核 API 的用户态实现

**已实现**:
- 基础类型定义 (`types.h`)
- 内存管理函数 (`memory.h`)
- IOCTL 宏定义 (`ioctl.h`)

**计划实现**:
- 字符设备 API (`cdev.h`)
- 设备模型 API (`device.h`)
- 同步原语 (`sync.h`)
- 中断处理 (`interrupt.h`)
- PCI 设备 API (`pci.h`)

## 性能优化设计

### 内存管理优化

1. **对象池**: 预分配常用对象
2. **内存池**: 减少系统调用
3. **零拷贝**: 尽量使用共享内存
4. **延迟释放**: 批量释放内存

### 并发优化

1. **无锁数据结构**: Ring Buffer 使用原子操作
2. **读写锁**: 区分读写操作
3. **细粒度锁**: 减小锁的范围
4. **线程局部存储**: 避免共享数据

### 缓存优化

1. **内存对齐**: 提高缓存命中率
2. **批量操作**: 减少系统调用
3. **预取**: 提前加载数据
4. **局部性**: 优化数据布局

## 错误处理和调试

### 错误处理策略

1. **返回值检查**: 所有函数返回错误码
2. **异常安全**: 使用 RAII 管理资源
3. **错误传播**: 明确的错误传播链
4. **日志记录**: 记录所有错误

### 调试支持

1. **日志系统**: 分级日志输出
2. **断言检查**: 开发期检查
3. **性能分析**: 支持性能统计
4. **内存检查**: 集成内存泄漏检测

## 安全性设计

### 权限控制

1. **设备访问控制**: 限制设备访问权限
2. **内存保护**: 防止越界访问
3. **参数验证**: 严格验证用户输入
4. **资源限制**: 防止资源耗尽

### 数据保护

1. **地址空间隔离**: 进程间内存隔离
2. **安全的内存操作**: 使用安全的内存函数
3. **输入验证**: 防止注入攻击
4. **输出过滤**: 防止信息泄露

---

**文档版本**: 2.0  
**最后更新**: 2026-02-10  
**维护者**: UsrLinuxEmu Team