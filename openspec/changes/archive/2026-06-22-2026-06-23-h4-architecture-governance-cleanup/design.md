# Design: h4-architecture-governance-cleanup

> **状态**: 🔵 Proposed (2026-06-23)
> **依赖**: proposal.md 已完成
> **作用**: 解释 HOW 实施 governance cleanup（ADR 编号、plans/ 归档、架构蓝图同步）

---

## Context

### 前置依赖（全部 archived）

| Change | Status | 关键产出 |
|---|---|---|
| H-1 (`h1-pushbuffer-validation-closeout`) | ✅ archived 2026-06-17 | fence_id 返回机制 + pushbuffer 验证 |
| H-2 (`h2-phase2-openspec-skeleton`) | ⏸️ DEPRECATED 2026-06-19 | Path D 决策证据保留 |
| H-2.5 (`h2-5-architecture-foundation`) | ✅ archived 2026-06-19 | IGpuDriver 抽象 + MockGpuDriver + DI |
| H-3 (`h3-phase2-management`) | ✅ archived 2026-06-22 | 5 Phase 2 ioctl wrapper + 12 test cases + CLI |

### 当前结构问题

**plans/ 混杂**（6 文件 + 1 目录，跨 2 个月时间跨度）：
- `2026-06-19-h2-phase2-openspec-skeleton/` — DEPRECATED H-2 骨架
- `2026-06-19-rebase-h1-onto-main.md` — H-1 rebase execution record
- `findings.md` (May 6) — H-2.5 之前接口分析
- `progress.md` (May 6) — H-2.5 之前进度
- `interface-unification-plan.md` (May 6) — H-2.5 之前计划
- `gpu_queue_architecture_research.md` (May 6) — AMD vs NVIDIA GPU queue research（content 仍有参考价值）
- `sync-plan.md` (Jun 23) — 当前同步计划（含已完成的 S0-S4 章节）

**ADR 治理缺位**（31 个 ADR，无 INDEX）：
- ADR-001 至 ADR-024：实质性决策
- ADR-025/026/028/029/030：⏸️ Deferred Phase 3+ 占位（5 份）
- ADR-027：🔄 linux-compat strategy 提议中
- ADR-031：✅ TTM migration priority（最近升级）
- 无 ADR-032+ 编号
- 无 INDEX / README.md

**架构蓝图落后**：
- `post-refactor-architecture.md` §1.3 标注 "v0.1.5 待加" — H-2.5 + H-3 后未更新
- `architecture.md` / `architecture_design.md` / `overview.md` 标注 "最后验证 2026-06-16" — pre-H-2.5/pre-H-3 内容

---

## Goals / Non-Goals

### Goals

1. **`plans/` 仅保留当前 sync-plan.md**（瘦身），其余 6 文件归档
2. **新增 4 个 ADR**（H-2.5 / H-3 / H-7 deferred / governance）+ INDEX.md
3. **`post-refactor-architecture.md` §1.3 v0.1.5 后**完整覆盖
4. **跨仓 sync** 双仓 push + openspec archive
5. **docs-audit 维持 36/36 PASS**（不引入新检查规则）
6. **下一波 change 引用 ADR-032/033/034** 而非 openspec archive design.md

### Non-Goals

- ❌ 不重写已有 31 个 ADR
- ❌ 不删除任何文档（统一归档保留）
- ❌ 不动 docs-audit 检查规则
- ❌ 不启动新实施（H-3.5 / H-7 / Phase 3 都在本 change 之后）
- ❌ 不画 5+ 张 diagram（3-4 张足够）
- ❌ 不重写 pre-v0.1.5 architecture.md / architecture_design.md / overview.md（加 deprecated 头标保留为历史）

---

## Decisions

### D1 — `plans/` 目录结构 = **B. 双层：当前 + archive**

**决策**：保留 `plans/` 当前文件 + 新增 `plans/archive/` 子目录

```
external/TaskRunner/plans/
├── README.md (新增，目录说明 + 归档索引)
├── sync-plan.md (瘦身版，仅 S5 ✅ + 未来 hooks)
└── archive/
    ├── 2026-06-19-h2-phase2-openspec-skeleton/ (整个目录)
    ├── 2026-06-19-rebase-h1-onto-main.md
    ├── findings.md (May 6)
    ├── progress.md (May 6)
    ├── interface-unification-plan.md (May 6)
    └── gpu_queue_architecture_research.md (DEPRECATED 标记保留)
```

**理由**：
- `plans/archive/` 保留 TaskRunner 子模块内部的归档独立（不污染 UsrLinuxEmu openspec archive）
- `sync-plan.md` 与 TaskRunner submodule 紧密相关，保留在子模块内
- 老 H-2 骨架 (`h2-phase2-openspec-skeleton/`) 移动到 UsrLinuxEmu openspec archive 与 H-3 路径对齐（保持一致性）

