# Roadmap-Driven Workflow Design (Unified Index + adr-076 Cross-Repo Integration)

| 属性 | 值 |
|------|-----|
| **状态** | 📋 DRAFT（brainstorming 阶段，待 user review） |
| **日期** | 2026-08-10 |
| **作者** | Sisyphus（基于 brainstorming 3 轮 + Oracle c3 推荐） |
| **评审者** | UsrLinuxEmu owner（待评审后转 proposal.md） |
| **关联 brainstorming** | 2026-08-10 session（3 边界问题 + Oracle consultation bg_82847956） |
| **关联问题** | rdd-workflow 入口割裂（adr-076 未走 improvements/）+ proposal-suggestions 空表 + proposal-approved ↔ archive 脱钩 + roadmap ↔ 产物无显式连接 |
| **关联文件** | `roadmap.md`, `docs/roadmap/stage-{0,1,2,3,4,5}-*.md`, `blueprint.md`, `proposal-suggestions.md`, `proposal-approved.md`, `improvements/2026-08-10-roadmap-unified-index.md`（self-referential, 待创建）, `openspec/changes/2026-08-10-roadmap-unified-index/`（待创建）, `adr-076` |
| **最终落地形态** | 通过 rdd-workflow 完整流程（add-improve → guide-plan → guide-ship），本 spec 文件作为 brainstorming 设计稿归档 + 转化为 `openspec/changes/2026-08-10-roadmap-unified-index/proposal.md` + `design.md` |

---

## 1. Context

### 1.1 现状与痛点

项目已成熟运行 rdd-workflow v3+（openspec CLI 1.4.1、.rddf/{plans,state,wt}/、21 个已 ship improvements、最近一周 5+ changes 归档）。roadmap.md 与 docs/roadmap/ 7 个 stage 文档结构成熟。

但 4 个 pre-existing inconsistencies 暴露入口割裂：

| # | Inconsistency | 证据 | 严重度 |
|---|---------------|------|--------|
| A | `roadmap.md:152` 引用 `proposal-suggestions.md` "含 5 个新增 entry: 2026-08-04 提交" — 但该文件实际是**空表** | `roadmap.md:152` vs `proposal-suggestions.md:3-8` | 🟡 中（直接说谎）|
| B | `proposal-approved.md` 标记 `implement-pm4-microcode-parsing` + `add-multi-engine-puller-instances` 为 2026-08-08 已实施 — 但 stage-5 doc 明确"本文件不授权实现"且 Stage 5 trigger-gated 未启动 | `proposal-approved.md` vs `stage-5-multi-engine-pm4.md:5,77` | 🔴 高（治理事故：标错 stage-5 trigger-gated items）|
| C | adr-076 五处引用 "ADR-035 §R3 cross-repo 协议" — 但 adr-035 §R3 实际是 **plans/ 双层归档**，cross-repo 规则在 §R5.1 + §R6.3 | `adr-076` lines 5, 77, 359, 370, 475 vs `adr-035:75-89,106-114` | 🟡 中（ADR 文档错误）|
| D | adr-076 状态用 📋 PROPOSED — adr-035 §R2.1 只允许 ✅/⏸️/🔄/🚫 四种状态符号 | `adr-076:3` vs `adr-035:R2.1` | 🟢 低（status 符号偏差）|

### 1.2 用户原始诉求

> "可以在 docs/roadmap 下创建和更新相应的路径图文档，然后在 roadmap.md 更新当前的执行吗，我想通过 roadmap.md 来推进改进提案和 change 的创建？或者你有更好的如何推进建议，请提供"

解读 = roadmap.md 升级为"叙事 + Unified Index 双层"，提供"派生建议"入口派发 improvements 和 changes；保留 rdd-workflow 流程不变。

### 1.3 设计方案选定

通过 brainstorming 3 轮边界问题 + Oracle 1m46s 咨询，敲定：

| 决策点 | 选项 | 选择 |
|--------|------|------|
| roadmap.md 目标角色 | A=纯 Dashboard / B=Driver / C=Unified Index | **C**（保留 rdd-workflow 流程，roadmap 是导航层） |
| 设计边界 | 只加 Unified Index / +部分 inconsistencies / +全部 inconsistencies | **+全部 4 个 inconsistencies**（用户确认含 B 治理事故追溯） |
| 自身工作流 | 纯 ad-hoc / 轻量 rdd-workflow / 完整 rdd-workflow | **完整 rdd-workflow canonical**（self-referential 示范） |
| adr-076 集成 | 现在派 improvement / 保持 ad-hoc / Hybrid placeholder | **Hybrid placeholder**（Oracle c3 推荐） |

