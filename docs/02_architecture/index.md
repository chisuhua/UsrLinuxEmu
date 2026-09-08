# 核心文档

这里包含理解 UsrLinuxEmu 所必需的核心概念和架构信息。

> **最后更新**: 2026-09-08（归档 3 个 pre-v0.1.5 DEPRECATED 文档；新增 4 个文档导航 + 修复 broken link）
> **SSOT**: [`post-refactor-architecture.md`](post-refactor-architecture.md)（v0.1.7+，持续维护）

## 导航

### 活跃文档（推荐阅读）

- [**post-refactor-architecture.md**](post-refactor-architecture.md) ⭐ **SSOT** — 重构后架构总览 + H-2.5 + H-3 跨仓架构（v0.1.7+，2026-06-23 更新）
- [**driver-stack-flow-post-stage-5-5-2.md**](driver-stack-flow-post-stage-5-5-2.md) ⭐ **Stage 5.5.2 驱动栈** — 控制/数据流完整图谱（Path A Full TLP + Path B Bypass AXI + Command 模式 + Adapter 通道；2026-09-07 新增，commit `bab64dd5` + `6d2ea90`）
- [**driver-stack-flow-post-stage-5-5-2-roadmap.md**](driver-stack-flow-post-stage-5-5-2-roadmap.md) 📋 **修订实施路径图** — 上述文档的 P0-P4 阶段修订计划（2026-09-08 新增）
- [**four-quadrant-architecture.md**](four-quadrant-architecture.md) — 4 象限目录布局（Q1-Q4，ADR-091 派生 SSOT）
- [**scale-up-fabric-architecture.md**](scale-up-fabric-architecture.md) 📋 Draft v0.5 — Scale-up Fabric 局部架构（节点内 L1 Switch + 统一 PA + UVM/PGAS；2026-08-14）
- [**multi-process-gpu-simulator-integration.md**](multi-process-gpu-simulator-integration.md) 📋 Draft v0.1 — 跨仓集成 SSOT（UsrLinuxEmu ↔ Multi-Process Vision；2026-08-14）
- [**stage4-cp-complete-gap-analysis.md**](stage4-cp-complete-gap-analysis.md) — Stage 4.4~4.6 差距分析快照（4.3 ✅ 后；2026-07-28）
- [**ROADMAP**](../roadmap/README.md) ⭐ **演进路线** — 4 阶段 + 蓝图，从 MVP 到 Linux 内核环境模拟
- [**API 参考**](../06-reference/api-reference.md) — 核心 API 接口文档

### 历史/参考文档

- [**refactor-history.md**](refactor-history.md) — Phase 0 → 2 演进时间轴（保留）
- [Pre-v0.1.5 归档文档](../archive/misc/) — overview / architecture / architecture_design 已归档（DEPRECATED 头标保留）

## 快速导航

| 我想了解... | 阅读这个 |
|------------|----------|
| **项目是什么 + 怎么开始** | [README.md](../../README.md) + [AGENTS.md](../../AGENTS.md) |
| **权威架构（SSOT）** | [post-refactor-architecture.md](post-refactor-architecture.md) |
| **Stage 5.5.2 驱动栈数据流/控制流** | [driver-stack-flow-post-stage-5-5-2.md](driver-stack-flow-post-stage-5-5-2.md) |
| **driver-stack-flow 修订计划（P0-P4）** | [driver-stack-flow-post-stage-5-5-2-roadmap.md](driver-stack-flow-post-stage-5-5-2-roadmap.md) |
| **4 象限目录布局（Q1-Q4）** | [four-quadrant-architecture.md](four-quadrant-architecture.md) |
| **Scale-up Fabric 局部架构** | [scale-up-fabric-architecture.md](scale-up-fabric-architecture.md) |
| **多进程 GPU 仿真跨仓集成** | [multi-process-gpu-simulator-integration.md](multi-process-gpu-simulator-integration.md) |
| **Stage 4 后续差距分析** | [stage4-cp-complete-gap-analysis.md](stage4-cp-complete-gap-analysis.md) |
| **架构如何演进（4 阶段）** | [ROADMAP](../roadmap/README.md) |
| **API 如何使用** | [API 参考](../06-reference/api-reference.md) |
| **如何构建项目** | [构建指南](../04-building/build_system.md) |
| **如何开发** | [开发指南](../03-development/guide.md) |
| **重构历史时间轴** | [refactor-history.md](refactor-history.md) |

## 阅读顺序（按角色）

### 🆕 新用户（首次接触）
1. [README.md](../../README.md) — 项目概览 + 快速上手
2. [AGENTS.md](../../AGENTS.md) — 构建命令 + 编码风格
3. [overview 归档](../archive/misc/overview-2026-08-deprecated.md)（可选，已 deprecated 但保留供历史参考）

### 🏛️ 架构理解者
1. [post-refactor-architecture.md](post-refactor-architecture.md)（**SSOT**）— 重构后权威架构
2. [four-quadrant-architecture.md](four-quadrant-architecture.md) — 4 象限目录布局（Stage 5.5+）
3. [driver-stack-flow-post-stage-5-5-2.md](driver-stack-flow-post-stage-5-5-2.md) — 驱动栈数据流
4. [driver-stack-flow-post-stage-5-5-2-roadmap.md](driver-stack-flow-post-stage-5-5-2-roadmap.md) — 修订实施路径

### 🔧 驱动开发者
1. [AGENTS.md](../../AGENTS.md) — 编码规范 + IOCTL 编号
2. `plugins/gpu_driver/shared/gpu_ioctl.h` — System C IOCTL 定义
3. [post-refactor-architecture.md](post-refactor-architecture.md) §1.6 — IOCTL 体系

### 🌐 跨仓 / 多进程 / Scale-up 架构师
1. [multi-process-gpu-simulator-integration.md](multi-process-gpu-simulator-integration.md) — 跨仓集成 SSOT
2. [scale-up-fabric-architecture.md](scale-up-fabric-architecture.md) — Scale-up 局部 SSOT
3. [ADR-087](../00_adr/adr-087-multi-process-device-fabric-seam.md) — 跨仓设备与 fabric seam ADR

---

**最后更新**: 2026-09-08（archiving 3 deprecated + index refresh）
