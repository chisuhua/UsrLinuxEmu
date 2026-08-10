# Roadmap Unified Index Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Upgrade `roadmap.md` + `docs/roadmap/stage-*.md` to a "Unified Index + 派生建议" navigation layer, fix 4 pre-existing documentation inconsistencies, and self-referentially demonstrate the rdd-workflow pipeline for future cross-repo consumer-side ADRs (first example: adr-076).

**Architecture:** Roadmap documents remain narrative + stage-narrative, but add 4 new navigation segments (派生建议 / 跨仓评审中 ADRs / 在途 changes / 已归档 changes). Each stage doc becomes 4-quadrant (ADRs / active improvements / in-flight changes / archived changes) + trigger-conditions. rdd-workflow pipeline is unchanged. adr-076 stays canonical in `docs/00_adr/`; roadmap gains a "评审中" placeholder pointer until it升 Accepted. All changes flow through rdd-workflow 6-step (add-improve → guide-plan → guide-ship).

**Tech Stack:** bash (ci-docs-audit), Markdown (specs/ADRs/roadmap), openspec CLI 1.4.1, rdd-workflow v3+ skills.

**Spec:** `docs/superpowers/specs/2026-08-10-roadmap-driven-workflow-design.md` (490 lines; this plan is the implementation breakdown).

---

## File Structure

| File | Operation | Task # | Purpose |
|------|-----------|--------|---------|
| `tools/docs-audit.sh` | Extend | 1 | 4 new checks: grep `5 个新增 entry` / `§R3 cross-repo` / `📋 PROPOSED` / 5-col alignment |
| `improvements/2026-08-10-roadmap-unified-index.md` | Create | 2 | self-referential source proposal |
| `proposal-suggestions.md` | Modify | 3 | 5-col header + self-referential row |
| `openspec/changes/2026-08-10-roadmap-unified-index/proposal.md` | Create | 4 | rdd-workflow proposal artifact |
| `openspec/changes/2026-08-10-roadmap-unified-index/design.md` | Create | 4 | rdd-workflow design artifact |
| `openspec/changes/2026-08-10-roadmap-unified-index/tasks.md` | Create | 4 | rdd-workflow tasks artifact (1:1 with this plan) |
| `openspec/changes/2026-08-10-roadmap-unified-index/specs/roadmap-unified-index/spec.md` | Create | 4 | capability spec |
| `roadmap.md` | Modify | 5 | 4 new segments + 152-line REPLACE |
| `docs/roadmap/stage-4-bar-ioremap.md` | Modify | 6 | 4-quadrant template (pilot) |
| `docs/roadmap/stage-5-multi-engine-pm4.md` | Modify | 7 | 4-quadrant + "Cross-Repo 集成评审中" section |
| `docs/roadmap/stage-0-mvp.md` | Modify | 8 | 4-quadrant template |
| `docs/roadmap/stage-1-kernel-emu.md` | Modify | 8 | 4-quadrant template |
| `docs/roadmap/stage-2-multi-device.md` | Modify | 8 | 4-quadrant template |
| `docs/roadmap/stage-3-v1.0.md` | Modify | 8 | 4-quadrant template |
| `proposal-approved.md` | Modify | 9 | 5-col header + B-incident trace (4-情况判定) |
| `adr-076-gpgpu-kernel-module-ioctl.md` | Modify | 10 | 5 §R3 references + 1 status symbol (📋 → 🔄) |
| `tools/docs-audit.sh` (final run) | Verify | 11 | All 4 new checks PASS + 98 catch2 binaries build |
| `openspec/changes/2026-08-10-roadmap-unified-index/` → `archive/` | Move | 12 | rdd-workflow archive-done |

**Total: 12 tasks, 1 file extended, 7 files created, 8 files modified, 1 directory moved.**

---

## Task 0: Setup Worktree + Verify Tools

**Files:**
- Verify: `openspec --version` returns 1.4.1+
- Verify: `ls .rddf/{plans,state,wt}/` exists
- Verify: `bash tools/docs-audit.sh --help` (or similar) works

- [ ] **Step 1: Create worktree**

```bash
cd /workspace/project/UsrLinuxEmu
git fetch origin main
git worktree add ../wt-roadmap-unified-index -b feat/roadmap-unified-index main
cd ../wt-roadmap-unified-index
```

- [ ] **Step 2: Verify openspec CLI**

```bash
openspec --version
```

Expected: `1.4.1` or later.

- [ ] **Step 3: Verify rdd-workflow state directories**

```bash
ls -la .rddf/{plans,state,wt}/
```

Expected: 3 directories exist (plans/ + state/ + wt/).

- [ ] **Step 4: Verify current docs-audit baseline**

```bash
bash tools/docs-audit.sh 2>&1 | tail -20
```

Expected: PASS or known-failure output (capture baseline for comparison after Task 11).

- [ ] **Step 5: Commit baseline (no changes yet)**

Skip — worktree starts clean from main.

---

## Task 1: Extend `tools/docs-audit.sh` With 4 New Checks (TDD: Write Failing Test First)

**Files:**
- Modify: `tools/docs-audit.sh` (extend with 4 new check functions + dispatcher)

- [ ] **Step 1: Write the failing test — add 4 new check functions**

Append to `tools/docs-audit.sh` (find a stable insertion point after the last existing check function):

