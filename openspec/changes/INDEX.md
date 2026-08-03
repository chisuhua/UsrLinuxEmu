# Active Changes Index

> **更新**: 2026-08-03
> **Owner**: UsrLinuxEmu Architecture Team
> **总数**: **0 个活跃 change** + 88 个已完成/已归档（截至 2026-08-03；4 个 Stage 4 closeout changes + 1 B-class foundation 都已 ship + archive）
> **Source**: 2026-08-03 INDEX 同步 — rebuild archive table from `openspec/changes/archive/`（88 dirs）+ 0 active changes
> （2026-08-01 INDEX 漏登记 59 个 directory entries；本次同步覆盖 2026-06 早期维护窗口 → 2026-08-01 stage4.6 → 2026-08-03 stage4 closeout batch + B-class foundation merge）

---

## ✅ 2026-08-03 归档 (Stage 4 closeout batch, 4 changes)

| 归档 | 摘要 | 状态 |
|------|------|------|
| `2026-08-03-stage4-6-green-context-pdl-closeout` | A2 inline HAL wrappers (commit 93e5f60) + A1 verify items closeout; P3-A3 deferred to sibling | ✅ archived |
| `2026-08-03-stage4-6-green-context-pdl-tests-standalone` | P3-A3: 10 test cases (52 assertions, all PASS) | ✅ archived |
| `2026-08-03-stage4-port-l2-linux-612-lts-build` | stage4 L2 build harness + CI workflow + operator handbook (ADR-072 §L2) | ✅ archived |
| `2026-08-03-stage4-port-bar-perf-baseline` | BAR perf baseline + CI gating (Stage 4 整体验收 §4) | ✅ archived |

---

## ⚪ 活跃 Changes

> **0 个活跃**（2026-08-03）。Stage 4 整体收尾 — 4 closeout changes 已 ship + archive。下一波 follow-up 由 [docs/roadmap/stage-4-bar-ioremap.md §"Stage 4 整体验收"](../../roadmap/stage-4-bar-ioremap.md) 与 [docs/architecture/stage4-gpu-cp-completion-gap-analysis.md §6](../../architecture/stage4-gpu-cp-completion-gap-analysis.md) 驱动。

--------|------|------|
| `2026-08-03-stage4-6-green-context-pdl-closeout` | P2-A2 inline HAL wrappers (hal_green_context_create/destroy + hal_pdl_launch/signal_completion) DONE in this session via gpu_hal.h；A1 verify items close-out via functional verify（tasks 1.6/1.7/2.4/3.5/9.1 functional 完整、勾选状态修正）；P3-A3 (8.1-8.7 test_green_context_standalone) deferred to sibling `2026-08-03-stage4-6-green-context-pdl-tests-standalone` | Pending impl review + A2-A1 close out approval |
| `2026-08-03-stage4-6-green-context-pdl-tests-standalone` | P3-A3: tests/test_green_context_standalone.cpp 创建 + 6 unit cases（8.2 GREEN priority override / 8.3 BROWN preempt GREEN / 8.4 GREEN resume / 8.5 GREEN↛GREEN / 8.6 FIFO / 8.7 HAL round-trip）；CMakeLists.txt 注册 + ctest；hal_mock.cpp 配合 | Pending impl (sibling to closeout) |
| `2026-08-03-stage4-port-l2-linux-612-lts-build` | P2-D-L2: ADR-072 L2 build harness — Linux 6.12 LTS 内核源码树编译 plugins/gpu_driver/drv/；CI workflow `l2-portability.yml` + 操作手册；零修改约束验证 | Pending impl（基础设施：kernel build env） |
| `2026-08-03-stage4-port-bar-perf-baseline` | P2-D-perf: Catch2 BENCHMARK ≥3 metrics（BAR_readl/writel + dma_coherent alloc）+ baseline JSON + CI gating 120% threshold + dedicated perf-runner workflow | Pending impl（perf-runner 环境） |

---

## ✅ 已完成 (83 归档)

按时间从早到晚排列。详细证据列采用 `commit / archive path / tasks N (count)` 三元组。

