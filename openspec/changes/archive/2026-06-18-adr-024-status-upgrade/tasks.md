# Tasks: adr-024-status-upgrade

> **依赖**: proposal ✅
> **预估工时**: 10-15 分钟
> **约束**: 单 commit + docs-audit 36/36 PASS

## 1. 准备

- [ ] 1.1 读 `docs/00_adr/adr-024-user-mode-queue-submission.md` line 1-20（状态段 + 修订记录）
- [ ] 1.2 读 `docs/00_adr/README.md` 索引表 + 关系图（ADR-024 出现位置）

## 2. 修改 ADR-024 文件

- [ ] 2.1 Line 3 状态：`**状态**: 提议中 (Proposed)` → `**状态**: ✅ 已接受 (v1)`
- [ ] 2.2 Line 14 修订记录追加一行：
  ```
  - 2026-05-12 v1: 初始版本
  - 2026-06-17 v1 落地: 经 SSOT v0.1.5 复审，Phase 2 实施完整（commit `7dc5cb2`/`5e0258e`/`b78edc9`/`85b2e5b`/`5a25099`/`38de565`），状态升 ✅ 已接受
  ```

## 3. 修改 ADR 索引

- [ ] 3.1 `docs/00_adr/README.md` 索引表第 23 行：
  ```
  | [adr-024](adr-024-user-mode-queue-submission.md) | 用户态队列命令提交架构 | 🔄 提议中 | 2026-05 |
  ```
  → 
  ```
  | [adr-024](adr-024-user-mode-queue-submission.md) | 用户态队列命令提交架构 | ✅ 已接受 (v1) | 2026-06 |
  ```

- [ ] 3.2 关系图（line ~110 "Phase 3+ 规划" 段）：`adr-024 (用户态队列提交 — 已提议)` → `adr-024 (用户态队列提交) ✅ v1`

## 4. 验证

- [ ] 4.1 `grep -E "🔄 提议中" docs/00_adr/adr-024-user-mode-queue-submission.md` 应返回 0 匹配
- [ ] 4.2 `grep -E "adr-024.*🔄 提议中|adr-024.*已提议" docs/00_adr/README.md` 应返回 0 匹配
- [ ] 4.3 `bash tools/docs-audit.sh --strict` 仍 36/36 PASS
- [ ] 4.4 `make -j4 -C build` 100% pass

## 5. 提交 + 归档

- [ ] 5.1 `git add docs/00_adr/adr-024-user-mode-queue-submission.md docs/00_adr/README.md`
- [ ] 5.2 单 commit：
  ```
  docs(adr): upgrade ADR-024 to ✅ Accepted v1 (Phase 2 implemented)

  ADR-024 (用户态队列命令提交架构) was marked 🔄 提议中 but
  Phase 2 implementation is complete per SSOT §1.5 timeline:
  - ring buffer + GpuQueueEmu (7dc5cb2)
  - multi-queue fetch (5e0258e)
  - LAUNCH_CB deletion (b78edc9)
  - queue ioctl wiring (85b2e5b)
  - ADR-024 itself (5a25099)
  - GlobalScheduler callback chain (38de565)

  This change closes the only remaining "🔄 提议中 but already
  implemented" gap in the ADR governance (other 4 提议中 ADRs
  011/012/013/014 are genuinely pending implementation).
  ```
- [ ] 5.3 `openspec archive adr-024-status-upgrade --yes`

## 6. 回滚预案

- ADR 文件改坏：`git checkout docs/00_adr/adr-024-user-mode-queue-submission.md docs/00_adr/README.md`
- archive 失败：`rm -rf openspec/changes/archive/2026-06-18-adr-024-status-upgrade/`