# UsrLinuxEmu 项目文档

欢迎来到 UsrLinuxEmu 项目文档中心。本文档集提供了完整的开发指南、API 参考和架构说明。

## 📖 文档导航

### 快速开始（新手必读）

如果你是第一次使用 UsrLinuxEmu，从这里开始：

| 文档 | 说明 | 预计时间 |
|------|------|----------|
| [安装指南](01-quickstart/installation.md) | 系统要求和依赖安装 | 10 分钟 |
| [构建指南](01-quickstart/building.md) | 编译项目和插件 | 5 分钟 |
| [第一个示例](01-quickstart/first-example.md) | 运行 GPU 示例程序 | 15 分钟 |

**总计**: 约 30 分钟上手

### 核心文档（理解项目）

深入理解项目架构和核心概念：

| 文档 | 说明 |
|------|------|
| [项目概述](02-core/overview.md) | 项目简介、目标和核心功能 |
| [架构设计](02-core/architecture.md) | 系统架构、模块设计和数据流 |
| [API 参考](06-reference/api-reference.md) | 核心 API 接口文档 |

### 开发指南（日常开发）

开发者的主要参考文档：

| 文档 | 说明 |
|------|------|
| [开发指南](03-development/guide.md) | 环境搭建、代码规范和开发流程 |
| [代码风格](03-development/coding-style.md) | 编码规范和最佳实践 |
| [添加新设备](03-development/adding-devices.md) | 如何扩展新设备类型（待添加） |
| [调试指南](03-development/debugging.md) | 调试技巧和工具（待添加） |

### 构建和测试

编译、测试和 CI/CD 相关：

| 文档 | 说明 |
|------|------|
| [构建系统](04-building/build-system.md) | CMake 配置和编译选项 |
| [测试指南](04-building/testing-guide.md) | 编写和运行测试 |
| [CI/CD](04-building/ci-cd.md) | 持续集成和部署（待添加） |

### 高级主题

深入理解系统的高级特性：

| 文档 | 说明 |
|------|------|
| [GPU 驱动架构](05-advanced/gpu-driver-architecture.md) | GPU 驱动仿真架构详细设计 |
| [插件开发](05-advanced/plugin-development.md) | 开发自定义设备插件（待添加） |
| [性能优化](05-advanced/performance.md) | 性能分析和优化技巧（待添加） |

### 参考资料

快速查阅的 API 和术语：

| 文档 | 说明 |
|------|------|
| [API 参考](06-reference/api-reference.md) | 完整的 API 接口文档 |
| [IOCTL 命令](06-reference/ioctl-commands.md) | 所有 IOCTL 命令参考（待添加） |
| [术语表](06-reference/glossary.md) | 技术术语解释（待添加） |
| [架构决策记录](06-reference/adr.md) | 重要架构决策记录 |

### 🗄️ 归档文档

历史文档和参考资料：

| 文档 | 说明 |
|------|------|
| [归档说明](archive/README.md) | 归档文档使用说明 |
| [规划文档](archive/planning/) | 项目路线图、开发计划等 |
| [其他归档](archive/misc/) | 其他历史文档 |

## 🎯 按角色查找文档

### 新手用户

1. [安装指南](01-quickstart/installation.md)
2. [构建指南](01-quickstart/building.md)
3. [第一个示例](01-quickstart/first-example.md)
4. [项目概述](02-core/overview.md)

### 应用开发者

1. [快速开始](01-quickstart/)
2. [API 参考](06-reference/api-reference.md)
3. [第一个示例](01-quickstart/first-example.md)
4. [调试指南](03-development/debugging.md)

### 驱动开发工程师

1. [架构设计](02-core/architecture.md)
2. [GPU 驱动架构](05-advanced/gpu-driver-architecture.md)
3. [开发指南](03-development/guide.md)
4. [代码风格](03-development/coding-style.md)
5. [添加新设备](03-development/adding-devices.md)

### 系统架构师

1. [架构设计](02-core/architecture.md)
2. [GPU 驱动架构](05-advanced/gpu-driver-architecture.md)
3. [架构决策记录](06-reference/adr.md)
4. [项目概述](02-core/overview.md)

### 测试工程师

1. [测试指南](04-building/testing-guide.md)
2. [构建系统](04-building/build-system.md)
3. [代码风格](03-development/coding-style.md)

## 📊 文档状态

| 分类 | 文档数 | 完成度 | 最后更新 |
|------|--------|--------|----------|
| 快速开始 | 4 | 75% | 2026-03-23 |
| 核心文档 | 3 | 100% | 2026-03-23 |
| 开发指南 | 4 | 50% | 2026-03-23 |
| 构建和测试 | 3 | 67% | 2026-03-23 |
| 高级主题 | 3 | 33% | 2026-03-23 |
| 参考资料 | 4 | 50% | 2026-03-23 |
| 归档文档 | 5 | 100% | 2026-03-23 |

**总体进度**: 约 65% 完成

## 🔧 文档维护

### 添加新文档

1. 确定文档类型（快速开始/核心/开发/等）
2. 放到对应目录
3. 更新该目录的 `index.md`
4. 更新本文档（`README.md`）

### 更新现有文档

1. 在文档末尾更新"最后更新日期"
2. 如有重大变更，在文档开头添加变更日志
3. 更新相关索引文件

### 文档规范

- 使用 Markdown 格式
- 文件命名使用 `kebab-case`（如 `coding-style.md`）
- 每个目录包含 `index.md` 作为导航
- 所有文档必须有"最后更新"日期
- 代码示例必须可编译运行（如适用）

## 📝 变更日志

### 2026-03-23 - 文档架构重构

**重大变更**: 完成文档系统重构，采用分类导航结构

**新增**:
- 创建 6 个分类目录（01-quickstart 到 06-reference）
- 新增快速开始系列（安装、构建、第一个示例）
- 新增代码风格指南
- 创建完整的导航索引系统

**移动**:
- ROADMAP.md 等规划文档移至 archive/planning/
- SUMMARY_CN.md 移至 archive/misc/

**修复**:
- 移除所有 file:///mnt/ubuntu/... 链接
- 统一文档格式和命名规范

### 2026-02-10 - 初始文档集

**新增**:
- 基础文档集（architecture.md, overview.md, 等）
- GPU 驱动架构文档
- 架构决策记录

## 📞 反馈和支持

如有文档相关问题：

- 📧 邮件：项目维护者
- 🐛 Issue: [GitHub Issues](https://github.com/chisuhua/UsrLinuxEmu/issues)
- 💬 讨论：[GitHub Discussions](https://github.com/chisuhua/UsrLinuxEmu/discussions)

---

**文档维护者**: UsrLinuxEmu Team  
**最后更新**: 2026-03-23  
**文档版本**: 2.0（重构版）