### 2026-06 维护窗口（ADR 治理 + Phase 2 OpenSpec 骨架）

| 原 ID | Change | 状态 | 证据 |
|-------|--------|------|------|
| | `2026-06-17-cleanup-adr-placeholders` | ✅ 已归档 | tasks 33/33；ADR 占位符清理 |
| | `2026-06-17-fix-gpu-pushbuffer-va-space-validation` | ✅ 已归档 | tasks 0/20（pre-checkbox 时代） |
| | `2026-06-17-h1-pushbuffer-validation-closeout` | ✅ 已归档 | tasks 21/24 |
| | `2026-06-17-ssot-deep-audit` | ✅ 已归档 | tasks 0/21 |
| | `2026-06-17-ssot-v0-1-7-comprehensive-fix` | ✅ 已归档 | tasks 0/31 |
| | `2026-06-18-adr-024-status-upgrade` | ✅ 已归档 | tasks 0/13 |
| | `2026-06-18-cleanup-gtest-residue` | ✅ 已归档 | tasks 0/15 |
| | `2026-06-18-cleanup-orphan-spec-purpose` | ✅ 已归档 | tasks 0/13 |
| | `2026-06-18-cleanup-orphan-struct-gpu-create-queue-args` | ✅ 已归档 | tasks 27/27 |
| | `2026-06-18-cleanup-shadow-dead-code` | ✅ 已归档 | tasks 0/26 |
| | `2026-06-18-fix-agents-md-ssot-link` | ✅ 已归档 | tasks 0/11 |
| | `2026-06-18-fix-sim-hardware-layout` | ✅ 已归档 | tasks 0/24 |
| | `2026-06-18-ssot-0-section-refresh` | ✅ 已归档 | tasks 0/13 |
| | `2026-06-18-ssot-v0-1-7-audit-closeout` | ✅ 已归档 | tasks 29/29 |
| | **adr-governance-refresh-v2** | ✅ 已归档 | ADR 治理刷新 v2 — 状态字段格式标准化 + ADR-027 不一致修复；tasks 19/19 |
| | `2026-06-19-h2-5-architecture-foundation` | ✅ 已归档 | tasks 0/66 |
| | `2026-06-19-h2-phase2-openspec-skeleton` | ✅ 已归档 | tasks 0/35 |
| | `2026-06-22-h3-phase2-management` | ✅ 已归档 | H-3 Phase 2 Management；tasks 6/55 |
| | `2026-06-22-2026-06-23-h4-architecture-governance-cleanup` | ✅ 已归档 | H-4 Architecture Governance Cleanup；tasks 7/73 |
| | **h5-taskrunner-scope-clarification** | ✅ 已归档 | H-5 TaskRunner Scope Clarification（双轨演进）；tasks 0/141 |
| | `2026-06-25-h3-5-followup-test-fixture-cleanup` | ✅ 已归档 | H-3.5 TaskRunner test-fixture 范畴清理；tasks 0/105 |
| | `2026-06-25-h5-1-taskrunner-scope-cleanup` | ✅ 已归档 | H-5.1 TaskRunner Scope Cleanup；tasks 30/31 |
| | `2026-06-26-2026-06-26-h3-6-issue-3-coordination` | ✅ 已归档 | H-3.6 ADR-034 Issue #3 修复协调（attached_queues 弱校验）；tasks 7/79 |
| | `2026-06-26-2026-06-26-h3-7-issue-2-coordination` | ✅ 已归档 | H-3.7 ADR-034 Issue #2 修复协调（ioctl path 绕过 GpuQueue）；tasks 9/41 |
| | `2026-06-26-2026-06-26-h3-8-issue-1-coordination` | ✅ 已归档 | H-3.8 ADR-034 Issue #1 修复协调（stream_id u32→u64）；tasks 7/93 |
| | `2026-06-26-2026-06-26-h3-maintenance-transition` | ✅ 已归档 | H-3 Maintenance Transition（3.6/3.7/3.8 收官清扫）；tasks 40/40 |
| | `iommu-invalidate-hal` | ✅ 已归档 | HAL IOMMU Invalidate 回调完整实现；tasks 0/16 |
| | `iommu-mmu-notifier` | ✅ 已归档 | mmu_notifier 回调完整实现；tasks 0/6 |
| | `kernel-tests` | ✅ 已归档 | kernel 层测试覆盖；tasks 0/7 |
| | `linux-compat-tests` | ✅ 已归档 | linux_compat 层测试覆盖；tasks 0/7 |
| | `hal-event-signal` | ✅ 已归档 | HAL Event Signal 扩展完整实现；tasks 8/8 |
| | `hal-iommu-full` | ✅ 已归档 | HAL IOMMU 扩展完整实现；tasks 13/13 |
| | `kfd-multi-file-complete` | ✅ 已归档 | KFD 多文件集成剩余 19%；tasks 0/7 |

