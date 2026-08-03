# Tasks: Stage 4 BAR Performance Baseline + CI Gating

> 实施 Stage 4 整体验收 §④ 性能基准（BAR readl/writel 延迟 ≤ 20% 回归阈值），含 baseline 测量 + CI gating harness。

---

## 1. Benchmark test file scaffold

- [ ] 1.1 创建 `tests/perf/test_bar_perf_standalone.cpp` (新文件)
- [ ] 1.2 Catch2 BENCHMARK framework + necessary includes
- [ ] 1.3 Helper: `make_bar0_mock_hal()` — 模拟 BAR0 + register fn-ptrs from `hal_mock.cpp` (per ADR-069 + ADR-073)
- [ ] 1.4 Helper: `disable_perf_variability()` — disable turbo + ASLR + pin CPU (perf-runner only)
- [ ] 1.5 Test case tags: `[perf][bar]`

## 2. Benchmarks (≥3)

- [ ] 2.1 `BAR_readl_latency`: `BENCHMARK("BAR_readl")` wrapping `hal_register_read(&hal, 0x4, &val)` (sim BAR0 path)
- [ ] 2.2 `BAR_writel_latency`: `BENCHMARK("BAR_writel")` wrapping `hal_register_write(&hal, 0x4, val)`
- [ ] 2.3 `BAR_dma_alloc_coherent_latency`: `BENCHMARK("dma_coherent_alloc_free")` wrapping `dma_alloc_coherent(4096, &dma) + dma_free_coherent` round-trip
- [ ] 2.4 (Optional) `BAR_ioremap_unmap_cycle`: full cycle benchmark

## 3. CMakeLists.txt registration

- [ ] 3.1 Append `add_executable(test_bar_perf_standalone ...)` block in `tests/CMakeLists.txt`
- [ ] 3.2 link `gpu_sim`, `kernel`, `hal_mock`
- [ ] 3.3 add benchmark ctest entry (but mark `[benchmark]` tag so default `ctest` doesn't run)
- [ ] 3.4 Alternative: `ctest -L perf` runs only perf tests

## 4. 首次 baseline 测量

- [ ] 4.1 Local run on ubuntu-22.04 host (or known perf-runner spec)
- [ ] 4.2 Run 3 cycles × 1000 iterations × warm-up 100 discarded
- [ ] 4.3 Capture median / mean / stddev per benchmark
- [ ] 4.4 Document baseline in `docs/04-building/perf-baseline-bar-2026-Q3.md`:
  - [ ] 4.4.1 Environment: CPU model + governor + kernel version + ASLR/turbo state
  - [ ] 4.4.2 Per-benchmark median ± stddev table
  - [ ] 4.4.3 Methodology note

## 5. CI gating workflow

- [ ] 5.1 Create `.github/workflows/perf-regression.yml`
- [ ] 5.2 Runs-on: dedicated perf-runner (GitHub-hosted `ubuntu-22.04-x64` initially)
- [ ] 5.3 Triggers: PR with changes to drv/, sim/, include/kernel/, tests/perf/
- [ ] 5.4 Steps: build → bench (--reporter=json) → gate (JSON compare vs `.perf-baseline.json`)
- [ ] 5.5 Failure action: `core.setFailed` + auto-PR-comment

## 6. Baseline JSON fixture

- [ ] 6.1 Create `.perf-baseline.json` with captured median/mean/stddev
- [ ] 6.2 Commit baseline as project artifact (updated via follow-up changes)
- [ ] 6.3 Document how to re-measure baseline (kernel upgrade / new runner)

## 7. Documentation

- [ ] 7.1 `docs/04-building/perf-baseline-bar-2026-Q3.md` 创建
- [ ] 7.2 Update `docs/04-building/perf-baseline-2026-Q3.md` cross-reference (Stage 3 baseline doc) + Stage 4 addition note
- [ ] 7.3 Update `docs/README.md` 04-building index entry
- [ ] 7.4 Update `AGENTS.md` "构建命令" section: mention perf-runner workflow

## 8. Validation + 集成

- [ ] 8.1 Local baseline PASS (build OK, benchmarks ≥3 emit median output)
- [ ] 8.2 CI workflow local simulation (act CLI) - syntax check
- [ ] 8.3 First PR after merge triggers perf-regression.yml successfully
- [ ] 8.4 docs-audit clean: no broken xref

## 9. Stage 4 整体验收 gate flip

- [ ] 9.1 `docs/roadmap/stage-4-bar-ioremap.md` §"Stage 4 整体验收" §④ [ ] → [x]
- [ ] 9.2 Baseline absolute comparison methodology noted in §④ (vs original "Stage 3 堆模型回退")
- [ ] 9.3 `docs/architecture/stage4-gpu-cp-completion-gap-analysis.md` §"6 总结"：基础设施差距 (perf gating 建立) 移除

## 10. Archive sync

- [ ] 10.1 `openspec archive 2026-08-03-stage4-port-bar-perf-baseline`
- [ ] 10.2 `openspec/changes/INDEX.md` 同步登记
- [ ] 10.3 PROJECTS state — rddf session 中 stage_arch / guide-design handoff updated if applicable

---

## 估计工作量

| Phase | Tasks | 估计 |
|-------|-------|------|
| Benchmark file (T1-T3) | 1.1-3.4 | 2-4 hrs |
| Baseline 测量 (T4) | 4.1-4.4 | 1-2 hrs (host machine) |
| CI workflow (T5-T6) | 5.1-6.3 | 3-5 hrs |
| Docs (T7) | 7.1-7.4 | 1-2 hrs |
| Validation (T8) | 8.1-8.4 | 2-3 hrs |
| Stage 4 gate flip (T9) | 9.1-9.3 | 30 min |
| Archive (T10) | 10.1-10.3 | 20 min |
| **Total** | | **10-17 hrs (1-2 sessions)** |
