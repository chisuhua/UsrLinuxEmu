# Tasks: stage3-2-hotpath-optimization

> **状态**: 📋 PROPOSED
> **创建**: 2026-07-10（原 2026-08-01，目录已重命名）
> **目标**: 基于 C-10 baseline 实现 hot path 优化（ioctl <0.20μs / pushbuffer ≥500/s / BO <0.30μs，**3 选 ≥2**）
> **基线文档**: [`docs/04-building/perf-baseline-2026-Q3.md`](../../docs/04-building/perf-baseline-2026-Q3.md)（commit `d63da5e`）

---

## 0. Decision Rules（机器可检查的判定规则）

> 这些规则在 §3 验收中引用。先固化以避免歧义。

### Rule 1: "≥2/3 hit" 计算方式

| # | Metric | Target | 判定公式 | 来源 |
|---|--------|--------|---------|------|
| 1 | ioctl mean | **< 0.20 μs** | `mean < 0.20` | `ioctl_dispatch_bench_standalone` 1000-sample report `mean` 行 |
| 2 | pushbuffer throughput | **≥ 500 submits/sec** | `actual_rate >= 500.0` | `pushbuffer_bench_standalone` (解除限速后) |
| 3 | BO ALLOC+FREE mean | **< 0.30 μs** | `mean < 0.30` | `mmap_overhead_bench_standalone` BO report `mean` 行 |

达标数 ≥ 2 → acceptance PASS。记录到 `docs/04-building/perf-baseline-2026-Q3.md` §C-11 Results 表格。

### Rule 2: No-op 优化处理

- **触发**：某优化项实施后 benchmark 改善 < 5% 或在测量噪声范围内
- **处理**：
  1. **不 revert** 代码改动（若改动本身正确，如 `std::map → unordered_map` 是合理重构）
  2. commit message 标注："perf(gpu): X (no measurable Δ, structural cleanup)"
  3. 本 tasks.md 中标 `[~]`（partial / no-op）
  4. **不计数为 hit** —— acceptance 只看数字
- **极端**：所有优化都 no-op → C-11 archive 为 "baseline established, no optimization win"，附 profiling 发现（"瓶颈在别处"）

### Rule 3: BO thread-local cache 毕业条件

- **当前状态**：**Deferred**（C-11 不实施，原提案放 `libgpu_core/` 违反 `gpu_buddy.h:4-5` 纯 C 无锁合约）
- **毕业条件（任一触发）**：
  - **A**: callgrind 显示 `gpu_buddy_alloc` + `gpu_buddy_free` 占 BO ALLOC+FREE 路径 > 40%
  - **B**: HandleManager + bo_map_ + cout 优化后 BO ALLOC+FREE 仍 > 0.50μs
- **触发后**：新建 `C-11b`，cache 放在 HAL 层（`plugins/gpu_driver/hal/`），**禁止碰 libgpu_core**

### Rule 4: Atomic commit boundary

**最小不可分割单元 = 一个函数的行为变更 + 对应 benchmark 数字**。

| 合法 atomic commit | 非法（太粗） | 非法（太细） |
|-------------------|-------------|-------------|
| `HandleManager std::map → bitset` + benchmark | HandleManager + bo_map_ + cout 合一 | 只改 .h 声明不改 .cpp 实现 |

每个 perf commit 必须包含：
1. 代码改动（.h + .cpp）
2. 对应 benchmark 输出片段（commit message body 或 PR 描述）
3. ctest 通过证据（`86/86 PASS`）

---

## 1. Baseline 确认（半天，no-regret）

> 不必重新跑 C-10 数字（已固化在 `perf-baseline-2026-Q3.md` commit `d63da5e`），只需确认现状。

- [ ] 1.1 读 `docs/04-building/perf-baseline-2026-Q3.md` 全文，固化 baseline 数字到 proposal §Why 段
- [ ] 1.2 在 Release 构建下跑 3 个 perf benchmark 一次，sanity check 环境
  ```bash
  cd /workspace/project/UsrLinuxEmu
  cmake -DUSR_LINUX_EMU_PERF_TESTS=ON -DCMAKE_BUILD_TYPE=Release -B build
  cmake --build build --target ioctl_dispatch_bench_standalone pushbuffer_bench_standalone mmap_overhead_bench_standalone
  ./build/bin/ioctl_dispatch_bench_standalone | head -30
  ./build/bin/pushbuffer_bench_standalone | head -30
  ./build/bin/mmap_overhead_bench_standalone | head -30
  ```
