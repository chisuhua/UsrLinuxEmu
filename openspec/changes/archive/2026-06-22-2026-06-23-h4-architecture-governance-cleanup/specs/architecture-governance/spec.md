# Capability: architecture-governance

> **状态**: 🔵 Proposed (2026-06-23)
> **Owner**: UsrLinuxEmu Architecture Team + TaskRunner owner
> **Prerequisite**: 无（纯 governance 框架）
> **Upstream**: 无（基础设施 capability）
> **变更提案**: [`openspec/changes/2026-06-23-h4-architecture-governance-cleanup/`](../)

本 capability 定义 UsrLinuxEmu + TaskRunner 跨仓项目的架构治理框架。一旦所有 ADDED Requirements 满足，本 capability 可归档为 SSOT（与其他 capability 不同，本 capability 永不"完成" — 它是持续生效的治理规则）。

---

## ADDED Requirements

### Requirement: ADR 编号规则（续号 + INDEX）

跨仓项目所有 Architecture Decision Record MUST遵循以下规则：

1. **续号规则**：新 ADR MUST从当前最大编号 + 1 起。当前最大编号 031，下一个 ADR MUST是 ADR-032
2. **跳号规则**：除非显式标注 ⏸️ Deferred（占位），否则不允许跳号。占位 ADR 一旦填充决策章节后，编号MUST保留，不得更换
3. **INDEX 同步**：每次新增 ADR MUST同步更新 `docs/00_adr/README.md` INDEX（包含编号、标题、状态、日期、关联 change 5 列）

#### Scenario: 新增 ADR-032

- **WHEN** 在 H-4 governance cleanup 期间新增 H-2.5 决策记录
- **THEN** 文件命名MUST是 `adr-032-h2-5-igpu-driver-abstraction.md`
- **AND** README.md INDEX MUST新增一行，包含 5 列
- **AND** 不得修改已有 ADR-001 至 ADR-031 的内容

#### Scenario: ADR 占位填充

- **WHEN** ADR-025 Phase 3+ 占位被 owner 认领
- **THEN** 编号 025 MUST保留，不得更换为新编号
- **AND** 状态从 ⏸️ Deferred 改为 ✅ Accepted
- **AND** 决策章节MUST填充内容（不得保留为空）

---

### Requirement: ADR 状态标记规则

每个 ADR 文件头MUST包含状态标记，仅允许以下 4 个状态值：

| 状态 | 含义 | 何时使用 |
|---|---|---|
| `✅ Accepted` | 决策已被采纳且实施 | 决策已落实（如 H-2.5, H-3 已 shippable） |
| `⏸️ Deferred` | 决策被推迟到未来触发条件 | 推迟到 Phase 3+ 触发（如 ADR-025..030 占位） |
| `🔄 Proposed` | 决策在评审中 | 评审窗口期（如 ADR-027 linux-compat strategy） |
| `🚫 Rejected` | 决策被驳回 | 备选方案被否决但需保留文档 |

**严禁使用** `📝 Draft` / `💭 Considered` / `❓ TBD` 等模糊状态。

#### Scenario: ADR-031 升级到 Accepted

- **WHEN** v1 决策被采纳（cleanup-adr-placeholders change）
- **THEN** ADR-031 状态从 `🔄 Proposed` 改为 `✅ Accepted`
- **AND** 修订记录（修订记录 section）MUST记录升级日期

#### Scenario: ADR-034 维持 Deferred

- **WHEN** H-7 3 个 owner-flagged issue 仍待 Phase 3 触发
- **THEN** ADR-034 状态保持 `⏸️ Deferred`
- **AND** 不得改为 `✅ Accepted`（即使有理由）

---

### Requirement: plans/ 目录结构（当前 + archive 双层）

`external/TaskRunner/plans/` 目录MUST遵循双层结构：

