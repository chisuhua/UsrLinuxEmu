# Change: stage3-2-hotpath-optimization

> **状态**: 📋 PROPOSED
> **优先级**: 🔵 P3
> **创建**: 2026-08-01
> **来源**: Issue #24 §3.2（依赖 C-10 测量结果）
> **依赖**: C-10 stage3-2-perf-bench-baseline
> **工作目录**: `openspec/changes/2026-08-01-stage3-2-hotpath-optimization/`

## Why

Stage 3.2 性能目标（基于 C-10 baseline 决定）：
- ioctl 派发 < 100ns（baseline 测量后定）
- pushbuffer ≥ 100K ops/sec
- mmap overhead < 5%

## What Changes

### 候选优化（基于 baseline 后定具体）

1. **VFS lookup**（`src/kernel/vfs.cpp`）：
   - 当前可能 O(n) linear scan
   - 改为 hash index

2. **ioctl dispatch**（`gpgpu_device.cpp`）：
   - 当前用 `IoctlEntry[]` + linear search
   - 改为 perfect hash 或 switch

3. **BO 分配路径**（`gpu_buddy_alloc`）：
   - 当前 buddy 分配
   - 加 thread-local cache

## Acceptance

- [ ] 性能目标 hit ≥ 2/3
- [ ] 无功能 regression
- [ ] ctest 100% PASS
- [ ] benchmark 数字记录到 `tests/perf/HISTORY.md`

## 测试方法

```bash
cd build
cmake -DUSR_LINUX_EMU_PERF_TESTS=ON ..
make
./bin/ioctl_dispatch_bench    # 测延迟
./bin/pushbuffer_bench        # 测吞吐
ctest                          # 功能回归
```

## Cross-Repo 影响

无（仅 UsrLinuxEmu 内部优化）

## Dependencies

- **C-10** stage3-2-perf-bench-baseline（先测 baseline）
