# 架构决策记录 (Architecture Decision Records)

本文档目录包含 UsrLinuxEmu 项目中所有已通过和提议中的架构决策记录。

> **最后更新**: 2026-06-23（H-4 governance cleanup 新增 ADR-032 ~ ADR-035；详见末尾 "H-4 governance 增量" 段）
> **维护者**: UsrLinuxEmu Architecture Team + TaskRunner owner
> **治理规则**: 见 [ADR-035](adr-035-governance-policy.md)

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
| [adr-024](adr-024-user-mode-queue-submission.md) | 用户态队列命令提交架构 | ✅ 已接受 (Accepted) | 2026-06 |
| [adr-022](adr-022-gpu-compute-unit-emulation.md) | GPU 计算单元仿真 | ✅ 已接受 (Accepted) | 2026-06 |
| [adr-025](adr-025-phase3-placeholder.md) | Phase 3+ 议题占位 | ⏸️ 显式 Deferred (Phase 3+) | 2026-06 |
| [adr-026](adr-026-phase3-placeholder.md) | Phase 3+ 议题占位 | ⏸️ 显式 Deferred (Phase 3+) | 2026-06 |
| [adr-027](adr-027-linux-compat-strategy.md) | Linux 内核兼容层扩展策略 | ✅ 已接受 (Phase 3 触发后细化) | 2026-06 |
| [adr-028](adr-028-phase3-placeholder.md) | Phase 3+ 议题占位 | ⏸️ 显式 Deferred (Phase 3+) | 2026-06 |
| [adr-029](adr-029-phase3-placeholder.md) | Phase 3+ 议题占位 | ⏸️ 显式 Deferred (Phase 3+) | 2026-06 |
| [adr-030](adr-030-phase3-placeholder.md) | Phase 3+ 议题占位 | ⏸️ 显式 Deferred (Phase 3+) | 2026-06 |
| [adr-031](adr-031-ttm-migration-priority.md) | TTM 迁移实施优先级 | ✅ 已接受 (Accepted) | 2026-06 |
| [adr-032](adr-032-h2-5-igpu-driver-abstraction.md) | **H-2.5 IGpuDriver 抽象层** | ✅ 已接受 | 2026-06-23 |
| [adr-033](adr-033-h3-phase2-lifecycle.md) | **H-3 Phase 2 Lifecycle** | ✅ 已接受 | 2026-06-23 |
| [adr-034](adr-034-h7-deferred-registry.md) | **H-7 Deferred Registry**（3 owner-flagged upstream issues）| ⏸️ 显式 Deferred | 2026-06-23 |
| [adr-035](adr-035-governance-policy.md) | **Architecture Governance Policy** | ✅ 已接受 | 2026-06-23 |
| [adr-036](adr-036-three-way-separation.md) | **3 区分架构原则 (3-Way Architectural Separation)** | ✅ 已接受 | 2026-06-23 |

## 状态分布总览（截至 2026-06-23）

| 状态 | 数量 | ADR 列表 |
|------|----:|----------|
| ✅ 已接受 | 29 | 001-013, 015, 016, 018-024, 027, 031-033, 035, 036 |
| ⏸️ 显式 Deferred | 6 | 025, 026, 028-030, 034 |
| 🔄 提议中 | 1 | 011-014, (027 即将升 v1) |
| **总计** | **36** | ADR-001 ~ ADR-036 |

## ADR 状态说明

| 状态 | 说明 |
|------|------|
| ✅ 已接受 | 已通过评审，正式采用的决策 |
| 🔄 提议中 | 正在评审或等待实现的决策 |
| ⏸️ 显式 Deferred | 暂不决策，等待明确触发条件后重新打开（2026-06-17 引入；详见下文"deferred policy"）|
| ⚠️ 已弃用 | 已被新决策替代的旧决策 |
| ❌ 已拒绝 | 评审后未采纳的决策 |

**详细治理规则**：见 [ADR-035](adr-035-governance-policy.md) §Rule 2 — ADR 状态标记规则（仅允许 ✅/⏸️/🔄/🚫 4 个状态）。

## ADR 格式

每个 ADR 包含以下部分：

- **状态**: 提议/已接受/已弃用/已替代
- **日期**: 决策日期
- **背景**: 决策背景和问题描述
- **决策**: 最终决定是什么
- **理由**: 为什么要这样决定
- **后果**: 决策的影响和权衡

**H-4 起标准模板**（ADR-032 ~ ADR-035 已采用）：

