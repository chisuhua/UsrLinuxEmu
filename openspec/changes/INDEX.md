# Active Changes Index

> **更新**: 2026-07-17
> **Owner**: UsrLinuxEmu Architecture Team
> **总数**: **2 个活跃 change** + 14 个已完成/已归档（含 C-12 stage1-4-kfd-multi-file-integration）
> **Source**: 2026-07-17 **C-12 Follow-up Wave 7 启动** — 创建 2 个新 openspec change 承接 C-12 deferred E.2.3 + E.2.4：
>   - `2026-07-16-three-sanitizer-infra` (E.2.3 — 32 tasks, 5 phases) — ASan/UBSan CMake infra + 修复暴露真 bug + 文档同步
>   - `2026-07-16-kfd-l1-l2-bridge-e2e` (E.2.4 — 41 tasks, 5 phases) — UsrLinuxEmu 端 E2E test + TaskRunner 端 change + ADR-035 §Rule 5.1 4-step 跨仓同步
> + 2026-07-16 C-12 已归档（openspec archive → `2026-07-16-2026-08-15-stage1-4-kfd-multi-file-integration/`，81% 原子任务完成） + 104/104 ctest PASS + docs-audit 43/43 PASS + TaskRunner 10/10 ctest ≈185 cases PASS

---

## ✅ 已完成 (14 归档)

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
| **C-11** | **stage3-2-hotpath-optimization** | ✅ 已归档 | PR #30 `perf/stage3-2-hotpath` branch（6 commits: tasks.md + P1 cout 移除 handleGetDeviceInfo `893715b` + P2 BO path cout 移除 `41f3704` + P3 HandleManager bitset no-op `a7bae7e` + P4 pushbuffer max-throughput bench `98ee8a1` + perf-baseline §C-11 Results `b064aa5`）；acceptance 2/3 hit（ioctl 11.6× / pushbuffer 1296× / BO 2.1× speedup） |
| **C-09** | **phase4-cu-mempool-alloc-real-va** | ✅ 已归档 | `ba88b5f feat(sim): real VA allocation in sim_mem_pool via gpu_buddy + mmap backing`（ADR-058 + Oracle report AMD KFD v6.10 + Nvidia UVM `uvm_range_allocator` 调研；新增 `sim_device_va_allocator.{h,cpp}` per-device gpu_buddy + std::mutex；`mem_pool.cpp` 重写 + mmap(MAP_ANONYMOUS\|MAP_PRIVATE\|MAP_FIXED_NOREPLACE) backing；18/18 tests, 86/86 ctest PASS, docs-audit clean, libgpu_core zero-modify per ADR-020） |
| **C-12** | **stage1-4-kfd-multi-file-integration** | ✅ 已归档（2026-07-16） | `openspec archive 2026-07-16-2026-08-15-stage1-4-kfd-multi-file-integration`（81% 原子任务完成，Phase A/B/C/D 全 [x]，Phase E 8/9 [x] + L1↔L2 skeleton + docs updates；104/104 ctest + docs-audit 43/43 PASS；E.2.3 sanitizer + E.2.4.2/4.3 cross-repo deferred to follow-up PRs per ADR-035 §Rule 5.1） |

---

## 🔄 活跃 Changes

### 本季度 (P3)

### `2026-07-16-three-sanitizer-infra` 🟡 (P3)
**Effort**: 3-5 天
**Why**: C-12 E.2.3 deferred — 补齐 ASan/UBSan CMake infra（TSan 已有）
**进度（2026-07-17）**: 0/32 tasks
- Phase A: CMake Infra (7 tasks)
- Phase B: ASan Run + Bug Fixes (8 tasks)
- Phase C: UBSan Run + Bug Fixes (7 tasks)
- Phase D: 文档 + CI 集成 (4 tasks)
- Phase E: 验证 + 归档 (6 tasks)

### `2026-07-16-kfd-l1-l2-bridge-e2e` 🟡 (P3)
**Effort**: 1-2 周
**Why**: C-12 E.2.4 deferred — 跨仓 L1↔L2 bridge 端到端验证（ADR-035 §Rule 5.1 4-step）
**进度（2026-07-17）**: 0/41 tasks
- Phase A: UsrLinuxEmu 端 E2E (11 tasks) — skeleton 已在 commit `ed9ce1e`
- Phase B: TaskRunner 端 Change (9 tasks) — 跨仓 PR
- Phase C: 跨仓 Submodule Bump (7 tasks) — ADR-035 §Rule 5.1 Step 3
- Phase D: 文档 + 归档 (6 tasks)
- Phase E: 验收 (5 tasks)

---

> **C-12 已归档**（2026-07-16）— 详见 [archive/2026-07-16-2026-08-15-stage1-4-kfd-multi-file-integration/](archive/2026-07-16-2026-08-15-stage1-4-kfd-multi-file-integration/)

## 依赖图

```
[C-12/kfd-multi-file] ✅ ARCHIVED 2026-07-16 ──> (Wave 7 follow-ups: three-sanitizer-infra + kfd-l1-l2-bridge-e2e)
```

---

## 推荐执行顺序

### 本季度
1. **C-09** ~~phase4-cu-mempool-alloc-real-va~~ ✅ archived（commit `ba88b5f`）
2. **C-10** ~~stage3-2-perf-bench-baseline~~ ✅ archived（commit `d63da5e`）
3. **C-11** ~~stage3-2-hotpath-optimize~~ ✅ archived（PR #30，acceptance PASS 2/3）
4. **C-12** ~~stage1-4-kfd-multi-file-integration~~ ✅ archived（2026-07-16；81% 原子任务完成）
5. **Wave 7 — three-sanitizer-infra** 🟡 active（0/32 tasks, 3-5 天）
6. **Wave 7 — kfd-l1-l2-bridge-e2e** 🟡 active（0/41 tasks, 1-2 周，跨仓）

**执行路径建议**：先推进 three-sanitizer-infra（低风险单仓工作），完成后启动 kfd-l1-l2-bridge-e2e（跨仓 PR 工作流）。

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