### Stage 1 — Linux 内核环境模拟（2026-07-02 ~ 07-05）

| 原 ID | Change | 状态 | 证据 |
|-------|--------|------|------|
| | `2026-07-02-stage-1-0-pcie-emu` | ✅ 已归档 | Stage 1.0 PCIe 设备模拟；tasks 52/55 |
| | `2026-07-02-stage-1-1-iommu-ats` | ✅ 已归档 | Stage 1.1 IOMMU + ATS；tasks 32/39 |
| | `2026-07-02-stage-1-2-drm-subset` | ✅ 已归档 | Stage 1.2 DRM 子集（drm_device 风格重构）；tasks 61/62 |
| | `2026-07-02-taskrunner-umd-backend-enable` | ✅ 已归档 | TaskRunner UMD backend 启用；tasks 16/16 |
| | `2026-07-04-stage-1-3-uvm-hmm` | ✅ 已归档 | Stage 1.3 UVM/HMM；tasks 47/59 |
| | `2026-07-04-stage-1-4-kfd-portability` | ✅ 已归档 | Stage 1.4 KFD 可移植性边界；tasks 0/56 |
| | `2026-07-04-stage-1-4-tier2-kfd-integration` | ✅ 已归档 | Stage 1.4 Tier-2 KFD 集成；tasks 96/96 |

### Stage 2 — 多设备插件化（2026-07-05）

| 原 ID | Change | 状态 | 证据 |
|-------|--------|------|------|
| | `2026-07-05-stage-2-multi-device` | ✅ 已归档 | Stage 2 多设备（net + storage 插件化）；tasks 55/55 |

### Stage 3 — v1.0 稳定 + 维护窗口（2026-07-06 ~ 07-23）

