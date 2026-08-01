# Active Changes Index

> **更新**: 2026-08-01
> **Owner**: UsrLinuxEmu Architecture Team
> **总数**: **0 个活跃 change** + 24 个已完成/已归档
> **Source**: 2026-08-01 INDEX 同步：补齐 stage4-4-gpu-cp-phase55 (commit `452e298`, 2026-07-28 已归档但原 INDEX 漏登记)。

---

## ✅ 已完成 (24 归档)

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
| **stage3-4** | **stage3-4-doxygen-docs** | ✅ 已归档 (2026-07-22) | Doxygen API 参考 + quickstart 完善 + docs-audit 48→48 PASS |
| **W7** | **kfd-l1-l2-bridge-e2e** | ✅ 已归档 | `177231a` — 44/44 tasks, IoctlEntry 扩展 + 3 E2E tests + TaskRunner PR + 跨仓 sync |
| **stage4.4** | **stage4-4-gpu-cp-phase55** | ✅ 已归档 (2026-07-28) | commit `452e298 feat: merge stage4-4-gpu-cp-phase55 — GPU CP Phase 5.5 (priority+sema+IB)` + `b28089f chore: archive stage4-4-gpu-cp-phase55`；28/28 tasks（Priority Scheduling + Semaphore/Barrier + Indirect Buffer）；17 files, +2357 lines（含 Puller FSM 扩展、GlobalScheduler multiset 重排、3 个新 standalone 测试）；`openspec/changes/archive/2026-07-28-stage4-4-gpu-cp-phase55/` |
| **W7-sanitizer** | **three-sanitizer-infra** | ✅ 已归档 (2026-07-22) | commit `5fc0006` — ASan/UBSan/TSan CMake infra + CI require jobs + bug 修复；34/34 tasks；`openspec/changes/archive/2026-07-17-2026-07-16-three-sanitizer-infra/` |
| **W7-bridge-e2e** | **kfd-l1-l2-bridge-e2e** | ✅ 已归档 (2026-07-22) | commit `177231a` + TaskRunner `d94719c` + submodule bump `aac4be5`；44/44 tasks；`openspec/changes/archive/2026-07-18-2026-07-16-kfd-l1-l2-bridge-e2e/` |
| **v1.0** | **v1-0-release-prep** | ✅ 已归档 (2026-07-31) | commit `006bae2 chore(openspec): archive v1-0-release-prep` — 13/13 tasks（CHANGELOG.md + RELEASE_NOTES.md + docs/10-migration/v0-to-v1.md + Dockerfile + .github/workflows/release.yml + plan-handoff 标记完成）；`openspec/changes/archive/v1-0-release-prep/` |
| **stage4.5-preempt** | **stage4-5-cp-phase6-preemption-engine-finish** | ✅ 已归档 (2026-07-30) | commit `888b7cc chore(state): mark preemption-timeline-sem-gaps as archived in iteration.json`；53/53 tasks；`openspec/changes/archive/2026-07-30-stage4-5-cp-phase6-preemption-engine-finish/` |
| **stage4.5-predicate** | **stage4-5-cp-phase6-predication-aql** | ✅ 已归档 (2026-07-31) | commit `c0cbe94 chore(plan): remove plan files for archived changes (predication-aql + gaps)`；40/40 tasks；`openspec/changes/archive/2026-07-31-stage4-5-cp-phase6-predication-aql/` |
| **stage4.5-gaps** | **stage4-5-cp-phase6-preemption-timeline-sem-gaps** | ✅ 已归档 (2026-07-31) | commit `9153073 archive: preemption-timeline-sem-gaps change archived` — 81/81 tasks；`openspec/changes/archive/2026-07-31-stage4-5-cp-phase6-preemption-timeline-sem-gaps/` |
| **stage4.6** | **stage4-6-cp-phase7-green-context-pdl** | ✅ 已归档 (2026-08-01) | merge commit `c6f6ed3` — 19 atomic commits（ContextType + MQD.context_type + GREEN priority override + ChannelManager preemption + GPU_OP_PDL_LAUNCH + sim_pdl_launch + HAL +4 fn-ptrs）；ADR-056 Accepted；127/127 ctest PASS（+2 new tests）；docs-audit 53/53 PASS；HAL fn-ptr 29→33；MQD 128B ABI 保留；`openspec/changes/archive/2026-08-01-stage4-6-cp-phase7-green-context-pdl/` |

---

## 🔄 活跃 Changes

> **当前 0 个活跃 change**（2026-08-01）。所有 P1/P2/P3 changes 均已归档。下一步请通过 `openspec-propose` 创建新提案。

---

## 依赖图

```
（无活跃 change — 依赖图清空）
所有 P1/P2/P3 changes 已 archived (2026-07-22 / 2026-07-31)
```

---

## 推荐执行顺序

### 本季度
1. ~~**v1-0-release-prep**~~ ✅ archived (2026-07-31; 13/13 tasks; commit `006bae2`)
2. ~~stage3-4-doxygen-docs~~ ✅ archived（2026-07-22；15/15 tasks）
3. ~~C-09~~ phase4-cu-mempool-alloc-real-va ✅ archived
4. ~~C-10~~ stage3-2-perf-bench-baseline ✅ archived
5. ~~C-11~~ stage3-2-hotpath-optimize ✅ archived
6. ~~C-12~~ stage1-4-kfd-multi-file-integration ✅ archived
7. ~~Wave 7 — three-sanitizer-infra~~ ✅ archived
8. ~~Wave 7 — kfd-l1-l2-bridge-e2e~~ ✅ archived
9. ~~Stage 4.4 — gpu-cp-phase55~~ ✅ archived (2026-07-28; 28/28 tasks; commit `452e298`)
10. ~~Stage 4.5 — preemption-engine-finish~~ ✅ archived (2026-07-30)
11. ~~Stage 4.5 — predication-aql~~ ✅ archived (2026-07-31)
12. ~~Stage 4.5 — preemption-timeline-sem-gaps~~ ✅ archived (2026-07-31)
13. ~~Stage 4.6 — green-context-pdl~~ ✅ archived (2026-08-01)

**当前状态**: v1.0 发布准备完成 (P2) + Stage 3.4 文档完善 (P1) + Stage 4.5 preemption/predication 全栈 (P1) 均已交付。下一步建议根据 [roadmap.md](roadmap.md) 进入后续阶段（Stage 5+ 或真机对接）。

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