```bash
# --- ADR-076 / roadmap cross-reference checks (added 2026-08-10) ---

check_roadmap_152_fake_ref() {
    # Inconsistency A: roadmap.md:152 references proposal-suggestions.md with stale
    # "5 个新增 entry" text that contradicts the file's actual empty state.
    if grep -nE "5 个新增 entry" roadmap.md 2>/dev/null; then
        echo "FAIL: roadmap.md:152 contains stale '5 个新增 entry' reference"
        return 1
    fi
    return 0
}

check_adr076_r3_cross_repo_ref() {
    # Inconsistency C: adr-076 cites non-existent "ADR-035 §R3 cross-repo 协议"
    # in 5 places (lines 5, 77, 359, 370, 475). §R3 in adr-035 is plans/ 双层归档,
    # not a cross-repo rule. The actual cross-repo rules are §R5.1 + §R6.3.
    if grep -nE "ADR-035 §R3 cross-repo" docs/00_adr/adr-076-gpgpu-kernel-module-ioctl.md 2>/dev/null; then
        echo "FAIL: adr-076 still references non-existent 'ADR-035 §R3 cross-repo 协议'"
        return 1
    fi
    return 0
}

check_adr076_status_symbol() {
    # Inconsistency D: adr-076 uses 📋 PROPOSED, but adr-035 §R2.1 allows only
    # ✅/⏸️/🔄/🚫 four status symbols.
    if grep -nE "📋 PROPOSED" docs/00_adr/adr-076-gpgpu-kernel-module-ioctl.md 2>/dev/null; then
        echo "FAIL: adr-076 status uses 📋 PROPOSED, must use 🔄 Proposed per adr-035 §R2.1"
        return 1
    fi
    return 0
}

check_proposal_approved_5col_alignment() {
    # D5: proposal-approved.md must have 5 columns (提案 / 优先级 / 来源 / 批准日期 / 批准人)
    # matching header and all body rows. This catches malformed table drift.
    local file="proposal-approved.md"
    local header_col_count
    header_col_count=$(head -10 "$file" | grep -E "^\| .* \|" | head -1 | awk -F'|' '{print NF-2}')
    if [ "$header_col_count" != "5" ]; then
        echo "FAIL: $file header has $header_col_count columns, expected 5"
        return 1
    fi
    # Verify body rows have same column count
    local bad_row
    bad_row=$(grep -nE "^\| .* \|" "$file" | tail -n +2 | awk -F'|' 'NF-2 != 5 {print NR": "$0; exit}')
    if [ -n "$bad_row" ]; then
        echo "FAIL: $file has body row with wrong column count: $bad_row"
        return 1
    fi
    return 0
}
```

Then find the main dispatcher section (typically at the bottom of the script) and add the 4 new check invocations in the same style as existing checks. For example, if the existing pattern is:

```bash
# Existing dispatcher
run_check "check_xyz" || exit 1
```

Add:

```bash
# New checks (added 2026-08-10)
run_check "check_roadmap_152_fake_ref" || exit 1
run_check "check_adr076_r3_cross_repo_ref" || exit 1
run_check "check_adr076_status_symbol" || exit 1
run_check "check_proposal_approved_5col_alignment" || exit 1
```

(Adjust the dispatcher invocation to match the existing convention in `tools/docs-audit.sh` — read the script first to confirm the pattern.)

- [ ] **Step 2: Run audit to verify it fails (TDD red)**

```bash
bash tools/docs-audit.sh 2>&1 | tail -30
```

Expected: 4 FAIL lines (one per new check), since none of the underlying fixes are in place yet.

- [ ] **Step 3: Commit failing test**

```bash
git add tools/docs-audit.sh
git commit -m "test(docs-audit): add 4 checks for adr-076 + roadmap inconsistencies (red)"
```

---

## Task 2: Create Self-Referential Improvement File

**Files:**
- Create: `improvements/2026-08-10-roadmap-unified-index.md`

- [ ] **Step 1: Create improvement file**

```bash
cat > improvements/2026-08-10-roadmap-unified-index.md <<'EOF'
# Improvement: Roadmap Unified Index + 4 Inconsistencies Fix

## Status
- **Priority**: P1
- **Source**: Spec `docs/superpowers/specs/2026-08-10-roadmap-driven-workflow-design.md` (2026-08-10)
- **Filed**: 2026-08-10
- **Reviewer**: UsrLinuxEmu owner

## Problem
4 pre-existing documentation inconsistencies expose entry fragmentation in the rdd-workflow pipeline:

| # | Inconsistency | Evidence |
|---|---------------|----------|
| A | `roadmap.md:152` references `proposal-suggestions.md` "含 5 个新增 entry: 2026-08-04 提交" but the file is empty | `roadmap.md:152` vs `proposal-suggestions.md:3-8` |
| B | `proposal-approved.md` marks `implement-pm4-microcode-parsing` + `add-multi-engine-puller-instances` as 已实施 2026-08-08, but `stage-5-multi-engine-pm4.md` says Stage 5 not started and the doc "不授权实现" | `proposal-approved.md` vs `stage-5-multi-engine-pm4.md:5,77` |
| C | adr-076 cites non-existent "ADR-035 §R3 cross-repo 协议" in 5 places | `adr-076` lines 5, 77, 359, 370, 475 vs `adr-035:75-89,106-114` |
| D | adr-076 status uses 📋 PROPOSED, but adr-035 §R2.1 allows only ✅/⏸️/🔄/🚫 | `adr-076:3` vs `adr-035:R2.1` |

## Goal
- roadmap.md + docs/roadmap/stage-*.md become Unified Index (4 象限 + 派生建议 + 跨仓评审中段)
- All 4 inconsistencies fixed
- adr-076 cross-repo consumer-side ADR pattern documented as Oracle c3 recommendation
- Self-referential: this change itself goes through rdd-workflow as the canonical example

## Approach
- 10-phase TDD 5-step migration (see `openspec/changes/2026-08-10-roadmap-unified-index/tasks.md`)
- 7 git commits (roadmap / stage-4 / stage-5 / stage-0,1,2,3 / proposal-suggestions / proposal-approved / adr-076)
- 4 new ci-docs-audit checks enforced

## Acceptance
- 98 catch2 binaries build + runnable
- 4 new ci-docs-audit checks PASS
- B-incident trace conclusion documented
- adr-076 C+D fixed
- adr-076 评审 continues (not blocked by this change)
EOF
```

- [ ] **Step 2: Verify file created with correct content**

```bash
head -5 improvements/2026-08-10-roadmap-unified-index.md
```

Expected: 5 lines starting with `# Improvement: Roadmap Unified Index + 4 Inconsistencies Fix`.

- [ ] **Step 3: Commit**

```bash
git add improvements/2026-08-10-roadmap-unified-index.md
git commit -m "feat(improvements): add self-referential roadmap-unified-index source"
```

---

## Task 3: Reactivate `proposal-suggestions.md` (5-Col Header + Self-Referential Row)

**Files:**
- Modify: `proposal-suggestions.md`

- [ ] **Step 1: Read current state**

```bash
cat proposal-suggestions.md
```

Expected: Header line + empty table. Confirm 5-col header (提案 / 优先级 / 来源 / 状态 / 添加时间) is already in place per spec D4. If not, normalize it to 5 columns in Step 2.

- [ ] **Step 2: Add self-referential row**

Append (preserving table structure) the new row right after the table header separator line:

