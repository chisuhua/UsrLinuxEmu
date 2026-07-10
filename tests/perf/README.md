# tests/perf/ — Performance Benchmarks (Stage 3.2 baseline, C-10)

> **状态**: ✅ ACTIVE（2026-07-10，PR ready）
> **依赖**: `USR_LINUX_EMU_PERF_TESTS=ON` CMake option（默认 OFF）
> **基准测量工具**: Catch2 v3.7.0 `BENCHMARK` macro（已 vendor，无需引入新依赖）
> **关联 change**: `openspec/changes/2026-07-22-stage3-2-perf-bench-baseline`

## 目的

为 Stage 3 v1.0 稳定目标提供**性能基线**，解开 `stage3-2-hotpath-optimization` (C-11) 决策依赖。

- **ioctl 派发延迟**（baseline: ~500 ns/op）
- **pushbuffer submit 吞吐**（baseline: 100 submits/sec 可达成）
- **mmap / BO 分配开销**（对照裸 syscall 的用户态模拟开销）

## 三个 Bench

| 文件 | 测量对象 | 关键 metric |
|------|----------|-------------|
| `ioctl_dispatch_bench.cpp` | `GpgpuDevice::ioctl → kTable[GET_DEVICE_INFO]` | mean / p50 / p99 / p999 latency (μs) |
| `pushbuffer_bench.cpp` | `GpgpuDevice::ioctl → handlePushbufferSubmitBatch → q->submit` | single latency + 100 submits/sec sustained throughput |
| `mmap_overhead_bench.cpp` | 裸 `mmap` vs `GPU_IOCTL_MAP_BO + ALLOC/FREE` round-trip | BO ALLOC+FREE 1MB mean + p99 latency |

## 启动方式

```bash
# 项目根目录
mkdir -p build && cd build
cmake -DUSR_LINUX_EMU_PERF_TESTS=ON -DCMAKE_BUILD_TYPE=Release ..
make perf-tests -j4
cd ..

# 单跑一个 bench
./build/bin/ioctl_dispatch_bench_standalone

# 一键全跑（make target）
make -C build run-perf
```

## 设计要点

- **不污染 ctest**：`add_subdirectory(perf)` 仅在 `USR_LINUX_EMU_PERF_TESTS=ON` 时生效；常规 `ctest` 仍 86/86 PASS
- **共享 fixture**：`perf_fixture.h` 提供 `GpuPerfFixture`（对齐 `tests/test_gpu_plugin.cpp` 的 `GpuPluginTestFixture`）
- **单 entry pushbuffer**：避免 ring buffer overflow 让 throughput 测试稳定
- **CI 非阻塞**：见 `.github/workflows/perf-nightly.yml`（每周日 02:00 UTC + 手动 trigger，artifacts 上传 90 天）

## 已知 caveat

- `pushbuffer_bench` 当前用单 entry 测量（`GPU_OP_LAUNCH_KERNEL`），避免 ring buffer 满导致 -ENOENT
  - 真实 driver 中 multi-entry batch 更常见；后续可加 `count=16` 变体 baseline
- BO benchmark 当前只测 `ALLOC + FREE`，未含 `MAP` + 真实写入（`MAP_BO` 需要 user-VA，userspace 模拟未支持 multi-VM mmap）
- 测量均在 **Release** build；Debug build 数字不可比（5-10× 慢）

## 后续（C-11 hotpath-opt）

拿到 baseline 后，C-11 决定优化目标：

| metric | baseline (2026-07-10) | 目标 |
|--------|---------------------|------|
| ioctl mean | 0.51 μs | < 0.20 μs（追 5x 加速） |
| pushbuffer throughput | 100 submits/sec | ≥ 500 submits/sec |
| BO ALLOC+FREE mean | 0.76 μs | < 0.30 μs（追 2.5× 加速） |

具体路线（hash lookup / dispatch perfect hash / mmap overlay）由 C-11 决定。
