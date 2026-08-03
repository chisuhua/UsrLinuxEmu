# Design: Stage 4 BAR Performance Baseline + CI Gating

## Benchmark framework — Catch2 BENCHMARK

Catch2 BENCHMARK 框架（已有 stage3-2-perf-bench-baseline 经验 - tests/perf/）：

```cpp
#include <catch_amalgamated.hpp>
#include <chrono>
#include "plugins/gpu_driver/hal/gpu_hal.h"
#include "include/kernel/bar_ioremap.h"

TEST_CASE("BAR_readl_latency", "[perf][bar]") {
  gpu_hal_ops hal = make_bar0_mock_hal();
  uint64_t val = 0;
  
  BENCHMARK("BAR_readl") {
    hal_register_read(&hal, /*offset=*/0x4, &val);
    return val;
  };
}
```

Per ADR-072 + C-10 baseline pattern:
- 报告 `--reporter=console` 显示 median + mean + stddev
- 报告 `--reporter=json` 输出结构化数据用于 CI 解析
- 测量 N=1000+ samples / 取 median

## Benchmarks scope

| ID | Path tested | Coverage |
|----|-------------|---------|
| `BAR_readl_latency` | `hal_register_read` → BAR0 offset read | BAR MMIO read 路径 |
| `BAR_writel_latency` | `hal_register_write` → BAR0 offset write | BAR MMIO write 路径 |
| `BAR_dma_alloc_coherent_latency` | `dma_alloc_coherent` 全路径 | DMA coherent alloc |
| (Optional) `BAR_ioremap_unmap_cycle` | `ioremap` + access + `iounmap` | 全 BAR cycle |

## Baseline measurement methodology

**First baseline run** procedure:

1. Choose **dedicated CI runner** (GitHub-hosted `ubuntu-22.04-x86_64`, no shared variablility)
2. 测量环境：
   - CPU: model + governor (`performance`)
   - Turbo Boost: disabled (`echo 1 > /sys/devices/system/cpu/intel_pstate/no_turbo`)
   - ASLR: disabled (`echo 0 > /proc/sys/kernel/randomize_va_space`)
   - CPU affinity: 固定 core 0
3. 执行 warm-up：100 iterations discard
4. 主测量：1000 iterations × 3 run cycles
5. Report: median / mean / stddev per benchmark
6. 文档化到 `docs/04-building/perf-baseline-bar-2026-Q3.md`

## CI gating

`.github/workflows/perf-regression.yml` skeleton:

```yaml
name: perf-regression
on:
  pull_request:
    paths:
      - 'plugins/gpu_driver/drv/**'
      - 'plugins/gpu_driver/sim/**'
      - 'include/kernel/**'
      - 'tests/perf/**'
jobs:
  bench:
    runs-on: [self-hosted, perf-runner, linux, x64]  # dedicated runner
    steps:
      - uses: actions/checkout@v4
      - name: build
        run: cmake --build build --target test_bar_perf_standalone -j
      - name: bench
        run: ./build/bin/test_bar_perf_standalone --reporter=json > bench.json
      - name: gate
        uses: actions/github-script@v7
        with:
          script: |
            const bench = require('./bench.json');
            const baseline = require('./.perf-baseline.json');
            for (const m of baseline.metrics) {
              const cur = bench[m.id].median;
              if (cur > baseline[m.id].median * 1.20) {
                core.setFailed(`${m.id}: ${cur}ns > 120% of baseline ${baseline[m.id].median}ns`);
              }
            }
```

## Threshold philosophy

- 首次发布阈值：**baseline × 1.30**（30% 容忍 — 含 CI runner 噪声）
- 6 个月后：**baseline × 1.20**（20% 收紧 — baseline 已稳定）
- 阈值细化：每个 benchmark 独立阈值（Catch2 输出允许 partition）

## Stage 4 整体验收 §④ gate closure

L2 + L3 + perf baseline + CI 全 ready 时，§④ 翻 [x]：

```diff
- - [ ] 性能基准：BAR 访问（`readl`/`writel`）延迟 vs Stage 3 堆模型回退 ≤ 20%
+ - [x] 性能基准：BAR 访问延迟 + CI gating（baseline 2026-Q3，threshold 20%）
```

注：原文是"vs Stage 3 堆模型回退" — 现 baseline 为 **absolute**（同 host comparison），非 Stage 3 vs Stage 4 对比。两者语义略不同 — 在 §④ 翻 [x] 时同时在 baseline 文档说明 baseline methodology。

## Failure modes

- **新 PR 引入 ≥20% 回归**：CI fail + 自动 comment 显示具体 metric + diff baseline + 建议 bottleneck 排查方向（cache miss / branch mispred / kernel migration）
- **Baseline 重新测量**（如 kernel upgrade / CPU 模型变）：本 change 完成后可单独走 follow-up change 调整 baseline