| 原 ID | Change | 状态 | 证据 |
|-------|--------|------|------|
| | `2026-07-06-2026-07-05-sim-stream-primitive-support` | ✅ 已归档 | sim stream primitive 支持；tasks 0/90 |
| **C-01** | `2026-07-07-2026-07-07-fix-docs-audit-runstage2-and-baseline` | ✅ 已归档 | PR #28 `87680fb` |
| **C-02** | `2026-07-07-2026-07-07-stage3-ioctl-dispatch-completeness` | ✅ 已归档 | PR #26 `edeee6e` |
| **C-03** | `2026-07-07-sim-fence-id-base-cleanup` | ✅ 已归档 | `13477ff` (HAL: `gpu_drm_driver.cpp`) + `7740a75` (drv: `gpgpu_device.cpp::handleWaitFence` 引入时即用宏，无 magic number 遗留) |
| **C-04** | `2026-07-08-2026-07-07-docs-tadr-mirror-sync` | ✅ 已归档 | `f679763` `ecfc648` |
| **C-05** | `2026-07-08-2026-07-08-stage3-1-ci-matrix-multi-platform` | ✅ 已归档 | PR #29 `2cba6a0` |
| | `2026-07-08-2026-07-08-stage3-3-errno-coverage-audit` | ✅ 已归档 | errno coverage 审计；tasks 0/18 |
| | `2026-07-08-2026-07-08-stage3-3-error-injection-tests` | ✅ 已归档 | error injection tests；tasks 0/43 |
| **C-06/07** | `2026-07-08-2026-07-08-stage3-3-error-injection-tests` (merged with errno) | ✅ 已归档 | `07e40ef` `f5dd6ac` `ef962e2` `d24061d` |
| **stage3-3-tadr-47-305** | `2026-07-08-2026-07-08-tadr-mirror-47-and-305` | ✅ 已归档 | tadr mirror 47 + 305；tasks 18/18 |
| **C-13** | `2026-07-09-sim-fence-id-comments-ssot` | ✅ 已归档 | `e4b3378` (sim/fence_id.h + fence_id.cpp 注释字面量 `(1<<32)`/`INT64_MAX` → `SIM_FENCE_ID_BASE`/`SIM_FENCE_ID_MAX`；86/86 ctest PASS) |
| **C-08** | `2026-07-09-mem-pool-async-fence-coverage` | ✅ 已归档 | `a035e7b` (post-`TaskRunner/test-cu-graph-coverage-fixes` follow-up; async-fence round-trip for MEM_POOL_ALLOC_ASYNC + MEM_POOL_FREE_ASYNC) |
| | `2026-07-09-phase4-sim-graph-launch-test-gaps` | ✅ 已归档 | sim graph launch 测试补全；tasks 15/15 |
| **C-08** | `2026-07-09-2026-07-15-phase4-sim-graph-launch-real-impl` | ✅ 已归档 | 完整 artifacts: proposal.md + design.md + spec.md + tasks.md + .openspec.yaml；INDEX 漏登记现已修复；tasks 33/33 |
| | `2026-07-10-2026-07-08-stage3-3-errno-coverage-audit` | ✅ 已归档 | errno coverage 审计（迁移）；tasks 0/18 |
| | `2026-07-10-2026-07-08-stage3-3-error-injection-tests` | ✅ 已归档 | error injection tests（迁移）；tasks 0/24 |
| **C-11** | `2026-07-10-2026-07-10-stage3-2-hotpath-optimization` | ✅ 已归档 | PR #30 `perf/stage3-2-hotpath` branch（6 commits: tasks 5/16 + 4 优化项 + perf-baseline 结果）；acceptance 2/3 hit（ioctl 11.6× / pushbuffer 1296× / BO 2.1× speedup） |
| **C-10** | `2026-07-10-2026-07-22-stage3-2-perf-bench-baseline` | ✅ 已归档 | `d63da5e` (tests/perf/ Catch2 BENCHMARK 框架 + 3 个 benchmark binary；tasks 17/19；docs/04-building/perf-baseline-2026-Q3.md baseline 文档；adjusted targets 表) |
| **C-09** | `2026-07-11-2026-07-15-phase4-cu-mempool-alloc-real-va` | ✅ 已归档 | `ba88b5f feat(sim): real VA allocation in sim_mem_pool via gpu_buddy + mmap backing`（ADR-058 + Oracle 调研；新增 `sim_device_va_allocator.{h,cpp}`；tasks 17/17；18/18 tests, 86/86 ctest PASS） |
| **C-12** | `2026-07-16-2026-08-15-stage1-4-kfd-multi-file-integration` | ✅ 已归档（2026-07-16） | 81% 原子任务完成（tasks 64/97），Phase A/B/C/D 全 [x]，Phase E 8/9 [x] + L1↔L2 skeleton；104/104 ctest + docs-audit 43/43 PASS |
| **W7** | `2026-07-17-2026-07-16-three-sanitizer-infra` | ✅ 已归档 (2026-07-22) | commit `5fc0006` — ASan/UBSan/TSan CMake infra + CI require jobs + bug 修复；tasks 34/34 |
| **W7-bridge-e2e** | `2026-07-18-2026-07-16-kfd-l1-l2-bridge-e2e` | ✅ 已归档 (2026-07-22) | commit `177231a` + TaskRunner `d94719c` + submodule bump `aac4be5`；tasks 47/47 |
| | `2026-07-20-2026-07-18-cuda-e2e-real-path` | ✅ 已归档 | CUDA E2E real path 验证；tasks 14/75 |
| | `2026-07-21-stage3-3-errno-coverage-audit` | ✅ 已归档 | errno coverage 最终归档（合并自 07-08 / 07-10）；tasks 27/27 |
| **stage3-4** | `2026-07-22-stage3-4-doxygen-docs` | ✅ 已归档 (2026-07-22) | Doxygen API 参考 + quickstart 完善 + docs-audit 48→48 PASS；tasks 15/15 |
| | `2026-07-23-stage3-4-doc-completion` | ✅ 已归档 | stage3-4 文档收官；tasks 0/13 |

