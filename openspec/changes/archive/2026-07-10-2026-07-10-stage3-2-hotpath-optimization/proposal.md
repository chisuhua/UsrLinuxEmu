# Change: stage3-2-hotpath-optimization

> **状态**: 📋 PROPOSED
> **优先级**: 🔵 P3
> **创建**: 2026-07-10（原 2026-08-01，目录日期已修正）
> **来源**: Issue #24 §3.2
> **依赖**: C-10 `stage3-2-perf-bench-baseline` ✅（commit `d63da5e`）
> **工作目录**: `openspec/changes/2026-07-10-stage3-2-hotpath-optimization/`
> **基线文档**: [`docs/04-building/perf-baseline-2026-Q3.md`](../../docs/04-building/perf-baseline-2026-Q3.md)

## Why

Stage 3.2 性能目标 —— **采纳 C-10 baseline 文档中的 adjusted targets**：

| metric | baseline（C-10，commit `d63da5e`） | C-11 目标 | 待优化空间 |
|--------|----------------------------------|---------|-----------|
| ioctl mean | 0.51 μs | **< 0.20 μs** | 2.5× 加速 |
| pushbuffer sustained throughput | 100 submits/sec | **≥ 500 submits/sec** | 5× 加速 |
| BO ALLOC+FREE mean | 0.76 μs | **< 0.30 μs** | 2.5× 加速 |

> **注意**：原 Proposal 草案中 `ioctl < 100ns / pushbuffer ≥ 100K ops/sec / mmap overhead < 5%` 目标针对**真机 GPU driver**，与 userspace 模拟层（VFS + plugin loader + handler dispatch）的物理极限不可调和。C-10 baseline 文档 §"Stage 3 性能目标（来自 Issue #24 §3.2）" 显式承认此点（`perf-baseline-2026-Q3.md:84-88`），并给出上述可达对比项。**原目标仅作为 aspirational reference**，不作为 acceptance 判定。

### 切入点（基于代码审查 + callgrind 预分析）

1. **`handleGetDeviceInfo` 中 `std::cout`（`gpgpu_device.cpp:180-182`）每次调用均同步 I/O**，几乎肯定是 0.51μs baseline 的主要贡献者
2. **`HandleManager::allocate()` 线性扫描 65535 handle（`gpgpu_device.cpp:64-72`，`gpgpu_device.h:57-66`）** — O(n) worst case
3. **`bo_map_` 用 `std::map<u32, BoInfo>`（`gpgpu_device.h:74`）** — O(log n) find/insert/erase
4. **`ioctl()` 派发表 32-entry 线性扫描（`gpgpu_device.cpp:128-137`）** — 平均 16 比较
5. **`pushbuffer_bench.cpp:122-123` 用 `sleep_until` 限速到 100/sec** — bench 自身瓶颈阻碍真实 max throughput 测量
6. **VFS lookup 已是 O(1)**（`devices_` 是 `std::unordered_map`，见 `vfs.cpp:47-50` + `vfs.h:37`）——**无需优化，提案中"VFS hash lookup"候选已裁剪**

## What Changes

### 优化项（基于代码审查真实瓶颈，优先级 = 实现顺序）

#### 1. **ioctl dispatch 派发表 → sorted array + binary search**（`gpgpu_device.cpp:128-137`）

- **当前**：`IoctlEntry[]` 32-entry 线性扫描
- **改为**：sorted array + `std::lower_bound`（O(log 32) ≈ 5 比较，GCC `-O3` 可自动生成 jump table）
- **Scope**：② driver（`gpgpu_device.h:IoctlEntry` struct + `gpgpu_device.cpp:ioctl()` 成员函数）
- **Boundary 风险**：低，纯 ② 内部改造，不碰 ① kernel SHARED / ③ sim / HAL
- **ROI**：低-中（dispatch 本身可能只占 ~10-20ns，cout 占大头；但仍做，因为是 trivial cleanup）

#### 2. **HandleManager::allocate → free-bitmap**（`gpgpu_device.cpp:64-72` + `gpgpu_device.h:57-66`）

- **当前**：`std::map<u32, bool> handles_` 线性扫描 `1 → 65535` 查找空闲 handle
- **改为**：`std::bitset<65536>` free-bitmap + `std::countr_zero()` 找第一个 0 bit
- **API 兼容**：`allocate()` / `release()` / `is_valid()` / `lookup()` 签名不变
- **Scope**：② driver
- **Boundary 风险**：低，纯 ② 内部数据结构
- **ROI**：高（HandleManager 占 BO ALLOC+FREE 路径 20-40% 估值）
- **Stop condition**：若 benchmark 显示 HandleManager::allocate 在 BO ALLOC 路径占比 < 10%，可退化为 no-op 标注

#### 3. **bo_map_ `std::map → std::unordered_map`**（`gpgpu_device.h:74`）

- **当前**：`std::map<u32, BoInfo>` O(log n) find/insert/erase
- **改为**：`std::unordered_map<u32, BoInfo>` O(1) 平均
- **Scope**：② driver（声明 + 3 个使用处：`handleAllocBo / handleFreeBo / handleMapBo`）
- **Boundary 风险**：极低，trivial 容器替换
- **ROI**：低-中（n=活跃 BO 数通常 < 100，log n ≈ 7 比较 → ~3-4 比较），零成本

#### 4. **`handleGetDeviceInfo` 移除 `std::cout`**（`gpgpu_device.cpp:180-182`）

