# UsrLinuxEmu 文档清理计划

## 任务概述

**目标**: 清理和重组项目文档，将过时或参考性文档归档，建立清晰的开发用文档架构

**创建时间**: 2026-03-23  
**预计完成**: 2026-03-23  
**执行者**: Sisyphus Agent

---

## 当前文档状况评估

### 现有文档清单

```
docs/
├── README.md                              # 文档索引 - 保留并更新
├── overview.md                            # 项目概述 - 保留并更新
├── architecture.md                        # 架构设计 - 保留并更新
├── development_guide.md                   # 开发指南 - 保留并更新
├── api_reference.md                       # API 参考 - 保留并更新
├── build_system.md                        # 构建系统 - 保留并更新
├── testing_guide.md                       # 测试指南 - 保留并更新
├── ROADMAP.md                             # 开发路线图 - 移动到归档
├── development_implementation_plan.md     # 开发实施计划 - 移动到归档
├── linux_driver_compatibility_plan.md     # Linux 驱动兼容性计划 - 移动到归档
├── linux_driver_compatibility_test_plan.md # 兼容性测试计划 - 移动到归档
├── gpu_driver_architecture.md             # GPU 驱动仿真架构 - 保留并更新
├── ADR.md                                 # 架构决策记录 - 保留并更新
└── SUMMARY_CN.md                          # 中文摘要 - 移动到归档
```

### 文档分类评估

#### 🟢 活跃文档（保留在 docs/，需要更新）

| 文档 | 状态 | 问题 | 操作 |
|------|------|------|------|
| README.md (根目录) | 良好 | 部分信息过时 | 更新版本信息 |
| docs/README.md | 良好 | 索引结构需调整 | 更新导航结构 |
| docs/overview.md | 良好 | 包含 file://链接 | 修复链接格式 |
| docs/architecture.md | 良好 | 内容完整 | 微调格式 |
| docs/development_guide.md | 良好 | 部分示例需更新 | 更新示例代码 |
| docs/api_reference.md | 一般 | API 可能不完整 | 补充缺失 API |
| docs/build_system.md | 良好 | 内容准确 | 无需大改 |
| docs/testing_guide.md | 待检查 | 需确认内容 | 阅读后决定 |
| docs/gpu_driver_architecture.md | 待检查 | 需确认内容 | 阅读后决定 |
| docs/ADR.md | 待检查 | 需确认内容 | 阅读后决定 |

#### 🟡 参考文档（移动到 docs/archive/）

| 文档 | 原因 | 目标位置 |
|------|------|----------|
| ROADMAP.md | 长期规划，非日常开发参考 | docs/archive/planning/ |
| development_implementation_plan.md | 开发计划，历史参考价值 | docs/archive/planning/ |
| linux_driver_compatibility_plan.md | 专项计划，参考性质 | docs/archive/planning/ |
| linux_driver_compatibility_test_plan.md | 专项测试计划 | docs/archive/planning/ |
| SUMMARY_CN.md | 摘要文档，重复内容 | docs/archive/ |

#### 🔴 问题文档（需要修复）

| 文档 | 问题 | 修复方案 |
|------|------|----------|
| overview.md | 包含 file:///mnt/ubuntu/... 链接 | 改为相对链接或移除 |
| api_reference.md | 包含 file:///mnt/ubuntu/... 链接 | 改为相对链接或移除 |
| development_guide.md | 包含 file:///mnt/ubuntu/... 链接 | 改为相对链接或移除 |

---

## 新文档架构设计

### 目标结构

```
docs/
├── README.md                          # 文档索引（更新版）
│
├── 01-quickstart/                     # 快速开始
│   ├── index.md                       # 快速开始索引
│   ├── installation.md                # 安装指南
│   ├── building.md                    # 构建指南
│   └── first-example.md               # 第一个示例
│
├── 02-core/                           # 核心文档
│   ├── index.md                       # 核心文档索引
│   ├── overview.md                    # 项目概述（从 overview.md 更新）
│   ├── architecture.md                # 架构设计（保留）
│   └── api-reference.md               # API 参考（从 api_reference.md 更新）
│
├── 03-development/                    # 开发指南
│   ├── index.md                       # 开发索引
│   ├── guide.md                       # 开发指南（从 development_guide.md 更新）
│   ├── coding-style.md                # 代码风格（新增）
│   ├── adding-devices.md              # 添加新设备（从 guide 提取）
│   └── debugging.md                   # 调试指南（从 guide 提取）
│
├── 04-building/                       # 构建和测试
│   ├── index.md                       # 构建索引
│   ├── build-system.md                # 构建系统（从 build_system.md 更新）
│   ├── testing-guide.md               # 测试指南（保留）
│   └── ci-cd.md                       # CI/CD（新增，如果适用）
│
├── 05-advanced/                       # 高级主题
│   ├── index.md                       # 高级索引
│   ├── gpu-driver-architecture.md     # GPU 驱动架构（保留）
│   ├── plugin-development.md          # 插件开发（新增）
│   └── performance.md                 # 性能优化（新增）
│
├── 06-reference/                      # 参考资料
│   ├── index.md                       # 参考索引
│   ├── api-reference.md               # 完整 API 参考
│   ├── ioctl-commands.md              # IOCTL 命令参考（新增）
│   └── glossary.md                    # 术语表（新增）
│
└── archive/                           # 归档文档
    ├── README.md                      # 归档说明
    ├── planning/                      # 规划文档
    │   ├── ROADMAP.md
    │   ├── development_implementation_plan.md
    │   ├── linux_driver_compatibility_plan.md
    │   └── linux_driver_compatibility_test_plan.md
    └── misc/                          # 其他归档
        └── SUMMARY_CN.md
```