---

## 2. Decision

### D1: roadmap.md 从"叙事文档"升级为"叙事 + Unified Index 双层"

#### D1.1 保留段（不破坏现有结构）
- 架构原则（3 区分 + HAL 桥）
- 阶段总览表
- 阶段关系图
- Stage 5 触发条件
- 阅读顺序
- 跨引用

#### D1.2 新增 4 段
1. **派生建议** — 基于 ADR trigger conditions 列出 candidate improvements。例：
   - "ADR-049 Phase 6+ trigger 未满足 → multi-engine Puller 真实并行 improvements 待 trigger"
   - "ADR-052 Phase 6.5 trigger 未满足 → PM4 microcode parsing improvements 待 trigger"
   - 列出当前 ADR trigger conditions 中已满足、可派发的 candidate improvements
2. **跨仓评审中 ADRs** — 指向 PROPOSED-state cross-repo ADR（首例：adr-076）。状态："待 owner + consumer 双评审"。关联改进待 adr-076 Accepted 后创建。
3. **在途 OpenSpec changes** — 指向 `openspec/changes/INDEX.md` 当前活跃项
4. **已归档 OpenSpec changes 最近 N 个** — 指向 `openspec/changes/archive/`，N = 5（保持文档简洁）

#### D1.3 修复段（REPLACE 现有内容，不属于保留段）
- **roadmap.md:152 假引用** → REPLACE 现有 "已批准改进提案：见 proposal-suggestions.md（含 5 个新增 entry: 2026-08-04 提交）" 为指向 `proposal-suggestions.md` 实际状态 + `proposal-approved.md` + `openspec/changes/INDEX.md` 的 3 链接段落
- 段标题"改进提案入口"是 **REPLACE 152 行**（不是新增，也不是保留）—— 必须在实施时显式删除原引用

### D2: `docs/roadmap/stage-X.md` 4-象限模板

每份 stage 文档统一为 4 象限 + 触发条件段：

```markdown
# Stage X: <Title>

> 状态: <✅/📋/⏸️>
> 触发: <ADR-NNN 引用>

## 概述
...

## 4 象限

### 象限 1: 对应 ADRs
| ADR | 标题 | 状态 | 触发阶段 |
|-----|------|------|----------|

### 象限 2: 活跃 improvements (待派发)
| Improvement | 来源 | 优先级 | 状态 |
|-------------|------|--------|------|
(pointing to proposal-suggestions.md)

### 象限 3: 在途 changes
| Change | 状态 | 起点 | 预计完成 |
|--------|------|------|----------|
(pointing to openspec/changes/INDEX.md)

### 象限 4: 已归档 changes
(最近 N 个, pointing to openspec/changes/archive/)

## 触发条件
- ADR-NNN §"<trigger section>" — <condition>
```

**试点顺序**：
1. Phase 2: `stage-4-bar-ioremap.md`（内容最丰富，先验证模板）
2. Phase 3: `stage-5-multi-engine-pm4.md`（需加 cross-repo 段）
3. Phase 4: `stage-0/1/2/3` docs 改造（follow template）

### D3: `docs/roadmap/stage-5-multi-engine-pm4.md` 特别处理

- **scope 保持**：ADR-049/052（multi-engine + PM4）—— **不扩张**
- **加段**："Cross-Repo 集成评审中" — 指向 adr-076，**明确不属本 stage scope**（防止命名撞 scope）
- **4 象限 + cross-repo 子段**

### D4: `proposal-suggestions.md` 重新激活

- **规范化表头**（5 列：提案 / 优先级 / 来源 / 状态 / 添加时间）
- **加 self-referential 行**：`2026-08-10-roadmap-unified-index`（来源 = 本 spec）

### D5: `proposal-approved.md` 修复（含 B 治理事故追溯）

