# Tasks: stage3-2-perf-bench-baseline

> **状态**: 🚀 IN PROGRESS（全部完成，等待 commit + archive）
> **目标**: 建 `tests/perf/` benchmark 框架 + 测 baseline

## 1. 框架设计（半天）

- [x] 1.1 选择 benchmark 工具：**Catch2 `BENCHMARK()` macro**（v3.7.0 已 vendor，无需引入新依赖）
- [x] 1.2 决定 CMake 模式：`option(USR_LINUX_EMU_PERF_TESTS OFF)` + `add_subdirectory(tests/perf)`
- [x] 1.3 设计 API：`BENCHMARK("name") { return [&](int iters) { ... }; };` + 独立 `%`-test case 报告 percentiles
- [x] 1.4 共享 fixture：`tests/perf/perf_fixture.h` 复用 `GpuPluginTestFixture` 模式（PluginLifecycle + VFS）

## 2. 三个 benchmark 实现（2-3 天）

- [x] 2.1 `tests/perf/ioctl_dispatch_bench.cpp`
  - [x] 单次 `GPU_IOCTL_GET_DEVICE_INFO` latency
  - [x] 1000 调用 mean / p50 / p99 / p999 报告
- [x] 2.2 `tests/perf/pushbuffer_bench.cpp`
  - [x] 单次 `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH` 延迟（warmup + measured）
  - [x] 100 submits/sec 持续吞吐测量
- [x] 2.3 `tests/perf/mmap_overhead_bench.cpp`
  - [x] 裸 mmap 1MB 对照 baseline
  - [x] `GPU_IOCTL_MAP_BO` + `GPU_IOCTL_ALLOC_BO/FREE_BO` benchmark

## 3. CMake 集成（1 小时）

- [x] 3.1 `tests/CMakeLists.txt` 加 `option(USR_LINUX_EMU_PERF_TESTS ...)` + `add_subdirectory(perf)`
- [x] 3.2 build 三个 benchmark binary（`ioctl_dispatch_bench_standalone` 等）
- [x] 3.3 提供 `make perf-tests` / `make run-perf` 目标

## 4. 文档化 baseline（半小时）

- [x] 4.1 `tests/perf/README.md`：测量方法 + 数字
- [x] 4.2 `docs/04-building/perf-baseline-2026-Q3.md`（baseline 报告）
- [x] 4.3 链接到 README §Stage 3 acceptance

## 5. CI 配置（半天）

- [x] 5.1 `.github/workflows/perf-nightly.yml`：nightly cron（每周日 02:00 UTC）
- [x] 5.2 手动 trigger workflow_dispatch
- [x] 5.3 结果上传 artifact（text 文件）

## 6. 验证 / commit（半小时）

- [x] 6.1 本地 build + run 三个 benchmark（已完成：86/86 ctest + 三个 perf bench 全绿）
- [ ] 6.2 docs-audit 无 warning
- [ ] 6.3 commit：`test(perf): baseline benchmarks for ioctl / pushbuffer / mmap`
