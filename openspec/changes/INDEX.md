# Active Changes Index

> **更新**: 2026-07-10
> **Owner**: UsrLinuxEmu Architecture Team
> **总数**: 3 个活跃 change (原 12+1+1 个，11 个已完成/已归档)
> **Source**: 2026-07-07 后续工作梳理 + 2026-07-09 收割 + 2026-07-09 sim-fence-id-base-cleanup 归档 + 2026-07-09 新增 C-13 fence_id 注释 SSOT + 2026-07-09 C-13 闭环 + 2026-07-10 C-10 perf-bench 归档 + 2026-07-10 C-11 目录日期修正 2026-08-01 → 2026-07-10 + 2026-07-10 INDEX stale entry 清理（C-08 sim-graph-launch 已在 archive）

---

## ✅ 已完成 (11 归档)

| 原 ID | Change | 状态 | 证据 |
|-------|--------|------|------|
| C-01 | fix-docs-audit-runstage2-and-baseline | ✅ 已归档 | PR #28 `87680fb` |
| C-02 | stage3-ioctl-dispatch-completeness | ✅ 已归档 | PR #26 `edeee6e` |
| C-03 | sim-fence-id-base-cleanup | ✅ 已归档 | `13477ff` (HAL: `gpu_drm_driver.cpp`) + `7740a75` (drv: `gpgpu_device.cpp::handleWaitFence` 引入时即用宏，无 magic number 遗留) |
| C-05 | stage3-1-ci-matrix-multi-platform | ✅ 已归档 | PR #29 `2cba6a0` |
| C-06/07 | stage3-3-errno-and-error-injection (merged) | ✅ 已归档 | `07e40ef` `f5dd6ac` `ef962e2` `d24061d` |
| C-04 | docs-tadr-mirror-sync | ✅ 已归档 | `f679763` `ecfc648` |
| C-08 | mem-pool-async-fence-coverage | ✅ 已归档 | `a035e7b` (post-`TaskRunner/test-cu-graph-coverage-fixes` follow-up; async-fence round-trip for MEM_POOL_ALLOC_ASYNC + MEM_POOL_FREE_ASYNC) |
| C-08 | phase4-sim-graph-launch-real-impl | ✅ 已归档 | `openspec/changes/archive/2026-07-09-2026-07-15-phase4-sim-graph-launch-real-impl/` (完整 artifacts: proposal.md + design.md + spec.md + tasks.md + .openspec.yaml；INDEX 未及时同步，现已修复) |
| C-13 | sim-fence-id-comments-ssot | ✅ 已归档 | `e4b3378` (sim/fence_id.h + fence_id.cpp 注释字面量 `(1<<32)`/`INT64_MAX` → `SIM_FENCE_ID_BASE`/`SIM_FENCE_ID_MAX`；86/86 ctest PASS) |
| **C-10** | **stage3-2-perf-bench-baseline** | ✅ 已归档 | `d63da5e` (tests/perf/ Catch2 BENCHMARK 框架 + 3 个 benchmark binary；docs/04-building/perf-baseline-2026-Q3.md baseline 文档；adjusted targets 表) |

---

## 🔄 活跃 Changes

### 本季度 (P3)

### `2026-07-15-phase4-cu-mempool-alloc-real-va` 🔵
**依赖**: C-02 ✅
**Effort**: 1 周
**Why**: TaskRunner PR #7 deferred #2 — synthetic VA → 真实 gpu_buddy first-fit VA

### `2026-07-10-stage3-2-hotpath-optimization` 🔵
**依赖**: C-10 ✅
**Effort**: 2 周
**Why**: Issue #24 §3.2 — 基于 C-10 baseline 实现 hot path 优化（HandleManager / bo_map_ / ioctl dispatch / 移除 std::cout）。VFS hash 优化已裁剪（VFS 已是 O(1) unordered_map），BO thread-local cache 延后（见 Rule 3 触发条件）

### `2026-08-15-stage1-4-kfd-multi-file-integration` ⚫ (sub-project)
**Effort**: 6-8 周
**Why**: README 后继 + Stage 1.4 Tier-2 deferred §3.2/§3.3。~50K LOC amdgpu port

---

## 依赖图

```
[C-09/cu-mempool-real-va] ──> (C-02 ✅)

[C-10/perf-bench ✅] ──> [C-11/hotpath-optimize]

[C-12/kfd-multi-file] ──> (Phase 4 mainline stable prerequisite)
```

---

## 推荐执行顺序

### 本季度
1. **C-09** phase4-cu-mempool-alloc-real-va（1 周）— 需先补 proposal.md + tasks.md
2. **C-10** ~~stage3-2-perf-bench-baseline~~ ✅ archived（commit `d63da5e`）
3. **C-11** stage3-2-hotpath-optimize（2 周，after C-10 ✅）— 当前 ready，proposal+tasks 完备
4. **C-12** kfd-multi-file（6-8 周 sub-project）

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