### Stage 4 — BAR + ioremap + GPU CP 完整化（2026-07-26 ~ 08-03）

| 原 ID | Change | 状态 | 证据 |
|-------|--------|------|------|
| | `2026-07-26-stage4-1-bar-ioremap` | ✅ 已归档 | Stage 4.1 — 真实 BAR + ioremap + DMA Coherent；tasks 30/30 |
| | `2026-07-27-stage4-1-bar-ioremap` | ✅ 已归档 | Stage 4.1 BAR ioremap + DMA Coherent（TDD 直接实施）；tasks 21/21 |
| | `stage4-1-bar-ioremap` | ✅ 已归档 | Stage 4.1 最终归档名；tasks 25/25 |
| | `2026-07-27-stage4-3-cp-phase5-method-hyperqueue` | ✅ 已归档 | Stage 4.3 GPU CP Phase 5 Method + HyperQueue；tasks 27/50（部分跨 batch 任务）|
| | `stage4-3-integration-wiring` | ✅ 已归档 | Stage 4.3 Integration Wiring；tasks 22/22 |
| **stage4.4** | `2026-07-28-stage4-4-gpu-cp-phase55` | ✅ 已归档 (2026-07-28) | commit `452e298 feat: merge stage4-4-gpu-cp-phase55 — GPU CP Phase 5.5 (priority+sema+IB)` + `b28089f chore: archive`；tasks 28/28（Priority Scheduling + Semaphore/Barrier + Indirect Buffer）；17 files, +2357 lines（含 Puller FSM 扩展、GlobalScheduler multiset 重排、3 个新 standalone 测试） |
| | `2026-07-29-stage4-5-cp-phase6-preemption-timeline-sem` | ✅ 已归档 | Stage 4.5 Preemption Timeline Sem；tasks 40/43（3 项 verify/deps 残） |
| **stage4.5-preempt** | `2026-07-30-stage4-5-cp-phase6-preemption-engine-finish` | ✅ 已归档 (2026-07-30) | commit `888b7cc chore(state): mark preemption-timeline-sem-gaps as archived`；tasks 42/42；ADR-046 preemption engine core |
| **stage4.5-predicate** | `2026-07-31-stage4-5-cp-phase6-predication-aql` | ✅ 已归档 (2026-07-31) | commit `c0cbe94 chore(plan): remove plan files for archived changes`；tasks 31/40（ADR-051 + ADR-052 AQL — 部分 PM4 stub 残 follow Phase 6.5） |
| **stage4.5-gaps** | `2026-07-31-stage4-5-cp-phase6-preemption-timeline-sem-gaps` | ✅ 已归档 (2026-07-31) | commit `9153073 archive: preemption-timeline-sem-gaps change archived` — concurrent test + ASan/UBSan/TSan 验证 + docs-audit 清理 + preemption-spec-correction addendum；tasks 79/81（2 项 sanitizer gate 残） |
| **stage4.6** | `2026-08-01-stage4-6-cp-phase7-green-context-pdl` | ✅ 已归档 (2026-08-01) | merge commit `c6f6ed3` — 19 atomic commits（ContextType + MQD.context_type + GREEN priority override + ChannelManager preemption + GPU_OP_PDL_LAUNCH + sim_pdl_launch + HAL +4 fn-ptrs）；ADR-056 Accepted；tasks 71/85（14 项 verify/inline HAL helpers/standalone tests 残 follow-up）；127/127 ctest PASS（+2 new tests）；docs-audit 53/53 PASS；HAL fn-ptr 29→33 |

### 维护窗口收尾（2026-07）

