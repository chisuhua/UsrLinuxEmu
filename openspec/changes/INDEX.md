# Active Changes Index

> **更新**: 2026-07-08
> **Owner**: UsrLinuxEmu Architecture Team
> **总数**: 7 个活跃 change (原 12 个，5 个已完成/已归档)
> **Source**: 2026-07-07 后续工作梳理

---

## ✅ 已完成 (7 归档)

| 原 ID | Change | 状态 | 证据 |
|-------|--------|------|------|
| C-01 | fix-docs-audit-runstage2-and-baseline | ✅ 已归档 | PR #28 `87680fb` |
| C-02 | stage3-ioctl-dispatch-completeness | ✅ 已归档 | PR #26 `edeee6e` |
| C-03 | sim-fence-id-base-cleanup | ✅ 已实现 | `13477ff` driver 路径 magic number 已清 |
| C-05 | stage3-1-ci-matrix-multi-platform | ✅ 已归档 | PR #29 `2cba6a0` |
| C-06/07 | stage3-3-errno-and-error-injection (merged) | ✅ 已归档 | `07e40ef` `f5dd6ac` `ef962e2` `d24061d` |
| C-04 | docs-tadr-mirror-sync | ✅ 已归档 | `f679763` `ecfc648` |

---

## 🔄 活跃 Changes

### 本季度 (P3)

### `2026-07-15-phase4-sim-graph-launch-real-impl` 🔵
**依赖**: C-02 ✅
**Effort**: 1 周
**Why**: F.6 follow-up。当前 `sim_graph_launch` PoC 立即 signal，要改成真实 `GpuQueueEmu::submit(uint64_t, uint32_t)` 异步
**Ref**: `external/TaskRunner/docs/superpowers/cross-repo-prs/...` B-1 decision

### `2026-07-15-phase4-cu-mempool-alloc-real-va` 🔵
**依赖**: C-02 ✅
**Effort**: 1 周
**Why**: TaskRunner PR #7 deferred #2 — synthetic VA → 真实 gpu_buddy first-fit VA

### `2026-07-22-stage3-2-perf-bench-baseline` 🔵
**Effort**: 1 周
**Why**: Issue #24 §3.2 — 建 `tests/perf/` 基准（ioctl/pushbuffer/mmap）。先 baseline 后优化

### `2026-08-01-stage3-2-hotpath-optimization` 🔵
**依赖**: C-10
**Effort**: 2 周
**Why**: Issue #24 §3.2 — VFS lookup hash index / ioctl perfect hash / BO cache

### `2026-08-15-stage1-4-kfd-multi-file-integration` ⚫ (sub-project)
**Effort**: 6-8 周
**Why**: README 后继 + Stage 1.4 Tier-2 deferred §3.2/§3.3。~50K LOC amdgpu port

---

## 依赖图

```
[C-08/sim-graph-launch] ──> (C-02 ✅)
[C-09/cu-mempool-real-va] ──> (C-02 ✅)

[C-10/perf-bench] ──> [C-11/hotpath-optimize]

[C-12/kfd-multi-file] ──> (Phase 4 mainline stable prerequisite)
```

---

## 推荐执行顺序

### 本月
1. **C-08** phase4-sim-graph-launch-real-impl（1 周）
2. **C-09** phase4-cu-mempool-alloc-real-va（1 周）

### 本季度
3. **C-10** stage3-2-perf-bench-baseline（1 周）
4. **C-11** stage3-2-hotpath-optimize（2 周，after C-10）
5. **C-12** kfd-multi-file（6-8 周 sub-project）

---

## Status Tracking

每完成一个 change：
```bash
# 1. apply change in branch
git checkout -b <change>-impl
# ... work ...

# 2. PR + merge to main
gh pr create ...
gh pr merge ...

# 3. after merge, archive
openspec archive <change-name>

# 4. update this INDEX.md
```

---

## Housekeeping (not changes)

| Task | Time | Why |
|------|------|------|
| Close Issue #12 | 5 min | "fence_id extension" already implemented in 2026-04-29 (comment confirms) |
| Evaluate Issue #8/#9 | 30 min | Sync S0/S1 from Apr 2026, stale |
| Update Issue #24 | 15 min | Reflect actual progress |
