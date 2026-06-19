# Design: ssot-deep-audit

> **依赖**: `proposal.md` 已完成
> **作用**: 解释 HOW 实施 v0.1.4 审计

## Context

**当前状态**（v0.1.2 勘误后）：
- SSOT `post-refactor-architecture.md` 661 行，13 个章节
- 已有 1 份 v0.1.2 勘误，10 项偏差
- 4 个并行 explore agent 在 C 审计阶段覆盖了 5 个区域

**未覆盖区域**（来自 proposal）：
1. **§1.2 架构图硬件仿真层**（libgpu_core 命名 / sim/ 子目录引用真实性）
2. **§1.7 测试框架声明**（4 文件 + 1 ADR + AGENTS.md 隐含状态）
3. **§1.8 权威文档空白**（v0.1.2 写"缺 SSOT"，本 change 闭环后是否还有空缺）
4. **附录 A 结构体字段**（13 个 struct 字段 vs SSOT §1.3 描述）

**约束**：
- 4 个 agent 必须**并行**执行（避免单 agent 主观偏误 + 缩短 wall time）
- 每个 agent 必须**只读**，不做任何修改
- 产出格式必须统一（便于聚合）

## Goals / Non-Goals

**Goals**:
- 完整覆盖上述 4 个未审计区域
- 产出统一格式的 v0.1.4 审计报告（markdown）
- 若发现偏差，v0.1.4 SSOT 勘误在 SSOT 变更记录追加
- 建立 `docs/02_architecture/audit-reports/` 目录作为未来审计沉淀位置

**Non-Goals**:
- 不修复发现的偏差（开 follow-up change）
- 不修改 SSOT 现有章节内容（仅追加变更记录）
- 不做 CI 自动化（留未来 change）
- 不审计 4 个区域之外的 SSOT 章节（§0/§2/§3/§4/§5 已在 v0.1.2 之前审计过；本次聚焦真正的盲区）

## Decisions

### D1: 4 个并行 explore agent 的职责切分

| Agent | 区域 | 关键证据源 |
|-------|------|-----------|
| **A1** | §1.2 架构图 | `plugins/gpu_driver/sim/` 子目录树、`libgpu_core/*.h/*.c` 实际文件、`src/kernel/` 14 cpp 计数 |
| **A2** | §1.7 测试框架 | 5 处声明源（README.md / AGENTS.md / copilot-instructions.md / CONTRIBUTING.md / ADR-010）+ `tests/catch_amalgamated.{hpp,cpp}` 实证 |
| **A3** | §1.8 权威空白 | `AGENTS.md` 现状 + `post-refactor-architecture.md` 引用关系 + `docs/CHANGELOG.md` 最近 1 个月变更 |
| **A4** | 附录 A struct 字段 | `plugins/gpu_driver/shared/gpu_ioctl.h` + `gpu_queue.h` 的 13 个 struct 定义 vs SSOT §1.3 描述 + 附录 A 表格的 struct 字段列表 |

**理由**：
- 4 个区域**正交**（无相互依赖），可完全并行
- 每个 agent 输入 prompt ≤ 30 行，输出 ≤ 50 行偏差表
- 避免单个 agent 跨 4 个区域的认知负担

### D2: 审计报告格式 = 统一 markdown 表格

每个 agent 产出格式：

```markdown
## A{n} 审计结果

| 偏差 | SSOT 描述 | 实际状态 | 严重度 | 证据 | 建议 |
|------|----------|---------|--------|------|------|
| #1 | (SSOT 原文) | (实测) | 🔴/🟠/🟡/🟢 | (文件:行号) | (修复方向) |
| #2 | ... | ... | ... | ... | ... |

**汇总**: N 个偏差（🔴 x / 🟠 y / 🟡 z / 🟢 w）
```

聚合后的 v0.1.4 报告 = 4 个 A{n} 块 + 顶部汇总表。

**理由**：
- 表格形式便于跨章节对比
- 严重度四档与 SSOT §3 P0/P1/P2/P3 保持一致
- 每行带证据（文件:行号）便于复核

### D3: agent prompt 模板结构

每个 agent prompt 包含 6 段（与 Phase 0 explore agent 最佳实践一致）：