- [ ] 1.3 用 callgrind 量化 cout 占比（确认 ROI 假设 #4）
  ```bash
  valgrind --tool=callgrind --callgrind-out-file=build/cg_ioctl.out \
    ./build/bin/ioctl_dispatch_bench_standalone
  callgrind_annotate build/cg_ioctl.out | grep -A2 "std::cout\|operator<<"
  ```
  **预期**：`handleGetDeviceInfo` 中 `std::cout << info.marketing_name << ...` 占显著比例（>30%）
- [ ] 1.4 基于 §1.3 数据，决定 §2.4 vs §2.2 的优先级
  - cout 占比 > 30% → 先做 §2.4，再做其他
  - cout 占比 < 10% → 优先 §2.2 (HandleManager)

---

## 2. 优化实施（每个优化一个 atomic commit）

### ❌ 2.0 VFS hash lookup（已裁剪，不实施）

- **理由**：VFS `devices_` 已是 `std::unordered_map`（`src/kernel/vfs.cpp:47-50`, `vfs.h:37`），优化 = no-op
- **Action**：tasks 中不创建对应 task；如 PR review 有人提，可引用此段作为拒绝理由

### 2.1 ioctl dispatch 派发表 → sorted array + binary search（优先级 P5）

- **文件**：`plugins/gpu_driver/drv/gpgpu_device.cpp:128-137`（`ioctl()`）+ `plugins/gpu_driver/drv/gpgpu_device.h`（`IoctlEntry` struct）
- **改动**：
  - [ ] 按 `request` 数值排序 `IoctlEntry[]`
  - [ ] 替换 linear scan 为 `std::lower_bound`（或 GCC `__builtin_expect` switch hint）
  - [ ] 编译验证 + ctest 86/86 PASS
- [ ] Benchmark：`ioctl_dispatch_bench_standalone` 对比 before/after mean
- [ ] commit：`perf(gpu): ioctl dispatch → sorted array + binary search`

### 2.2 HandleManager::allocate → free-bitmap（优先级 P2，最高 ROI #2）

- **文件**：`plugins/gpu_driver/drv/gpgpu_device.cpp:64-72` + `plugins/gpu_driver/drv/gpgpu_device.h:57-66`
- **当前**：`std::map<u32, bool> handles_` + linear scan `1 → 65535`
- **改为**：`std::bitset<65536>` + `std::countr_zero(bitset_)` 找第一个 0 bit
- **API 不变**：`allocate() / release() / is_valid() / lookup()`
- **改动**：
  - [ ] 在 `gpgpu_device.h` 加 `std::bitset<65536> free_bits_` 成员
  - [ ] 重写 `HandleManager::allocate()` 为位图扫描
  - [ ] 重写 `HandleManager::release()` 为单 bit 清零
  - [ ] 保留 `handles_` map 用于 reverse-lookup（如有外部查询），或同步删除
  - [ ] 编译验证 + ctest 86/86 PASS
- [ ] Benchmark：`mmap_overhead_bench_standalone` 对比 before/after BO mean
- [ ] commit：`perf(gpu): HandleManager::allocate → free-bitmap`

### 2.3 bo_map_ `std::map → std::unordered_map`（优先级 P3）

- **文件**：`plugins/gpu_driver/drv/gpgpu_device.h:74`（声明）+ `gpgpu_device.cpp`（3 处使用：`handleAllocBo / handleFreeBo / handleMapBo`）
- **当前**：`std::map<u32, BoInfo>` O(log n)
- **改为**：`std::unordered_map<u32, BoInfo>` O(1) 平均
- **改动**：
  - [ ] 替换声明类型
  - [ ] 替换 3 处使用（注意自定义 hash / 比较器是否有需要保留）
  - [ ] 编译验证 + ctest 86/86 PASS
- [ ] Benchmark：`mmap_overhead_bench_standalone` 测 BO mean
- [ ] commit：`perf(gpu): bo_map_ → unordered_map`

### 2.4 handleGetDeviceInfo 移除 `std::cout`（优先级 P1，最高 ROI #1）

- **文件**：`plugins/gpu_driver/drv/gpgpu_device.cpp:180-182`（+ 全文件 30+ 处 `std::cout`/`std::cerr`）
- **当前**：每次调用 `std::cout << info.marketing_name << ...` 同步 I/O
- **改为**：
  - **Step A（最小修改）**：用 `#ifndef NDEBUG` 包裹所有 `std::cout`/`std::cerr`
  - **Step B（更彻底）**：迁移到 `Logger::debug()`（已通过 `kernel/logger.h` 链接）