```
# ADR-NNN: <Title>
**状态**: ...
**日期**: YYYY-MM-DD
**提案人**: ...
**评审者**: ...
**关联 ADR**: ...
**关联 Change**: ...
## Context / Decision / Consequences / Migration
```

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
            │               └── adr-036 (3 区分架构原则) ✅ Accepted
            │                       └── 关联: adr-018, adr-023, adr-035
            │
            └── (Phase 3+ 规划 — 已分流：已接受 / 显式 Deferred)
                    ├── adr-022 (GPU 计算单元仿真) ✅
                    ├── adr-031 (TTM 迁移优先级) ✅
                    ├── adr-024 (用户态队列提交) ✅
                    ├── adr-027 (Linux 兼容层扩展) ✅
                    ├── adr-025 (Phase 3+ 议题) ⏸️ Deferred
                    ├── adr-026 (Phase 3+ 议题) ⏸️ Deferred
                    ├── adr-028 (Phase 3+ 议题) ⏸️ Deferred
                    ├── adr-029 (Phase 3+ 议题) ⏸️ Deferred
                    └── adr-030 (Phase 3+ 议题) ⏸️ Deferred

    └── H-2.5 + H-3 + H-4 跨仓架构 (2026-06-23)
            │
            ├── adr-032 (H-2.5 IGpuDriver 抽象层) ✅
            │       └── 关联: adr-015 (IOCTL), adr-024 (User Mode Queue), adr-017 (GPFIFO)
            │
            ├── adr-033 (H-3 Phase 2 Lifecycle) ✅
            │       └── 关联: adr-032 (H-2.5)
            │
            ├── adr-034 (H-7 Deferred Registry) ⏸️ Deferred
            │       └── 关联: adr-033 (H-3), adr-024 (User Mode Queue)
            │
            └── adr-035 (Architecture Governance Policy) ✅
                    └── 元决策: 规范 ADR 治理规则本身
```

## 维护指南

### 添加新 ADR

1. 在本目录创建新文件 `adr-XXX-title.md`（XXX = 当前最大编号 + 1）
2. 使用标准 ADR 格式（参见 "H-4 起标准模板"）
3. 更新本索引表 + 状态分布表
4. 在相关 ADR 中添加交叉引用
5. 关联 change 路径：`openspec/changes/<change-name>/` 或 `archive/YYYY-MM-DD-...`

### 更新现有 ADR

1. 修改对应文件
2. 更新"最后更新"日期
3. 如状态变更，在索引表与状态分布表中同步更新

### 废弃 ADR

1. 将状态改为"🚫 已拒绝"（仅在评审否决时使用，详见 ADR-035 §Rule 2）
2. 添加"被替代者"引用
3. 说明废弃原因

---

**维护者**: UsrLinuxEmu Architecture Team
**最后更新**: 2026-06-23（H-4 governance cleanup 新增 ADR-032 ~ ADR-035）

## 编号 gap 治理（2026-06-16 → 2026-06-17）

2026-06-16 之前的 ADR 编号 022 + 025~031 一直缺失（被 `tools/docs-audit.sh` §3.1/3.2 标记为 "intentional placeholder"）。2026-06-16 本轮治理补齐了 8 份**占位骨架 ADR**，明确每份占位的"决策待定"状态和潜在候选方向。

- 022：GPU 计算单元仿真（具体 topic）
- 025/026/028/029/030：通用 Phase 3+ 占位（候选 A/B/C/D 列出）
- 027：Linux 兼容层扩展策略（具体 topic）
- 031：TTM 迁移优先级（具体 topic，承接 adr-019 §6）

### 2026-06-17 二次治理（change `cleanup-adr-placeholders`）

2026-06-17 由 OpenSpec change `cleanup-adr-placeholders` 完成第二轮治理：

- **ADR-022**：从占位升级为 ✅ v1（operator-level emulation，4 个 kernel template）
- **ADR-031**：从占位升级为 ✅ v1（TTM thin wrapper over `libgpu_core/gpu_buddy`）
- **ADR-025/026/028/029/030**：从占位转为 **⏸️ 显式 Deferred**，每份附加明确 Phase 3 触发条件
- **ADR-027**：保持 `🔄 提议中`（承接 linux_compat 规划，已迁移至 ADR-027 v1；未在本次清理范围）

### Deferred Policy（2026-06-17 引入）

**025/026/028/029/030** 标注为 `⏸️ 显式 Deferred` 而非 `🔄 提议中`：

- **区别于"永久拒绝"**：`⏸️ 显式 Deferred` 表示"暂不决策，触发条件满足后重新打开"
- **每份 ADR 必须有明确触发条件**：写在 ADR 文件的 `## Phase 3 触发条件` 段（commit 事件 / issue 编号 / 第一个用例）
- **重新打开工作流**：owner 认领后：
  1. 更新 `## 决策` 章节并把 status 改为 `✅ 已接受`
  2. 保留 `## 讨论历史 (v0 占位)` 附录（v0 候选项不删除）
  3. 同步更新本 README 的索引表与关系图
