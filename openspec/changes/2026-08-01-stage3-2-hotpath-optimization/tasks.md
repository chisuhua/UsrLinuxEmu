# Tasks: stage3-2-hotpath-optimization

> **状态**: 📋 PROPOSED
> **目标**: 基于 C-10 baseline 实施 hot path 优化

## 1. Baseline 分析（半天）

- [ ] 1.1 跑 C-10 benchmarks 获取 baseline 数字
- [ ] 1.2 识别 hot path（用 perf / callgrind）
- [ ] 1.3 决定哪些优化在 scope（基于 ROI）

## 2. 优化实施（每个优化 1-3 天）

- [ ] 2.1 VFS hash lookup
  - [ ] Design hash function（likely path-bucketed）
  - [ ] 实现 + 单元测试
  - [ ] benchmark 测改善
- [ ] 2.2 ioctl dispatch perfect hash
  - [ ] 生成 hash function
  - [ ] 重构 GpgpuDevice::ioctl 派发
  - [ ] benchmark
- [ ] 2.3 BO alloc thread-local cache
  - [ ] cache hit/miss 测量
  - [ ] cache invalidation on pool shrink

## 3. 验证 / commit（每个优化后）

- [ ] 3.1 ctest 100% PASS
- [ ] 3.2 benchmarks 改善（target ≥ 2/3 hit）
- [ ] 3.3 记录数字到 `tests/perf/HISTORY.md`
- [ ] 3.4 atomic commit per optimization（避免巨型 commit）

## 4. 文档化（1 天）

- [ ] 4.1 `docs/05-advanced/performance-tuning.md` 更新
- [ ] 4.2 设计决策 ADR（如有架构性改动）
- [ ] 4.3 README badges 更新（如有 perf-related）