- **当前**：每次调用 `std::cout << info.marketing_name << ...` 同步 I/O
- **改为**：`#ifndef NDEBUG` 包裹 stdout 日志 + 后续迁移到 `Logger::debug()`（已通过 `kernel/logger.h` 链接，SHARED 库已就绪）
- **Scope**：② driver + 30+ 处 `std::cout`/`std::cerr`（按行为分批替换）
- **Boundary 风险**：低，但需确认 Logger 本身在 Release build 中不也是同步 I/O（否则只是换不同步 I/O 的实现）
- **ROI**：**最高**（预计单此项可能直接使 ioctl mean 从 0.51μs → 0.10-0.15μs，达到 ioctl target）

#### 5. **pushbuffer_bench 解除 rate-limit**（`tests/perf/pushbuffer_bench.cpp:122-123`）

- **当前**：`std::this_thread::sleep_until(next)` 硬限 100/sec（baseline 数值被该 sleep 锁住，无法测真实 max throughput）
- **改为**：去掉 sleep_until，新增 "max throughput" variant（1000 次连续提交，无 sleep），保留旧版作为 "constant-rate" 测试
- **Scope**：tests/perf/（非产品代码，仅 benchmark 工具）
- **Boundary 风险**：无
- **ROI**：中（若解除限速后实测 already ≥500/s，证明 pushbuffer 无需代码优化只是 bench bug）

### 候选优化（已显式裁剪）

- ❌ **VFS lookup hash** — VFS 已是 `std::unordered_map`（`src/kernel/vfs.cpp:47-50`），优化 = no-op
- ⏸ **BO thread-local cache** — 提案原计划放在 `libgpu_core/`，但该库合约禁止（`libgpu_core/include/gpu_buddy.h:4-5` "不进行任何内存分配，不自加锁"）。**延后**到 `C-11b`，触发条件见 Rule 3

## Acceptance

### 验收条件（机器可检查）

- [ ] **3 个 adjusted targets 中 ≥ 2 个达标**（Rule 1）：
  - [ ] ioctl mean **< 0.20 μs**（来源：`ioctl_dispatch_bench_standalone` 1000-sample report `mean` 行）
  - [ ] pushbuffer sustained throughput **≥ 500 submits/sec**（来源：解除限速后的 max throughput 报告）
  - [ ] BO ALLOC+FREE mean **< 0.30 μs**（来源：`mmap_overhead_bench_standalone` BO report `mean` 行）
- [ ] **无功能 regression**：86/86 ctest PASS（与 C-10 归档态一致）
- [ ] **每个优化项独立 commit**，对应 benchmark 数字记录到 [`docs/04-building/perf-baseline-2026-Q3.md`](../../docs/04-building/perf-baseline-2026-Q3.md) §C-11 Results 段
- [ ] **No-op 优化处理**：若实施后改善 < 5%，按 Rule 2 标注但不 revert

### 记录方式（Rule 1 + Rule 2 应用）

在 baseline 文档追加：
```markdown
## C-11 Results (2026-07-XX)

| metric | C-10 baseline | post-C-11 | target | hit? |
|--------|---------------|-----------|--------|------|
| ioctl mean | 0.51 μs | X.XX μs | < 0.20 μs | ✅/❌ |
| pushbuffer | 100/s (限速) | XXX/s | ≥ 500/s | ✅/❌ |
| BO ALLOC+FREE | 0.76 μs | X.XX μs | < 0.30 μs | ✅/❌ |

**Acceptance**: X/3 hit (≥2 required) → ✅ PASS / ❌ FAIL
```

## 测试方法

> **重要**：所有 benchmark binary 名带 `_standalone` 后缀（`tests/perf/CMakeLists.txt:42` `string(APPEND benchname "_standalone")`）。必须从项目根目录运行。

```bash
cd /workspace/project/UsrLinuxEmu

# 1. 构建（含 perf tests）
mkdir -p build && cd build
cmake -DUSR_LINUX_EMU_PERF_TESTS=ON -DCMAKE_BUILD_TYPE=Release ..
make -j4

# 2. 跑功能回归（必须从项目根目录）
cd ..
cd build && ctest --output-on-failure && cd ..

# 3. 跑 perf benchmarks
./build/bin/ioctl_dispatch_bench_standalone
./build/bin/pushbuffer_bench_standalone       # 含解除 rate-limit 后的 max-throughput variant
./build/bin/mmap_overhead_bench_standalone    # BO ALLOC+FREE round-trip

# 4. 收集数字到 docs/04-building/perf-baseline-2026-Q3.md §C-11 Results
```

## Decision Rules

详见 tasks.md §0 节，关键规则：

- **Rule 1** "≥2/3 hit" 计算方式
- **Rule 2** No-op 优化处理（commit message + tasks.md 标注，但不 revert）
- **Rule 3** BO cache 毕业条件（callgrind 显示 buddy_alloc > 40% 时间 或 其他优化后 BO 仍 > 0.50μs）
- **Rule 4** Atomic commit boundary（一个函数行为变更 + benchmark 数字 = 一个 commit）

## Cross-Repo 影响

无（仅 UsrLinuxEmu 内部优化）。`kernel` SHARED 库不变；HAL ABI 不变；libgpu_core 合约不变。

## Dependencies

- ✅ **C-10** `stage3-2-perf-bench-baseline`（commit `d63da5e`）—— 已归档，提供 baseline 数字

## Unblocks

- 无（Stage 3.2 交付项，无下游消费者）
- C-12 `stage1-4-kfd-multi-file-integration`（sub-project，独立）

## 相关 ADR / 参考

- ADR-010（GTest 迁移 → Catch2 only）
- ADR-020（libgpu_core 纯 C 无锁零分配合约）
- ADR-036（三区分架构 + HAL 桥）
- [`docs/02_architecture/post-refactor-architecture.md`](../../docs/02_architecture/post-refactor-architecture.md) §1.5（目录演进）
- [`docs/04-building/perf-baseline-2026-Q3.md`](../../docs/04-building/perf-baseline-2026-Q3.md)（C-10 baseline 报告，权威数字来源）