**权衡**：
- 双层归档（plans/archive + openspec/changes/archive）增加目录层级
- 但归档检索更直接（不需要跨仓搜索）

### D2 — ADR 编号规则 = **A. 续号 + INDEX**

**决策**：新增 ADR 从 032 续号，新增 README.md 作为 INDEX

```
UsrLinuxEmu/docs/00_adr/
├── README.md (新增 INDEX，列出全部 35 个 ADR 状态)
├── adr-001-user-mode-emulation.md
├── ...
├── adr-031-ttm-migration-priority.md (现有)
├── adr-032-h2-5-igpu-driver-abstraction.md (新增)
├── adr-033-h3-phase2-lifecycle.md (新增)
├── adr-034-h7-deferred-registry.md (新增)
└── adr-035-governance-policy.md (新增)
```

**理由**：
- 续号保持单调（032-035 紧接 031）
- README.md INDEX 让新贡献者一眼看到全 ADR 状态（不需 grep）
- 不修改已有 ADR（避免 history 污染）

**ADR 模板**（用于 032-035）：
```markdown
# ADR-NNN: <Title>

**状态**: <✅ Accepted / ⏸️ Deferred / 🔄 Proposed / 🚫 Rejected>
**日期**: YYYY-MM-DD
**提案人**: <Name>
**评审者**: <Names>
**关联 ADR**: ADR-XXX, ADR-YYY
**关联 Change**: openspec/changes/<change-name>/
**关联 Source**: <openspec archive design.md §X>

## Context
...

## Decision
...

## Consequences
...

## Migration
...
```

### D3 — 架构蓝图同步 = **A. 在 post-refactor-architecture.md 加 H-2.5 + H-3 章节**

**决策**：在 `post-refactor-architecture.md` 新增 §1.3 后续 + 3-4 张 Mermaid diagram

**新章节**：
- §1.3.1 H-2.5 IGpuDriver 抽象层（基于 ADR-032）
- §1.3.2 H-3 Phase 2 lifecycle（基于 ADR-033）
- §1.4 (existing) 扩展 + Mermaid
- §X 3 张 Mermaid：
  - `IGpuDriver` 实现关系图
  - Phase 2 ioctl 流（CLI → GpuDriverClient → gpgpu_device）
  - openspec change lifecycle（active → archive）

**理由**：
- `post-refactor-architecture.md` 已是 SSOT，扩展而非重写
- 3-4 张 diagram 足以表达关键架构（避免 over-engineering）
- 加 H-2.5 + H-3 章节让 SSOT 与代码实际状态对齐

**架构.md / architecture_design.md / overview.md 处理**：加 deprecated 头标，不重写：
```markdown
> ⚠️ **DEPRECATED**: 此文档最后验证于 2026-06-16 (commit `374d463`)，pre-v0.1.5。
> **请使用 SSOT**: [`post-refactor-architecture.md`](post-refactor-architecture.md)（持续更新至 v0.1.7+）。
```

### D4 — ADR 占位 (025/026/028-030) 处理 = **B. 保留不动**

**决策**：不重写 ADR-025/026/028-030 内容，仅在 README.md INDEX 中显式标记

**理由**：
- ADR-025 内容已经是 ⏸️ Deferred 状态（包含触发条件）
- 其他 4 份状态可能类似（需确认）
- 本 change scope 是 governance cleanup，不动 Phase 3+ roadmap 决策
- 占位本身有 metadata 价值（编号不浪费）

**ADR-026/028/029/030 内容审查**（Phase 3 内）：在 INDEX.md 中标注，待未来 owner 认领时填入决策

### D5 — 跨仓 sync 策略 = **A. 先 TaskRunner push，再 UsrLinuxEmu combined commit**

**决策**：与 H-3 sync 流程一致

```
Step 1: TaskRunner commit + push (plans/ 归档 + sync-plan.md 瘦身)
Step 2: UsrLinuxEmu commit (submodule pointer + ADR 4 个 + INDEX + 蓝图 + H-4 archive)
Step 3: UsrLinuxEmu push
```

**理由**：
- TaskRunner 先 push 确保 submodule HEAD 可见
- UsrLinuxEmu combined commit 引用 submodule HEAD
- 与 H-3 sync 流程一致（已验证）

---

## Target Structure (Final State)

### TaskRunner side
```
external/TaskRunner/
├── plans/
│   ├── README.md (新增，目录说明)
│   ├── sync-plan.md (瘦身)
│   └── archive/
│       ├── 2026-06-19-h2-phase2-openspec-skeleton/ (整个)
│       ├── 2026-06-19-rebase-h1-onto-main.md
│       ├── findings.md
│       ├── progress.md
│       ├── interface-unification-plan.md
│       └── gpu_queue_architecture_research.md (DEPRECATED 标记)
```