- **规范化表头**（5 列对齐，行 4 列 → 5 列）
- **B 追溯步骤**：
  1. 查 `openspec/changes/archive/2026-08-08-implement-pm4-microcode-parsing` + `2026-08-08-add-multi-engine-puller-instances` 实际 merge commit
  2. 对照 `stage-5-multi-engine-pm4.md` §"Stage 5 触发条件" 判定 trigger 是否满足
  3. **情况 1**：stage-4 follow-up 误命名 → 标"已实施（实属 stage-4 follow-up，命名误用 stage5）"
  4. **情况 2**：真属 stage-5 trigger 后 → 标"已实施（stage-5 trigger 静默满足；需 ADR-049/052 确认是否更新 trigger 状态）"
  5. 追溯结论写为 1 段说明 + 在 README 或 improvement 内引用

### D6: `adr-076` 文档错误修复（C + D）

- **C 修复**：5 处 "ADR-035 §R3 cross-repo 协议" → 正确引用
  - 5 处分别改为：
    - 引用 §R5.1（4-step cross-repo commit order）— 主引用
    - 引用 §R6.3（mirror protocol）— 辅助引用
    - 移除 "§R3 cross-repo 协议" 字面（§R3 是 plans/ 双层归档，与 cross-repo 无关）
- **D 修复**：状态符号 `📋 PROPOSED` → `🔄 Proposed`（按 R2.1 四状态之一）
- **不动**：
  - adr-076:28 预先命名的 change 名 `2026-08-15-stage5-ptxemu-...`（撞 stage-5 doc scope —— 已知问题，rename 是 **follow-up item**，不属本 design scope；本次仅在 stage-5 doc 加"Cross-Repo 集成评审中"段，明确命名冲突但不强行改 ADR 内容）
  - adr-076 §Acceptance Gate / §Migration / §D1-D8 决策内容（这些都是 adr-076 自身 review scope，本次仅修文档错误）
- **adr-076 文档错误的 follow-up**（本 design 收尾后）：
  - adr-076:28 change 名 rename 为非 stage5- 前缀（建议 `2026-08-XX-cross-repo-ptxemu-kernel-module-hal-extension/`）
  - adr-076 §Acceptance Gate 关系 1 引用 §R5.1 后，附录 A"对齐表" ioctl 编号修订（"39/40/41 超过 8-bit"注释可移除，因 §D1 已用 0x27/0x28/0x29）

### D7: Self-Referential rdd-workflow Canonical 示范

roadmap 改造自身走完整 rdd-workflow 流程，作为**未来 cross-repo ADR 集成的 self-referential 模板**：

| 步骤 | 产出 | 验证 |
|------|------|------|
| 1. add-improve | `improvements/2026-08-10-roadmap-unified-index.md` | rdd-workflow skill ✓ |
| 2. proposal-suggestions.md | 加 self-referential 行 | 表头 5 列对齐 |
| 3. 评审 → proposal-approved.md | 加行（含来源 = 本 spec）| 5 列对齐 |
| 4. guide-plan | `openspec/changes/2026-08-10-roadmap-unified-index/` 含 proposal.md / design.md / tasks.md / spec.md | rdd-workflow plan-done |
| 5. guide-ship | 7 commit (roadmap / stage-4 / stage-5 / stage-0,1,2,3 / proposal-suggestions / proposal-approved / adr-076) | 每 commit ctest PASS |
| 6. 归档 | 移到 `openspec/changes/archive/2026-08-10-roadmap-unified-index/` | rdd-workflow archive-done |

---

## 3. Architecture Overview

```
┌──────────────────────────────────────────────────────────┐
│  ADR-035 §R5.1 + R6.3 (cross-repo canonical / mirror)   │  ← 治理硬约束
└────────────────────┬─────────────────────────────────────┘
                     │
       ┌─────────────┼─────────────┐
       │             │             │
   docs/00_adr/  improvements/  proposal-suggestions.md   proposal-approved.md
   (ADR 仓库)    (源提案)       (入口)                (审批)         (consumed)
       │             │             │                       │             │
       └────────┬────┴─────────────┴───────────────────────┘             │
                │                                                       │
                ▼                                                       ▼
        ┌──────────────────────────────────────┐                ┌────────────────┐
        │  roadmap.md + docs/roadmap/stage-X.md │ ◄──(派生建议)──│  guide-plan    │
        │  = Unified Index (4 象限 + 派生段)    │                │  → openspec    │
        │  + cross-repo 评审中段 (adr-076)     │                │    changes/    │
        └──────────────────────────────────────┘                └────────────────┘
                │                                                       │
                ▼                                                       ▼
        ┌──────────────────────────────────────┐                ┌────────────────┐
        │  ADR trigger conditions              │                │  archive/      │
        │  → 派生建议段自动/手动列出候选        │                │  (实施完成)    │
        └──────────────────────────────────────┘                └────────────────┘
```

