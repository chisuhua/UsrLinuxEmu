# Active Changes Index

> **Created**: 2026-07-07
> **Owner**: UsrLinuxEmu Architecture Team
> **Total**: 12 changes
> **Source**: 2026-07-07 后续工作梳理

---

## 紧急 (P0) — 今天

### `2026-07-07-fix-docs-audit-runstage2-and-baseline` 🔴
**阻塞**: PR #26 merge, main CI green
**Effort**: 15 min
**Unblocks**: C-02, C-05, all subsequent PRs
**Why**: docs-audit.sh 双 bug（RUN_STAGE2 未初始化 + §2.6 hardcoded 13）导致 CI 持续 FAIL

### `2026-07-07-stage3-ioctl-dispatch-completeness` 🔴
**阻塞**: Phase 4 TaskRunner 真实化
**Depends**: C-01
**Effort**: ~5 min（=merge PR #26）+27 tests
**Why**: PR #20 把 18 个 IOCTL 加入 DRM 表但未加入 runtime kTable；只有 3 个真正可达；architectural bug

---

## 本周 (P1)

### `2026-07-07-sim-fence-id-base-cleanup` 🟡
**Effort**: 30 min
**Why**: PR #20 review follow-up #2，HAL 已修，driver 漏
**Find**: `grep -rn "1ULL << 32" plugins/gpu_driver/drv/`，替换为 `SIM_FENCE_ID_BASE`

### `2026-07-07-docs-tadr-mirror-sync` 🟢
**Effort**: 30 min
**Why**: ADR-035 §Rule 5.1 Step 3 — 跨仓 TADR mirror 同步
**Find**: `external/TaskRunner/docs/shared/adr/` 最新 TADR，更新 `docs/00_adr/README.md`

---

## 本月 (P2)

### `2026-07-08-stage3-1-ci-matrix-multi-platform` 🟢
**Effort**: 1 day
**Why**: Issue #24 §3.1 — CI matrix 当前只 ubuntu-latest，加 ubuntu-22.04 LTS

### `2026-07-08-stage3-3-errno-coverage-audit` 🟢
**Depends**: C-02
**Effort**: 1-2 days
**Why**: Issue #24 §3.3 — 20 个新 IOCTL handler errno 正确性审计（参考 PR #27 `-ENOMEM` 模式）

### `2026-07-08-stage3-3-error-injection-tests` 🟢
**Depends**: C-05
**Effort**: 2-3 days
**Why**: Issue #24 §3.3 — 错误注入测试框架 + critical path ≥ 80% 覆盖

---

## 本季度 (P3)

### `2026-07-15-phase4-sim-graph-launch-real-impl` 🔵
**Depends**: C-02
**Effort**: 1 week
**Why**: F.6 follow-up（已 doc）。当前 sim_graph_launch PoC 立即 signal，要改成真实 `GpuQueueEmu::submit` 异步

### `2026-07-15-phase4-cu-mempool-alloc-real-va` 🔵
**Depends**: C-02
**Effort**: 1 week
**Why**: TaskRunner PR #7 deferred #2 — synthetic VA → 真实 gpu_buddy first-fit VA

### `2026-07-22-stage3-2-perf-bench-baseline` 🔵
**Effort**: 1 week
**Why**: Issue #24 §3.2 — 建 `tests/perf/` 基准（ioctl/pushbuffer/mmap）。先 baseline 后优化

### `2026-08-01-stage3-2-hotpath-optimization` 🔵
**Depends**: C-10
**Effort**: 2 weeks
**Why**: Issue #24 §3.2 — VFS lookup hash index / ioctl perfect hash / BO cache

### `2026-08-15-stage1-4-kfd-multi-file-integration` ⚫ (sub-project)
**Effort**: 6-8 weeks
**Why**: README 后继 + Stage 1.4 Tier-2 deferred §3.2/§3.3。~50K LOC amdgpu port
**Scope**: `plugins/gpu_driver/drv/kfd/` 多文件切分 + Tier-2 deferred 真实化 + 2 FIXME 清理

---

## 依赖图

```
[C-01] ──┬──> [C-02] ──┬──> [C-05] ──> [C-06]
         │             ├──> [C-08]
         │             └──> [C-09]
         │
         └──> (any PR can now merge)

[C-03] ──> (independent, small)

[C-07] ──> (independent, 30 min)

[C-04] ──> (independent)

[C-10] ──> [C-11]

[C-12] ──> (Phase 4 mainline stable prerequisite)
```

---

## 推荐执行顺序

### 今日（30 min - 1 hour）
1. **C-01** fix-docs-audit (15 min)
2. **C-02** stage3-ioctl-dispatch (= merge PR #26, 5 min)

### 本周（1-2 hour total）
3. **C-03** sim-fence-id-base-cleanup (30 min)
4. **C-07** docs-tadr-mirror-sync (30 min)

### 本月（parallel）
5. **C-04** stage3-1-ci-matrix (1 day)
6. **C-05** stage3-3-errno-coverage (1-2 days, after C-02)
7. **C-06** stage3-3-error-injection (2-3 days, after C-05)

### 本季度
8. **C-08** phase4-sim-graph-launch (1 week)
9. **C-09** phase4-cu-mempool-real-va (1 week)
10. **C-10** stage3-2-perf-bench-baseline (1 week)
11. **C-11** stage3-2-hotpath-optimize (2 weeks, after C-10)
12. **C-12** kfd-multi-file (6-8 weeks sub-project)

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
|------|------|-----|
| Close Issue #12 | 5 min | "fence_id extension" already implemented in 2026-04-29 (comment confirms) |
| Evaluate Issue #8/#9 | 30 min | Sync S0/S1 from Apr 2026, stale |
| Update Issue #24 | 15 min | Reflect C-02/C-05 actual progress |
