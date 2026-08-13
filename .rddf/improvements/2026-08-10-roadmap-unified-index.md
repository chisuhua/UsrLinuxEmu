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