**关键不变量**：
- rdd-workflow 流程（add-improve → guide-plan → guide-ship）**零修改**
- ADR 仍是架构决策唯一权威源（per adr-035 §R5.1）
- roadmap 改造本身走 self-referential 完整 rdd-workflow 流程（**示范未来怎么走**）
- adr-076 保持 canonical 不动（per Oracle c3）

---

## 4. Components

### 4.1 `roadmap.md` 顶部结构改造

| 段 | 状态 | 说明 |
|----|------|------|
| 架构原则 | 保留 | 3 区分 + HAL 桥 |
| 阶段总览 | 保留 | 4 阶段 + 蓝图 |
| 阶段关系图 | 保留 | Mermaid-style |
| **派生建议** | **新增** | 基于 ADR trigger conditions 列出 candidate improvements |
| **跨仓评审中 ADRs** | **新增** | 指向 PROPOSED-state cross-repo ADR（首例 adr-076） |
| Stage 5 触发条件 | 保留 | 引用 ADR-049/052 |
| 阅读顺序 | 保留 | — |
| 跨引用 | 保留 | — |
| 改进提案入口 | **修复** | roadmap.md:152 假引用 → 真实状态 |
| **在途 OpenSpec changes** | **新增** | 指向 `openspec/changes/INDEX.md` |
| **已归档 OpenSpec changes** | **新增** | 最近 5 个，指向 `archive/` |

### 4.2 `docs/roadmap/stage-X.md` 4-象限模板

每份 stage 文档统一结构（见 D2）。4 象限 + 触发条件段。

### 4.3 `docs/roadmap/stage-5-multi-engine-pm4.md`

- scope 保持 ADR-049/052
- 加 "Cross-Repo 集成评审中" 段（指向 adr-076，**明确不属本 stage scope**）
- 4 象限 + cross-repo 子段

### 4.4 `proposal-suggestions.md`

- 规范化表头（5 列）
- 加 self-referential 行

### 4.5 `proposal-approved.md`

- 规范化表头（5 列对齐）
- B 治理事故追溯（4 步）

### 4.6 `adr-076` 文档错误

- C 修复（5 处 §R3 引用）
- D 修复（状态符号）

### 4.7 Self-Referential rdd-workflow

完整 6 步流程（见 D7 表）。

---

## 5. Data Flow

### 5.1 入口流（用户读 roadmap）

```
读 roadmap.md
  → 看 "派生建议" 段
  → 看 "跨仓评审中 ADRs" 段
  → 跳转到对应 stage-X doc
  → 看 4 象限
  → 链接到 proposal-suggestions / improvements / openspec/changes/INDEX / archive
```

### 5.2 派发流（基于 roadmap 派发 improvement）

```
读 roadmap "派生建议" 段 / stage-X "活跃 improvements" 象限
  → 决定派发某个 candidate improvement
  → add-improve skill 创建 improvements/X.md
  → proposal-suggestions.md 加行（手动 or add-improve 自动）
  → 评审 → proposal-approved.md
  → guide-plan → openspec/changes/X/
  → guide-ship 实施
  → 归档 → openspec/changes/archive/X/
  → stage-X doc 4 象限"在途" / "已归档" 段更新
```

### 5.3 跨仓流（adr-076 模式，Oracle c3）

```
上游 ship (e.g., PTX-EMU v0.1.0)
  → UsrLinuxEmu owner 评审 adr-076 (本仓)
  → roadmap "跨仓评审中 ADRs" 段加 entry
  → stage-5 doc (或相关 stage) "Cross-Repo 集成评审中" 段加 entry
  → adr-076 升 Accepted
  → create improvements/implement-adr-076-...
  → proposal-suggestions.md 加行
  → 正常 rdd-workflow 流程
  → adr-076 status change 同时:
     - roadmap "跨仓评审中 ADRs" 段移除 entry
     - roadmap "已批准改进提案" 段加 entry
     - stage-5 doc 4 象限更新
```

**Generalization**（Oracle 推荐 pattern）：
> "PROPOSED ADR → roadmap 派生建议 placeholder (status: 评审中) → on Accepted, add-improve creates improvement + suggestions row → guide-plan approval → change"

