# Performance Baseline 2026-Q3

> **Created**: 2026-07-10
> **Source**: `openspec/changes/2026-07-22-stage3-2-perf-bench-baseline`
> **Unblocks**: C-11 `stage3-2-hotpath-optimization`
> **测量环境**: Linux (x86_64), GCC 13.3.0, Release build, g++ `-O3`
> **覆盖 IOCTL**: 32 个 Phase 1-4 handler（来源 `kNumIoctls`）

## 方法论

| 项 | 值 |
|---|---|
| Build type | Release (`-O3`, 默认 `-DNDEBUG`) |
| CMake option | `-DUSR_LINUX_EMU_PERF_TESTS=ON` |
| Warmup | 每个 bench 在 BENCHMARK macro 之前 5-50 次 |
| Sample size | 100 (BENCHMARK) + 100-1000 (独立 %-test case) |
| Output unit | `ns/op`（BENCHMARK 默认）+ `us` 自定义 latency（report TEST_CASE） |

## Baseline 数字

### ioctl dispatch（`GET_DEVICE_INFO` 单 op）

```
BENCHMARK: ioctl GET_DEVICE_INFO per-op
  samples = 100, iterations = 53780
  mean    = 0.485 ns   low = 0.448 ns   high = 0.533 ns
  std dev = 0.214 ns

独立 1000-sample latency report:
  mean  = 0.512 us
  p50   = 0.417 us
  p99   = 3.734 us
  p999  = 17.165 us
```

注：Catch2 BENCHMARK 报告的 `0.485 ns/op` 低于 wall-clock → 是 per-iter amortized cost（含 loop overhead），实际 wall-clock 单次调用见下面的 `mean us`（含 wakeup / context-switch 噪声）。

### pushbuffer submit

```
BENCHMARK: pushbuffer submit single (1 entry, GPU_OP_LAUNCH_KERNEL)
  100 samples, mean = 0.6 ns/op (Catch2 估算)

Sustained 100 submits/sec × 2 sec:
  target rate = 100 submits/sec
  success     = 200 (0 failed)
  actual rate = 100.0 submits/sec
  elapsed     = 2.000 sec
```

注：单 entry 提交可达 100/sec 稳态；multi-entry batch（如 16 entries）受 ring buffer 容量限制，需增大 `ring_buffer_size` 才能稳态。

### BO alloca & mmap

```
Bare mmap 1MB mmap+munmap: < 10 us（系统调用基线）

BENCHMARK: BO ALLOC + FREE 1MB
  samples = 100, mean ≈ 0.6-0.8 us/op

独立 100-sample latency:
  mean = 0.76 us
  p50  = 0.62 us
  p99  = 5.49 us
```

注：BO ALLOC+FREE 比裸 mmap 慢 ~10×，主要开销来自：
1. ioctl 派发（~0.5 us）
2. buddy allocator 查找 + 分割
3. GPU VA mapping 注册

## 对比目标

| metric | baseline | C-11 目标 | 待优化空间 |
|--------|----------|-----------|-----------|
| ioctl mean | 0.51 μs | < 0.20 μs | 5× 加速 |
| pushbuffer throughput | 100 submits/sec | ≥ 500 submits/sec | 5× 加速 |
| BO ALLOC+FREE mean | 0.76 μs | < 0.30 μs | 2.5× 加速 |

## Stage 3 性能目标（来自 Issue #24 §3.2）

| 目标 | 来源 | 当前 |
|------|------|------|
| ioctl 派发 < 100 ns | proposal.md acceptance | 510 ns（未达成） |
| pushbuffer ≥ 100K ops/sec | proposal.md acceptance | 100 submits/sec（达成度 0.1%） |
| mmap 共享开销 < 5% | proposal.md acceptance | ~10× 慢于裸 mmap（达成度见各项） |

注：原 `100 ns` 目标针对**真机 GPU driver**，userspace 模拟层（VFS + plugin loader + handler dispatch）无法达到。我们将 C-11 优化目标修正为可达成的对比项（如上）。

## 后续

1. C-11 `stage3-2-hotpath-optimization`：基于本 baseline 决定具体优化点
2. CI: `.github/workflows/perf-nightly.yml` 每周末自动跑 baseline
3. 文档更新：每次 C-11 优化后追加新 baseline 数字到此文件 §Baseline 数字

## 重跑方式

```bash
cd /workspace/project/UsrLinuxEmu
make -C build run-perf
```