| 原 ID | Change | 状态 | 证据 |
|-------|--------|------|------|
| **v1.0** | `v1-0-release-prep` | ✅ 已归档 (2026-07-31) | commit `006bae2 chore(openspec): archive v1-0-release-prep` — tasks 13/13（CHANGELOG.md + RELEASE_NOTES.md + docs/10-migration/v0-to-v1.md + Dockerfile + .github/workflows/release.yml + plan-handoff 标记完成） |
| **version-policy** | `version-policy-adr` | ✅ 已归档 | Version Policy ADR；tasks 24/24 |

### IOCTL E2E 测试完备性审计（2026-08-03 同日批量入库）

| 原 ID | Change | 状态 | 证据 |
|-------|--------|------|------|
| **wire-mmu-fw-cb** | `2026-08-03-wire-mmu-fw-callback-ioctls-to-active-dispatch` | ✅ 已归档 | P0 — MMU/FW callback IOCTLs 接入 active dispatch 表；tasks 26/26；Wire-mfw 实现 |
| **e2e-register-queue-ring** | `2026-08-03-add-e2e-tests-for-register-gpu-and-map-queue-ring` | ✅ 已归档 | P1 — register_gpu + map_queue_ring E2E 测试；tasks 24/24 |
| **semantic-va-query** | `2026-08-03-strengthen-semantic-assertions-for-destroy-va-space-and-query-queue` | ✅ 已归档 | P1 — destroy_va_space + query_queue 语义断言增强；tasks 25/25 |
| **abi-dispatch-consistency** | `2026-08-03-add-abi-dispatch-consistency-test` | ✅ 已归档 | P2 — ABI dispatch 一致性测试；tasks 28/28 |

---

## 依赖图

```
stage4-6-green-context-pdl-closeout (active, pending review; A2-A1 close-out)
   ↓ (defers P3-A3 to sibling)
stage4-6-green-context-pdl-tests-standalone (P3-A3, pending impl)
       ↘  ↙ (Stage 4 整体验收 gates)
stage4-port-l2-linux-612-lts-build (P2-D-L2, pending impl; Linux kernel build env)
stage4-port-bar-perf-baseline    (P2-D-perf, pending impl; Catch2 BENCHMARK harness)
```

---

## 推荐执行顺序

### 本季度（已完成）

1-83. ~~所有归档 changes~~ ✅ archived（见上表，按时间从 2026-06-17 → 2026-08-03）。

### 下一波建议（P2/P3 follow-up）

按 [docs/architecture/stage4-gpu-cp-completion-gap-analysis.md §6](../docs/architecture/stage4-gpu-cp-completion-gap-analysis.md) 与 [docs/roadmap/stage-4-bar-ioremap.md §"Stage 4 整体验收"](../docs/roadmap/stage-4-bar-ioremap.md)：

```
1. ~~stage4-6-cp-phase7-green-context-pdl~~ ✅ archived (2026-08-01; 71/85)
2. stage4-6-green-context-pdl-closeout (P2-A1+A2) — 14 残留 verify + inline HAL helpers
3. stage4-6-green-context-pdl-tests-standalone (P3-A3) — 9 unit cases + PDL tests
4. linux-612-lts-portability (P2-D-L2) — ADR-072 L2 可移植性 gating
5. bar-perf-baseline (P2-D-perf) — BAR 性能基准 + ≤20% CI gate
6. ADR-052 Phase 6.5 PM4 microcode (P3-B) — 独立 change
7. ADR-049 multi-engine Puller (P3-B) — 真机 driver 验证触发
8. ADR-011 multiprocess support (P3-B 偶挂) — Phase 3 trigger
```

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

# 4. update this INDEX.md（每次 archive 后必须）
```

---

## Housekeeping (not changes)

| Task | Time | Why |
|------|------|-----|
| Close Issue #12 | 5 min | "fence_id extension" already implemented in 2026-04-29 (comment confirms) |
| Evaluate Issue #8/#9 | 30 min | Sync S0/S1 from Apr 2026, stale |
| Update Issue #24 | 15 min | Reflect actual progress |
| **NEW** 同步 INDEX.md 表头到 2026-08-03 | 已完成 | 修复 2026-08-01 INDEX 漏登记 59 个 dirs |
