# ADR-035: Architecture Governance Policy (跨仓 Governance 框架)

**状态**: ✅ 已接受 (Accepted)
**日期**: 2026-06-23
**提案人**: TaskRunner owner 协同 (H-4 governance cleanup 阶段)
**评审者**: UsrLinuxEmu Architecture Team + TaskRunner owner
**关联 Capability**: `architecture-governance`（UsrLinuxEmu `openspec/specs/architecture-governance/`）
**关联 Change**: `openspec/changes/2026-06-23-h4-architecture-governance-cleanup/`

---

## Context

UsrLinuxEmu + TaskRunner 跨仓项目经历 H-1 (2026-04) → H-2.5 (2026-06-19) → H-3 (2026-06-23) 三轮实施，积累：

- 31 个 ADR (ADR-001 至 ADR-031)，无 INDEX，新贡献者需 grep 检索
- 19 个 archived openspec change，分散在 `openspec/changes/archive/` 19 子目录
- 6 份 `plans/` 历史文件（pre-H-2.5），与 IGpuDriver 架构矛盾
- 6 份 architecture 文档，2 份 pre-v0.1.5 已 outdated 但仍引用

治理规则未形式化，导致：
1. 新 openspec change 提案时无统一结构（proposal.md / design.md / tasks.md / spec.md）
2. ADR 编号跳号（025/026/028-030 是 Phase 3+ 占位）
3. plans/ 混杂（当前 vs 历史）
4. 架构蓝图与代码实际状态不同步

H-4 governance cleanup 提炼治理规则为正式 ADR。

---

## Decision

建立跨仓 governance 框架，覆盖 ADR 治理、plans/ 治理、架构蓝图、跨仓 sync、openspec lifecycle。

### Rule 1 — ADR 编号规则（续号 + INDEX）

**R1.1 续号规则**：新 ADR 必须从当前最大编号 + 1 起。当前最大编号 031，下一个 ADR 必须是 ADR-032。

**R1.2 跳号规则**：除非显式标注 ⏸️ Deferred（占位），否则不允许跳号。占位 ADR 一旦填充决策章节后，编号必须保留，不得更换。

**R1.3 INDEX 同步**：每次新增 ADR 必须同步更新 `docs/00_adr/README.md` INDEX（包含编号、标题、状态、日期、关联 change 5 列）。

**R1.4 ADR 模板**（新建 ADR 必含）:
```markdown
# ADR-NNN: <Title>

**状态**: <✅ Accepted / ⏸️ Deferred / 🔄 Proposed / 🚫 Rejected>
**日期**: YYYY-MM-DD
**提案人**: <Name>
**评审者**: <Names>
**关联 ADR**: ADR-XXX, ADR-YYY
**关联 Change**: openspec/changes/<change-name>/

## Context
## Decision
## Consequences
## Migration
```

### Rule 2 — ADR 状态标记规则

**R2.1 4 个允许状态**:

| 状态 | 含义 | 何时使用 |
|------|------|---------|
| `✅ Accepted` | 决策已被采纳且实施 | 决策已落实（如 H-2.5, H-3 已 shippable）|
| `⏸️ Deferred` | 决策被推迟到未来触发条件 | 推迟到 Phase 3+ 触发（如 ADR-025..030 占位）|
| `🔄 Proposed` | 决策在评审中 | 评审窗口期（如 ADR-027 linux-compat strategy）|
| `🚫 Rejected` | 决策被驳回 | 备选方案被否决但需保留文档 |

**R2.2 严禁**：`📝 Draft` / `💭 Considered` / `❓ TBD` 等模糊状态。

**R2.3 状态变更必须记录**：修订记录（revision history）section 必须记录变更日期与原因。

### Rule 3 — plans/ 目录结构（双层归档）

**R3.1 TaskRunner 本地归档**（`external/TaskRunner/plans/archive/`）:
- 仅 archive H-2.5 之前的历史文件（与 IGpuDriver 架构无关）
- 文件原状保留（不修改内容）
- 文件头可加 DEPRECATED 标记（指向 ADR 取代关系）

**R3.2 UsrLinuxEmu 跨仓归档**（`openspec/changes/archive/`）:
- 所有 openspec change（含 DRAFT/DEPRECATED skeleton）必须移至此目录
- 命名规范：`openspec/changes/archive/YYYY-MM-DD-<change-name>/`
- 仅在 change 完成（archived）后才移动

**R3.3 plans/ 当前文件**:
- 仅保留 `sync-plan.md`（精简版）+ `README.md`（目录说明）
- 其他历史文件必须归档

### Rule 4 — 架构蓝图 SSOT

**R4.1 SSOT 路径**：`UsrLinuxEmu/docs/02_architecture/post-refactor-architecture.md`