```
plans/
├── README.md (目录说明 + archive 索引)
├── sync-plan.md (当前同步计划，仅保留 S5+ 状态)
└── archive/ (历史文件归档，仅供追溯)
    ├── YYYY-MM-DD-change-name/ (整个 openspec skeleton 目录)
    ├── YYYY-MM-DD-execution-record.md
    └── YYYY-MM-topic-name.md
```

**严禁**：将历史文件直接保留在 `plans/` 根目录（除非是当前活跃计划）。

#### Scenario: 新历史文件归档

- **WHEN** 某 openspec change 完成并归档到 UsrLinuxEmu `openspec/changes/archive/`
- **THEN** 其 TaskRunner-side skeleton MUST移至 `plans/archive/`（如 `2026-06-19-h2-phase2-openspec-skeleton/`）
- **AND** `plans/README.md` archive 索引MUST新增一行

#### Scenario: 当前 sync-plan.md 瘦身

- **WHEN** 同步点 S0-S4 已完成（S5 也已完成）
- **THEN** `plans/sync-plan.md` MUST移除已完成的同步点章节
- **AND** 仅保留 S5+ 状态 + 未来 hooks
- **AND** 文件行数应小于原始 50%

---

### Requirement: 架构蓝图 SSOT（post-refactor-architecture.md）

`UsrLinuxEmu/docs/02_architecture/post-refactor-architecture.md` MUST 是项目架构的 SSOT：

1. **每次 openspec change 归档后**，MUST更新 §1.3+ 章节反映新架构状态
2. **每次新增 IGpuDriver 方法或 IOCTL ABI 变更**，MUST更新 §1.3 或新增子章节
3. **每次新增 Phase**（H-N 实施），MUST更新 §1.4 数据流 + Mermaid diagrams
4. **Pre-v0.1.5 文档**（architecture.md / architecture_design.md / overview.md）MUST加 deprecated 头标，不再更新内容

#### Scenario: H-3 归档后蓝图更新

- **WHEN** H-3 (`h3-phase2-management`) 在 2026-06-22 归档
- **THEN** `post-refactor-architecture.md` §1.3 MUST新增 "H-3 Phase 2 Lifecycle" 子章节
- **AND** §1.4 Mermaid diagrams MUST新增 Phase 2 ioctl 流图
- **AND** 不得将 H-3 决策仅记录在 `openspec/changes/archive/2026-06-22-h3-phase2-management/design.md` 而不沉淀到 SSOT

#### Scenario: 下一波 change 引用蓝图

- **WHEN** H-7 ADR 起草时引用 H-3 决策
- **THEN** MUST引用 `post-refactor-architecture.md §1.3.2`（H-3 Phase 2 Lifecycle）
- **AND** 不得引用 `openspec/changes/archive/2026-06-22-h3-phase2-management/design.md`（即使 design.md 内容正确）

---

### Requirement: 跨仓 sync 流程

跨仓 sync MUST遵循固定流程：

```
Step 1: TaskRunner commit + push (submodule-internal changes)
Step 2: UsrLinuxEmu commit (submodule pointer + cross-repo changes)
Step 3: UsrLinuxEmu push
Step 4: Archive openspec change (after combined commit, before/with push)
```

**严禁**：跨仓顺序颠倒（先 UsrLinuxEmu commit 后 TaskRunner commit 会导致 submodule 指针引用未来 commit）。

#### Scenario: H-4 sync 流程

- **WHEN** H-4 governance cleanup 完成（Phase 2-4）
- **THEN** TaskRunner 先 push plans/ 归档 + sync-plan.md 瘦身（commit hash 例如 `abc1234`）
- **AND** UsrLinuxEmu commit 引用 `external/TaskRunner` submodule HEAD = `abc1234`
- **AND** UsrLinuxEmu push + openspec archive 合并在最后

#### Scenario: docs-audit 失败时阻断 push

- **WHEN** `tools/docs-audit.sh --strict` 输出非零失败数
- **THEN** 双仓 push MUST阻止
- **AND** 修复 docs-audit 失败项后再 push

---

### Requirement: openspec change lifecycle

每次 openspec change MUST经历以下生命周期阶段：

