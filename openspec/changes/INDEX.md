# Active Changes Index

> **更新**: 2026-07-16
> **Owner**: UsrLinuxEmu Architecture Team
> **总数**: 1 个活跃 change (13 个已完成/已归档)
> **Source**: 2026-07-16 C-12 Wave 1 收尾（B.4.3 sim_signal_path 集成 + B.4.6 关父任务 + C.2.3 并发进程测试 PASS） + 2026-07-16 C-12 测试套 101/101 ctest PASS + docs-audit 43/43 PASS + 2026-07-11 C-09 phase4-cu-mempool-alloc-real-va 归档（commit `ba88b5f` + ADR-058 + Oracle 报告） + 2026-07-11 C-12 B.1.10 thread infrastructure PoC（ADR-060 §1.3 落地） + 2026-07-14 ADR-061/062 → Accepted + C-12 Phase A gate cleared（Oracle/Metis review 修复） + 2026-07-15 C-12 Phase A 文档化完成（ADR-059 Accepted + kfd-multi-file.md + kfd-abi-comparison-report.md） + 2026-07-15 Phase B 启动（B.1.1 bridge + B.1.3 pasid + B.1.5 process + B.1.7 mutex + B.1.8/1.9 topology/svm stubs） + 2026-07-16 Phase B 持续推进（B.2.1 dispatch IOCTL + B.3.1 mmu + B.4.1 events + B.4.4 sim_event + B.4.6 TSan hardening） + 2026-07-16 Phase C 真实化（iommu remap/invalidate/迁移 + sim page fault handler）+ Phase D FIXME cleanup（kfd_queue_buffer_put 移除 + `_locked` 变体）

---

## ✅ 已完成 (13 归档)

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

---

## 🔄 活跃 Changes

### 本季度 (P3)

### `2026-08-15-stage1-4-kfd-multi-file-integration` ⚫ (sub-project)
**Effort**: 6-8 周
**Why**: README 后继 + Stage 1.4 Tier-2 deferred §3.2/§3.3。~50K LOC amdgpu port
**进度（2026-07-16）**: 48/80 原子任务 ≈ **60%**（tasks.md 实际勾选 48/96 = 50%，含验收项）
- ✅ **Phase A** 文档化（4/4 完成，2026-07-15）— ADR-059/060/061/062 Accepted + kfd-multi-file.md + kfd-abi-comparison-report.md
- 🟡 **Phase B** 模块切分（27/27 任务全开 + 3 验收延后 Phase E；26 [x]）— B.1 module/pasid/process/topology/svm + B.2 dispatch + B.3 mmu/HAL ops + B.4 events/sim_signal_event/TSan（B.4.3 ✅ day-1 stub 集成 sim_signal_event；B.4.6 ✅ 关父任务）
- 🟡 **Phase C** Tier-2 deferred 真实化（10/18）— C.1 IOMMU invalidation（5/10）+ C.2 mm_shim wire-up（4 含 C.2.3 ✅ 并发进程测试 PASS，31 assertions/2 cases）
- ✅ **Phase D** FIXME 清理（3 [x]，7 任务已合并）— `kfd_queue_buffer_put` 移除 + `_locked` 变体
- 📋 **Phase E** 集成 + E2E（2/24）— E.0 集成测试（E.0.1-0.3 全 [ ]）+ E.1 build 验证（E.1.1-1.3 全 [ ]）+ E.2 TaskRunner E2E（E.2.1-2.4 全 [ ]）+ E.3 docs 更新（E.3.1-3.9 全 [ ]）+ E.4 PR + merge + 归档（E.4.1-4.7 全 [ ]）
**代码产出**: `plugins/gpu_driver/drv/kfd/` 21 文件（kfd_module/pasid/process/dispatch/mmu/events/topology/svm/queue + sim_bridge + types + priv）+ `plugins/gpu_driver/sim/sim_event.c`
**测试基线**: 101 ctest binary（Stage 2 86 + C-12 新增 15），**101/101 PASS** + docs-audit 43/43 PASS + 0 warnings

---

## 依赖图

```
[C-12/kfd-multi-file] ──> (Phase 4 mainline stable prerequisite)
```

---

## 推荐执行顺序

### 本季度
1. **C-09** ~~phase4-cu-mempool-alloc-real-va~~ ✅ archived（commit `ba88b5f`）
2. **C-10** ~~stage3-2-perf-bench-baseline~~ ✅ archived（commit `d63da5e`）
3. **C-11** ~~stage3-2-hotpath-optimize~~ ✅ archived（PR #30，acceptance PASS 2/3）
4. **C-12** kfd-multi-file（6-8 周 sub-project，60% 已完成）
   - **下一批（Wave 2）**：E.0.1 `test_kfd_end_to_end_standalone`（5 KFD ioctl E2E，最高价值）→ E.0.2 fault_handling → E.0.3（依赖 C.2.3 已 ✅）
   - **后续（Wave 3-6）**：E.1.1-1.3 build 验证 → E.2.1-2.4 TaskRunner E2E（含 L1↔L2 bridge）→ E.3.1-3.9 docs → E.4.1-4.7 PR + merge + 归档

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