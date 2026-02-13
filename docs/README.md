# UsrLinuxEmu 项目文档

欢迎阅读 UsrLinuxEmu 项目文档。本文档集提供了项目的完整说明，包括架构设计、开发指南和使用说明。

## 文档导航

### 📚 基础文档

1. **[项目概述](overview.md)** - 项目简介、目标和核心功能
2. **[架构设计](architecture.md)** - 系统架构、模块设计和数据流
3. **[架构决策记录](ADR.md)** - 重要架构决策的记录和说明
4. **[构建系统](build_system.md)** - 构建配置和编译说明

### 🛠️ 开发文档

5. **[开发指南](development_guide.md)** - 环境搭建、代码规范和开发流程
6. **[贡献指南](../CONTRIBUTING.md)** - 如何为项目做贡献
7. **[API 参考](api_reference.md)** - 核心 API 接口说明
8. **[测试指南](testing_guide.md)** - 测试框架、编写测试和运行测试

### 📋 计划文档

9. **[开发路线图](ROADMAP.md)** - 项目愿景、短期和长期计划
10. **[开发实施计划](development_implementation_plan.md)** - 详细的开发计划和时间表
11. **[Linux 驱动兼容性计划](linux_driver_compatibility_plan.md)** - Linux 内核驱动兼容层设计
12. **[Linux 驱动兼容性测试计划](linux_driver_compatibility_test_plan.md)** - 兼容性测试方案

## 快速开始

### 构建项目

```bash
# 克隆仓库
git clone <repository-url>
cd UsrLinuxEmu

# 构建项目
./build.sh

# 或手动构建
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### 运行测试

```bash
cd build
make test

# 或运行特定测试
./bin/test_gpu_submit
```

### 运行 CLI 工具

```bash
./run_cli.sh
# 或
./build/bin/cli_tool
```

## 项目结构速览

```
UsrLinuxEmu/
├── docs/                   # 项目文档
├── include/                # 头文件
│   ├── kernel/            # 内核框架头文件
│   └── linux_compat/      # Linux 兼容层头文件
├── src/                    # 源代码
│   └── kernel/            # 内核框架实现
├── drivers/                # 设备驱动实现
│   └── gpu/               # GPU 驱动
├── simulator/              # 设备模拟器
│   └── gpu/               # GPU 模拟器
├── tests/                  # 测试代码
├── tools/                  # 工具程序
│   └── cli/               # CLI 工具
└── plugins/                # 插件示例
```

## 核心概念

### 设备抽象框架
UsrLinuxEmu 提供了统一的设备抽象接口，支持多种设备类型：
- 串口设备 (Serial Device)
- 内存设备 (Memory Device)
- GPGPU 设备 (GPGPU Device)

### 虚拟文件系统 (VFS)
模拟 Linux 内核的 VFS 机制，支持设备节点的注册和查找。

### 插件化架构
支持动态加载设备插件，实现模块化和可扩展性。

### GPGPU 支持
完整的 GPU 驱动模拟，包括：
- 内存管理 (Buddy Allocator)
- 命令队列 (Ring Buffer)
- 地址空间管理
- GPU 指令模拟

## 技术栈

- **语言**: C++17
- **构建系统**: CMake 3.14+
- **测试框架**: 自定义测试 (计划迁移到 GTest)
- **日志**: 自定义日志系统

## 贡献指南

请参阅 [开发指南](development_guide.md) 了解如何为项目做贡献。

## 许可证

请参阅项目根目录的 LICENSE 文件。

## 联系方式

如有问题或建议，请通过以下方式联系：
- 提交 GitHub Issue
- 发送邮件到项目维护者

---

**最后更新**: 2026-02-10
