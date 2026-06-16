# 架构决策记录 (Architecture Decision Records)

本文档目录包含 UsrLinuxEmu 项目中所有已通过和提议中的架构决策记录。

## ADR 索引

| 编号 | 标题 | 状态 | 日期 |
|------|------|------|------|
| [adr-001](adr-001-user-mode-emulation.md) | 采用用户态模拟而非内核模块 | ✅ 已接受 | 2025-12 |
| [adr-002](adr-002-cpp17-language.md) | 采用 C++17 作为开发语言 | ✅ 已接受 | 2025-12 |
| [adr-003](adr-003-plugin-architecture.md) | 采用插件化架构 | ✅ 已接受 | 2025-12 |
| [adr-004](adr-004-buddy-allocator.md) | 使用 Buddy Allocator 管理 GPU 内存 | ✅ 已接受 | 2025-12 |
| [adr-005](adr-005-ring-buffer.md) | 使用 Ring Buffer 管理 GPU 命令队列 | ✅ 已接受 | 2025-12 |
| [adr-006](adr-006-layered-architecture.md) | 采用分层架构设计 | ✅ 已接受 | 2025-12 |
| [adr-007](adr-007-cmake-build-system.md) | 使用 CMake 作为构建系统 | ✅ 已接受 | 2025-12 |
| [adr-008](adr-008-linux-api-compat.md) | 提供 Linux 内核 API 兼容层 | ✅ 已接受 | 2026-01 |
| [adr-009](adr-009-singleton-pattern.md) | 采用单例模式实现核心服务 | ✅ 已接受 | 2026-01 |
| [adr-010](adr-010-gtest-migration.md) | 测试框架选型 — Catch2（最终采用）vs GTest | ✅ 已接受 | 2026-02 |
| [adr-011](adr-011-multiprocess-support.md) | 多进程支持方案 | 🔄 提议中 | 2026-03 |
| [adr-012](adr-012-performance-optimization.md) | 性能优化策略 | 🔄 提议中 | 2026-03 |
| [adr-013](adr-013-error-handling-strategy.md) | 错误处理策略 | 🔄 提议中 | 2026-03 |
| [adr-014](adr-014-logging-enhancement.md) | 日志系统增强 | 🔄 提议中 | 2026-03 |
| [adr-015](adr-015-gpu-ioctl-unification.md) | GPU IOCTL 接口统一 | ✅ 已接受 | 2026-04 |
| [adr-016](adr-016-gpu-memory-domain.md) | GPU Memory Domain 模型 | ✅ 已接受 | 2026-04 |
| [adr-017](adr-017-gpfifo-queue-abstraction.md) | GPFIFO/Queue 抽象 | ✅ 已接受 | 2026-04 |
| [adr-018](adr-018-driver-sim-separation.md) | 驱动/仿真代码分离策略 | ✅ 已接受 | 2026-05 |
| [adr-019](adr-019-drm-gem-ttm-alignment.md) | DRM/GEM/TTM 标准接口对齐路径 | ✅ 已接受 | 2026-05 |
| [adr-020](adr-020-libgpu-core-extraction.md) | libgpu_core 算法核心提取 | ✅ 已接受 | 2026-05 |
| [adr-021](adr-021-hardware-puller.md) | Hardware Puller GPFIFO 状态机构架 | ✅ 已接受 | 2026-05 |
| [adr-023](adr-023-hal-interface.md) | 仿真层接口契约 (HAL) | ✅ 已接受 | 2026-05 |
| [adr-024](adr-024-user-mode-queue-submission.md) | 用户态队列命令提交架构 | 🔄 提议中 | 2026-05 |
| [adr-022](adr-022-gpu-compute-unit-emulation.md) | GPU 计算单元仿真 | 🔄 提议中 (Phase 3+ 占位骨架) | 2026-06 |
| [adr-025](adr-025-phase3-placeholder.md) | Phase 3+ 议题占位 | 🔄 提议中 (Phase 3+ 占位骨架) | 2026-06 |
| [adr-026](adr-026-phase3-placeholder.md) | Phase 3+ 议题占位 | 🔄 提议中 (Phase 3+ 占位骨架) | 2026-06 |
| [adr-027](adr-027-linux-compat-strategy.md) | Linux 内核兼容层扩展策略 | 🔄 提议中 (Phase 3+ 占位骨架) | 2026-06 |
| [adr-028](adr-028-phase3-placeholder.md) | Phase 3+ 议题占位 | 🔄 提议中 (Phase 3+ 占位骨架) | 2026-06 |
| [adr-029](adr-029-phase3-placeholder.md) | Phase 3+ 议题占位 | 🔄 提议中 (Phase 3+ 占位骨架) | 2026-06 |
| [adr-030](adr-030-phase3-placeholder.md) | Phase 3+ 议题占位 | 🔄 提议中 (Phase 3+ 占位骨架) | 2026-06 |
| [adr-031](adr-031-ttm-migration-priority.md) | TTM 迁移实施优先级 | 🔄 提议中 (Phase 3+ 占位骨架) | 2026-06 |

