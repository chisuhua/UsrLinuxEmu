# Tasks: H-3 Maintenance Transition

> **依赖**: proposal ✅, design ✅
> **预估总工时**: ~2h（50% 自动化 / 50% 手动 review）
> **前置条件**: H-3.6 + H-3.7 + H-3.8 全部完成
> **后续约束**: 完成后进入维护期，可启动 Phase D 设计评估

## Phase A：归档 3 个 openspec changes（15min）✅ 已完成 (commit 36f708f)

### A.1 归档 H-3.6

- [x] A.1.1 `openspec archive 2026-06-26-h3-6-issue-3-coordination -y --skip-specs`
- [x] A.1.2 确认 archive 成功（文件移入 openspec/changes/archive/）
- [x] A.1.3 `git add openspec/changes/archive/` + `git rm -r openspec/changes/2026-06-26-h3-6-issue-3-coordination/`

### A.2 归档 H-3.7

- [x] A.2.1 `openspec archive 2026-06-26-h3-7-issue-2-coordination -y --skip-specs`
- [x] A.2.2 确认 archive 成功
- [x] A.2.3 `git add openspec/changes/archive/` + `git rm -r openspec/changes/2026-06-26-h3-7-issue-2-coordination/`

### A.3 归档 H-3.8

- [x] A.3.1 `openspec archive 2026-06-26-h3-8-issue-1-coordination -y --skip-specs`
- [x] A.3.2 确认 archive 成功
- [x] A.3.3 `git add openspec/changes/archive/` + `git rm -r openspec/changes/2026-06-26-h3-8-issue-1-coordination/`

### A.4 提交归档

- [x] A.4.1 `git commit -m "chore(openspec): archive H-3.6/3.7/3.8 coordination (H-7 resolved)"`
- [x] A.4.2 `git push origin main`

## Phase B：TaskRunner 端文档更新（~1h）✅ 已完成 (submodule tip b682fdd)

### B.1 sync-plan.md v2.2

- [x] B.1.1 在 `§一、协调工作流` 新增 H-3.5~H-3.8 架构状态段
- [x] B.1.2 更新 `§三、同步点` 移除 H-3.5/3.6/3.7/3.8 待办项

### B.2 cross-repo-h7-template.md

- [x] B.2.1 更新历史 PR 范例表：5 个 TBD → 实际 commit hashes
- [x] B.2.2 更新 `@维护` 段的更新时机描述

### B.3 提交

- [x] B.3.1 `git add . && git commit -m "docs(maintenance): sync-plan v2.2 + cross-repo template TBD fill"`
- [x] B.3.2 `git push origin main`

## Phase C：全量 build + test 验证（~30min）✅ 已完成 (2026-06-26)

### C.1 TaskRunner test-fixture 模式

- [x] C.1.1 `cmake -B build && cmake --build build -j4`
- [x] C.1.2 `ctest --test-dir build -V`
- [x] C.1.3 验证: test_cuda_scheduler 8/8 + test_gpu_architecture 11/11 + test_gpu_phase2 12/12

### C.2 TaskRunner umd-evolution 模式

- [x] C.2.1 `cmake -B build -DTASKRUNNER_BUILD_MODE=umd-evolution && cmake --build build -j4`
- [x] C.2.2 `ctest --test-dir build -V`
- [x] C.2.3 验证: test_umd_skeleton 3/3

### C.3 UsrLinuxEmu 端

- [x] C.3.1 `cmake -B build && cmake --build build -j4`
- [x] C.3.2 `ctest --test-dir build -V`
- [x] C.3.3 验证: test_gpu_plugin + test_gpu_fence_return + test_queue_puller_integration 全部 pass

### C.4 ABI 拓宽回归确认

- [x] C.4.1 确认 test_gpu_phase2 12 cases 中 queue create + submit 相关的 4 cases 全部 pass
- [x] C.4.2 确认无 new warning（编译器 + LSP）

## Phase D：UsrLinuxEmu 端同步（~15min）✅ 已完成 (commit 853eb8f)

- [x] D.1 确认 TaskRunner 端所有清扫 commit 已 push
- [x] D.2 `cd /workspace/project/UsrLinuxEmu && git add external/TaskRunner`
- [x] D.3 `git commit -m "chore(submodule): bump TaskRunner to b682fdd (H-3 maintenance transition)"`
- [x] D.4 `git push origin main`
- [x] D.5 可选: 补充 ADR-034 完成详情段（commit 15f9ac6 + bcd00cf）

## Verification Checklist（最终验收）

- [x] A.4.2 验证 3 个 openspec changes 均已归档到 archive/
- [x] B.3.2 验证 sync-plan.md 显示 H-3.5~H-3.8 完成
- [x] B.3.2 验证 cross-repo template 历史表无 TBD
- [x] C.1.3 验证 TaskRunner test-fixture: ~31 cases pass ✅ (31/31)
- [x] C.2.3 验证 TaskRunner umd-evolution: 3 cases pass ✅ (3/3)
- [x] C.3.3 验证 UsrLinuxEmu: 所有测试 pass ✅ (34/34)
- [x] D.4 验证 UsrLinuxEmu submodule bump 推送（commit 853eb8f）

## Status Tracking

- **Phase A**: ✅ 已完成 (commit 36f708f)
- **Phase B**: ✅ 已完成 (submodule tip b682fdd)
- **Phase C**: ✅ 已完成 (2026-06-26 验证)
- **Phase D**: ✅ 已完成 (commit 853eb8f)

## Dependencies

```
H-3.6 issue-3-coordination (✅ 修复 + 跨仓同步完成)
    ↓
H-3.7 issue-2-coordination (✅ 修复 + 跨仓同步完成)
    ↓
H-3.8 issue-1-coordination (✅ 修复 + 跨仓同步完成)
    ↓
H-3-maintenance-transition (📋 PROPOSED, 2026-06-26 ← 本 change)
    ↓
维护期 (可选: Phase D 设计评估 / PR 2 预热)
```