### 文档编号规则

- `01-quickstart/` - 新手入门，最简单内容
- `02-core/` - 核心概念，理解项目必备
- `03-development/` - 开发相关，日常开发参考
- `04-building/` - 构建测试，CI/CD 相关
- `05-advanced/` - 高级主题，深入理解
- `06-reference/` - 参考资料，查询用途
- `archive/` - 历史文档，参考价值

---

## 实施计划

### 阶段 1: 准备工作 (15 分钟)

**任务**:
- [ ] 创建归档目录结构
- [ ] 创建新目录结构
- [ ] 创建各目录的 index.md 模板
- [ ] 备份当前文档状态

**交付物**:
- 完整的目录结构
- 所有 index.md 文件

### 阶段 2: 文档归档 (15 分钟)

**任务**:
- [ ] 移动 ROADMAP.md 到 archive/planning/
- [ ] 移动 development_implementation_plan.md 到 archive/planning/
- [ ] 移动 linux_driver_compatibility_plan.md 到 archive/planning/
- [ ] 移动 linux_driver_compatibility_test_plan.md 到 archive/planning/
- [ ] 移动 SUMMARY_CN.md 到 archive/misc/
- [ ] 创建 archive/README.md 说明文档

**交付物**:
- 归档文档就位
- 归档说明文档

### 阶段 3: 文档修复 (30 分钟)

**任务**:
- [ ] 修复 overview.md 中的 file:// 链接
- [ ] 修复 api_reference.md 中的 file:// 链接
- [ ] 修复 development_guide.md 中的 file:// 链接
- [ ] 检查并修复其他文档中的无效链接

**交付物**:
- 所有链接修复完成
- 文档内容准确

### 阶段 4: 文档重组 (45 分钟)

**任务**:
- [ ] 复制 overview.md 到 02-core/overview.md
- [ ] 复制 architecture.md 到 02-core/architecture.md
- [ ] 复制 api_reference.md 到 06-reference/api-reference.md
- [ ] 复制 development_guide.md 到 03-development/guide.md
- [ ] 复制 build_system.md 到 04-building/build-system.md
- [ ] 复制 testing_guide.md 到 04-building/testing-guide.md
- [ ] 复制 gpu_driver_architecture.md 到 05-advanced/gpu-driver-architecture.md
- [ ] 复制 ADR.md 到 06-reference/adr.md

**交付物**:
- 文档按新结构组织

### 阶段 5: 新增内容 (45 分钟)

**任务**:
- [ ] 创建 01-quickstart/ 系列文档
- [ ] 创建 03-development/coding-style.md
- [ ] 创建 03-development/adding-devices.md
- [ ] 创建 03-development/debugging.md
- [ ] 创建 05-advanced/plugin-development.md
- [ ] 创建 06-reference/ioctl-commands.md
- [ ] 创建 06-reference/glossary.md

**交付物**:
- 新增文档完成

### 阶段 6: 更新索引 (30 分钟)

**任务**:
- [ ] 更新 docs/README.md 为新索引结构
- [ ] 更新所有子目录的 index.md
- [ ] 更新根目录 README.md 的文档引用
- [ ] 添加文档导航和交叉引用

**交付物**:
- 完整的导航系统

### 阶段 7: 验证和清理 (15 分钟)

**任务**:
- [ ] 检查所有 Markdown 链接是否有效
- [ ] 检查文档格式是否一致
- [ ] 验证目录结构符合设计
- [ ] 创建文档变更日志

**交付物**:
- 验证报告
- 变更日志

---

## 成功标准

1. **结构清晰**: 文档按用途和读者类型清晰分组
2. **导航便捷**: 通过 index.md 文件可快速定位任何文档
3. **归档完整**: 历史文档保存在 archive/，不丢失任何内容
4. **链接有效**: 所有 Markdown 链接指向正确位置
5. **格式统一**: 所有文档使用一致的格式和风格
6. **开发友好**: 开发者可在 5 分钟内找到所需信息

---

## 风险和挑战

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| 文档链接断裂 | 高 | 仔细检查所有链接，使用相对路径 |
| 内容重复 | 中 | 建立清晰的文档职责边界 |
| 归档混乱 | 中 | archive/ 内保持清晰分类 |
| 更新不及时 | 低 | 在文档中注明最后更新日期 |

---

## 度量指标

- 文档总数：14 (活跃) + 5 (归档) = 19
- 新增文档：~7
- 目录层级：最多 3 层 (docs/ -> category/ -> file.md)
- 导航页面：7 (每个分类一个 index.md)

---

## 维护指南

### 添加新文档

1. 确定文档类型（quickstart/core/development/等）
2. 放到对应目录
3. 更新该目录的 index.md
4. 更新 docs/README.md

### 更新现有文档

1. 在文档末尾更新"最后更新日期"
2. 如有重大变更，在文档开头添加变更日志
3. 更新相关索引文件

### 归档旧文档

1. 移动到 archive/对应子目录
2. 在原文档位置添加重定向说明
3. 更新 docs/README.md

---

**计划版本**: 1.0  
**创建时间**: 2026-03-23  
**下次审查**: 2026-04-23
