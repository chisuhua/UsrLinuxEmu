# 架构设计文档

## 系统架构

UsrLinuxEmu 是一个用户态 Linux 内核模拟环境，主要由以下核心组件构成：

### 核心框架层
- **设备抽象层**：[device.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/device/device.h) 定义了统一的设备接口
- **文件操作层**：[file_ops.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/file_ops.h) 提供文件操作的抽象接口
- **虚拟文件系统层**：[vfs.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/vfs.h) 实现设备节点的注册与查找机制
- **插件管理器**：[plugin_manager.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/plugin_manager.h) 负责动态加载和管理设备插件
- **服务注册器**：[service_registry.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/service_registry.h) 提供全局服务注册与获取

### 设备实现层
- **串口设备**：[serial_device.h/cpp](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/device/serial_device.h)
- **内存设备**：[memory_device.h/cpp](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/device/memory_device.h)
- **GPGPU 设备**：[gpgpu_device.h/cpp](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/device/gpgpu_device.h)

### 驱动实现层
- **GPU 驱动**：实现 GPU 设备的驱动逻辑，包括内存管理、命令提交等
- **内存管理器**：使用 Buddy Allocator 算法管理 GPU 本地物理内存
- **Ring Buffer**：管理命令队列，实现异步命令处理
- **地址空间管理**：管理 GPU 虚拟地址空间

### 模拟器层
- **基础 GPU 模拟器**：模拟 GPU 执行行为
- **命令解析器**：解析 GPU 命令包

## 模块详细设计

### 设备抽象模块

#### [device.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/device/device.h)
定义了所有设备的基类，提供了设备的通用接口：
- open: 设备打开操作
- close: 设备关闭操作
- ioctl: 设备控制操作
- mmap: 内存映射操作
- write: 数据写入操作
- read: 数据读取操作

#### 具体设备实现
- [serial_device.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/device/serial_device.h): 串口设备的具体实现
- [memory_device.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/device/memory_device.h): 内存设备的具体实现
- [gpgpu_device.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/device/gpgpu_device.h): GPGPU 设备的具体实现

### 文件操作模块

#### [file_ops.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/file_ops.h)
封装了不同设备的文件操作实现，提供统一的接口调度机制。

### 虚拟文件系统模块

#### [vfs.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/vfs.h)
实现设备节点的注册与查找，使用户程序能够通过标准文件接口访问模拟设备。

### GPU 驱动模块

#### Buddy 内存分配器
- [buddy_allocator.h/cpp](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/drivers/gpu/buddy_allocator.h)
- 实现高效的内存分配和回收
- 支持不同大小的内存块分配
- 减少内存碎片

#### Ring Buffer
- [ring_buffer.h/cpp](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/drivers/gpu/ring_buffer.h)
- 管理 GPU 命令队列
- 实现生产者-消费者模式
- 支持多线程并发访问

#### GPU 驱动
- [gpu_driver.h/cpp](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/drivers/gpu/gpu_driver.h)
- 处理 GPU 相关的 ioctl 命令
- 管理 GPU 命令提交
- 协调内存分配和命令执行

## 数据流设计

### GPU 内存分配流程
```
[用户程序] 
    ↓ (cudaMalloc 或类似 API)
[UsrLinuxEmu 框架] 
    ↓ (映射为 ioctl 调用)
[ioctl(fd, GPGPU_ALLOC_MEM, &handle)]
    ↓
[BuddyAllocator.allocate(size, &phys_addr)]
    ↓
[返回 phys_addr = GPU 设备本地物理地址]
    ↓
[用户拿到 user_ptr = phys_addr （由 mmap 提供）]
```

### GPU 命令执行流程
```
[用户程序]
    ↓ (submit_kernel(..., phys_addr))
[UsrLinuxEmu 框架]
    ↓ (write 命令到 GPU 命令队列)
[GpuDriver.write(NV_GPU_COMMAND_QUEUE, ...)]
    ↓
[触发 GPU 执行]
    ↓
[GPU 模拟器 copy_from_device(...) → 访问 SYSTEM_UNCACHED 内存]
```

## 接口设计

### IOCTL 接口
定义在 [ioctl_gpgpu.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/drivers/gpu/ioctl_gpgpu.h) 中，包括：
- GPGPU_ALLOC_MEM: 分配 GPU 内存
- GPGPU_FREE_MEM: 释放 GPU 内存
- GPGPU_MAP_MEM: 映射 GPU 内存
- GPGPU_SUBMIT_COMMAND: 提交 GPU 命令

### 设备注册接口
通过 VFS 实现设备节点的注册和查找。

## 扩展性设计

### 插件化架构
- 通过 [plugin_manager.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/plugin_manager.h) 实现插件的动态加载
- 支持新增设备类型而无需修改核心框架
- 插件通过标准接口与框架交互

### 设备类型扩展
- 通过继承 [device.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/device/device.h) 中定义的基类可以轻松添加新设备类型
- 当前已实现: 串口设备、内存设备、GPGPU 设备
- 可扩展: 网络设备、存储设备等

## 线程安全设计

- 使用 [sync_utils.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/sync_utils.h) 提供同步原语
- Ring Buffer 支持多线程并发访问
- 内存分配器支持并发分配和释放操作