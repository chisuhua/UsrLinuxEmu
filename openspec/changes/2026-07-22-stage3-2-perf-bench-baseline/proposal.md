# Change: stage3-2-perf-bench-baseline

> **状态**: 📋 PROPOSED
> **优先级**: 🔵 P3
> **创建**: 2026-07-22
> **来源**: Issue #24 §3.2（Stage 3.2 性能基线）
> **依赖**: 无（独立）
> **工作目录**: `openspec/changes/2026-07-22-stage3-2-perf-bench-baseline/`

## Why

Stage 3.2 要求性能目标：
- ioctl 派发延迟 < 100ns
- pushbuffer 提交吞吐 ≥ 100K ops/sec
- mmap 共享开销 < 5%

但**当前没有任何 benchmark 框架**。需要先建 + 测量 baseline，然后才有优化依据。

## What Changes

### 1. 新建 `tests/perf/` 模块

- `tests/perf/ioctl_dispatch_bench.cpp` — ioctl 派发延迟
- `tests/perf/pushbuffer_bench.cpp` — pushbuffer 提交吞吐
- `tests/perf/mmap_overhead_bench.cpp` — mmap 共享开销

### 2. CMake 集成

非阻塞 CI gating（用 Catch2 benchmark 选项 + 单独 target）：

```cmake
# tests/CMakeLists.txt 加 option
option(USR_LINUX_EMU_PERF_TESTS "Build performance benchmarks" OFF)
if(USR_LINUX_EMU_PERF_TESTS)
    add_executable(ioctl_dispatch_bench perf/ioctl_dispatch_bench.cpp)
    ...
endif()
```

### 3. 文档化 baseline

`tests/perf/README.md` 记录：
- 测量方法（输入 + 输出）
- 当前 baseline 数字
- 历史 trend（如后续 re-run）

## Acceptance

- [ ] 3 个 benchmark binary 构建成功
- [ ] Local run 输出 baseline numbers
- [ ] docs 记录数字
- [ ] CI 非阻塞（手动触发 or nightly）

## 测试方法

```bash
cd build
cmake -DUSR_LINUX_EMU_PERF_TESTS=ON ..
make
./bin/ioctl_dispatch_bench
./bin/pushbuffer_bench
./bin/mmap_overhead_bench
```

## Cross-Repo 影响

无（仅 UsrLinuxEmu 内部）

## Dependencies

无（独立）

## Unblocks

- C-11 stage3-2-hotpath-optimization（基于 baseline 决定优化目标）
