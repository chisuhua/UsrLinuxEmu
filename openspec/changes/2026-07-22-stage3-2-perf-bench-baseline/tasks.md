# Tasks: stage3-2-perf-bench-baseline

> **状态**: 📋 PROPOSED
> **目标**: 建 `tests/perf/` benchmark 框架 + 测 baseline

## 1. 框架设计（半天）

- [ ] 1.1 选择 benchmark 工具（Google Benchmark / 自实现 Catch2 BENCHMARK）
- [ ] 1.2 决定 CMake 模式（option-gated, OFF by default）
- [ ] 1.3 设计 API（BENCHMARK(ioctl_dispatch_dispatch, iters)）

## 2. 三个 benchmark 实现（2-3 天）

- [ ] 2.1 `tests/perf/ioctl_dispatch_bench.cpp`
  - [ ] 单次 ioctl 调用 latency
  - [ ] 1000 调用总耗时
  - [ ] 平均 / p99 / p999 报告
- [ ] 2.2 `tests/perf/pushbuffer_bench.cpp`
  - [ ] 单 pushbuffer submit 延迟
  - [ ] 100 submits/sec 持续吞吐
- [ ] 2.3 `tests/perf/mmap_overhead_bench.cpp`
  - [ ] mmap 1MB latency
  - [ ] mmap + read + munmap 总开销

## 3. CMake 集成（1 小时）

- [ ] 3.1 `tests/CMakeLists.txt` 加 option + if block
- [ ] 3.2 build 三个 benchmark binary
- [ ] 3.3 提供 `make perf-tests` / `make all-perf` 目标

## 4. 文档化 baseline（半小时）

- [ ] 4.1 `tests/perf/README.md`：测量方法 + 数字
- [ ] 4.2 在 `docs/05-advanced/` 加 perf baseline 报告
- [ ] 4.3 链接到 Issue #24 §3.2 acceptance criteria

## 5. CI 配置（半天）

- [ ] 5.1 nightly cron workflow（每周跑一次）
- [ ] 5.2 或手动 trigger workflow_dispatch
- [ ] 5.3 结果上传 artifact（text 文件）

## 6. 验证 / commit（半小时）

- [ ] 6.1 本地 build + run 三个 benchmark
- [ ] 6.2 docs-audit 无 warning
- [ ] 6.3 commit：`test(perf): baseline benchmarks for ioctl / pushbuffer / mmap`
