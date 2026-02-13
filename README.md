# UsrLinuxEmu - 用户态 Linux 内核模拟环境

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)]()
[![License](https://img.shields.io/badge/license-MIT-blue)]()
[![Version](https://img.shields.io/badge/version-0.1.0-orange)]()

## 项目简介

UsrLinuxEmu 是一个**用户态 Linux 内核模拟环境**，专为设备驱动开发和测试而设计。它允许开发者在**无需 root 权限**、**无需内核编译**的情况下，开发、测试和调试设备驱动程序，特别是支持 GPGPU 等复杂设备的完整模拟。

### 核心特性

- 🚀 **用户态运行** - 无需 root 权限或内核模块
- 🔌 **插件化架构** - 支持动态加载设备插件
- 🎮 **GPU 支持** - 完整的 GPGPU 驱动和模拟器
- 🏗️ **模块化设计** - 清晰的分层架构，易于扩展
- 🔧 **Linux 兼容** - 提供 Linux 内核 API 的用户态实现
- 📊 **完整日志** - 统一的日志系统，便于调试

### 支持的设备

- ✅ GPGPU 设备（GPU 内存管理、命令提交、模拟执行）
- ✅ 串口设备
- ✅ 内存设备
- ✅ PCIe 设备（基础支持）
- 🔜 网络设备（规划中）
- 🔜 存储设备（规划中）

## 快速开始

### 系统要求

- Linux 环境（推荐 Ubuntu 18.04+）
- CMake ≥ 3.14
- GCC/Clang 支持 C++17

### 构建项目

```bash
# 克隆仓库
git clone <repository-url>
cd UsrLinuxEmu

# 使用构建脚本
./build.sh

# 或手动构建
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### 运行测试

```bash
# 运行所有测试
cd build
make test

# 运行特定测试
./bin/test_gpu_submit
./bin/test_gpu_memory
```

### 运行示例

```bash
# 运行 CLI 工具
./run_cli.sh

# 或直接运行
./build/bin/cli_tool
```

## 项目结构

```
UsrLinuxEmu/
├── docs/                   # 📚 项目文档
│   ├── README.md          # 文档索引
│   ├── architecture.md    # 架构设计
│   ├── ROADMAP.md        # 开发路线图
│   └── ...
├── include/               # 头文件
│   ├── kernel/           # 内核框架
│   │   ├── device/       # 设备抽象
│   │   ├── pcie/         # PCIe 模拟
│   │   └── ...
│   └── linux_compat/     # Linux 兼容层
├── src/                   # 源代码
│   └── kernel/           # 内核框架实现
├── drivers/               # 设备驱动
│   └── gpu/              # GPU 驱动
├── simulator/             # 设备模拟器
│   └── gpu/              # GPU 模拟器
├── tests/                 # 测试代码
├── tools/                 # 工具程序
│   └── cli/              # CLI 工具
└── plugins/               # 插件示例
```

## 架构概览

UsrLinuxEmu 采用分层架构设计：

```
┌─────────────────────────────────────────┐
│        用户应用层                          │
│  (CUDA Apps, Test Programs, CLI Tools) │
└─────────────────────────────────────────┘
                  ↓
┌─────────────────────────────────────────┐
│       内核模拟框架层                        │
│  VFS | Plugin Manager | Service Registry│
└─────────────────────────────────────────┘
                  ↓
┌─────────────────────────────────────────┐
│         设备驱动层                          │
│  GPGPU Driver | Serial | Memory | PCIe │
└─────────────────────────────────────────┘
                  ↓
┌─────────────────────────────────────────┐
│        硬件模拟层                          │
│  GPU Simulator | Memory | Registers    │
└─────────────────────────────────────────┘
```

### GPU 内存分配流程

```
用户程序 (cudaMalloc)
    ↓
VFS 查找设备 (/dev/gpu)
    ↓
GpgpuDevice::ioctl(GPGPU_ALLOC_MEM)
    ↓
Buddy Allocator 分配物理地址
    ↓
返回 GPU 物理地址
    ↓
mmap 映射到用户空间
    ↓
用户获得内存指针
```

### GPU 命令执行流程

```
用户程序提交命令
    ↓
GPU Driver 入队命令
    ↓
Ring Buffer 管理队列
    ↓
GPU Simulator 执行
    ↓
访问系统内存完成操作
```

## 文档

详细文档请参阅 [docs/](docs/) 目录：

- 📖 [项目概述](docs/overview.md) - 项目详细介绍
- 🏗️ [架构设计](docs/architecture.md) - 系统架构和设计
- 🗺️ [开发路线图](docs/ROADMAP.md) - 项目规划和里程碑
- 👨‍💻 [开发指南](docs/development_guide.md) - 开发环境和代码规范
- 📘 [API 参考](docs/api_reference.md) - API 接口文档
- 🧪 [测试指南](docs/testing_guide.md) - 测试编写和运行
- 🔧 [构建系统](docs/build_system.md) - 构建配置说明

## 示例代码

### 创建设备

```cpp
#include "kernel/device/device.h"
#include "kernel/vfs.h"

// 创建设备实例
auto gpu_device = std::make_shared<GpgpuDevice>();

// 注册到 VFS
VFS::instance().register_device("/dev/gpu0", gpu_device);

// 打开设备
int fd = gpu_device->open(O_RDWR);
```

### GPU 内存分配

```cpp
// 分配 GPU 内存
struct alloc_mem_args args = {
    .size = 1024 * 1024,  // 1MB
    .flags = 0
};

gpu_device->ioctl(GPGPU_ALLOC_MEM, &args);
uint64_t gpu_addr = args.phys_addr;

// 映射到用户空间
void* user_ptr = gpu_device->mmap(NULL, args.size, 
                                  PROT_READ | PROT_WRITE,
                                  MAP_SHARED, 0, gpu_addr);
```

### 提交 GPU 命令

```cpp
// 构造命令包
struct command_packet cmd = {
    .type = CMD_COMPUTE,
    .data_addr = gpu_addr,
    .size = 1024
};

// 提交命令
gpu_device->write(&cmd, sizeof(cmd));
```

## 开发计划

项目正在积极开发中，当前版本为 **v0.1**。详细的开发计划请参阅 [开发路线图](docs/ROADMAP.md)。

### 近期目标 (Q1-Q2 2026)

- ✅ 核心框架和基础设备支持
- 🔄 测试框架升级（迁移到 GTest）
- 🔄 Linux 兼容层开发（目标 80% 完成度）
- 📝 完善文档系统
- ⚡ 性能优化

### 中期目标 (Q3-Q4 2026)

- 🎯 完整的 Linux 驱动兼容层
- 🌐 网络设备支持
- 💾 存储设备支持
- 🔧 高级调试工具
- 📦 稳定的 v1.0 版本

## 贡献指南

我们欢迎各种形式的贡献！

### 贡献方式

- 🐛 报告 bug
- 💡 提出新功能建议
- 📝 改进文档
- 🔧 提交代码修复
- ✨ 开发新功能

### 开始贡献

1. Fork 本仓库
2. 创建功能分支 (`git checkout -b feature/amazing-feature`)
3. 提交更改 (`git commit -m 'Add amazing feature'`)
4. 推送到分支 (`git push origin feature/amazing-feature`)
5. 创建 Pull Request

详细信息请参阅 [开发指南](docs/development_guide.md)。

## 常见问题

### Q: 为什么需要用户态内核模拟？
A: 用户态模拟允许在不需要 root 权限、不需要内核编译的情况下开发和测试驱动程序，大大降低了开发门槛和风险。

### Q: 性能如何？
A: 用户态模拟会有一定的性能开销，但对于开发和测试场景是可以接受的。我们持续优化以减少开销。

### Q: 支持哪些 Linux API？
A: 我们正在逐步实现常用的 Linux 内核 API。当前支持基础的设备模型、内存管理等，目标是支持 80% 以上的常用 API。

### Q: 可以运行真实的 CUDA 程序吗？
A: 目前还不能完整支持 CUDA，但我们正在开发中。简单的 CUDA 程序已经可以运行。

更多问题请查看 [文档](docs/) 或提交 Issue。

## 许可证

本项目采用 MIT 许可证。详见 [LICENSE](LICENSE) 文件。

## 致谢

感谢所有为本项目做出贡献的开发者！

## 联系方式

- 📧 Email: [项目邮箱]
- 🐛 Issues: [GitHub Issues](https://github.com/chisuhua/UsrLinuxEmu/issues)
- 💬 Discussions: [GitHub Discussions](https://github.com/chisuhua/UsrLinuxEmu/discussions)

---

**当前版本**: v0.1.0  
**最后更新**: 2026-02-10  
**维护者**: UsrLinuxEmu Team
