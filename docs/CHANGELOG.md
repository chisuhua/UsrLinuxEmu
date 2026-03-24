# 文档变更日志

本文档记录 UsrLinuxEmu 项目文档系统的所有重大变更。

---

## 2026-03-23 - 文档架构重构（版本 2.0）

**类型**: 重大重构  
**影响范围**: 所有文档  
**执行者**: Sisyphus Agent

### 变更概述

完成项目文档系统的全面重构，采用分类导航结构，将活跃文档与历史文档分离。

### 新增内容

#### 目录结构

创建 6 个分类目录和归档系统：

```
docs/
├── 01-quickstart/          # 快速开始（新手指南）
├── 02-core/                # 核心文档（项目理解）
├── 03-development/         # 开发指南（日常开发）
├── 04-building/            # 构建和测试
├── 05-advanced/            # 高级主题
├── 06-reference/           # 参考资料（快速查阅）
└── archive/                # 归档文档（历史记录）
    ├── planning/           # 规划文档
    └── misc/               # 其他归档
```

#### 新增文档

**快速开始系列**（4 个文件）:
- `01-quickstart/index.md` - 快速开始索引
- `01-quickstart/installation.md` - 安装指南（新增）
- `01-quickstart/building.md` - 构建指南（新增）
- `01-quickstart/first-example.md` - 第一个 GPU 示例（新增）

**开发指南系列**（2 个新增）:
- `03-development/index.md` - 开发索引
- `03-development/coding-style.md` - 代码风格指南（新增）

**索引文件**（7 个）:
- `02-core/index.md`
- `03-development/index.md`
- `04-building/index.md`
- `05-advanced/index.md`
- `06-reference/index.md`
- `archive/README.md`
- `archive/planning/index.md`
- `archive/misc/index.md`

### 移动内容

#### 归档文档（5 个文件）

以下文档移至 `archive/` 目录：

| 原位置 | 新位置 | 原因 |
|--------|--------|------|
| `docs/ROADMAP.md` | `archive/planning/ROADMAP.md` | 长期规划，非日常参考 |
| `docs/development_implementation_plan.md` | `archive/planning/development_implementation_plan.md` | 开发计划，历史价值 |
| `docs/linux_driver_compatibility_plan.md` | `archive/planning/linux_driver_compatibility_plan.md` | 专项计划 |
| `docs/linux_driver_compatibility_test_plan.md` | `archive/planning/linux_driver_compatibility_test_plan.md` | 专项测试计划 |
| `docs/SUMMARY_CN.md` | `archive/misc/SUMMARY_CN.md` | 摘要文档，内容重复 |

#### 重组文档（8 个文件）

以下文档复制到新分类目录：

| 原文档 | 新位置 | 分类 |
|--------|--------|------|
| `overview.md` | `02-core/overview.md` | 核心文档 |
| `architecture.md` | `02-core/architecture.md` | 核心文档 |
| `api_reference.md` | `06-reference/api-reference.md` | 参考资料 |
| `development_guide.md` | `03-development/guide.md` | 开发指南 |
| `build_system.md` | `04-building/build-system.md` | 构建测试 |
| `testing_guide.md` | `04-building/testing-guide.md` | 构建测试 |
| `gpu_driver_architecture.md` | `05-advanced/gpu-driver-architecture.md` | 高级主题 |
| `ADR.md` | `06-reference/adr.md` | 参考资料 |

### 修复内容

#### 链接修复

修复以下文档中的 `file:///mnt/ubuntu/...` 链接：

- `overview.md`
- `api_reference.md`
- `development_guide.md`
- `testing_guide.md`
- `build_system.md`

**修复方式**: 移除 `file:///mnt/ubuntu/chisuhua/github/UsrLinuxEmu/` 前缀，改为相对路径或纯文本。

### 更新内容

#### 主索引文件

- `docs/README.md` - 完全重写，新增：
  - 分类导航表格
  - 按角色查找指南（新手/开发者/架构师/测试工程师）
  - 文档状态统计表
  - 变更日志章节

#### 根目录 README

- `README.md` - 更新文档引用部分：
  - 改为指向新的分类目录
  - 添加快速开始系列链接
  - 添加完整文档导航链接

### 文档规范

建立新的文档维护规范：

1. **命名规范**: 使用 `kebab-case`（如 `coding-style.md`）
2. **目录结构**: 每个分类目录包含 `index.md`
3. **日期标注**: 所有文档必须有"最后更新"日期
4. **归档策略**: 历史文档放入 `archive/`，保持活跃文档精简

### 影响评估

**破坏性变更**:
- ⚠️ 旧文档链接失效（如 `docs/ROADMAP.md`）
- ⚠️ 需要通过新的索引文件访问文档

**迁移指南**:
- 使用新的 `docs/README.md` 作为入口
- 归档文档仍可通过 `archive/` 目录访问

### 统计数据

**变更规模**:
- 新增文件：13 个
- 移动文件：13 个（5 个归档 + 8 个重组）
- 修复文件：5 个
- 更新文件：2 个

**文档状态**:
- 总计文档：约 25 个（含新增和归档）
- 活跃文档：17 个
- 归档文档：5 个
- 完成度：约 65%（截至 2026-03-23）

### 待办事项

以下文档标记为"待添加"：

- `03-development/adding-devices.md`
- `03-development/debugging.md`
- `04-building/ci-cd.md`
- `05-advanced/plugin-development.md`
- `05-advanced/performance.md`
- `06-reference/ioctl-commands.md`
- `06-reference/glossary.md`

### 后续计划

**短期（1-2 周）**:
- [ ] 完成标注为"待添加"的文档
- [ ] 更新所有文档中的交叉引用
- [ ] 添加文档搜索功能（如 Algolia）

**中期（1 个月）**:
- [ ] 建立文档审查流程
- [ ] 添加文档质量检查（链接检查器）
- [ ] 创建文档贡献指南

**长期（3 个月）**:
- [ ] 迁移到文档站点生成器（如 Docusaurus）
- [ ] 添加多语言支持
- [ ] 建立文档版本管理

---

## 2026-02-10 - 初始文档集（版本 1.0）

**类型**: 初始版本  
**影响范围**: N/A（首次发布）

### 新增内容

创建基础文档集：

- `README.md` - 项目主文档
- `docs/README.md` - 文档索引
- `docs/overview.md` - 项目概述
- `docs/architecture.md` - 架构设计
- `docs/development_guide.md` - 开发指南
- `docs/api_reference.md` - API 参考
- `docs/build_system.md` - 构建系统
- `docs/testing_guide.md` - 测试指南
- `docs/ROADMAP.md` - 开发路线图
- `docs/development_implementation_plan.md` - 实施计划
- `docs/gpu_driver_architecture.md` - GPU 驱动架构
- `docs/ADR.md` - 架构决策记录
- `docs/SUMMARY_CN.md` - 中文摘要

### 特点

- 文档采用扁平结构（所有文件在 `docs/` 根目录）
- 包含完整的项目介绍和开发指南
- 建立了架构决策记录（ADR）制度

---

**维护者**: UsrLinuxEmu Team  
**文档版本**: 2.0  
**最后更新**: 2026-03-23
