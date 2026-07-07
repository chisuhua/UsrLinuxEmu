---
MIRROR_OF: external/TaskRunner/docs/superpowers/cross-repo-prs/2026-07-05-usrlx-architecture-review.md
SCOPE: openspec-change
STATUS: PROPOSED
DATE: 2026-07-05
---

# TaskRunner 反馈文档（UsrLinuxEmu 侧镜像）

> **本文档是 TaskRunner 侧反馈的 UsrLinuxEmu 镜像**，原始版本在 [`external/TaskRunner/docs/superpowers/cross-repo-prs/2026-07-05-usrlx-architecture-review.md`](../../../external/TaskRunner/docs/superpowers/cross-repo-prs/2026-07-05-usrlx-architecture-review.md)。
>
> **路径深度说明**（按 TaskRunner AGENTS.md 跨仓文档引用规范）：
> - TaskRunner `docs/superpowers/cross-repo-prs/` → UsrLinuxEmu `docs/`：`../../../` (3 级)
> - UsrLinuxEmu `openspec/changes/` → TaskRunner `docs/superpowers/cross-repo-prs/`：`../../../../external/TaskRunner/` (4 级)

## 反馈速览

| 项 | 数量 | 严重度 |
|----|------|--------|
| 🔴 BLOCKER | 3 | TaskRunner 必须在 Step 2 之前修复 |
| 🟡 MUST-FIX | 4 | TaskRunner 实施 Step 3 时修复 |
| 🟢 NICE | 4 | 建议改进 |

### 🔴 BLOCKER 清单（TaskRunner 责任）

1. **B-1: GpuQueueEmu API 名称失配**
   - TaskRunner spec 引用 `GpuQueueEmu::submit_batch` 和 `GpuQueueEmu::enqueue`
   - 实际只有 `GpuQueueEmu::submit(uint64_t, uint32_t)`
   - **TaskRunner 行动**：更新 `phase3-stream-capture-design.md` 和 `phase3-mempool-design.md`

2. **B-2: Pool VA 范围架构未定**
   - UsrLinuxEmu **已决策**：采用 VA 子范围方案（Option B）
   - **TaskRunner 行动**：更新 `phase3-mempool-design.md §Pool Allocation` 适配 VA 子范围

3. **B-3: fence_id 生命周期迁移方案不完整**
   - UsrLinuxEmu **已决策**：HAL fence_id 范围 `[1, 1<<32)` 不变；sim fence_id 范围 `[1<<32, INT64_MAX]`
   - **TaskRunner 行动**：在 GpuDriverClient 中用 `int64_t` 接收 fence_id，不假设 `uint32_t`

### 🟡 MUST-FIX 清单

- **F-1**: capture mode 范围（仅 GLOBAL）
- **F-2**: pool attr value blob 布局（详见 design.md）
- **F-3**: kernargs=0 语义
- **F-4**: int64_t 返回约定

## 完整反馈内容

请直接查看 TaskRunner 侧原文档以获取完整内容：

[**`→ TaskRunner/docs/superpowers/cross-repo-prs/2026-07-05-usrlx-architecture-review.md`**](../../../external/TaskRunner/docs/superpowers/cross-repo-prs/2026-07-05-usrlx-architecture-review.md)

或运行：

```bash
cat external/TaskRunner/docs/superpowers/cross-repo-prs/2026-07-05-usrlx-architecture-review.md
```

## 关联文档

- [`proposal.md`](proposal.md) — UsrLinuxEmu 侧 OpenSpec 提案
- [`design.md`](design.md) — 详细技术设计
- [`tasks.md`](tasks.md) — 实施任务清单
- [`spec.md`](specs/sim-stream-primitive-support/spec.md) — Capability 规格
- [`fix-steps.md`](fix-steps.md) — UsrLinuxEmu 侧修复步骤（14 项）

## 协调时间线

| 日期 | 里程碑 | 责任方 |
|------|--------|--------|
| 2026-07-05 | 本反馈文档发出 | UsrLinuxEmu |
| 2026-07-08 | TaskRunner 接受 3 BLOCKER 修复 | TaskRunner |
| 2026-07-09 | TaskRunner 更新 Phase 3.1/3.2 spec | TaskRunner |
| 2026-07-10 | UsrLinuxEmu 应用 Fix-1 至 Fix-14 | UsrLinuxEmu |
| 2026-07-12 | OpenSpec 状态升级至 ACCEPTED | UsrLinuxEmu |
| 2026-07-15 | Step 2 merge (UsrLinuxEmu sim + IOCTL) | UsrLinuxEmu |
| 2026-07-22 | Step 3 merge (TaskRunner shim + E2E) | TaskRunner |
| 2026-07-25 | Step 4 submodule bump + 最终回归 | UsrLinuxEmu |