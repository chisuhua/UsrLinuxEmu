# Tasks: stage3-ioctl-dispatch-completeness

> **状态**: 📋 PROPOSED
> **目标**: 应用 PR #26 内容，把 18 个新 IOCTL 加入 runtime 派发表

## 1. 准备（5 分钟）

- [ ] 1.1 验证 PR #26 当前 CI 状态（应只剩 docs-audit 阻塞）
- [ ] 1.2 等 C-01 合并后 docs-audit 转绿

## 2. Merge PR #26（5 分钟）

- [ ] 2.1 PR #26 CI 全绿后用 `gh pr merge 26 --admin --squash`
- [ ] 2.2 验证 main CI 全绿（4/4 jobs pass）
- [ ] 2.3 验证 `kNumIoctls = 31`
- [ ] 2.4 验证 ctest 111/111 PASS

## 3. 验证 / 完成

- [ ] 3.1 Update `docs/00_adr/adr-015-gpu-ioctl-unification.md`（如需，kNumIoctls 31）
- [ ] 3.2 Update `docs/02_architecture/post-refactor-architecture.md` §1.10（如有 ioctl count 引用）
- [ ] 3.3 Issue #11/#13 验证（已 CLOSED，但仍要确认 G1-G4 边界契约）
- [ ] 3.4 在 Issue #24 (Stage 3 tracking) 中 mark "ioctl dispatch" sub-task 完成

## 4. 跨仓通知

- [ ] 4.1 在 TaskRunner 仓 issue 通知 PR #26 已 merge
- [ ] 4.2 TaskRunner Phase 4 真实化（PR #8 已 merge）现在可达 UsrLinuxEmu 真实 sim 层
