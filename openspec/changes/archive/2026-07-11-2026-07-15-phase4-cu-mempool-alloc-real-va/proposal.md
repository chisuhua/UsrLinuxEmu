# Change: phase4-cu-mempool-alloc-real-va

> **状态**: ✅ COMPLETED（2026-07-11 归档）
> **优先级**: 🔵 P3
> **创建**: 2026-07-15
> **完成**: 2026-07-11（commit `ba88b5f`）
> **更新**: 2026-07-11（Oracle 调研完成 + ADR-058 决策落地 + 实施完成）
> **来源**: TaskRunner PR #7 deferred item #2（`cuMemPoolAlloc` synthetic VA → real VA sub-range）
> **依赖**: C-02 stage3-ioctl-dispatch-completeness
> **关联 ADR**: [ADR-058](../../docs/00_adr/adr-058-sim-mem-pool-real-va.md) 📋 PROPOSED
> **关联 Research**: [docs/05-advanced/kfd-nvidia-mempool-va-research.md](../../docs/05-advanced/kfd-nvidia-mempool-va-research.md)
> **工作目录**: `openspec/changes/2026-07-15-phase4-cu-mempool-alloc-real-va/`

## Why

当前 `cuMemPoolAlloc` 返回 **synthetic non-dereferenceable VA**（PR #20 PoC）。无法用于真实 CUDA workload。需要：
- 与 `libgpu_core/gpu_buddy` 集成（per Fix-2 Option B 路径）
- VA 在 `[va_base, va_limit)` 真实 first-fit 分配
- 返回 dereferenceable VA

## What Changes

### 1. sim_mem_pool_alloc 重写

`plugins/gpu_driver/sim/mem_pool.cpp`：
- **当前**：分配从 `va_space_handle` 内部 counter，返回 synthetic VA
- **改为**：使用 `libgpu_core/gpu_buddy` 在 `[va_base, va_limit)` 真实 first-fit 4K 对齐分配

### 2. libgpu_core 集成

- sim 层新加 API：`sim_mem_pool_alloc(pool_handle, size, out_va)` → 调 gpu_buddy_alloc(range-limited)
- 不改 libgpu_core 本身（保持 single-purpose）

### 3. 测试更新

- existing: `test_sim_mem_pool_standalone` +11 cases
- 加：返回 VA 在 `[va_base, va_limit)` 范围检查
- 加：可读/写 smoke test

### 4. ADR-058 决策落地（2026-07-11 Oracle 调研）

经 Oracle 调研 AMD KFD v6.10 + Nvidia UVM `open-gpu-kernel-modules` 后，本 change 落地 4 个核心决策：

#### D1: va_base 来自 Per-Device Global gpu_buddy（Option β）
- 新建 `plugins/gpu_driver/sim/sim_device_va_allocator.{h,cpp}`（~75 LOC）
- 内部 `struct gpu_buddy device_buddy_` + `std::mutex`，base=`0x100000000`, size=`0x400000000` (16 GiB)
- 替换 `mem_pool.cpp:92` 的 `next_pool_va_base_` counter 逻辑
- 镜像 Nvidia UVM `uvm_range_allocator` per-semaphore-pool 模式

#### D2: Per-Pool gpu_buddy 实例
- `mem_pool.cpp` `PoolTableEntry` 新增 `pool_buddy_` + `buddy_mutex`
- `gpu_buddy_init(&entry.pool_buddy_, va_base, pool_size)` 自然约束分配范围
- `sim_mem_pool_alloc/free_async` 通过 mutex 保护调 `gpu_buddy_alloc/free`

#### D3: mmap Backing at Pool Create
- `sim_mem_pool_create` 中对 `[va_base, va_limit + POOL_VA_STRIDE)` 做 `mmap(MAP_ANONYMOUS|MAP_PRIVATE|MAP_FIXED_NOREPLACE)`
- lazy physical page allocation（demand paging）：1 GiB pool 写 1 MB → 物理 ~1 MB
- `sim_mem_pool_destroy` 中 `munmap(entry.mmap_base, ...)`
- **关键**：必须 `MAP_FIXED_NOREPLACE`（Linux 4.17+），防静默覆盖

#### D4: std::mutex Wrapping
- gpu_buddy 无内部锁（ADR-020 决策 3），wrapper 必须加 mutex
- per-device buddy 1 个 mutex + per-pool buddy 各 1 个 mutex
- 模式参照 Linux `drm_buddy`（header 注释）+ Nvidia `uvm_range_allocator_t`（内嵌 spin_lock）

### 5. libgpu_core 保持完全不变

