# Change: ssot-deep-audit

> **状态**: ✅ Completed（2026-06-17，本 change 归档时即闭环）
> **创建**: 2026-06-17
> **来源**: C 审计 v0.1.2 勘误（commit `9e5d5ea`，2026-06-17）的覆盖盲区
> **关联 SSOT**: `docs/02_architecture/post-refactor-architecture.md`（v0.1.2）

## Why

2026-06-17 完成的 C 审计（commit `9e5d5ea` v0.1.2 勘误）使用 4 个并行 explore agent 对照 SSOT 与代码，覆盖了：

- ✅ §1.3 关键数据流（handlePushbufferSubmitBatch 路径）
- ✅ §1.4 数据模型（VASpace / Queue / Ring Buffer）
- ✅ §1.5 仓库物理布局
- ✅ §1.6 + 附录 A IOCTL 宏编号（15/15 一致）
- ✅ 附录 B archive 目录清单

但留下 **4 个未覆盖区域**：

1. **§1.2 架构图**的硬件仿真层细节（`libgpu_core/` 的 `gpu_buddy.h` + `buddy.c` 命名漂移已在 v0.1.1 修正，但还有"`sim/buddy_allocator.cpp`、`sim/fence_sim.cpp` (shadow 编译)"的引用是否真实存在）
2. **§1.7 测试框架**"声称 vs 实际"表（4 个文档源 + 1 ADR + 1 隐含 AGENTS.md；v0.1.2 是否仍准确）
3. **§1.8 权威文档空白**（v0.1.2 写"缺少 SSOT"，但本 change 自身就在建立 SSOT，是否已闭环？）
4. **附录 A 结构体定义**（之前 agent 只对了宏编号 0x01–0x43，未对 `gpu_pushbuffer_args` / `gpu_alloc_bo_args` / `gpu_va_space_args` / `gpu_queue_args` 等 13 个 struct 的字段是否与 SSOT §1.3 描述对齐）

**Why now**：
- 审计不完整 → "虚假安全感"：项目 governance 以为 SSOT 准确，实际有未发现偏差
- 在 Phase 3+ 启动前必须闭环 SSOT 审计（任何偏差都会变成 Phase 3 设备插件的踩坑源）
- H-1 (fix-gpu-pushbuffer-va-space-validation) 实施时发现 SSOT 描述与代码不一致（v0.1.2 勘误），证明审计价值高

## What Changes

**新能力**：建立 SSOT 全章节审计方法论，可复用

**实施**：
- 启动 **4 个并行 explore agent**（每个 agent 对应一个未覆盖区域）
- 产出 **v0.1.4 SSOT 审计报告**（markdown），记录每个区域的"一致/偏差"状态
- 若发现偏差，**v0.1.4 SSOT 勘误**作为本 change 的子产物（在同一 commit 内更新 SSOT 变更记录）
- 发现的偏差**不开 follow-up change**——保持本 change 单一职责（仅审计，不修）

**审计方法论沉淀**：
- 在 `docs/02_architecture/audit-reports/` 目录建立 v0.1.4 报告（首次建立此目录）
- 在 SSOT 顶部加引用 `> 审计方法论与历史报告：见 audit-reports/`

## Capabilities

### New Capabilities
- `ssot-deep-audit`: SSOT 全章节审计方法论 + v0.1.4 首次实施

### Modified Capabilities
- 无（其他 capability 不涉及审计本身）

## Impact

| 影响项 | 范围 | 风险 |
|--------|------|------|
| 文档 | `docs/02_architecture/audit-reports/v0.1.4-audit.md` 新建；SSOT 顶部加引用 | 低（纯新增）|
| SSOT 自身 | 若发现偏差，v0.1.4 勘误在 SSOT 变更记录追加 | 低（追加，不修改旧版）|
| 探索 agent | 4 个并行 explore agent（约 2-4 分钟 wall time）| 低（只读）|
| 工具 | `openspec` 验证 | 极低（CI 已守）|

**不**影响：
- 不修改任何代码
- 不修改 SSOT 现有章节内容
- 不开 follow-up change（发现的偏差由用户在 `/opsx-apply` 后另开 change）

## 开放问题（待 design.md 解决）

1. **审计报告路径**：放在 `docs/02_architecture/audit-reports/v0.1.4-audit.md` 还是 `docs/02_architecture/SSOT_AUDITS.md` 单文件累积？
2. **agent prompt 模板化**：是否沉淀为 `tools/ssot-audit-prompts/` 模板供未来 v0.1.5+ 复用？
3. **审计触发频率**：manual only（每次 Phase 边界）vs CI 自动（每次 PR 跑 §1.5/附录 A 子集）？

**建议在 design.md 阶段解决**：本 change 仅做 v0.1.4 一次，沉淀路径即可；CI 自动化与模板化留到未来 change。
