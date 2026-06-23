# UsrLinuxEmu 项目文档

> **最后验证**: 2026-06-19 (change `adr-governance-refresh-v2`)
>
> **权威架构说明**: [docs/02_architecture/post-refactor-architecture.md](02_architecture/post-refactor-architecture.md)（SSOT）+ 顶层 [README.md](../README.md) + [AGENTS.md](../AGENTS.md)
>
> 本文档反映 Phase 1.5 / Phase 2 重构后的状态。如发现与上述 SSOT 冲突，以它们为准。

欢迎来到 UsrLinuxEmu 文档中心。本文档集提供开发指南、API 参考、架构说明与决策记录。

## 📖 文档导航

### 快速开始（新手必读）

第一次使用 UsrLinuxEmu，从这里开始：

| 文档 | 说明 | 预计时间 |
|------|------|----------|
| [安装指南](01-quickstart/installation.md) | 系统要求和依赖 | 10 分钟 |
| [构建指南](01-quickstart/building.md) | 编译项目和插件 | 5 分钟 |
| [第一个示例](01-quickstart/first-example.md) | 运行 GPU 示例程序 | 15 分钟 |

**总计**: 约 30 分钟上手

### 架构（理解项目）

深入理解项目架构和重构演进：

| 文档 | 说明 |
|------|------|
| [重构后架构 SSOT](02_architecture/post-refactor-architecture.md) | **权威架构说明**（Phase 2 后）|
| [架构概述](02_architecture/overview.md) | 项目简介、目标与核心功能 |
| [架构设计](02_architecture/architecture.md) | 系统架构、模块设计、数据流 |
| [重构历史](02_architecture/refactor-history.md) | Phase 0 → 1 → 1.5 → 2 演进记录 |

### 开发指南（日常开发）

开发者的主要参考：

| 文档 | 说明 |
|------|------|
| [开发指南](03-development/guide.md) | 环境搭建、代码规范和开发流程 |
| [代码风格](03-development/coding-style.md) | 编码规范和最佳实践（snake_case）|
| [添加新设备](03-development/adding-devices.md) | 如何扩展新设备类型 |
| [调试指南](03-development/debugging.md) | 调试技巧和工具 |

### 构建和测试

编译、测试和 CI/CD：

| 文档 | 说明 |
|------|------|
| [构建系统](04-building/build_system.md) | CMake 配置和编译选项 |
| [测试指南](04-building/testing_guide.md) | Catch2 测试编写和运行 |
| [CI/CD](04-building/ci-cd.md) | 持续集成和部署 |

### 高级主题

深入理解系统的高级特性：

| 文档 | 说明 |
|------|------|
| [GPU 驱动架构](05-advanced/gpu_driver_architecture.md) | GPU 驱动仿真架构详细设计 |
| [插件开发](05-advanced/plugin-development.md) | 开发自定义设备插件（ModuleLoader）|
| [性能优化](05-advanced/performance.md) | 性能分析和优化技巧 |

### 参考资料

快速查阅的 API 和术语：

| 文档 | 说明 |
|------|------|
| [API 参考](06-reference/api-reference.md) | 完整 API 接口文档 |
| [IOCTL 命令](06-reference/ioctl-commands.md) | System C IOCTL 命令参考 |
| [术语表](06-reference/glossary.md) | 技术术语解释 |
| [架构决策记录](00_adr/README.md) | 所有 ADR（001–031，含 022、025–031 占位/Deferred）|

### TaskRunner 集成

与外部子模块的对接：

| 文档 | 说明 |
|------|------|
| [TaskRunner 索引](07-integration/taskrunner-index.md) | TaskRunner 与 UsrLinuxEmu 的对接 |
| [GPU API 参考](07-integration/gpu-api-reference.md) | TaskRunner 侧的 GPU API |
| [GPU 集成指南](07-integration/gpu-integration-guide.md) | 集成步骤和约定 |
| [GPU 调试 FAQ](07-integration/gpu-debug-faq.md) | 常见问题与排查 |

### 规划与归档

进行中的计划和历史文档：

| 类别 | 说明 |
|------|------|
| [架构演进路线图](roadmap/README.md) | 4 阶段路线图 + 终态蓝图（3 区分架构原则）|
| [ADR 索引](00_adr/README.md) | 含 ADR-036（3 区分架构原则，✅ Accepted）|
| [归档说明](archive/README.md) | 归档文档使用说明 |
| [规划归档](archive/planning/) | 历史项目路线图与开发计划 |
| [其他归档](archive/misc/) | 其他历史文档 |

## 🎯 按角色查找文档

### 新手用户

1. [安装指南](01-quickstart/installation.md)
2. [构建指南](01-quickstart/building.md)
3. [第一个示例](01-quickstart/first-example.md)
4. [架构概述](02_architecture/overview.md)

### 应用开发者

1. [快速开始目录](01-quickstart/index.md)
2. [API 参考](06-reference/api-reference.md)
3. [第一个示例](01-quickstart/first-example.md)
4. [调试指南](03-development/debugging.md)

### 驱动开发工程师