- **可自动检测**：每份 deferred ADR 的 `## Phase 3 触发条件` 段给出 `git log` / `gh issue list` 等具体检测命令

后续 Phase 3+ 启动时，**owner 认领后应直接更新对应 ADR 的"决策"章节并将 status 改为 ✅ 已接受**，而不是新建一个文件。详细的占位 → 已接受工作流见各占位 ADR 的"## 后续"段。

## H-4 governance 增量（2026-06-23）

本轮 `h4-architecture-governance-cleanup` 新增 4 个 ADR + 本 INDEX 升级：

### 新增 ADR-032 ~ ADR-035 概要

| 编号 | 标题 | 状态 | 决策来源 |
|------|------|------|---------|
| [adr-032](adr-032-h2-5-igpu-driver-abstraction.md) | H-2.5 IGpuDriver 抽象层 | ✅ Accepted | `openspec/changes/archive/2026-06-19-h2-5-architecture-foundation/design.md` §D6-D11 |
| [adr-033](adr-033-h3-phase2-lifecycle.md) | H-3 Phase 2 Lifecycle | ✅ Accepted | `openspec/changes/archive/2026-06-22-h3-phase2-management/design.md` §D1-D5 + R2 |
| [adr-034](adr-034-h7-deferred-registry.md) | H-7 Deferred Registry | ⏸️ Deferred | H-3 design.md §R4 + §R5（3 owner-flagged upstream issues）|
| [adr-035](adr-035-governance-policy.md) | Architecture Governance Policy | ✅ Accepted | 元决策：本 change 自身 |

### 按 Capability 分组（新增）

- **gpu-driver-architecture** capability: ADR-032 (H-2.5), ADR-033 (H-3)
- **gpu-phase2-management** capability: ADR-033 (H-3), ADR-034 (H-7 deferred)
- **architecture-governance** capability: ADR-035 (governance)

### H-4 期间的 INDEX 升级

- 状态分布表（截至 2026-06-23）：29 Accepted + 6 Deferred + 1 Proposed = 36 total
- 关系图：新增 "H-2.5 + H-3 + H-4 跨仓架构" 子树
- 维护指南：补充 "H-4 起标准模板" 段落
- 末尾新增 "H-4 governance 增量" 段

### 跨仓镜像 (submodule) — TaskRunner TADR

TaskRunner 独立 ADR 体系（`TADR-NNN` 编号，2026-06-23 H-4.5 governance cleanup 建立），与本仓 ADR-NNN 区分。TaskRunner 仓的 consumer-lens 决策 + retro 决策 capture 详见 [external/TaskRunner/docs/adr/](../external/TaskRunner/docs/adr/README.md)。

| TADR | 主题 | 关联 UsrLinuxEmu ADR |
|------|------|---------------------|
| TADR-001 ~ TADR-004 | CUDA/Vulkan Runtime v0.1 决策 (retroactive) | — |
| TADR-005 | IGpuDriver 抽象层 consumer-lens (H-2.5) | [ADR-032](adr-032-h2-5-igpu-driver-abstraction.md) |
| TADR-006 | Phase 2 5 方法 consumer-lens (H-3) | [ADR-033](adr-033-h3-phase2-lifecycle.md) |
| TADR-007 | R2 mapping contract (LOW32 truncation 显式化) | [ADR-033 §R2](adr-033-h3-phase2-lifecycle.md) |
| TADR-008 | H-7 上游 issue TaskRunner 侧注册点 (⏸️ Deferred) | [ADR-034](adr-034-h7-deferred-registry.md) |

**维护政策**：本表是 canonical，TaskRunner `docs/adr/README.md` §索引 是 mirror。改动时先改本表，TaskRunner 端同步更新。同步协议遵循 ADR-035 §Rule 5.1 4 步流程。

### 跨引用规范

在 `docs/` 其他文档或 openspec change 中引用 ADR：

```markdown
详见 [ADR-032](../00_adr/adr-032-h2-5-igpu-driver-abstraction.md) §Decision。
```

```markdown
**关联 Source**: openspec/changes/archive/<source-change>/design.md §X
```