# BAR Performance Baseline — Stage 4 整体验收 §④

## ADDED Requirements

### Requirement: BAR register read latency benchmark

A standalone benchmark binary `test_bar_perf_standalone` SHALL provide a Catch2 BENCHMARK wrapper around `hal_register_read` exercising the BAR0 MMIO read path.

#### Scenario: BAR_readl emits median latency

- **GIVEN** `gpu_hal_ops *hal` references BAR0 mock fn-ptrs (per ADR-069 + ADR-073 emulation)
- **WHEN** the test invokes `BENCHMARK("BAR_readl")` returning `hal_register_read(hal, offset, &val)`
- **THEN** Catch2 SHALL report a median latency in nanoseconds
- **AND** the median SHALL be ≤ 120% of the recorded baseline

#### Scenario: BAR_readl baseline recorded

- **GIVEN** the first successful benchmark run on a fixed `perf-runner` host
- **WHEN** baseline measurement completes
- **THEN** `.perf-baseline.json` SHALL contain `{BAR_readl: {median: <ns>, mean: <ns>, stddev: <ns>}}`
- **AND** `docs/04-building/perf-baseline-bar-2026-Q3.md` SHALL document the baseline

### Requirement: BAR register write latency benchmark

The benchmark binary SHALL include a `BAR_writel` benchmark analogous to the read benchmark.

#### Scenario: BAR_writel emits median latency

- **GIVEN** `gpu_hal_ops *hal` references BAR0 mock fn-ptrs
- **WHEN** `BENCHMARK("BAR_writel")` runs
- **THEN** median latency SHALL be reported + ≤ 120% of baseline
- **THEN** no observable side effects beyond expected register state changes

### Requirement: DMA coherent allocation latency benchmark

The benchmark binary SHALL include a `dma_coherent_alloc_free` benchmark exercising the Stage 4.1 DMA allocation round-trip path.

#### Scenario: DMA alloc latency reported

- **GIVEN** `dma_alloc_coherent()` and `dma_free_coherent()` callable
- **WHEN** `BENCHMARK("dma_coherent_alloc_free")` runs a 4096-byte alloc + free cycle
- **THEN** median latency SHALL be reported + ≤ 120% of baseline

### Requirement: CI perf-regression workflow

A GitHub Actions workflow `.github/workflows/perf-regression.yml` SHALL trigger on PRs touching BAR-relevant paths and fail the merge if performance regresses beyond 20%.

#### Scenario: PR triggers perf CI

- **GIVEN** a PR modifies `plugins/gpu_driver/drv/**` or `plugins/gpu_driver/sim/**` or `include/kernel/**`
- **WHEN** PR is opened or synchronized
- **THEN** GitHub Actions SHALL schedule the `perf-regression` job
- **AND** the job SHALL run on a dedicated runner (`perf-runner` label)
- **AND** the job SHALL build + execute `test_bar_perf_standalone --reporter=json`

#### Scenario: 20% regression blocks PR merge

- **GIVEN** the benchmark median for any metric exceeds `.perf-baseline.json[metric].median * 1.20`
- **WHEN** the CI gating step runs
- **THEN** `core.setFailed(...)` SHALL fire with the specific metric + measured value
- **AND** the PR SHALL be blocked from merge by required status check

#### Scenario: First-time baseline update

- **GIVEN** the perf baseline has never been recorded (`.perf-baseline.json` missing or empty)
- **WHEN** the CI job runs
- **THEN** the job SHALL write `.perf-baseline.json` to the working tree
- **THEN** the PR SHALL pass with informational note "perf baseline initialized"

### Requirement: Stage 4 整体验收 §④ gate

Upon successful baseline + CI gating, the Stage 4 acceptance gate §④ SHALL be flipped to [x].

#### Scenario: L4 gate closure

- **GIVEN** L4 baseline + CI workflow established + first PR's perf CI run completes
- **WHEN** the closeout review accepts the baseline
- **THEN** `docs/roadmap/stage-4-bar-ioremap.md` §"Stage 4 整体验收" §④ SHALL flip from [ ] to [x]
- **AND** the `docs/architecture/stage4-gpu-cp-completion-gap-analysis.md` §"6 总结" SHALL reflect removed infrastructure gap
- **AND** `openspec/changes/INDEX.md` SHALL record the L4 milestone

### Requirement: Reproducible benchmark environment

The benchmark SHALL provide a helper function `disable_perf_variability()` that mitigates common sources of measurement noise.

#### Scenario: turbo + ASLR disabled

- **WHEN** `disable_perf_variability()` runs at benchmark setup
- **THEN** turbo boost SHALL be disabled (where supported)
- **THEN** ASLR SHALL be disabled
- **AND** the benchmark thread SHALL be pinned to a dedicated CPU core
- **AND** warm-up iterations SHALL be discarded before measurement

#### Scenario: baseline methodology documented

- **GIVEN** a baseline measurement session
- **THEN** `docs/04-building/perf-baseline-bar-2026-Q3.md` SHALL document:
  - CPU model + `performance` governor state
  - Turbo state (disabled)
  - ASLR state (disabled)
  - Warm-up iterations (≥100)
  - Main measurement iterations (≥1000)
  - Cycles (≥3) for stability