### UsrLinuxEmu side
```
UsrLinuxEmu/
├── docs/00_adr/
│   ├── README.md (新增 INDEX，35 个 ADR)
│   ├── adr-001..adr-031 (existing)
│   ├── adr-032-h2-5-igpu-driver-abstraction.md (新增)
│   ├── adr-033-h3-phase2-lifecycle.md (新增)
│   ├── adr-034-h7-deferred-registry.md (新增)
│   └── adr-035-governance-policy.md (新增)
├── docs/02_architecture/
│   ├── post-refactor-architecture.md (§1.3 v0.1.5+ 完整更新 + 3-4 张 Mermaid)
│   ├── architecture.md (deprecated 头标)
│   ├── architecture_design.md (deprecated 头标)
│   ├── overview.md (deprecated 头标)
│   ├── index.md (链接到 post-refactor-architecture.md)
│   └── refactor-history.md (保留)
├── openspec/changes/
│   └── archive/2026-06-23-h4-architecture-governance-cleanup/ (after archive)
```

---

## Risks / Trade-offs

| Risk | Impact | Mitigation |
|---|---|---|
| **R1**: 归档丢失历史上下文 | 中 | 归档目录保留完整文件 + 加 README 索引 + INDEX.md 交叉引用 |
| **R2**: 新增 ADR 与已有 ADR 编号冲突 | 低 | 续号从 032 起，明确无冲突 |
| **R3**: 蓝图更新引入新错误 | 中 | 加 3-4 张 diagram 后跑 docs-audit + 人工 review |
| **R4**: 跨仓 sync 失败 | 低 | 与 H-3 sync 流程一致，docs-audit 36/36 必须 PASS |
| **R5**: ADR 占位 (025/026/028-030) 内容不一致 | 低 | INDEX.md 标注状态，Phase 3 owner 认领时填充 |
| **R6**: plans/archive/ 与 openspec/archive/ 重复 | 低 | D1 决策：plans/archive 收 TaskRunner-internal 历史，openspec/archive 收跨仓 openspec change |

---

## Migration Plan

### Phase 1: OpenSpec 提案（已完成 proposal.md，正在写 design.md/tasks.md/spec.md/.openspec.yaml）

### Phase 2: 归档 plans/

```bash
cd external/TaskRunner
mkdir plans/archive
# Move H-2 skeleton to UsrLinuxEmu openspec archive (consistency with H-3 path)
mv plans/2026-06-19-h2-phase2-openspec-skeleton ../../openspec/changes/archive/2026-06-19-h2-phase2-openspec-skeleton
# Move other historical files to plans/archive
git mv plans/2026-06-19-rebase-h1-onto-main.md plans/archive/
git mv plans/findings.md plans/archive/
git mv plans/progress.md plans/archive/
git mv plans/interface-unification-plan.md plans/archive/
git mv plans/gpu_queue_architecture_research.md plans/archive/

# Slim down sync-plan.md
# (remove S0-S4 historical sections, keep S5 ✅ + future hooks only)

# Create plans/README.md (directory index)
```

### Phase 3: 新增 ADR

```bash
cd UsrLinuxEmu
# Create INDEX
touch docs/00_adr/README.md  # write INDEX content

# Create 4 new ADRs from openspec archive designs
touch docs/00_adr/adr-032-h2-5-igpu-driver-abstraction.md
touch docs/00_adr/adr-033-h3-phase2-lifecycle.md
touch docs/00_adr/adr-034-h7-deferred-registry.md
touch docs/00_adr/adr-035-governance-policy.md
```

### Phase 4: 架构蓝图

```bash
cd UsrLinuxEmu
# Update post-refactor-architecture.md
# - Add §1.3.1 (H-2.5 IGpuDriver)
# - Add §1.3.2 (H-3 Phase 2)
# - Add 3-4 Mermaid diagrams
# Update architecture.md / architecture_design.md / overview.md (deprecated header)
```

### Phase 5: 验证 + 双仓 sync

```bash
cd UsrLinuxEmu
# Verify docs-audit
bash tools/docs-audit.sh --strict  # expect 36/36 PASS

# Verify cross-references
grep -rn "openspec/changes/archive/2026-06-22-h3" UsrLinuxEmu/docs/  # check no broken refs
grep -rn "plans/2026-06-19-h2-phase2" UsrLinuxEmu/docs/  # check no broken refs

# Stage UsrLinuxEmu changes
git add openspec/changes/2026-06-23-h4-architecture-governance-cleanup/
git add docs/00_adr/ docs/02_architecture/

# Commit + archive
openspec archive h4-architecture-governance-cleanup -y

# Push
git push origin main
```

### Rollback

| Phase | Rollback |
|---|---|
| 2 | `git restore --staged external/TaskRunner/plans/ && git restore external/TaskRunner/plans/` |
| 3 | `git rm docs/00_adr/adr-03{2,3,4,5}-*.md docs/00_adr/README.md` |
| 4 | `git restore docs/02_architecture/post-refactor-architecture.md` |
| 5 | `git push --force-with-lease` 回退（仅本地未 push 状态） |

各阶段独立 commit，独立 revert，互不干扰。