1. [重构后架构 SSOT](02_architecture/post-refactor-architecture.md)
2. [GPU 驱动架构](05-advanced/gpu_driver_architecture.md)
3. [开发指南](03-development/guide.md)
4. [代码风格](03-development/coding-style.md)
5. [添加新设备](03-development/adding-devices.md)

### 系统架构师

1. [重构后架构 SSOT](02_architecture/post-refactor-architecture.md)
2. [GPU 驱动架构](05-advanced/gpu_driver_architecture.md)
3. [ADR 索引](00_adr/README.md)
4. [架构概述](02_architecture/overview.md)

### 测试工程师

1. [测试指南](04-building/testing_guide.md)（Catch2）
2. [构建系统](04-building/build_system.md)
3. [代码风格](03-development/coding-style.md)

### TaskRunner 集成方

1. [TaskRunner 索引](07-integration/taskrunner-index.md)
2. [GPU API 参考](07-integration/gpu-api-reference.md)
3. [重构后架构 §1.6 IOCTL 体系](02_architecture/post-refactor-architecture.md)

## 📊 文档状态

| 分类 | 文档数 | 完成度 | 最后更新 |
|------|--------|--------|----------|
| 快速开始 | 4 | 90% | 2026-06-16 |
| 架构（含 SSOT）| 6 | 95% | 2026-06-16 |
| 开发指南 | 6 | 80% | 2026-06-16 |
| 构建和测试 | 4 | 85% | 2026-06-16 |
| 高级主题 | 4 | 75% | 2026-06-16 |
| 参考资料 | 4 | 80% | 2026-06-16 |
| TaskRunner 集成 | 5 | 80% | 2026-06-16 |
| ADR | 31 | 95% | 2026-06-19 |

**总体进度**: 约 **85%** 完成（Phase 2 + P0 cleanup 后）

### ADR 编号说明

`docs/00_adr/` 当前收录 **31 份** ADR，编号范围 **001–031**：

- ✅ 已接受（22 个）：001–010、015–024、027、031
- ⏸️ 显式 Deferred (Phase 3+)：025/026/028/029/030
- 🔄 提议中：011/012/013/014（合法 backlog：多进程/性能/错误处理/日志）

详见 [ADR 索引](00_adr/README.md) 和重构后架构 §2.7。

## 🔧 文档维护

### 添加新文档

1. 确定文档类型（快速开始 / 架构 / 开发 / 等）
2. 放到对应目录
3. 更新该目录的 `index.md`
4. 更新本文档（`docs/README.md`）

### 更新现有文档

1. 在文档顶部更新"最后验证日期"和"对应 commit"
2. 如有重大变更，在文档开头添加变更说明
3. 更新相关索引文件

### 文档规范

- 使用 Markdown 格式
- 文件命名使用 **`snake_case`**（如 `build_system.md`、`testing_guide.md`）
- 每个目录包含 `index.md` 作为导航
- 所有文档顶部标注"最后验证日期"和"对应 commit hash"
- 代码示例必须可编译运行（如适用）
- **编码规范**：类名 `PascalCase`、函数/变量 `snake_case`、成员变量 `snake_case_` 后缀（详见 [代码风格](03-development/coding-style.md) 与 [AGENTS.md](../AGENTS.md)）

## 📝 变更日志

### 2026-06-16 - P0 文档清理 + SSOT 建立

**重大变更**: 完成 Phase 2 重构后的文档审计与修复，建立 post-refactor-architecture.md 作为权威架构 SSOT。

**修复**:
- 替换所有 kebab-case 链接为 snake_case（34 处，11 个文件）
- 删除对不存在文件的引用（如 `06-reference/adr.md` → `00_adr/README.md`）
- 更新完成度数字 65% → 85%
- 标注 ADR 编号缺失（022 占位）
- 新增 [重构后架构 SSOT](02_architecture/post-refactor-architecture.md) 交叉引用
- 编码规范统一为 snake_case（与 AGENTS.md 对齐）
- 测试框架声明统一为 Catch2

**保留**:
- 文档分类目录结构（01-quickstart 到 07-integration）
- 按角色导航章节
- 归档目录边界（`docs/archive/` vs 项目根 `/archive/`）

### 2026-03-23 - 文档架构重构

**新增**: 6 个分类目录、快速开始系列、代码风格指南、导航索引系统。

**移动**: ROADMAP.md 等规划文档移至 `archive/planning/`，SUMMARY_CN.md 移至 `archive/misc/`。

### 2026-02-10 - 初始文档集

**新增**: 基础文档集（architecture.md、overview.md 等）、GPU 驱动架构文档、架构决策记录。

## 📞 反馈和支持

如有文档相关问题：

- 🐛 Issue: [GitHub Issues](https://github.com/chisuhua/UsrLinuxEmu/issues)
- 💬 讨论：[GitHub Discussions](https://github.com/chisuhua/UsrLinuxEmu/discussions)
- 📧 邮件：项目维护者

---

**文档维护者**: UsrLinuxEmu Team
**最后验证**: 2026-06-19
**对应 change**: `adr-governance-refresh-v2`
**文档版本**: 3.1（ADR 治理刷新 v2）