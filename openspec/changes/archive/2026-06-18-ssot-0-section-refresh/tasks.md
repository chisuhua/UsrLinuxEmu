# Tasks: ssot-0-section-refresh

> **依赖**: proposal ✅
> **预估工时**: 5-10 分钟
> **约束**: 单 commit + docs-audit 36/36 PASS

## 1. 准备

- [ ] 1.1 读 `docs/02_architecture/post-refactor-architecture.md` line 42-68（§0 文档定位段）

## 2. SSOT §0 编辑（5 处 1-行替换）

- [ ] 2.1 **Line 52**: `AGENTS.md`（项目根）| 开发指南 + 架构要点 | 🟢 **相对准确**（唯一接近真相的文档，但定位为"开发指南"）|
  → `AGENTS.md`（项目根）| 开发指南 + 架构要点 | 🟢 **相对准确**（已通过反向引用 SSOT 闭环，commit `3faa3a7`）|

- [ ] 2.2 **Line 53**: `docs/02_architecture/architecture.md` | 旗舰架构文档 | 🔴 严重过期（2026-03-23，旧布局）|
  → `docs/02_architecture/architecture.md` | 旗舰架构文档 | 🟢 v3.0（2026-06-16 对齐 Phase 1.5 → 2；引用 SSOT）|

- [ ] 2.3 **Line 56**: **本文**（post-refactor-architecture.md）| **重构后架构 SSOT + docs 同步方案** | 🔄 待评审 |
  → **本文**（post-refactor-architecture.md）| **重构后架构 SSOT + docs 同步方案** | ✅ Approved（v0.1.7）|

- [ ] 2.4 **Line 64 关键事实段**: `AGENTS.md` 是事实上的"权威架构说明"，但**没有正式升级为架构文档**
  → `AGENTS.md` 已通过反向引用 `> **权威架构说明**：[post-refactor-architecture.md]` 与本文建立双向引用闭环（commit `3faa3a7`）

- [ ] 2.5 **Line 67 关键事实段**: 本文档目标是**接管 AGENTS.md 的架构部分**，成为正式的 SSOT
  → 本文档已升为 ✅ Approved（v0.1.7），实现 SSOT 接管；架构部分反向引用闭环由 AGENTS.md commit `3faa3a7` 完成

## 3. 验证

- [ ] 3.1 读 line 46-68 全文，确认 5 处编辑语义连贯
- [ ] 3.2 `grep -E "🔄 待评审|严重过期 \(2026-03-23|接管 AGENTS.md 的架构部分" docs/02_architecture/post-refactor-architecture.md` 应返回 0 匹配（所有过时描述都已删除）

## 4. 验证 gates

- [ ] 4.1 `bash tools/docs-audit.sh --strict` 仍 36/36 PASS
- [ ] 4.2 `make -j4 -C build` 100% pass（虽然纯文档）

## 5. 提交 + 归档

- [ ] 5.1 `git add docs/02_architecture/post-refactor-architecture.md`
- [ ] 5.2 单 commit：
  ```
  docs(ssot): refresh §0 self-description to match v0.1.7 + P1 changes

  Closes meta-stale description in §0 (5 places) discovered after
  v0.1.7 SSOT approval + 4 P1 follow-up changes closed. v0.1.6 audit
  (commit 211b48c) did not cover §0 (audit scope was §1.2/1.7/1.8
  + Appendix A); this change closes that gap.

  Updates:
  - L52 AGENTS.md status: "唯一接近真相" → "已通过反向引用闭环"
  - L53 architecture.md status: "🔴 严重过期" → "🟢 v3.0 对齐"
  - L56 self SSOT status: "🔄 待评审" → "✅ Approved (v0.1.7)"
  - L64 关键事实: AGENTS.md 已升级
  - L67 关键事实: 接管目标已达成

  5 single-line replacements. No other SSOT sections affected.
  ```
- [ ] 5.3 `openspec archive ssot-0-section-refresh --yes`

## 6. 回滚预案

- SSOT §0 编辑破坏：`git checkout docs/02_architecture/post-refactor-architecture.md`
- archive 失败：`rm -rf openspec/changes/archive/2026-06-18-ssot-0-section-refresh/`