## ADR 状态说明

| 状态 | 说明 |
|------|------|
| ✅ 已接受 | 已通过评审，正式采用的决策 |
| 🔄 提议中 | 正在评审或等待实现的决策 |
| ⚠️ 已弃用 | 已被新决策替代的旧决策 |
| ❌ 已拒绝 | 评审后未采纳的决策 |

## ADR 格式

每个 ADR 包含以下部分：

- **状态**: 提议/已接受/已弃用/已替代
- **日期**: 决策日期
- **背景**: 决策背景和问题描述
- **决策**: 最终决定是什么
- **理由**: 为什么要这样决定
- **后果**: 决策的影响和权衡

## ADR 关系图

```
adr-001 (用户态模拟)
    │
    ├── adr-002 (C++17)
    ├── adr-003 (插件化)
    ├── adr-006 (分层架构)
    │       │
    │       ├── adr-004 (Buddy Allocator)
    │       ├── adr-005 (Ring Buffer)
    │       └── adr-008 (Linux 兼容层)
    │
    ├── adr-007 (CMake)
    ├── adr-009 (单例模式)
    │
    └── adr-010 (GTest 迁移)
            │
            ├── adr-011 (多进程支持)
            ├── adr-012 (性能优化)
            ├── adr-013 (错误处理)
            └── adr-014 (日志系统)

    └── GPU 相关 (adr-015/016/017/018/019/020/021)
            │
            ├── adr-004 (Buddy Allocator - 内存子分配)
            ├── adr-005 (Ring Buffer - 命令队列)
            │
            ├── adr-015 (IOCTL 统一)
            │       ├── adr-016 (Memory Domain)
            │       └── adr-017 (GPFIFO/Queue)
            │
            ├── adr-018 (驱动/仿真分离)
            │       ├── adr-019 (DRM/GEM/TTM 对齐)
            │       ├── adr-020 (libgpu_core 提取)
            │       ├── adr-021 (Hardware Puller)
            │       └── adr-023 (HAL 接口契约)
            │
            └── (Phase 3+ 规划 — 占位骨架已就位，详情待 owner 填充)
                    ├── adr-022 (GPU 计算单元仿真)
                    ├── adr-024 (用户态队列提交 — 已提议)
                    ├── adr-025 (Phase 3+ 议题)
                    ├── adr-026 (Phase 3+ 议题)
                    ├── adr-027 (Linux 兼容层扩展)
                    ├── adr-028 (Phase 3+ 议题)
                    ├── adr-029 (Phase 3+ 议题)
                    ├── adr-030 (Phase 3+ 议题)
                    └── adr-031 (TTM 迁移优先级)
```

## 维护指南

### 添加新 ADR

1. 在本目录创建新文件 `adr-XXX-title.md`
2. 使用标准 ADR 格式
3. 更新本索引表
4. 在相关 ADR 中添加交叉引用

### 更新现有 ADR

1. 修改对应文件
2. 更新"最后更新"日期
3. 如状态变更，在索引表中更新状态

### 废弃 ADR

1. 将状态改为"已弃用"
2. 添加"被替代者"引用
3. 说明废弃原因

---

**维护者**: UsrLinuxEmu Architecture Team
**最后更新**: 2026-06-16 (commit 374d463)

## 编号 gap 治理（2026-06-16）

2026-06-16 之前的 ADR 编号 022 + 025~031 一直缺失（被 `tools/docs-audit.sh` §3.1/3.2 标记为 "intentional placeholder"）。本轮治理补齐了 8 份**占位骨架 ADR**，明确每份占位的"决策待定"状态和潜在候选方向。

- 022：GPU 计算单元仿真（具体 topic）
- 025/026/028/029/030：通用 Phase 3+ 占位（候选 A/B/C/D 列出）
- 027：Linux 兼容层扩展策略（具体 topic，承接 `docs/pending/linux_compat_plan.md`）
- 031：TTM 迁移优先级（具体 topic，承接 adr-019 §6）

后续 Phase 3+ 启动时，**owner 认领后应直接更新对应占位 ADR 的"决策"章节并将 status 改为 ✅ 已接受**，而不是新建一个文件。

详细的占位 → 已接受工作流见各占位 ADR 的"## 后续"段。