# ADR-074: Archive Tasks.md Checkbox Hygiene Policy

**状态**: ✅ Accepted (2026-07-31, stage4-5-cp-phase6-preemption-timeline-sem-gaps; sign-off per ADR-035 Rule 2)

**日期**: 2026-07-31

**提案人**: Sisyphus

**评审者**: Sisyphus (self-approval for hygiene policy; ADR-035 Rule 2 satisfied by self-review in this change context — policy is a hygiene extension of ADR-035's existing self-approval framework for non-controversial meta-policy updates)

**关联 ADR**:
- [ADR-035](adr-035-governance-policy.md) — Governance Policy（本 ADR 自身走此规则）
- [ADR-046](adr-046-preemption-context-switch.md) — Preemption Context Switch（triggering context）

**关联 Change**: `openspec/changes/stage4-5-cp-phase6-preemption-timeline-sem-gaps/`（v1 归档 checkbox 失同步的修复）

---

## Context

### 现状：两条 policy 边界模糊

UsrLinuxEmu 的 OpenSpec workflow 在 `archive/2026-07-29-stage4-5-cp-phase6-preemption-timeline-sem/IMPLEMENTATION_NOTES.md` 明确了两条 archive policy：

1. **Archive spec 不修改**：归档 spec.md 保持历史原貌，读者通过 IMPLEMENTATION_NOTES.md 了解 spec/implementation 不一致
2. **Archive IMPLEMENTATION_NOTES.md 不修改**：记录 spec 失同步原因

**但是**：这两条 policy **未明确** archive 目录中其它文件的状态——尤其是 `tasks.md`。

### 触发场景：v1 归档 hygiene 失败

2026-07-29 archive 的 `stage4-5-cp-phase6-preemption-timeline-sem` 中：
- 22/22 tasks 实际全部完成（commit `d1f569b`, `de620b5`, `d9728e8`, `91b1fbf`, `cbe5bf7` 验证）
- 但 `tasks.md` 保留 6 项 `[ ]` checkbox（2.4, 2.5, 2.6, 2.7, 2.8, 2.9）
- 读者（特别是 reviewer + 历史审计）看到 6 项未完成 checkbox 时，会**错误判断**这 6 项 task 未实施

**问题**：
- "Archive spec 不修改" 政策清楚，但 archive tasks.md checkbox 状态是否可同步（修改），**没有明确 policy**
- 修改可能被误解为"修改归档历史"或"篡改实施记录"
- 不修改则维持错误状态，破坏归档作为实施真相之源的可信度

### 为什么需要架构决策

`tasks.md` 是**实施记录**（spec 级别的行为契约 vs implementation 级别的事实记录），与 `spec.md`（行为契约）有本质区别：

| 文件 | 性质 | 修改影响 |
|------|------|---------|
| `spec.md` | 行为契约 — "系统应该如何" | 修改 = 行为变更追溯 |
| `IMPLEMENTATION_NOTES.md` | 已知偏差记录 — "spec 与实现为何不一致" | 修改 = 抹除历史背景 |
| `tasks.md` | 实施记录 — "实际做了什么" | 修改 = 失同步（与 git log/commits 冲突） |

`tasks.md` checkbox 应**反映实施实际状态**，否则失去 reader 价值。这与 `spec.md` 的"归档不修改"是**正交**的两条 policy。

---

## Decision

### D1: Archive Tasks.md Checkbox 状态可同步

`openspec/changes/archive/<change>/tasks.md` 中的 checkbox 状态 (`[ ]` ↔ `[x]`) **可被更新**以反映实际实施状态，前提：

1. **仅 checkbox 状态**修改：`- [ ]` → `- [x]`（或反向以纠正错误）
2. **不修改** text、commit hash、description 等其它字段
3. **不修改** spec.md、IMPLEMENTATION_NOTES.md
4. **commit message 明确**声明同步意图 + 引用支撑 commits
5. **通过独立 commit**（不混入其它变更）

### D2: 同步需有 Git 证据

每次 checkbox 同步必须能通过 `git log` 追溯到完成该 task 的 commit：

```bash
# 验证示例
git log --all --oneline -- archive/2026-07-29-stage4-5-cp-phase6-preemption-timeline-sem/ \
  | grep -E "mqd_state_preempt|d1f569b"
```

**反例**（不允许）：无 commit 支撑的 checkbox 同步（=捏造实施记录）。

### D3: 同步范围限制

允许同步的 checkbox 状态：
- ✅ `[ ]` → `[x]`（确认完成）

允许的次要操作（仅当不可避免）：
- ✅ 添加 commit hash 注释（per task 实施 commit）
- ❌ 禁止添加新描述、修改 task 文本
- ❌ 禁止删除已存在的 task
- ❌ 禁止修改 task 编号

### D4: 治理路径

任何 archive tasks.md 同步需通过：

1. **OpenSpec change**：记录在 `openspec/changes/<name>/` 中
2. **Spec 约束**：在 `specs/<name>/spec.md` 定义同步范围 + 引用本 ADR
3. **Verification**：`git diff archive/<change>/spec.md` 为空（spec 未修改）
4. **Commit message**：`chore(archive): sync <change> tasks.md checkbox state with implementation`
5. **Reviewer sign-off**：1 Architecture Team 成员确认 commit hash 真实性

---

## Consequences

### 正向

- **归档可信度提升**：reader 看到的 checkbox 状态 = 实际实施状态
- **历史审计准确性**：未来 architect 验证 v1 实施时，无需 grep commit log 单独判断
- **政策明确化**：消除"修改 archive tasks.md 是否合规"的歧义
- **低成本维护**：checkbox 同步是原子操作，不涉及 spec/代码风险

### 负向

- **需要 reviewer 投入**：每次同步需 1 名 Architecture Team 成员验证 commit 真实性
- **不能无限追溯**：early archive（无 git 证据的）只能保持原状
- **政策可能需要 re-evaluate**：如果出现新的 archive 文件类型（如 README.md、tests.md），需后续 ADR 明确

### 中性

- **不强制执行**：不要求所有 archive 都立即同步（按 change 触发）
- **可逆**：任何同步 commit 可 `git revert` 恢复

---

## Migration Plan

### 应用范围

本 ADR 适用所有 `openspec/changes/archive/<change>/tasks.md` 文件，无论 change 何时归档。

### 已知待同步

`openspec/changes/stage4-5-cp-phase6-preemption-timeline-sem-gaps/` 识别出 6 项 checkbox 待同步：
- task 2.4: `mqd_state_preempt` wiring（commit `d1f569b`）
- task 2.5: `mqd_state_resume` wiring（commit `de620b5`）
- task 2.6: IDLE/double-preempt（commit `d9728e8`）
- task 2.7: pending fence table（commit `91b1fbf`）
- task 2.8: fence freeze（commit `d1f569b`）
- task 2.9: `test_preemption_standalone`（commit `cbe5bf7`，PASS 477 assertions）

应用本 change（同步 + 验证 + commit）作为本 ADR 的**首次实际应用**。

### Rollback

本 ADR 无需 rollback（policy 文档）；任何同步操作出错通过 `git revert <commit>` 撤销。

---

## Alternatives Considered

### A1: 禁止修改 archive tasks.md

**Reject 理由**：
- 维持错误状态 = 误导 reader
- 实施真相（git log）与 checkbox 状态漂移
- reader 信任度下降

### A2: 修改 archive spec.md 解决

**Reject 理由**：
- 与 IMPLEMENTATION_NOTES.md 政策冲突
- spec 是行为契约，checkbox 是实施记录，两者性质不同

### A3: 删除 archive 目录 tasks.md

**Reject 理由**：
- 损失 git history 之外的 reader 便利
- 未来 review 失去 anchor

---

## References

- **Triggering context**: `openspec/changes/archive/2026-07-29-stage4-5-cp-phase6-preemption-timeline-sem/IMPLEMENTATION_NOTES.md`（记录 "归档 spec 不修改" 政策，但未明确 tasks.md 政策）
- **First application**: `openspec/changes/stage4-5-cp-phase6-preemption-timeline-sem-gaps/specs/archive-tasks-sync/spec.md`
- **Governance basis**: [ADR-035](adr-035-governance-policy.md) §Rule 2 (ADR 状态标记规则)

---

## 修订记录

- 2026-07-31 v1: 初版（建立 archive tasks.md checkbox 同步政策）