```markdown
| [2026-08-10-roadmap-unified-index](improvements/2026-08-10-roadmap-unified-index.md) | P1 | spec:2026-08-10-roadmap-driven-workflow-design | 🔄 Proposed | 2026-08-10 |
```

(If the existing table separator line is missing — e.g., the file has only the header row without the `|---|---|---|---|---|` line — add it before the data row.)

- [ ] **Step 3: Verify file structure**

```bash
cat proposal-suggestions.md
```

Expected: Header (5 cols) + separator + 1 data row (self-referential). Total table body = 1 row.

- [ ] **Step 4: Commit**

```bash
git add proposal-suggestions.md
git commit -m "feat(proposal): reactivate suggestions table with self-referential row"
```

---

## Task 4: Create OpenSpec Change Artifacts (4-File Standard)

**Files:**
- Create: `openspec/changes/2026-08-10-roadmap-unified-index/proposal.md`
- Create: `openspec/changes/2026-08-10-roadmap-unified-index/design.md`
- Create: `openspec/changes/2026-08-10-roadmap-unified-index/tasks.md`
- Create: `openspec/changes/2026-08-10-roadmap-unified-index/specs/roadmap-unified-index/spec.md`

- [ ] **Step 1: Create change directory**

```bash
mkdir -p openspec/changes/2026-08-10-roadmap-unified-index/specs/roadmap-unified-index
```

- [ ] **Step 2: Create proposal.md**

```bash
cat > openspec/changes/2026-08-10-roadmap-unified-index/proposal.md <<'EOF'
# Proposal: Roadmap Unified Index + 4 Inconsistencies Fix

## Why
roadmap.md 与 rdd-workflow 产物 (improvements / proposal-suggestions / openspec/changes) 无显式连接，导致：
1. 读者看不到"Stage X 实际产出了哪些 ADRs/changes/improvements"
2. adr-076 类 cross-repo consumer-side ADR 无统一入口（直接落在 docs/00_adr/，未走 improvements/）
3. proposal-suggestions.md 空表 + proposal-approved.md 误标 stage-5 trigger-gated items 已实施
4. roadmap.md:152 假引用 + adr-076 5 处 §R3 引用错误

## What Changes
- roadmap.md: 4 段新增（派生建议 / 跨仓评审中 ADRs / 在途 changes / 已归档 changes）+ 152 行 REPLACE
- docs/roadmap/stage-{0,1,2,3,4,5}-*.md: 4 象限模板 + stage-5 加 cross-repo 段
- proposal-suggestions.md: 5 列对齐 + self-referential 行
- proposal-approved.md: 5 列对齐 + B 治理事故追溯
- adr-076: 5 处 §R3 引用修复 + 状态符号 📋→🔄
- tools/docs-audit.sh: 4 项新检查

## Impact
- 影响: roadmap / docs/roadmap/ / proposal-* / adr-076 / tools/
- 不影响: rdd-workflow 流程本身（add-improve / guide-plan / guide-ship 零修改）
- 不影响: 98 catch2 binaries (纯文档 + 1 shell 脚本扩展)
- 不影响: adr-076 评审（保持 PROPOSED 状态；roadmap 显式挂载"评审中"段）

## Acceptance
- 7 commit 全部 ship + 98 catch2 binaries build + runnable
- 4 项 ci-docs-audit 新检查 PASS
- B 治理事故追溯结论明确（4 情况判定）
- adr-076 C/D 修复完成
EOF
```

- [ ] **Step 3: Create design.md (reference the spec)**

```bash
cat > openspec/changes/2026-08-10-roadmap-unified-index/design.md <<'EOF'
# Design: Roadmap Unified Index

完整设计见 spec: `docs/superpowers/specs/2026-08-10-roadmap-driven-workflow-design.md` (490 行)

## Key Decisions (摘要)
- D1: roadmap.md 升级为"叙事 + Unified Index 双层"，新增 4 段
- D2: stage-X doc 统一为 4 象限模板
- D3: stage-5 doc 加 "Cross-Repo 集成评审中" 段，scope 不扩张
- D4: proposal-suggestions.md 重新激活
- D5: proposal-approved.md 5 列对齐 + B 治理事故 4 情况追溯
- D6: adr-076 C/D 修复（5 §R3 引用 + 1 状态符号）
- D7: Self-Referential 完整 rdd-workflow 6 步 + 7 commit

## Oracle Consultation
Oracle 1m46s 推荐 c3 (Hybrid):
- adr-076 保持 canonical 不动
- roadmap 显式挂载"评审中" placeholder
- improvement + suggestions row 只在 adr-076 Accepted 后才创建

## 10-Phase Migration
详见 `tasks.md`（1:1 对应本 plan 的 Task 5-11）。
EOF
```

