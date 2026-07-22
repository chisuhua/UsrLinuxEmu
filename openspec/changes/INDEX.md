# Active Changes Index

> **更新**: 2026-07-22
> **Owner**: UsrLinuxEmu Architecture Team
> **总数**: **1 个活跃 change** + 17 个已完成/已归档
> **Source**: 2026-07-22 Stage 3.4 Doxygen 文档完成并归档，仅剩 v1.0 发布准备

---

## ✅ 已完成 (16 归档)

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

---

## 🔄 活跃 Changes

### 本季度 (P3)

### `2026-07-16-three-sanitizer-infra` ✅ (P3)
**Effort**: 3-5 天
**Why**: C-12 E.2.3 deferred — 补齐 ASan/UBSan CMake infra（TSan 已有）
**进度（2026-07-22）**: 34/34 tasks ✅ **已归档**
- Phase A: CMake Infra (8 tasks) — root CMakeLists.txt + tests/CMakeLists.txt
- Phase B: Plugin Artifact Isolation (5 tasks) — `scripts/stage-plugin.sh`
- Phase C: ASan Run + Bug Fixes (8 tasks) — fix: move device VA base to sanitizer-safe window
- Phase D: UBSan Run + Bug Fixes (7 tasks) — 0 runtime error
- Phase E: CI Integration (6 tasks) — sanitizer-asan/ubsan/tsan matrix jobs
**证据**: commit `5fc0006` — ASan/UBSan/TSan CMake infra + CI require jobs + bug 修复

### `2026-07-16-kfd-l1-l2-bridge-e2e` ✅ (P3)
**Effort**: 1-2 周
**Why**: C-12 E.2.4 deferred — 跨仓 L1↔L2 bridge 端到端验证（ADR-035 §Rule 5.1 4-step）
**进度（2026-07-22）**: 44/44 tasks ✅ **已归档**
- Phase A: UsrLinuxEmu 端 E2E (17 tasks) — GpgpuDevice IoctlEntry 扩展 (32→36) + 3 个真实 E2E 测试
- Phase B: TaskRunner 端 Change (9 tasks) — GpuDriverClient +4 KFD methods + 跨仓 PR
- Phase C: 跨仓 Submodule Bump (7 tasks) — 双仓 submodule sync
- Phase D: 文档 + 归档 (6 tasks) — taskrunner-index + kfd-portability-boundary 更新
- Phase E: 验收 (5 tasks) — 104/104 + TaskRunner 13/13 ctest PASS
**证据**: commit `177231a` + TaskRunner `d94719c` + submodule bump `aac4be5`

### `v1-0-release-prep` 🟡 (P2)
**Effort**: 2-3 天 (不含 Docker 可选)
**Why**: v1.0 发布必备 — CHANGELOG + Migration Guide + Binary Release + Docker
**进度（2026-07-22）**: 0/29 tasks
- Phase 1: CHANGELOG & Release Notes (3 tasks)
- Phase 2: Migration Guide (5 tasks) — System B→C ioctl mapping + kernel SHARED + directory restructure
- Phase 3: Binary Release Workflow (4 tasks) — GitHub Actions release.yml + static linking
- Phase 4: Docker (2 tasks, optional)
- Phase 5: plan-handoff Update (1 task)

---

> **C-12 已归档**（2026-07-16）— 详见 [archive/2026-07-16-2026-08-15-stage1-4-kfd-multi-file-integration/](archive/2026-07-16-2026-08-15-stage1-4-kfd-multi-file-integration/)
> **Stage 3.4 已归档**（2026-07-22）— 详见 [archive/2026-07-22-stage3-4-doxygen-docs/](archive/2026-07-22-stage3-4-doxygen-docs/)

## 依赖图

```
[v1-0-release-prep] ← 唯一活跃 change，无阻塞依赖
[C-12/kfd-multi-file] ✅ ARCHIVED 2026-07-16
[stage3-4-doxygen-docs] ✅ ARCHIVED 2026-07-22
```

---

## 推荐执行顺序

### 本季度
1. **v1-0-release-prep** 🟡 P2（0/29 tasks） — CHANGELOG + Migration Guide + Binary Release
2. ~~stage3-4-doxygen-docs~~ ✅ archived（2026-07-22；15/15 tasks）
3. ~~C-09~~ phase4-cu-mempool-alloc-real-va ✅ archived
4. ~~C-10~~ stage3-2-perf-bench-baseline ✅ archived
5. ~~C-11~~ stage3-2-hotpath-optimize ✅ archived
6. ~~C-12~~ stage1-4-kfd-multi-file-integration ✅ archived
7. ~~Wave 7 — three-sanitizer-infra~~ ✅ archived
8. ~~Wave 7 — kfd-l1-l2-bridge-e2e~~ ✅ archived

**当前状态**: Stage 3.4 文档完善 (P1) + v1.0 发布准备 (P2)。无其他待执行 change。

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