适用于未来 cross-repo consumer-side ADR。

---

## 6. Migration（10 Phase TDD 5-步结构 + ci-docs-audit 4 项新检查）

| Phase | 内容 | 验证 |
|-------|------|------|
| 0 | OpenSpec artifacts 提交（improvement + proposal-suggestions + change 4-artifact）| rdd-workflow guide-plan ✓ |
| 1 | `roadmap.md` 改造（4 段新增 + 152 行 REPLACE）| grep "5 个新增 entry" 不返回 |
| 2 | `stage-4-bar-ioremap.md` 4 象限模板试点 | 4 象限齐全 + 内容非空 |
| 3 | `stage-5-multi-engine-pm4.md` 改造（加 cross-repo 段 + 4 象限）| cross-repo 段指向 adr-076 |
| 4 | `stage-0/1/2/3` docs 改造（follow template）| 4 个 stage doc 都过模板校验 |
| 5 | `proposal-suggestions.md` 重新激活 | 表头 5 列 + self-referential 行 |
| 6 | `proposal-approved.md` 修复（含 B 治理事故追溯）| B 追溯结论明确 + 5 列对齐 |
| 7 | `adr-076` 文档错误修复（C + D）| grep "ADR-035 §R3 cross-repo" 0 处；状态符号 🔄 |
| 8 | 验证（98 catch2 binaries 全部 build + runnable + ci-docs-audit 4 项新检查）| 全部 PASS |
| 9 | 归档 | rdd-workflow archive |

**ci-docs-audit 4 项新检查**（Phase 8 加入 `tools/docs-audit.sh`）：
- `roadmap.md` 假引用（grep `5 个新增 entry` 0 返回）
- `adr-076` `§R3 cross-repo` 引用（grep 0 返回）
- `adr-076` 状态符号（grep `📋 PROPOSED` 0 返回）
- `proposal-approved.md` 5 列对齐（脚本校验表头列数 == 实际行列数）

**TDD 5-步结构**（per rdd-workflow）：
1. Write failing test（e.g., ci-docs-audit 4 项新检查先实现，期望 fail）
2. Verify fail（运行 audit，确认 fail）
3. Implement（修 roadmap/stage-X/proposal-suggestions/proposal-approved/adr-076）
4. Verify pass（运行 audit + ctest 98/98）
5. Commit（per rdd-workflow "分 Phase commit，失败立即 revert"）

---

## 7. Risks

| 风险 | 等级 | 缓解 |
|------|------|------|
| B 治理事故追溯发现额外问题（scope creep）| 🟡 中 | Phase 6 实施时如发现额外问题（如 ADR-049/052 trigger 状态需更新），升级为独立 follow-up change，**不塞进本次** |
| 6+ commit 中任一 break build | 🟢 低 | 每 commit 后 ctest；rdd-workflow "失败立即 revert" |
| ci-docs-audit 4 项新检查误报 | 🟢 低 | 本地+CI 双重验证；误报立即调规则 |
| proposal-suggestions/approved 维护成本上升 | 🟡 中 | add-improve skill 集成自动加行（本次手动；后续 follow-up）|
| adr-076 评审 owner 误读 roadmap "跨仓评审中" 段为"已批准" | 🟡 中 | 段标题明确"评审中" + 段内说明"待双评审" + 状态显式 PROPOSED |
| stage-5 doc 改造后读者误解 cross-repo 段属 stage-5 scope | 🟢 低 | 段标题"Cross-Repo 集成评审中" + 段内强调"不属本 stage scope" |

---

## 8. Acceptance Gates（必须全过）

1. 10 Phase 全部 ship + 98 catch2 binaries 全部 build + runnable
2. 4 象限模板在 stage-0/1/2/3/4/5 全部上线
3. ci-docs-audit 4 项新检查 PASS
4. B 治理事故追溯完成 + 修正正确（写为 1 段说明 + 在 README 或 improvement 内引用；情况 3/4 时升级 follow-up）
5. adr-076 C/D 修复完成（5 处 §R3 引用 + 1 处状态符号）
6. self-referential improvement 归档完成（roadmap 改造自身完整走 rdd-workflow）
7. adr-076 评审不被本次改造阻塞（保持 PROPOSED 状态，roadmap 显式挂载 + 段标题"评审中"）

---

## 9. Cross-References

