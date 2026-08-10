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
