# Proposal: Stage 4 BAR Performance Baseline + CI Gating

> **OpenSpec change**: 2026-08-03-stage4-port-bar-perf-baseline
> **Trigger**: Stage 4 整体验收 + roadmap.md Stage 5 触发条件
> **Owner**: UsrLinuxEmu Architecture Team
> **关联 ADR**: ADR-072 §L3 (CI gating) + Stage 4 整体验收 §"性能基准"
> **关联 roadmap**: docs/roadmap/stage-4-bar-ioremap.md §"Stage 4 整体验收" 第 4 项

---

## Why

Stage 4 roadmap §"Stage 4 整体验收" 列出 4 项 gate，其中第 4 项为：

> [ ] 性能基准：BAR 访问（`readl`/`writel`）延迟 vs Stage 3 堆模型回退 ≤ 20%

Stage 4.1 实施已交付 BAR-backed ioremap（ADR-069 + ADR-073），但**没有建立 perf baseline + CI gating**。本 change 设计与建立:

1. Catch2 BENCHMARK 测试覆盖 BAR ioremap path（readl/writel 延迟）
2. 首次 baseline 测量 + 文档化
3. CI gating harness：每次 BAR 相关 PR 跑 perf test，回归 > 20% 阻断

## What Changes

| 范围 | 内容 |
|------|------|
| `tests/perf/test_bar_perf_standalone.cpp` | 新建 Catch2 BENCHMARK 测试 |
| `tests/perf/CMakeLists.txt` (或并入 `tests/CMakeLists.txt`) | +1 benchmark binary 注册 |
| `docs/04-building/perf-baseline-2026-Q3.md` (扩展) | BAR-specific baseline 数据 |
| `.github/workflows/perf-regression.yml` | 性能回归 CI gating（independent of L2, parallel） |

**Non-scope**：
- ❌ 修改 sim/drv/HAL 实现（已实现，change 是 test-only）
- ❌ perf 优化（performance 优化是后续单独 change）
- ❌ 已有 pushbuffer / BO / ioctl perf test（已在 `tests/perf/` Stage 3 设立）

## Acceptance Criteria

- [ ] `tests/perf/test_bar_perf_standalone.cpp` 创建：Catch2 BENCHMARK 框架 + ≥3 benchmarks
  - [ ] `BAR_readl_latency`: BAR0 register read latency (ns)
  - [ ] `BAR_writel_latency`: BAR0 register write latency (ns)
  - [ ] `BAR_dma_alloc_coherent_latency`: DMA coherent alloc round-trip (ns)
  - [ ] (Optional) `BAR_ioremap_unmap_cycle`: mmap + ioremap + unmap cycle
- [ ] `tests/CMakeLists.txt` 注册 `test_bar_perf_standalone`
- [ ] `./build/bin/test_bar_perf_standalone --reporter=console` 运行：3+ benchmarks 输出 median/mean/stddev
- [ ] 首次 baseline 测量（host: ubuntu-22.04, kernel 6.x, x86_64, no turbo）：3 metrics 数值 baseline 文档化
- [ ] `docs/04-building/perf-baseline-bar-2026-Q3.md` 创建：每个 metric 的 baseline + 设备 + CI 规则
- [ ] `.github/workflows/perf-regression.yml` 创建：每次 `plugins/gpu_driver/drv/**` 或 `include/**` 变更触发 perf job
- [ ] CI gating rule：current > baseline × 1.20 → ❌ 阻断 PR merge
- [ ] Stage 4 整体验收 §④ 翻 [x]（当 baseline + CI 通过）

## Risks

| 风险 | 概率 | 缓解 |
|------|------|------|
| 性能在 CI runner 与开发机差异大导致 CI 假阳性 | 高 | CI runner 必须使用 dedicated perf runner (label: `perf-runner`)；baseline 测量需在同型号 runner 上多次取 median |
| 20% 阈值过于严格导致 PR 频繁受阻 | 中 | 阈值可分阶段（首次 30%, 收紧至 20%）；baseline 数据集用 P95 而非 P50 |
| BENCHMARK 结果输出格式变化导致 CI parsing 失败 | 低 | 用 Catch2 --reporter=json 输出 + jq 解析 |

## Linked ADRs / docs

- ADR-072（驱动可移植性 L1/L2/L3 — L3 performance gating）
- ADR-069（BAR/ioremap 仿真架构 — Stage 4.1 实施 ADR）
- ADR-073（DMA coherent 仿真架构 — Stage 4.1 实施 ADR）
- ADR-064（内存模型分阶段策略 — Stage 4 trigger D3）
- docs/roadmap/stage-4-bar-ioremap.md §"Stage 4 整体验收" §④
- docs/04-building/perf-baseline-2026-Q3.md（C-10 perf-bench 既有 baseline）
- tests/perf/ (Stage 3 perf baseline location)