### 9.1 内部引用
- `roadmap.md` — 主改造目标
- `docs/roadmap/stage-{0,1,2,3,4,5}-*.md` — 4 象限模板应用
- `docs/roadmap/blueprint.md` — 终态愿景（保持不变）
- `proposal-suggestions.md` — 重新激活
- `proposal-approved.md` — 修复（含 B 治理事故追溯）
- `adr-035-governance-policy.md` — §R5.1 + §R6.3 引用源
- `adr-076-gpgpu-kernel-module-ioctl.md` — C/D 修复目标 + 跨仓评审中段引用
- `adr-023-hal-interface.md` — HAL append-only 治理（HAL fn-ptrs 不在本 design scope）
- `tools/docs-audit.sh` — 4 项新检查加入

### 9.2 外部引用
- `/home/ubuntu/.agents/skills/rdd-workflow/skills/add-improve/SKILL.md` — add-improve 入口
- `/home/ubuntu/.config/opencode/skills/superpowers/brainstorming/SKILL.md` — 本 spec 的设计流程
- `openspec/changes/2026-08-10-roadmap-unified-index/` — 本 spec 转化后的 change artifacts

---

## 10. Appendix A: Oracle Consultation Summary (bg_82847956, 1m 46s)

**Oracle 验证证据**：
- adr-035 §R3 = plans/ 双层归档（**不是** cross-repo canonical source）
- adr-035 §R5.1 = 4-step cross-repo commit order
- adr-035 §R6.3 = mirror protocol
- adr-076 引用 "§R3 cross-repo 协议" 5 次（lines 5, 77, 359, 370, 475）—— 不存在
- adr-076:28 预先命名 change 为 `2026-08-15-stage5-ptxemu-...` 撞 stage-5 doc scope

**Oracle 推荐 c3**（Hybrid）：
- adr-076 保持 canonical 不动
- roadmap/stage doc 加"评审中" placeholder 指向 adr-076
- improvement + suggestions row 只在 adr-076 Accepted 后才创建
- 理由：避免在 cross-repo 合同冻结前派发 pipeline artifact；同时给 Unified Index 真实位置

**Oracle 推荐的 generalization pattern**：
> "PROPOSED ADR → roadmap 派生建议 placeholder (评审中) → on Accepted, add-improve creates improvement + suggestions row → guide-plan approval → change"

适用于未来 cross-repo consumer-side ADR。

---

## 11. Appendix B: 4 Inconsistencies 详细修复计划

### B.1 Inconsistency A — `roadmap.md:152` 假引用

**当前**（roadmap.md:152）：
> "已批准改进提案： 见 proposal-suggestions.md（含 5 个新增 entry: 2026-08-04 提交）"

**修复后**：
> "改进提案入口： 见 [proposal-suggestions.md](proposal-suggestions.md)（待审批提案）/ [proposal-approved.md](proposal-approved.md)（已批准）/ [openspec/changes/INDEX.md](openspec/changes/INDEX.md)（在途 changes）"

**验证**：grep "5 个新增 entry" 0 返回。

### B.2 Inconsistency B — `proposal-approved.md` stage-5 trigger-gated items 误标

**当前**：`implement-pm4-microcode-parsing` + `add-multi-engine-puller-instances` 标 2026-08-08 已实施 — 但 stage-5 doc 明确"本文件不授权实现"。

**追溯步骤**：
1. **查实际 merge commit**：
   - `git log openspec/changes/archive/2026-08-08-implement-pm4-microcode-parsing/` 找 commit
   - `git log openspec/changes/archive/2026-08-08-add-multi-engine-puller-instances/` 找 commit
2. **查 stage-5 trigger 状态**：
   - 读 `stage-5-multi-engine-pm4.md` §"Stage 5 触发条件"
   - 读 ADR-049 §"Phase 6+ 触发条件"
   - 读 ADR-052 §"Phase 6.5 触发条件"
3. **判定**：
   - **情况 1**：merge commit 在 stage-4 范围内（如 B-class L2 follow-up）但命名误用 stage5 → 标"已实施（实属 stage-4 follow-up，命名误用 stage5）"
   - **情况 2**：merge commit 真属 stage-5 trigger 后 → 标"已实施（stage-5 trigger 静默满足；需 ADR-049/052 确认是否更新 trigger 状态）"
   - **情况 3**：merge commit 与 stage-5 无关（如纯文档/测试改进被误归类） → 标"已实施（与 stage-5 无关，需追溯实际 stage 归属）"
   - **情况 4**：以上都不是 → 升级为独立 follow-up change，**不塞进本次**