```
1. CONTEXT: 项目背景 + 审计目标章节
2. GOAL: 具体要核对的 SSOT 描述项
3. DOWNSTREAM: 报告将用于 v0.1.4 SSOT 勘误
4. REQUEST: 列出具体要 grep/read/ls 的文件 + 输出格式
5. SCOPE: 明确边界（不修代码，不修 SSOT，只读）
6. SKIP: 明确不审计的相邻区域（避免范围蔓延）
```

**理由**：与之前 C 审计 4 个 agent 用的格式一致，便于横向对比

### D4: 报告沉淀路径 = `docs/02_architecture/audit-reports/`

- 新建目录 `docs/02_architecture/audit-reports/`
- 首份报告 `v0.1.4-audit.md`
- 未来 v0.1.5+ 报告追加为 `v0.1.5-audit.md` 等
- SSOT 顶部加一行引用：`> 历史审计报告：docs/02_architecture/audit-reports/`

**理由**：
- 集中存放便于查阅
- 路径与 SSOT 同目录，无需跨目录跳转
- 与 v0.1.2/v0.1.3 变更记录对齐（v0.1.4 也会作为 SSOT 变更记录条目）

### D5: 偏差修复 = 开 follow-up change，不在本 change scope

发现的偏差**不**在本 change 修复，原因是：
- 本 change 单一职责（仅审计）
- 修复涉及"修改 SSOT/代码"是另一次 work item
- 多次并发修改变更会让 PR 难以 review
- 偏差数量不可预测（0-N 个）

用户在 `/opsx-apply` 完成后，根据 v0.1.4 报告决定开 1-N 个 follow-up change。

### D6: agent 启动策略 = `run_in_background=true` 真正并行

**理由**：
- 4 个 agent 真正并行（不是顺序）
- Wall time 约 2-4 分钟（之前 C 审计同模式）
- 收集结果用 `background_output` 等 `<system-reminder>` 通知
- 与之前 C 审计同流程，成熟模式

## Risks / Trade-offs

| Risk | Impact | Mitigation |
|------|--------|------------|
| **R1**: agent prompt 表述不清 → 产出格式不统一 | 中 | D3 强制 6 段结构 + D2 强制 markdown 表格输出 |
| **R2**: 4 个 agent 都发现同一偏差（重复计入）| 低 | 报告聚合时去重 + 标注交叉引用 |
| **R3**: agent 漏掉某些隐藏偏差（agent 偏误）| 中 | 每个 agent 限定 1 个区域减少认知负担；未来 v0.1.5 复审时二次检查 |
| **R4**: 发现的偏差太多 → 大量 follow-up change 排队 | 低（不是本 change 问题）| 一次性产出报告，决策权交给用户 |
| **R5**: v0.1.4 勘误写错 → SSOT 变更记录污染 | 低 | 追加到变更记录，保留 v0.1.2/v0.1.3；任何错误后续 change 可覆盖 |

## Migration Plan

1. **Phase 1: 准备 4 个 agent prompt**（读 v0.1.2 报告格式作为模板）
2. **Phase 2: 启动 4 个并行 agent**（`run_in_background=true`）
3. **Phase 3: 收集结果**（等 `<system-reminder>` 后用 `background_output` 拉取）
4. **Phase 4: 聚合写报告**（`docs/02_architecture/audit-reports/v0.1.4-audit.md`）
5. **Phase 5: SSOT v0.1.4 勘误**（仅追加变更记录条目，不修改正文）
6. **Phase 6: 验证**（`tools/docs-audit.sh --strict` 仍 36/36）

**Rollback**：任一 Phase 失败可独立 revert；最坏情况是 audit-reports/ 多了一份报告，删掉即可。

## Open Questions

无（proposal §开放问题已通过 D1-D6 解决）。

未来可能性（如需开 v0.1.5+）：
- D4 可扩展为 `audit-reports/index.md` 导航
- D1 可扩展为 5+ 个 agent（如 §1.1 重构时间轴的 commit 引用真实性）
- D6 可改为 CI 自动化（每次 PR 跑 §1.5 + 附录 A 子集）