| 阶段 | 状态标记 | 必含文件 | 必含内容 |
|---|---|---|---|
| 1. 提案 | 🔵 Proposed | proposal.md, design.md, tasks.md, spec.md, .openspec.yaml | Why/What/Impact + D-decisions + phased checklist + ADDED Requirements |
| 2. 评审 | 🟡 Under Review | (同上 + review comments) | 评审意见 + 修复应用 |
| 3. 实施 | 🟢 Active | (同上 + implementation commits) | 实际代码/文档改动 |
| 4. 归档 | 📦 Archived | (原 files 移至 `archive/YYYY-MM-DD-change-name/`) | 历史追溯 |

#### Scenario: 完整 lifecycle 示例（H-4）

- **WHEN** H-4 governance cleanup 启动
- **THEN** Phase 1：创建 `openspec/changes/2026-06-23-h4-architecture-governance-cleanup/` 含 5 文件
- **AND** Phase 2：实施 plans/ 归档（在 archive 之前）
- **AND** Phase 3：实施 ADR 新增（在 archive 之前）
- **AND** Phase 4：实施蓝图更新（在 archive 之前）
- **AND** Phase 5：`openspec archive h4-architecture-governance-cleanup` 移动至 `openspec/changes/archive/`
- **AND** Phase 6：`openspec list` 显示 "No active changes found"

#### Scenario: 归档后 cross-reference MUST指向 archive 路径

- **WHEN** 某 change 归档至 `openspec/changes/archive/YYYY-MM-DD-change-name/`
- **THEN** 任何 docs/ 中的 cross-reference MUST更新为 archive 路径
- **AND** 不得保留指向 active change 路径（`openspec/changes/<change-name>/`）的引用

---

### Requirement: docs-audit 36/36 PASS 强制门槛

每次跨仓 commit 后MUST维持 `tools/docs-audit.sh --strict` 输出 36 passed / 0 failed：

**严禁**：在 docs-audit 失败状态下 commit + push（即使变更与 docs-audit 范围无关）。

#### Scenario: H-4 合并 commit 触发 docs-audit

- **WHEN** UsrLinuxEmu combined commit（含 submodule pointer + ADR + 蓝图 + archive）
- **THEN** pre-commit hook 自动运行 `tools/docs-audit.sh --strict`
- **AND** MUST输出 36 passed / 0 failed
- **AND** 失败时 commit 阻止

#### Scenario: 故意跳过 docs-audit

- **WHEN** 开发者绕过 pre-commit hook（`--no-verify`）
- **THEN** 后续 reviewer MUST手动验证 docs-audit
- **AND** CI（如有）MUST再次运行 docs-audit 阻断

---

## MODIFIED Requirements

_None — 本 capability 是纯 ADDED 框架，不修改任何已有 capability。_

---

## REMOVED Requirements

_None._

---

## Cross-references

- **ADR-032** (H-2.5 IGpuDriver 抽象): [`../../docs/00_adr/adr-032-h2-5-igpu-driver-abstraction.md`](../../docs/00_adr/)（由本 change 创建）
- **ADR-033** (H-3 Phase 2 Lifecycle): [`../../docs/00_adr/adr-033-h3-phase2-lifecycle.md`](../../docs/00_adr/)（由本 change 创建）
- **ADR-034** (H-7 Deferred Registry): [`../../docs/00_adr/adr-034-h7-deferred-registry.md`](../../docs/00_adr/)（由本 change 创建）
- **ADR-035** (Governance Policy): [`../../docs/00_adr/adr-035-governance-policy.md`](../../docs/00_adr/)（由本 change 创建 — 即本 capability 的 ADR 形式）
- **SSOT**: [`../../docs/02_architecture/post-refactor-architecture.md`](../../docs/02_architecture/post-refactor-architecture.md)
- **验证**: `tools/docs-audit.sh --strict`（36 checks pre-commit hook）
- **历史**: H-1/H-2.5/H-3 6 个 openspec change 均已 archived（见 `openspec/changes/archive/`）