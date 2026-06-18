# Change: ssot-v0-1-7-audit-closeout

> **状态**: ✅ Completed（2026-06-18，本 change 归档时即闭环）
> **创建**: 2026-06-18
> **来源**: v0.1.6 SSOT 审计报告（change `ssot-deep-audit`，commit `211b48c`）的 25 项偏差 + v0.1.7 综合修复后状态回归
> **关联 SSOT**: `docs/02_architecture/post-refactor-architecture.md`（v0.1.7 ✅ Approved）
> **关联审计报告**:
> - 上轮: [`docs/02_architecture/audit-reports/v0.1.6-audit.md`](../audit-reports/v0.1.6-audit.md)
> - 本轮: [`docs/02_architecture/audit-reports/v0.1.7-audit.md`](../audit-reports/v0.1.7-audit.md)（新增）

## Why

v0.1.6 审计（change `ssot-deep-audit`）发现 SSOT 25 项偏差：
- 🔴 1 项 P0（A4 #1: 附录 A `gpu_pushbuffer_args` 缺 `va_space_handle` 字段）
- 🟠 4 项 P1（A1 #2 sim/hardware 布局、A2 #1 copilot-instructions GTest、A3 #2 AGENTS.md 反向链接、A3 #3 orphan spec Purpose）
- 🟡 14 项 P2
- 🟢 6 项 P3

按 v0.1.6 审计设计 D5（"审计与修复分离"），25 项偏差由独立 follow-up changes 修复。本 change 负责：

1. **回归审计 v0.1.6 偏差的解决状态**（4 个并行 explore agent 覆盖 A1/A2/A3/A4）
2. **修补 v0.1.6 报告未覆盖的盲点**（ND-A2.β: CONTRIBUTING.md 残留 GTest 单元测试示例代码块）
3. **沉淀 v0.1.7 审计报告**为 `docs/02_architecture/audit-reports/v0.1.7-audit.md`

### 本 change 覆盖的工作

| 任务 | 偏差 ID | 来源 |
|------|---------|------|
| A1 §1.2 sim 层回归 | A1 #1-#5 | v0.1.6 audit |
| A2 §1.7 测试框架回归 + 微补丁 | A2 #1-#5 + ND-A2.β | v0.1.6 audit + 本 change 新发现 |
| A3 §1.8 SSOT 空白回归 | A3 #1-#6 | v0.1.6 audit |
| A4 附录 A struct 回归 | A4 #1-#9 | v0.1.6 audit |
| 新发现 P3 旁观 | ND-A4.α | 本 change 新发现（不修，留给下个 change）|
| SSOT 变更记录 + footer 更新 | - | 本 change |
| v0.1.7 审计报告沉淀 | - | 本 change |

## What Changes

### 1. 审计回归（4 agent 并行 + 微补丁）

- 验证 v0.1.6 → v0.1.7 修复周期内的 25 项偏差解决状态
- 发现并修复 v0.1.6 报告覆盖盲点（ND-A2.β）
- 旁观 P3 项（ND-A4.α）记录在审计报告中，不在本 change 范围内

### 2. 修复 ND-A2.β（CONTRIBUTING.md GTest 残留）

- `CONTRIBUTING.md:436-466` GTest 单元测试示例代码块 → Catch2 `TEST_CASE` + 实际 libgpu_core API
- `CONTRIBUTING.md:436-438` 新增 Catch2 显式声明 + GTest 禁止声明 + ADR-010 交叉引用
- 修复 v0.1.6 A2 #2 PARTIALLY 状态（A2 #2 修复仅触及 L158-159，遗漏 L438-462 代码块）

### 3. 沉淀 v0.1.7 审计报告

- 新建 `docs/02_architecture/audit-reports/v0.1.7-audit.md`
- 含 25 项 v0.1.6 偏差的解决状态表 + 新偏差 + 跨区域分析 + 修复路径 commit 链
- 与 v0.1.6 报告并列，形成"发现 → 修复 → 回归"完整闭环证据链

### 4. SSOT 同步

- `post-refactor-architecture.md:759` 变更记录新增 v0.1.7 审计 + 微补丁条目
- `post-refactor-architecture.md:763-767` footer 更新：最后更新日期 + 历史审计报告链接

## 闭环率

| 维度 | v0.1.6 audit 时 | v0.1.7 修复周期后 | 本 change 微补丁后 |
|------|----------------|------------------|-------------------|
| v0.1.6 偏差闭环 | 0% | 24/25 (96%) | **25/25 (100%)** |
| SSOT §1.7 11 源一致性 | 9/11 (82%) | 10/11 (91%) | **11/11 (100%)** |
| 新偏差（ND-A2.β）| - | 1 (UNRESOLVED) | **1/1 (100%)** |

## 关键 commit 引用

| Commit | 角色 |
|--------|------|
| `211b48c` | v0.1.6 审计（**上轮** change `ssot-deep-audit`）|
| `5fa1f71` | v0.1.7 综合修复（change `ssot-v0-1-7-comprehensive-fix`，本 change 上游）|
| `3e306d1` | GTest 残留清理（change `cleanup-gtest-residue`，本 change 上游）|
| `880eb4f` | sim/hardware 布局（change `fix-sim-hardware-layout`，本 change 上游）|
| `3faa3a7` | AGENTS.md 反向链接（change `fix-agents-md-ssot-link`，本 change 上游）|
| `e871738` | shadow 死代码清理（change `cleanup-shadow-dead-code`，本 change 上游）|
| `d290ee8` | SSOT §0 状态升级（change `ssot-0-section-refresh`，本 change 上游）|
| `93f11ba` | ADR-024 状态升级（change `adr-024-status-upgrade`，本 change 上游）|
| **本 change commit** | v0.1.7 回归审计 + ND-A2.β 微补丁 + 审计报告沉淀 + SSOT 同步 |

## 残留项

- **ND-A4.α（🟢 P3 旁观）**: orphan struct `gpu_create_queue_args` 存在于 `plugins/gpu_driver/shared/gpu_queue.h:55-62`，未被任何 IOCTL 使用（IOCTL 0x40 用 `gpu_queue_args`）。建议下次 OpenSpec change `cleanup-orphan-struct-gpu-create-queue-args` 处理（不在本 change 范围）。