**R4.2 更新触发**:
- 每次 openspec change 归档后，必须更新 §1.3+ 章节反映新架构状态
- 每次新增 IGpuDriver 方法或 IOCTL ABI 变更，必须更新 §1.3 或新增子章节
- 每次新增 Phase (H-N 实施)，必须更新 §1.4 数据流 + Mermaid diagrams

**R4.3 Pre-v0.1.5 文档处理**:
- `architecture.md` / `architecture_design.md` / `overview.md` 必须加 deprecated 头标
- 不再更新内容（由 SSOT 取代）

### Rule 5 — 跨仓 sync 流程

**R5.1 固定 4 步顺序**:
```
Step 1: TaskRunner commit + push (submodule-internal changes)
Step 2: UsrLinuxEmu commit (submodule pointer + cross-repo changes)
Step 3: UsrLinuxEmu push
Step 4: Archive openspec change (after combined commit, before/with push)
```

**R5.2 严禁**: 跨仓顺序颠倒（先 UsrLinuxEmu commit 后 TaskRunner commit 会导致 submodule 指针引用未来 commit）。

**R5.3 docs-audit 阻断**: `tools/docs-audit.sh --strict` 输出非零失败数时，双仓 push 必须阻止。

### Rule 6 — openspec change lifecycle

**R6.1 4 阶段状态**:

| 阶段 | 状态标记 | 必含文件 | 必含内容 |
|---|---|---|---|
| 1. 提案 | 🔵 PROPOSED | proposal.md, design.md, tasks.md, spec.md, .openspec.yaml | Why/What/Impact + D-decisions + phased checklist + ADDED Requirements |
| 2. 评审 | 🟡 UNDER REVIEW | (同上 + review comments) | 评审意见 + 修复应用 |
| 3. 实施 | 🟢 ACTIVE | (同上 + implementation commits) | 实际代码/文档改动 |
| 4. 归档 | 📦 ARCHIVED | (原 files 移至 `archive/YYYY-MM-DD-change-name>/`) | 历史追溯 |

**R6.2 .openspec.yaml status 字段**: PROPOSED → ACTIVE → ARCHIVED，每次变更必更新。

**R6.3 cross-reference 路径**: 任何 docs/ 中引用某 change 必须指向 `openspec/changes/archive/...`（归档后），不得保留 `openspec/changes/<name>/`（active 路径）。

---

## Consequences

### Positive

- ✅ ADR 治理统一（编号规则 + 状态标记 + INDEX 同步）
- ✅ plans/ 目录清晰（当前 vs 归档双层）
- ✅ 架构蓝图与代码同步（SSOT 维护规则）
- ✅ 跨仓 sync 流程形式化（避免指针错位）
- ✅ openspec change lifecycle 显式（追溯链清晰）

### Negative

- ⚠️ 每新增 ADR 需同步更新 INDEX（人工维护成本）
- ⚠️ 每 openspec change 归档后必须更新 SSOT（docs-audit 检查 36/36）
- ⚠️ Pre-v0.1.5 文档 deprecated 后需 redirect 用户到 SSOT

### Mitigation

- 📚 README.md INDEX 提供统一导航入口
- 📚 docs-audit pre-commit hook 36/36 强制 baseline
- 📚 跨仓 sync 流程文档化（CONTRIBUTING.md）

---

## Migration

### 已应用（H-4 governance cleanup 阶段）

- ✅ Rule 1: ADR-032 (H-2.5), ADR-033 (H-3), ADR-034 (H-7), ADR-035 (governance) 新增
- ✅ Rule 1.3: `docs/00_adr/README.md` INDEX 同步创建
- ✅ Rule 3: `external/TaskRunner/plans/` 归档（6 文件）+ slim sync-plan v2.0 + plans/README.md
- ✅ Rule 4: 待 Phase 4 更新 `post-refactor-architecture.md` §1.3

### 待完成

- ⏸️ Rule 4.3: deprecated 头标（待 Phase 4）
- ⏸️ Rule 5.3: docs-audit 阻断（已存在 pre-commit hook，验证 36/36）
- ⏸️ Rule 6.4: 本 change 归档流程（待 Phase 5）

---

## 验证

- ✅ docs-audit 36/36 PASS（H-4 提交后必须维持）
- ✅ ADR-035 自实施（`architecture-governance` capability spec.md 已写）
- ✅ INDEX (README.md) 列出全部 35 ADR

---

## 跨引用

- **ADR-032** (H-2.5 IGpuDriver Abstraction)
- **ADR-033** (H-3 Phase 2 Lifecycle)
- **ADR-034** (H-7 Deferred Registry)
- **openspec/specs/architecture-governance/spec.md**: 本 ADR 的 spec 形式（7 个 ADDED Requirements）
- **tools/docs-audit.sh**: 36/36 PASS baseline enforcement

---

**最后更新**: 2026-06-23（H-4 governance cleanup 阶段）