- **禁止修改** `libgpu_core/include/gpu_buddy.h` 与 `libgpu_core/src/buddy.c`
- ADR-020 纯度约束（no malloc/no lock/no syscall/no STL）保持
- 所有 VA 范围感知 + mmap + mutex 逻辑均在 sim 层 wrapper

## Acceptance

- [x] sim_mem_pool_alloc 返回 VA 在 `[va_base, va_limit)` 范围
- [x] VA dereferenceable（`*va = 0xDEADBEEF; CHECK(*va == 0xDEADBEEF)` 测试通过）
- [x] 不修改 libgpu_core 单一职责（`git diff libgpu_core/` 为空）
- [x] first-fit 4K 对齐（gpu_buddy 原生行为）
- [x] free + realloc 复用测试通过
- [x] Cross-repo TaskRunner test_cu_mem_pool 端到端 PASS（Oracle 报告 §2.5 确认零破坏点）
- [x] ctest 全绿（实际 86/86，5/5 稳定运行）
- [x] tools/docs-audit.sh --strict 无 warning（43 passed / 0 failed / 0 warnings）
- [x] `sim_device_va_reset_for_test` 测试钩子就位

## 测试方法

```bash
# UsrLinuxEmu 侧
cd build && ctest -R "test_sim_mem_pool|test_kfd_portability_phase31" -V

# 跨仓
cd external/TaskRunner/build && ctest -R "test_cu_mem_pool"
```

## 文件清单

### 新建文件（~75 LOC）
- `plugins/gpu_driver/sim/sim_device_va_allocator.h` (~25 LOC)
- `plugins/gpu_driver/sim/sim_device_va_allocator.cpp` (~50 LOC)

### 修改文件（~104 LOC delta）
- `plugins/gpu_driver/sim/mem_pool.cpp` (~60 LOC delta)
- `plugins/gpu_driver/sim/CMakeLists.txt` (~1 LOC delta)
- `tests/test_sim_mem_pool_standalone.cpp` (~40 LOC delta)
- `docs/00_adr/adr-058-sim-mem-pool-real-va.md` (新建, ~150 LOC)
- `docs/00_adr/README.md` (~3 LOC delta)
- `docs/05-advanced/kfd-nvidia-mempool-va-research.md` (新建, 414 LOC Oracle 报告)

### 不修改文件（ADR-020 保持）
- `libgpu_core/include/gpu_buddy.h` ❌
- `libgpu_core/src/buddy.c` ❌

## Cross-Repo 影响

- **TaskRunner 测试零破坏点**：TaskRunner `test_cu_mem_pool` 用 `MockGpuDriver`（`mock_gpu_driver.hpp:317`），不验证 VA dereferenceability。Oracle 报告 §2.5 确认。
- **tadr-305 (MEM_POOL_EXPORT Shareable) 正交**：本次不改 export 路径
- **cuMemPoolFree 当前 no-op**（`external/TaskRunner/src/umd/libcuda_shim/cu_mem_pool.cpp:86-90`）：本 change 内 sim 层 `sim_mem_pool_free_async` 真实化，TaskRunner 侧 free 路径不在本 change 范围

## Dependencies

- **C-02** stage3-ioctl-dispatch-completeness ✅ 已归档
- libgpu_core/gpu_buddy 已稳定（**无需改动**）
- ADR-020 (libgpu_core 纯度约束) ✅ Accepted
- ADR-031 (TTM Migration Priority) ✅ Accepted
- ADR-036 (3 区分架构) ✅ Accepted

## Open Questions

1. **TaskRunner 侧 `test_cu_mem_pool` 是否已断言 VA 可解引用？** 需 TaskRunner owner 确认（Oracle Open Question #1）
2. **`DEVICE_VA_BASE = 0x100000000` 是否与 UsrLinuxEmu CLI/tools 自身 mmap 冲突？** Day 2 验证 `cat /proc/self/maps`，若冲突改 `0x200000000`（Oracle Open Question #2）

## 参考资料

- **Oracle 研究报告**: `docs/05-advanced/kfd-nvidia-mempool-va-research.md`（414 行，AMD KFD + Nvidia UVM 调研）
- **ADR-058**: `docs/00_adr/adr-058-sim-mem-pool-real-va.md`（4 决策完整论述）
- **PR #20**: 引入 synthetic VA PoC（commit `138f15a`, 2026-07-06）
- **Fix-2 Option B**: 2026-07-05 accepted（VA 子范围预留方案）
- **ADR-004**: Buddy Allocator ✅ Accepted
- **ADR-020**: libgpu_core 提取 ✅ Accepted
- **ADR-039**: MEM_POOL_EXPORT IOCTL 0x68 ✅ Accepted（正交）