- **改动**：
  - [ ] Step A：`#ifndef NDEBUG` 包裹 30+ 处 `std::cout`/`std::cerr`
  - [ ] 编译验证 + ctest 86/86 PASS
  - [ ] Benchmark：`ioctl_dispatch_bench_standalone` 测 mean
  - [ ]（可选）Step B：全文件迁移到 `Logger::debug()`，按行为分批
- [ ] commit：`perf(gpu): wrap std::cout in #ifndef NDEBUG`（Step A，原子）
- [ ]（如做 Step B）commit：`refactor(gpu): migrate std::cout → Logger::debug()`（分批）

### 2.5 pushbuffer_bench 解除 rate-limit（优先级 P4）

- **文件**：`tests/perf/pushbuffer_bench.cpp:122-123`
- **当前**：`std::this_thread::sleep_until(next)` 硬限 100/sec
- **改为**：
  - [ ] 新增 "max throughput" mode（无 sleep_until，1000 次连续提交测总时间）
  - [ ] 保留旧版作为 "constant-rate" 测试
  - [ ] 更新 `tests/perf/README.md` 文档用法
  - [ ] 编译验证（非 ctest 的一部分，但不应破坏）
- [ ] Benchmark：跑 max throughput mode，记录实际 throughput
- [ ] commit：`test(perf): pushbuffer_bench — add max-throughput variant`

### ⏸ 2.6 BO thread-local cache（已 defer 到 C-11b，Rule 3）

- **当前状态**：Defer（libgpu_core 合约禁止）
- **毕业触发**：见 Rule 3
- **如触发**：新建 `openspec/changes/2026-XX-XX-stage3-2b-bo-cache/`，不放本 change

---

## 3. 验证 / commit（每个优化后）

- [ ] 3.1 `cd build && ctest --output-on-failure` —— 86/86 PASS
- [ ] 3.2 跑对应 perf benchmark，对比 before/after，判定 Rule 1 / Rule 2
- [ ] 3.3 把 before/after 数字 + hit/no-op 标记记录到 `docs/04-building/perf-baseline-2026-Q3.md` §C-11 Results 表
- [ ] 3.4 atomic commit per optimization（Rule 4）—— 禁止一个 commit 改 2+ 个不相关函数

---

## 4. 收尾（半天）

- [ ] 4.1 完整 §C-11 Results 表格填齐：
  - ioctl: baseline / post / target / hit
  - pushbuffer: baseline / post / target / hit
  - BO: baseline / post / target / hit
  - **Acceptance**: X/3 hit
- [ ] 4.2 跑最终 ctest + 全套 perf benchmark，验证 acceptance
- [ ] 4.3 `openspec archive stage3-2-hotpath-optimization`（按归档流程）
- [ ] 4.4 更新 `openspec/changes/INDEX.md`：C-11 移到"已完成"表
- [ ] 4.5（如有架构性改动）写 ADR 候选，例如 `docs/00_adr/adr-XXX-handle-manager-bitmap.md`

---

## 5. 文档化（半天，可与 §2 并行）

> 注意：原 tasks §4 引用 `docs/05-advanced/performance-tuning.md` + `tests/perf/HISTORY.md`，**两个都不存在**。改为引用已有文件。

- [ ] 5.1 更新 `docs/05-advanced/performance.md`（清理 Google Benchmark 引用，替换为 Catch2 BENCHMARK 链接）
- [ ] 5.2 更新 `docs/04-building/perf-baseline-2026-Q3.md` 添加 §C-11 Results 段（Rule 1/2 应用结果）
- [ ] 5.3 README badges 仅在 perf-related 数字有显著变化时更新（如 ioctl baseline 数字变化 > 10x 才考虑）

---

## 6. 已显式排除项（防回归）

> 这些项目**不应**在 C-11 范围内重新引入。如 review 提到，可引用此段作为拒绝理由。

- ❌ **VFS hash index**（VFS 已是 `std::unordered_map`，优化 = no-op，可能破坏跨插件路径语义）
- ❌ **libgpu_core 加锁 / TLS / malloc**（违反 `gpu_buddy.h:4-5` 纯 C 无锁零分配合约）
- ❌ **Google Benchmark 替换 Catch2 BENCHMARK**（违反 ADR-010）
- ❌ **在不修 pushbuffer bench 限速情况下声称 pushbuffer 优化有效**（bench 被 sleep 掩盖）
- ❌ **巨型 commit 混合多个不相关优化**（违反 Rule 4，无法归因）