4. **写追溯结论**为 1 段说明 + 在 README 或 improvement 内引用

**验证**：
- 追溯结论明确写为 1 段
- 5 列对齐（提案 / 优先级 / 来源 / 批准日期 / 批准人）
- 追溯引用：阶段判定依据 + 实际 commit SHA

### B.3 Inconsistency C — `adr-076` 5 处 §R3 引用错误

**当前**：adr-076 lines 5, 77, 359, 370, 475 引用 "ADR-035 §R3 cross-repo 协议" — 但 adr-035 §R3 是 plans/ 双层归档。

**修复**（逐处）：

| 行 | 当前 | 修复为 |
|----|------|--------|
| 5 | "ADR-035 §R3 治理：跨仓 ABI 变更走 canonical ADR + consumer-side TADR" | "ADR-035 §R5.1 治理：跨仓 ABI 变更走 canonical ADR + consumer-side TADR（4-step commit order）" |
| 77 | "ADR-035 §R3 cross-repo 协议要求" | "ADR-035 §R5.1 cross-repo 4-step commit order 要求" |
| 359 | "跨仓 commit 顺序错位：§Migration 提供严格 4 步 commit 顺序（per ADR-035 §R3）" | "跨仓 commit 顺序错位：§Migration 提供严格 4 步 commit 顺序（per ADR-035 §R5.1）" |
| 370 | "ADR-035 §R3 治理：跨仓 ABI 变更走 canonical ADR" | "ADR-035 §R5.1 治理：跨仓 ABI 变更走 canonical ADR（4-step commit order）" |
| 475 | "ADR-035 §R3 治理：跨仓 ABI 变更走 canonical ADR（本 ADR）+ consumer-side TADR" | "ADR-035 §R5.1 治理：跨仓 ABI 变更走 canonical ADR（本 ADR）+ consumer-side TADR（4-step commit order）" |

**验证**：grep "ADR-035 §R3 cross-repo" 0 返回。

### B.4 Inconsistency D — `adr-076` 状态符号

**当前**：adr-076:3 状态 `📋 PROPOSED`

**修复**：状态 `🔄 Proposed`（按 adr-035 §R2.1 四状态之一）

**验证**：grep "📋 PROPOSED" adr-076 0 返回。

---

## 12. Appendix C: 7 Self-Referential Commits 详细

| # | Commit | 文件 | 内容 |
|---|--------|------|------|
| 1 | feat(roadmap): upgrade to Unified Index | `roadmap.md` | 4 段新增 + 152 行 fix |
| 2 | feat(roadmap): apply 4-quadrant template to stage-4 | `docs/roadmap/stage-4-bar-ioremap.md` | 4 象限模板（试点）|
| 3 | feat(roadmap): apply 4-quadrant template to stage-5 + cross-repo section | `docs/roadmap/stage-5-multi-engine-pm4.md` | 4 象限 + cross-repo 段 |
| 4 | feat(roadmap): apply 4-quadrant template to stage-0/1/2/3 | `docs/roadmap/stage-{0,1,2,3}-*.md` | 4 象限 |
| 5 | feat(proposal): reactivate suggestions + add self-referential row | `proposal-suggestions.md` | 5 列对齐 + self-referential 行 |
| 6 | fix(proposal): correct B incident + normalize approved table | `proposal-approved.md` | B 追溯结论 + 5 列对齐 |
| 7 | fix(adr-076): correct §R3 references + status symbol | `adr-076` | C + D 修复 |

每 commit 独立 ctest 验证。

---

## 13. Appendix D: 备选方案（已否决）

### 13.1 方案 A — 纯 Dashboard（最小改动）
- **否决理由**：不符合用户原话"推进"；不解决痛点 1（入口割裂）

### 13.2 方案 B — Driver 模式（重设计 rdd-workflow 入口）
- **否决理由**：与 rdd-workflow 流程 + ADR-035 治理规则冲突；改动大；ad-hoc 跨仓 ADR 无法挂上；风险高

---

**维护者**: UsrLinuxEmu Architecture Team + brainstorming author
**最后更新**: 2026-08-10（DRAFT，待 user review）
**关联 brainstorming session**: 2026-08-10
