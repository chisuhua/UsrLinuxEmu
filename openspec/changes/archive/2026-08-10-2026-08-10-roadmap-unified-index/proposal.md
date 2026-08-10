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
