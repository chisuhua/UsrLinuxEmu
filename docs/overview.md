# UsrLinuxEmu 项目概述

## 项目简介

UsrLinuxEmu 是一个用户态 Linux 内核模拟环境，用于在用户空间中仿真内核设备驱动行为，支持如 GPGPU 等复杂外设的建模与测试。这个项目允许开发者在不依赖真实硬件的情况下测试和验证设备驱动程序的行为。

## 项目目标

- 在用户态运行，无需 root 权限或内核编译
- 模拟 Linux 内核设备行为（如 ioctl、mmap、VFS 注册等）
- 提供可扩展的插件化设备模型，便于快速开发和验证新型设备驱动逻辑
- 支持 GPU 类设备的内存管理、命令提交、寄存器访问等关键路径仿真

## 核心功能

### 设备抽象框架
基于 [device.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/device/device.h) 实现统一设备接口，支持串口、内存设备、GPGPU 设备等。

### 文件操作抽象
通过 [file_ops.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/file_ops.h) 模拟字符设备文件操作（open/ioctl/mmap/release）。

### VFS 设备注册与查找
使用 [vfs.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/vfs.h) 实现设备节点的注册与查找机制。

### PCIe 设备仿真
通过 [pcie_emu.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/pcie/pcie_emu.h) 模拟 PCIe 总线设备发现与配置。

### GPGPU 驱动模拟
- 支持 cudaMalloc 对应的 ioctl(GPGPU_ALLOC_MEM) 内存分配
- 使用 Buddy Allocator 管理 GPU 本地物理内存
- Ring Buffer 管理命令队列，触发 GPU 模拟执行
- mmap 映射设备内存供用户程序直接访问

### 插件化架构
支持动态加载设备插件（如 [plugin_gpu.cpp](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/drivers/gpu/plugin_gpu.cpp)），实现模块解耦。

### 日志与调试支持
提供 [logger.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/logger.h) 统一日志输出，便于调试。

### 轮询等待机制
[poll_watcher.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/poll_watcher.h) 支持 poll/select 事件通知。

## 项目架构

### 目录结构

```
UsrLinuxEmu/
├── CMakeLists.txt
├── README.md
├── build.sh
├── run_cli.sh
├── drivers/                 # 设备驱动实现
│   ├── gpu/                # GPGPU 驱动实现
│   │   ├── ring_buffer.h/cpp
│   │   ├── buddy_allocator.h/cpp
│   │   ├── address_space.h/cpp
│   │   ├── gpu_command_packet.h
│   │   ├── gpu_driver.h/cpp
│   │   ├── ioctl_gpgpu.h
│   │   └── plugin_gpu.cpp
├── include/                # 核心框架头文件
│   └── kernel/
│       ├── device/         # 设备抽象类
│       │   ├── device.h
│       │   ├── gpgpu_device.h
│       │   ├── memory_device.h
│       │   └── serial_device.h
│       ├── pcie/
│       │   └── pcie_emu.h
│       ├── config_manager.h
│       ├── file_ops.h
│       ├── ioctl.h
│       ├── logger.h
│       ├── module.h
│       ├── module_loader.h
│       ├── pcie_device.h
│       ├── plugin_manager.h
│       ├── poll_watcher.h
│       ├── service_registry.h
│       ├── sync_utils.h
│       ├── types.h
│       ├── vfs.h
│       └── wait_queue.h
├── plugins/                # 插件配置与构建入口
├── simulator/              # 设备行为模拟器
│   └── gpu/
│       ├── basic_gpu_simulator.h/cpp
│       ├── command_parser.h/cpp
│       └── gpu_register.h
├── src/                    # 框架核心实现
│   └── kernel/
├── tests/                  # 单元测试
├── tools/                  # 工具
│   └── cli/                # 命令行交互工具
├── zpoline/                # 独立实验性代码
└── docs/                   # 项目文档（新添加）
```

### 技术架构图

```
[User App] 
    ↓ (system call wrappers)
[Libc → ioctl/mmap/write]
    ↓ (redirected to emu)
[UsrLinuxEmu Kernel Core]
    ├── VFS → Device Lookup
    ├── File Operations Dispatcher
    └── Plugin Manager → Load plugin_gpu.so
        ↓
[GpgpuDevice] ↔ [GpuDriver] ↔ [BuddyAllocator, RingBuffer]
        ↓
[BasicGpuSimulator] ← command packet
        ↓
copy_from_device(phys_addr) → System Uncached Memory
```

## 设计模式

- **抽象工厂模式**: [device.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/device/device.h) 定义设备基类，派生出 [gpgpu_device.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/device/gpgpu_device.h)、[serial_device.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/device/serial_device.h) 等具体设备
- **策略模式**: [file_ops.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/file_ops.h) 封装不同设备的 ioctl/mmap 策略
- **观察者模式**: [poll_watcher.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/poll_watcher.h) 实现事件监听与通知
- **插件模式**: [plugin_manager.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/plugin_manager.h) + [module_loader.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/module_loader.h) 实现运行时插件加载
- **单例模式**: [service_registry.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/service_registry.h) 提供全局服务注册与获取

## 技术选型

- 前端: 无 GUI，纯 C++ 控制台工具
- 后端: C++17
- 构建系统: CMake 3.14+
- 核心库: STL, pthread（隐式）
- 模拟器: 自研 basic_gpu_simulator
- 通信机制: ioctl + mmap + ring buffer

## 版本和兼容性要求

- CMake ≥ 3.14
- C++17 编译支持（set(CMAKE_CXX_STANDARD 17)）

## 开发环境设置

### 必需工具
- CMake ≥ 3.14
- GCC 或 Clang 支持 C++17
- Make/Ninja
- Linux 环境（推测，因使用 ioctl/mmap 等系统调用模拟）

### 搭建开发环境

```bash
cd /mnt/ubuntu/chisuhua/github/UsrLinuxEmu
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### 运行环境
- 构建命令: `./build.sh`（项目根目录脚本）
- 本地开发: 构建后运行 `./run_cli.sh` 启动 CLI 工具
- CLI 入口: [tools/cli/main.cpp](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/tools/cli/main.cpp)
- 测试命令: 编译后的测试二进制位于 `bin/`，例如 `bin/test_gpu_submit`

## 组件交互流程

1. 用户程序调用标准 API（如 ioctl）→ 被重定向至模拟框架
2. VFS 根据设备名查找对应设备实例 → 调用其 file_operations 回调
3. GPGPU 设备通过 BuddyAllocator 分配物理地址 → 返回 handle
4. mmap 将设备内存映射到用户空间
5. write/write_reg 触发命令写入 → GpuDriver 解析并推送到 RingBuffer
6. 模拟器线程消费命令 → BasicGpuSimulator 执行模拟动作

## 已知问题

- [zpoline/](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/zpoline) 目录包含独立示例代码，与主项目关系未明
- [plugins/Makefile](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/plugins/Makefile) 存在但无说明文档，插件构建流程未清晰定义
- 多线程同步细节（如 RingBuffer 并发访问）依赖 [sync_utils.h](file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/include/kernel/sync_utils.h)，但其实现未提供