- [ ] **Step 4: Create tasks.md (mirror this plan's tasks 5-11)**

```bash
cat > openspec/changes/2026-08-10-roadmap-unified-index/tasks.md <<'EOF'
# Tasks: Roadmap Unified Index

## Phase 0 (Task 1-4 of plan): rdd-workflow artifacts
- [x] Task 1: Extend ci-docs-audit (failing test)
- [x] Task 2: Create self-referential improvement
- [x] Task 3: Reactivate proposal-suggestions
- [x] Task 4: Create OpenSpec change artifacts

## Phase 1-9 (Task 5-12 of plan): Implementation
- [ ] Task 5: roadmap.md upgrade (4 段新增 + 152 REPLACE)
- [ ] Task 6: stage-4 4-quadrant pilot
- [ ] Task 7: stage-5 4-quadrant + cross-repo section
- [ ] Task 8: stage-0/1/2/3 4-quadrant
- [ ] Task 9: proposal-approved fix + B trace
- [ ] Task 10: adr-076 C+D fixes
- [ ] Task 11: Final verification (4 audits + 98 binaries)
- [ ] Task 12: Archive change
EOF
```

- [ ] **Step 5: Create spec.md (capability spec)**

```bash
cat > openspec/changes/2026-08-10-roadmap-unified-index/specs/roadmap-unified-index/spec.md <<'EOF'
# Roadmap Unified Index Specification

## Purpose
TBD - created by archiving change 2026-08-10-roadmap-unified-index. Update Purpose after archive.

## Requirements

### Requirement: roadmap.md Unified Index Structure

`roadmap.md` SHALL provide a Unified Index view of all architectural work, comprising 4 mandatory segments in addition to existing narrative sections:

#### Scenario: 派生建议 segment present

- **WHEN** a reader inspects `roadmap.md`
- **THEN** a 派生建议 segment SHALL be present listing candidate improvements based on ADR trigger conditions (e.g., "ADR-049 Phase 6+ trigger not met → multi-engine Puller improvements 待 trigger")

#### Scenario: 跨仓评审中 ADRs segment present

- **WHEN** a cross-repo consumer-side ADR is in PROPOSED-state (e.g., adr-076)
- **THEN** a 跨仓评审中 ADRs segment SHALL list it with status "待 owner + consumer 双评审" and a link to the canonical ADR file

#### Scenario: 在途 OpenSpec changes segment present

- **WHEN** `openspec/changes/` contains non-archived changes
- **THEN** a 在途 OpenSpec changes segment SHALL list them (pointing to `openspec/changes/INDEX.md`)

#### Scenario: 已归档 OpenSpec changes segment present

- **WHEN** `openspec/changes/archive/` exists
- **THEN** a 已归档 OpenSpec changes segment SHALL list the 5 most recent archived changes (pointing to `openspec/changes/archive/`)

### Requirement: stage-X 4-Quadrant Template

Each file `docs/roadmap/stage-{0,1,2,3,4,5}-*.md` SHALL follow a 4-quadrant template:

#### Scenario: All 4 quadrants present

- **WHEN** a reader inspects any `stage-X-*.md` file
- **THEN** the file SHALL contain 4 mandatory quadrants: 对应 ADRs / 活跃 improvements / 在途 changes / 已归档 changes
- **AND** a 触发条件 segment referencing the relevant ADR §<trigger section>

### Requirement: Inconsistency Fixes

#### Scenario: roadmap.md:152 REPLACE applied

- **WHEN** `roadmap.md` is read
- **THEN** line ~152 SHALL NOT contain the text "含 5 个新增 entry"
- **AND** SHALL contain a 3-link paragraph pointing to proposal-suggestions.md / proposal-approved.md / openspec/changes/INDEX.md

#### Scenario: adr-076 §R3 references corrected

- **WHEN** `adr-076` is read
- **THEN** it SHALL NOT contain "ADR-035 §R3 cross-repo" in any of the 5 original lines (5, 77, 359, 370, 475)
- **AND** the corrected lines SHALL reference "ADR-035 §R5.1" or "§R6.3" instead

#### Scenario: adr-076 status symbol corrected

- **WHEN** `adr-076` frontmatter is read
- **THEN** the status SHALL use "🔄 Proposed" (not "📋 PROPOSED")

#### Scenario: proposal-approved.md 5-column alignment

- **WHEN** `proposal-approved.md` is read
- **THEN** the table header SHALL have 5 columns (提案 / 优先级 / 来源 / 批准日期 / 批准人)
- **AND** all body rows SHALL also have 5 columns
EOF
```

- [ ] **Step 6: Verify all 4 files exist**

```bash
ls -la openspec/changes/2026-08-10-roadmap-unified-index/
ls -la openspec/changes/2026-08-10-roadmap-unified-index/specs/roadmap-unified-index/
```

Expected: 4 files at root + 1 spec.md = 5 files total. No errors.

- [ ] **Step 7: Validate with openspec CLI**

```bash
openspec validate 2026-08-10-roadmap-unified-index --type change
```

Expected: PASS (or list of warnings, not errors).

- [ ] **Step 8: Commit**

```bash
git add openspec/changes/2026-08-10-roadmap-unified-index/
git commit -m "feat(openspec): add 2026-08-10-roadmap-unified-index change artifacts"
```

---

## Task 5: Upgrade `roadmap.md` (4 New Segments + 152 REPLACE)

**Files:**
- Modify: `roadmap.md` (line ~152 REPLACE + 4 new segments)

- [ ] **Step 1: Read current state of `roadmap.md` to identify exact insertion points**

```bash
grep -n "^##" roadmap.md
```

Expected: List of all `##` section headers. Identify:
- Where the 152-line "已批准改进提案" segment currently sits (likely just before "Stage 5 触发条件")
- Where new segments should be inserted

- [ ] **Step 2: REPLACE line ~152 (Inconsistency A)**

Find the existing "已批准改进提案" segment (likely a single line referencing "proposal-suggestions.md（含 5 个新增 entry: 2026-08-04 提交）") and REPLACE it with a 3-link paragraph:

```markdown
## 改进提案入口

- 待审批提案：见 [proposal-suggestions.md](proposal-suggestions.md)
- 已批准提案：见 [proposal-approved.md](proposal-approved.md)
- 在途 OpenSpec changes：见 [openspec/changes/INDEX.md](openspec/changes/INDEX.md)
- 已归档 OpenSpec changes 最近 5 个：见 [openspec/changes/archive/](openspec/changes/archive/)（按目录名倒序）

> 维护说明：roadmap.md 不手工维护 "N 个新增 entry" 类计数，引用实时文件即可（计数易与实际状态漂移）。
```

(Adjust the exact line numbers based on Step 1's output.)

- [ ] **Step 3: Add 4 new segments after "阶段关系图" (or appropriate position)**

Insert in this order (relative to existing sections):

```markdown
## 派生建议

> 基于 ADR trigger conditions 自动/手动列出可派发的 candidate improvements。

| ADR | Trigger 条件 | 候选 improvement | 状态 |
|-----|--------------|------------------|------|
| ADR-049 §"Phase 6+ 触发条件" | multi-engine Puller 真实并行 | add-multi-engine-puller-real-parallel | ⏸️ trigger 未满足 |
| ADR-052 §"Phase 6.5 触发条件" | PM4 microcode 解析完整实现 | implement-pm4-microcode-full | ⏸️ trigger 未满足 |

> **维护说明**：派生建议是 roadmap 的"导航入口"——读者读 roadmap 时可一眼看到"哪些 ADR 触发后可派发哪些 improvement"。具体派发由 add-improve skill 引导（不强制走 roadmap）。

## 跨仓评审中 ADRs

| ADR | 标题 | 状态 | 评审方 | 链接 |
|-----|------|------|--------|------|
| adr-076 | GPGPU Kernel Module IOCTL（HAL Extension for PTX-EMU Image Executor 集成）| 🔄 Proposed | UsrLinuxEmu owner + TaskRunner owner | [docs/00_adr/adr-076-gpgpu-kernel-module-ioctl.md](docs/00_adr/adr-076-gpgpu-kernel-module-ioctl.md) |

> **维护说明**：跨仓评审中 ADRs 是"待 owner + consumer 双评审"的 cross-repo consumer-side ADR 的临时占位段。ADR 升 Accepted 后：从本段移除 + 加入对应 stage-X doc 4 象限的"对应 ADRs"表 + 创建 improvement + 加入 proposal-suggestions.md。
```

(Add the 4th segment "在途 OpenSpec changes" + "已归档 OpenSpec changes" by reading the actual INDEX.md and archive/ in Step 4 first.)

- [ ] **Step 4: Add 在途 + 已归档 segments (after reading actual files)**

```bash
# Get current active changes from INDEX.md
cat openspec/changes/INDEX.md
echo '---'
# Get 5 most recent archive directories
ls -t openspec/changes/archive/ | head -5
```

Then in `roadmap.md`, add:

```markdown
## 在途 OpenSpec changes

见 [openspec/changes/INDEX.md](openspec/changes/INDEX.md) 获取实时活跃项。

> 当前活跃项：(从 INDEX.md 读取并填入)

## 已归档 OpenSpec changes（最近 5 个）

| Change | 归档日期 | 主题 |
|--------|----------|------|
| (从 `ls -t openspec/changes/archive/ | head -5` 读取) | (git log 取日期) | (从目录名) |

> 完整归档列表：见 [openspec/changes/archive/](openspec/changes/archive/)。
```

(Fill the actual values from Step 4's output before committing.)

- [ ] **Step 5: Verify no 152-line stale text remains**

```bash
grep -n "5 个新增 entry" roadmap.md
```

Expected: 0 matches (no output). If matches, fix.

- [ ] **Step 6: Run ci-docs-audit (Task 1's check_roadmap_152_fake_ref should now PASS)**

```bash
bash tools/docs-audit.sh 2>&1 | grep -A1 "check_roadmap_152"
```

Expected: PASS (no "FAIL: roadmap.md:152 contains stale..." line).

- [ ] **Step 7: Verify file structure**

```bash
grep -nE "^##" roadmap.md
```

Expected: 4 new sections present (派生建议 / 跨仓评审中 ADRs / 在途 OpenSpec changes / 已归档 OpenSpec changes) + 1 replaced section (改进提案入口).

- [ ] **Step 8: Commit**

```bash
git add roadmap.md
git commit -m "feat(roadmap): upgrade to Unified Index (4 new segments + 152 REPLACE)"
```

---

## Task 6: Apply 4-Quadrant Template to `stage-4-bar-ioremap.md` (Pilot)

**Files:**
- Modify: `docs/roadmap/stage-4-bar-ioremap.md`

- [ ] **Step 1: Read current state**

```bash
grep -nE "^##" docs/roadmap/stage-4-bar-ioremap.md
```

Expected: Existing section headers. Identify where to insert 4 quadrants.

- [ ] **Step 2: Add 4-quadrant block before "触发条件" (or end of doc)**

Insert:

```markdown
## 4 象限

### 象限 1: 对应 ADRs

| ADR | 标题 | 状态 | 触发阶段 |
|-----|------|------|----------|
| ADR-069 | 真实 PCIe BAR + ioremap 仿真架构 | ✅ Accepted | Stage 4.1 |
| ADR-072 | 驱动代码可移植性验证框架 | ✅ Accepted | Stage 4.1 |
| ADR-073 | DMA 一致性内存仿真架构 | ✅ Accepted | Stage 4.1 |
| ADR-075 | Stage 4.7 B-class L2 Foundation Removal 回顾记录 | ✅ Accepted | Stage 4.7 |

### 象限 2: 活跃 improvements (待派发)

| Improvement | 来源 | 优先级 | 状态 |
|-------------|------|--------|------|
| (如无可派发: "无 — Stage 4 已全部 ship + 归档") | — | — | — |

> 数据源：[proposal-suggestions.md](../../proposal-suggestions.md)

### 象限 3: 在途 changes

| Change | 状态 | 起点 | 预计完成 |
|--------|------|------|----------|
| (从 openspec/changes/INDEX.md 读取) | (从 INDEX 读取) | (从 INDEX 读取) | (从 INDEX 读取) |

> 数据源：[openspec/changes/INDEX.md](../../openspec/changes/INDEX.md)

### 象限 4: 已归档 changes

| Change | 归档日期 | 主题 |
|--------|----------|------|
| (从 `ls -t openspec/changes/archive/ | grep stage4 | head -5` 读取) | (git log) | (目录名) |

> 数据源：[openspec/changes/archive/](../../openspec/changes/archive/) (filter: stage4*)
```

(Fill actual values from the read commands before committing.)

- [ ] **Step 3: Verify 4 quadrants present**

```bash
grep -nE "^### 象限 [1-4]:" docs/roadmap/stage-4-bar-ioremap.md
```

Expected: 4 matches (象限 1, 2, 3, 4).

- [ ] **Step 4: Commit**

```bash
git add docs/roadmap/stage-4-bar-ioremap.md
git commit -m "feat(roadmap): apply 4-quadrant template to stage-4 (pilot)"
```

---

## Task 7: Apply 4-Quadrant Template to `stage-5-multi-engine-pm4.md` + Cross-Repo Section

**Files:**
- Modify: `docs/roadmap/stage-5-multi-engine-pm4.md`

- [ ] **Step 1: Read current state**

```bash
cat docs/roadmap/stage-5-multi-engine-pm4.md
```

Expected: Stage 5 placeholder doc with "本文件不授权实现" disclaimer + "Stage 5 触发条件" section.

- [ ] **Step 2: Add "Cross-Repo 集成评审中" section (BEFORE 4-quadrant block)**

```markdown
## Cross-Repo 集成评审中

| ADR | 标题 | 状态 | 备注 |
|-----|------|------|------|
| adr-076 | GPGPU Kernel Module IOCTL | 🔄 Proposed | **不属本 stage scope**（属 cross-repo 集成；命名仅因 adr-076 建议 change 名 `stage5-ptxemu-...` 临时撞 scope） |

> **scope 说明**：Stage 5 严格限定为 ADR-049 Phase 6+ / ADR-052 Phase 6.5 触发的 multi-engine Puller + PM4 microcode 解析工作。cross-repo 集成（如 adr-076 PTX-EMU HAL backend）**不属本 stage**，挂在 roadmap "跨仓评审中 ADRs" 段单独跟踪。
```

- [ ] **Step 3: Add 4-quadrant block (same template as Task 6 Step 2)**

Insert 4 quadrants with stage-5-specific data. For Quadrant 1 (ADRs):
- ADR-049 (multi-engine scheduler) — 🔄 Proposed (Phase 6+ trigger)
- ADR-052 (AQL/PM4 native support) — ✅ Accepted (PM4 parsing deferred to Phase 6.5)

For Quadrants 2-4: list relevant improvements/changes (most likely none for Stage 5 since it's not started).

- [ ] **Step 4: Verify 4 quadrants + cross-repo section present**

```bash
grep -nE "^## Cross-Repo|^### 象限 [1-4]:" docs/roadmap/stage-5-multi-engine-pm4.md
```

Expected: 5 matches (1 cross-repo + 4 quadrants).

- [ ] **Step 5: Commit**

```bash
git add docs/roadmap/stage-5-multi-engine-pm4.md
git commit -m "feat(roadmap): apply 4-quadrant + cross-repo section to stage-5"
```

---

## Task 8: Apply 4-Quadrant Template to `stage-0/1/2/3` Docs

**Files:**
- Modify: `docs/roadmap/stage-0-mvp.md`
- Modify: `docs/roadmap/stage-1-kernel-emu.md`
- Modify: `docs/roadmap/stage-2-multi-device.md`
- Modify: `docs/roadmap/stage-3-v1.0.md`

- [ ] **Step 1: For each of the 4 stage docs, repeat Task 6 Step 1-4**

For each file:
1. Read current `##` structure
2. Add 4-quadrant block (with stage-specific ADR/change/archive data)
3. Verify 4 quadrants present
4. Stage-specific data: read each ADR-XXX README.md + improvements/* + archive/ to populate

- [ ] **Step 2: Stage 0 (MVP) data**

Quadrant 1 ADRs: ADR-001~010 (基本架构 ADR).
Quadrant 4 archives: `ls -t openspec/changes/archive/ | head -5` (most recent regardless of stage).

- [ ] **Step 3: Stage 1 (Kernel Emu) data**

Quadrant 1 ADRs: ADR-011~014 (Proposed) + ADR-015~021 (Accepted) + C-12 ADRs (059~063).
Quadrant 4 archives: filter `*kfd*` or `*kernel*` in archive.

- [ ] **Step 4: Stage 2 (Multi-Device) data**

Quadrant 1 ADRs: ADR-037, ADR-038 (network stack).
Quadrant 4 archives: `2026-07-05-multi-device-*` related changes.

- [ ] **Step 5: Stage 3 (v1.0) data**

Quadrant 1 ADRs: (no specific ADRs; this is stability phase, references all prior).
Quadrant 4 archives: `2026-07-23-*` related changes.

- [ ] **Step 6: Commit all 4 docs in 1 commit (per spec Appendix C)**

```bash
git add docs/roadmap/stage-0-mvp.md docs/roadmap/stage-1-kernel-emu.md docs/roadmap/stage-2-multi-device.md docs/roadmap/stage-3-v1.0.md
git commit -m "feat(roadmap): apply 4-quadrant template to stage-0/1/2/3"
```

---

## Task 9: Fix `proposal-approved.md` (5-Col Header + B-Incident Trace)

**Files:**
- Modify: `proposal-approved.md`

- [ ] **Step 1: Read current state**

```bash
cat proposal-approved.md
```

- [ ] **Step 2: Trace B incident: find actual merge commits for the 2 mis-marked entries**

```bash
# Find merge commit for implement-pm4-microcode-parsing
git log --all --oneline -- openspec/changes/archive/2026-08-08-implement-pm4-microcode-parsing/ 2>/dev/null | head -3
# Find merge commit for add-multi-engine-puller-instances
git log --all --oneline -- openspec/changes/archive/2026-08-08-add-multi-engine-puller-instances/ 2>/dev/null | head -3
# Read each change's proposal.md to determine actual stage
cat openspec/changes/archive/2026-08-08-implement-pm4-microcode-parsing/proposal.md 2>/dev/null | head -30
echo '---'
cat openspec/changes/archive/2026-08-08-add-multi-engine-puller-instances/proposal.md 2>/dev/null | head -30
```

- [ ] **Step 3: Determine B-incident judgment (per spec D5 4-情况判定)**

Based on the merge commit + proposal.md content, classify each entry as:
- 情况 1: stage-4 follow-up 误命名
- 情况 2: 真属 stage-5 trigger 后
- 情况 3: 与 stage-5 无关（如纯文档/测试改进误归类）
- 情况 4: 都不是 → follow-up

- [ ] **Step 4: Normalize 5-col table header (if needed)**

Current header may have 4 cols. Ensure 5 cols: 提案 / 优先级 / 来源 / 批准日期 / 批准人.

```markdown
| 提案 | 优先级 | 来源 | 批准日期 | 批准人 |
|------|--------|------|----------|--------|
```

- [ ] **Step 5: Add 5th column "批准人" to all existing rows (if missing)**

If existing rows have 4 cols, add a default value for 批准人 (e.g., "guide-arch" or "owner").

- [ ] **Step 6: Update B-incident rows with trace conclusion**

For the 2 mis-marked entries, change the 状态 column to reflect the trace judgment from Step 3. Example:

```markdown
| [implement-pm4-microcode-parsing](improvements/implement-pm4-microcode-parsing.md) | P1 | 2026-08-08 | guide-arch | (情况 X: ...) |
```

- [ ] **Step 7: Add trace conclusion note at end of file**

```markdown
## B 治理事故追溯结论（2026-08-10）

| Entry | 实际 merge commit | 追溯判定 |
|-------|-------------------|----------|
| implement-pm4-microcode-parsing | (commit SHA from Step 2) | 情况 X: ... |
| add-multi-engine-puller-instances | (commit SHA from Step 2) | 情况 Y: ... |

> 追溯方法：见 `openspec/changes/2026-08-10-roadmap-unified-index/tasks.md` Task 9。
```

- [ ] **Step 8: Run ci-docs-audit (check_proposal_approved_5col_alignment should now PASS)**

```bash
bash tools/docs-audit.sh 2>&1 | grep -A1 "check_proposal_approved_5col"
```

Expected: PASS.

- [ ] **Step 9: Commit**

```bash
git add proposal-approved.md
git commit -m "fix(proposal): correct B incident + normalize approved table to 5-col"
```

---

## Task 10: Fix `adr-076` (5 §R3 References + 1 Status Symbol)

**Files:**
- Modify: `docs/00_adr/adr-076-gpgpu-kernel-module-ioctl.md`

- [ ] **Step 1: Read current state of 5 §R3 lines**

```bash
grep -n "ADR-035 §R3 cross-repo" docs/00_adr/adr-076-gpgpu-kernel-module-ioctl.md
```

Expected: 5 matches at lines 5, 77, 359, 370, 475 (per spec B.3 table).

- [ ] **Step 2: Replace line 5 reference**

Use `edit` to replace:
- Old: text containing "ADR-035 §R3 治理：跨仓 ABI 变更走 canonical ADR + consumer-side TADR"
- New: "ADR-035 §R5.1 治理：跨仓 ABI 变更走 canonical ADR + consumer-side TADR（4-step commit order）"

- [ ] **Step 3: Replace line 77 reference**

- Old: "ADR-035 §R3 cross-repo 协议要求"
- New: "ADR-035 §R5.1 cross-repo 4-step commit order 要求"

- [ ] **Step 4: Replace line 359 reference**

- Old: "跨仓 commit 顺序错位：§Migration 提供严格 4 步 commit 顺序（per ADR-035 §R3）"
- New: "跨仓 commit 顺序错位：§Migration 提供严格 4 步 commit 顺序（per ADR-035 §R5.1）"

- [ ] **Step 5: Replace line 370 reference**

- Old: "ADR-035 §R3 治理：跨仓 ABI 变更走 canonical ADR"
- New: "ADR-035 §R5.1 治理：跨仓 ABI 变更走 canonical ADR（4-step commit order）"

- [ ] **Step 6: Replace line 475 reference**

- Old: "ADR-035 §R3 治理：跨仓 ABI 变更走 canonical ADR（本 ADR）+ consumer-side TADR"
- New: "ADR-035 §R5.1 治理：跨仓 ABI 变更走 canonical ADR（本 ADR）+ consumer-side TADR（4-step commit order）"

- [ ] **Step 7: Fix status symbol (Inconsistency D)**

```bash
sed -i 's/📋 PROPOSED/🔄 Proposed/g' docs/00_adr/adr-076-gpgpu-kernel-module-ioctl.md
```

- [ ] **Step 8: Verify all 4 checks PASS**

```bash
# Check C: §R3 references gone
grep -nE "ADR-035 §R3 cross-repo" docs/00_adr/adr-076-gpgpu-kernel-module-ioctl.md
echo "Expected: 0 matches above"
echo '---'
# Check D: status symbol updated
grep -nE "📋 PROPOSED" docs/00_adr/adr-076-gpgpu-kernel-module-ioctl.md
echo "Expected: 0 matches above"
echo '---'
# Check new status symbol present
grep -nE "🔄 Proposed" docs/00_adr/adr-076-gpgpu-kernel-module-ioctl.md | head -3
echo "Expected: 1+ matches above"
```

- [ ] **Step 9: Run ci-docs-audit (check_adr076_* should now PASS)**

```bash
bash tools/docs-audit.sh 2>&1 | grep -A1 "check_adr076"
```

Expected: 2 PASS lines (one for r3_cross_repo, one for status_symbol).

- [ ] **Step 10: Commit**

```bash
git add docs/00_adr/adr-076-gpgpu-kernel-module-ioctl.md
git commit -m "fix(adr-076): correct 5 §R3 references + status symbol (📋→🔄)"
```

---

## Task 11: Final Verification (4 Audit Checks + 98 Catch2 Binaries)

**Files:**
- No new files; verification task.

- [ ] **Step 1: Run full ci-docs-audit**

```bash
bash tools/docs-audit.sh 2>&1
```

Expected: All 4 new checks PASS (no FAIL lines).

- [ ] **Step 2: Run all catch2 binaries (from project root, not build/)**

```bash
cd /workspace/project/wt-roadmap-unified-index  # (or current worktree)
ls build/bin/test_*_standalone 2>/dev/null | wc -l
```

Expected: 98 binaries (per AGENTS.md).

- [ ] **Step 3: Build + run a sample of binaries to verify project not broken**

```bash
# Quick sanity check: build and run 3 representative binaries
cd build && cmake --build . --target test_gpu_ioctl_standalone test_hardware_puller_emu_standalone test_module_load_and_vfs_standalone 2>&1 | tail -10
cd ..
./build/bin/test_gpu_ioctl_standalone 2>&1 | tail -3
./build/bin/test_hardware_puller_emu_standalone 2>&1 | tail -3
./build/bin/test_module_load_and_vfs_standalone 2>&1 | tail -3
```

Expected: All 3 binaries build + run + output PASS-like results.

- [ ] **Step 4: Verify all 7 commits are in the worktree**

```bash
git log --oneline main..HEAD
```

Expected: 7 commits (each per Appendix C of spec):
- feat(roadmap): upgrade to Unified Index
- feat(roadmap): apply 4-quadrant template to stage-4
- feat(roadmap): apply 4-quadrant template to stage-5 + cross-repo section
- feat(roadmap): apply 4-quadrant template to stage-0/1/2/3
- feat(proposal): reactivate suggestions + add self-referential row
- fix(proposal): correct B incident + normalize approved table
- fix(adr-076): correct §R3 references + status symbol

(NOTE: 4 stage docs may be 1 or 4 separate commits per spec; spec Appendix C lists 7 file-groups as 1 commit each, so total = 7 file-group commits. Verify actual commit count matches expected.)

- [ ] **Step 5: Document verification result in tasks.md**

```bash
# Update tasks.md to mark all phases complete
sed -i 's/- \[ \]/- [x]/g' openspec/changes/2026-08-10-roadmap-unified-index/tasks.md
```

- [ ] **Step 6: Commit tasks.md update**

```bash
git add openspec/changes/2026-08-10-roadmap-unified-index/tasks.md
git commit -m "docs(tasks): mark all phases complete after verification"
```

---

## Task 12: Archive the Change

**Files:**
- Move: `openspec/changes/2026-08-10-roadmap-unified-index/` → `openspec/changes/archive/`

- [ ] **Step 1: Run rdd-workflow archive skill**

```bash
# If rdd-workflow provides an archive command:
openspec archive 2026-08-10-roadmap-unified-index 2>&1 | tail -10
```

OR if openspec doesn't have archive command, do it manually:

```bash
cd /workspace/project/wt-roadmap-unified-index
git mv openspec/changes/2026-08-10-roadmap-unified-index openspec/changes/archive/2026-08-10-roadmap-unified-index
```

- [ ] **Step 2: Update INDEX.md (if maintained manually)**

```bash
# Read INDEX.md
cat openspec/changes/INDEX.md | head -30
# Remove this change from active list (if listed) and ensure archive is mentioned
```

- [ ] **Step 3: Verify archive structure**

```bash
ls openspec/changes/archive/2026-08-10-roadmap-unified-index/
```

Expected: 4 files (proposal.md, design.md, tasks.md, specs/.../spec.md).

- [ ] **Step 4: Commit archive move**

```bash
git add openspec/changes/
git commit -m "archive(2026-08-10-roadmap-unified-index): move to archive/ after completion"
```

- [ ] **Step 5: Final ci-docs-audit + 98 binaries verification (post-archive)**

```bash
bash tools/docs-audit.sh 2>&1 | grep -cE "^FAIL"
echo "Expected: 0"
echo '---'
cd build && ctest -L unit --output-on-failure 2>&1 | tail -5
cd ..
```

Expected: 0 FAIL lines + ctest PASS.

- [ ] **Step 6: Verify final commit count**

```bash
git log --oneline main..HEAD
```

Expected: 8+ commits (7 implementation + 1 verification + 1 archive).

- [ ] **Step 7: Merge worktree to main (or push branch + open PR)**

```bash
cd /workspace/project/UsrLinuxEmu
git checkout main
git merge feat/roadmap-unified-index --no-ff -m "Merge feat/roadmap-unified-index"
git worktree remove ../wt-roadmap-unified-index
```

OR (if using rdd-workflow ship protocol):

```bash
git push origin feat/roadmap-unified-index
# Open PR via gh CLI
gh pr create --title "feat(roadmap): upgrade to Unified Index" --body "..."
```

(Choose merge vs PR based on project's git policy per AGENTS.md / openSpec conventions.)

---

## Self-Review

### 1. Spec coverage

| Spec Section | Implementing Task |
|--------------|-------------------|
| D1 (roadmap.md 4 段 + 152 REPLACE) | Task 5 |
| D2 (stage-X 4-quadrant) | Task 6 (pilot) + Task 7 (stage-5) + Task 8 (stage-0/1/2/3) |
| D3 (stage-5 cross-repo section) | Task 7 Step 2 |
| D4 (proposal-suggestions reactivate) | Task 3 |
| D5 (proposal-approved fix + B trace) | Task 9 |
| D6 (adr-076 C+D) | Task 10 |
| D7 (self-referential rdd-workflow) | Task 2 (improvement) + Task 3 (suggestions) + Task 4 (change artifacts) + Task 12 (archive) |
| §5 派生建议 (Oracle 派生建议) | Task 5 Step 3 (派生建议 segment) |
| §5 跨仓评审中 ADRs (Oracle c3) | Task 5 Step 3 (跨仓评审中 ADRs segment) + Task 7 Step 2 (stage-5 cross-repo) |
| ci-docs-audit 4 new checks | Task 1 (write failing test) + verified by Tasks 5/9/10 |
| TDD 5-步 (per rdd-workflow) | Task 1 only (other tasks are doc-only; test = ci-docs-audit grep) |
| 98 catch2 binaries | Task 11 Step 2-3 |
| B 治理事故 4 情况判定 | Task 9 Step 3-7 |
| adr-076 评审不阻塞 | (no implementation; adr-076 stays PROPOSED; roadmap "评审中" segment) |

**Coverage: 14/14 spec sections mapped to tasks. No gaps.**

### 2. Placeholder scan

Searched for: `TBD` / `TODO` / `FIXME` / `XXX` / `HACK` / "implement later" / "fill in details" / "appropriate error handling" / "similar to Task N".

Findings:
- Task 4 Step 2 contains "TBD" in `spec.md` Purpose section — **kept intentionally** (rdd-workflow convention: "TBD - created by archiving change ... Update Purpose after archive." This is a rdd-workflow template placeholder, not a planning placeholder. Acceptable.)
- All other tasks have concrete file paths, exact grep patterns, specific commit messages.

**No placeholders to fix.**

### 3. Type consistency

- Function names: `check_roadmap_152_fake_ref`, `check_adr076_r3_cross_repo_ref`, `check_adr076_status_symbol`, `check_proposal_approved_5col_alignment` — used consistently in Task 1 (definition), Task 5 Step 6 (invoke), Task 9 Step 8 (invoke), Task 10 Step 9 (invoke), Task 11 Step 1 (full run).
- File paths: `roadmap.md`, `docs/roadmap/stage-{0,1,2,3,4,5}-*.md`, `proposal-suggestions.md`, `proposal-approved.md`, `adr-076-gpgpu-kernel-module-ioctl.md`, `tools/docs-audit.sh`, `openspec/changes/2026-08-10-roadmap-unified-index/...` — consistent across all tasks.
- Commit message format: `<type>(<scope>): <subject>` per AGENTS.md Conventional Commits.

**Type consistency verified.**

---

## Execution Handoff

**Plan complete and saved to `docs/superpowers/plans/2026-08-10-roadmap-unified-index.md` (12 tasks). Two execution options:**

**1. Subagent-Driven (recommended)** — I dispatch a fresh subagent per task, review between tasks, fast iteration. Best for this 12-task plan because:
- Tasks 1, 5, 9, 10 require read-validate-edit cycles (high parallelism potential)
- Task 4 has 4 sub-files (parallel subagent dispatch possible)
- Fresh subagent per task avoids context bloat from large file reads

**2. Inline Execution** — Execute tasks in this session using executing-plans, batch execution with checkpoints. Best if:
- User wants to inspect every commit
- Project git policy prefers single-session execution
- Limited subagent budget

**Which approach?**

(Spec also has a follow-up ADR-renaming item — adr-076:28 pre-named change `stage5-ptxemu-...` collides with stage-5 doc scope; rename is out-of-scope for this plan and should be a separate improvement per spec D6 follow-up note.)
