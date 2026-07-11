# ADR-058: sim_mem_pool Real VA Allocation via gpu_buddy + mmap Backing

**状态**: 📋 PROPOSED（Phase 4）
**日期**: 2026-07-11
**提案人**: Sisyphus（Phase 4 cu-mempool-alloc-real-va change）
**关联 ADR**:
- ADR-004 (Buddy Allocator) — ✅ Accepted
- ADR-018 (Driver/Sim Separation) — ✅ Accepted
- ADR-020 (libgpu_core Extraction) — ✅ Accepted（**核心约束**）
- ADR-031 (TTM Migration Priority) — ✅ Accepted
- ADR-036 (3-Way Separation) — ✅ Accepted
- ADR-039 (MEM_POOL_EXPORT IOCTL 0x68) — ✅ Accepted（**正交**）

**关联 Change**: `openspec/changes/2026-07-15-phase4-cu-mempool-alloc-real-va/`
**关联 TADR**: `tadr-305-mempool-export-shareable`（TaskRunner 侧 consumer-lens，正交）
**关联 Research**: `docs/05-advanced/kfd-nvidia-mempool-va-research.md`（Oracle 调研基础）

---

## Context

`cuMemPoolAlloc` 在 `plugins/gpu_driver/sim/mem_pool.cpp` 当前实现（PR #20, commit `138f15a`, 2026-07-06）返回 **synthetic non-dereferenceable VA**：

- VA 来自全局单调计数器 `next_pool_va_base_`（`mem_pool.cpp:29-56`，起始 `0x100000000`，步进 64 MiB）
- `sim_mem_pool_alloc`（`mem_pool.cpp:117-141`）用线性 `find_first_fit` 在 `[va_base, va_limit)` 内分配
- `bo_handle` 固定为 0（PoC）
- 分配出的 VA **没有 mmap backing**，dereference 必 segfault

阻塞 TaskRunner PR #7 deferred item #2：真实 CUDA workload（如 `cudaMallocAsync` + `cudaMemcpyAsync`）需要 **dereferenceable VA** 才能把数据写入 GPU 可见内存。

### 设计约束（继承自既有 ADR）

1. **ADR-020 纯度约束**：`libgpu_core/` 必须保持 single-purpose（no malloc/no lock/no syscall/no STL），是 Linux kernel 可移植代码。**禁止修改 `gpu_buddy.h`** 增加 range 参数。
2. **ADR-036 3 区分**：sim 层负责硬件行为模拟，VA 分配属于 sim 域。
3. **Fix-2 Option B（2026-07-05 accepted）**：每个 pool 创建时预留 `[va_base, va_limit)` 子范围，不与其他 pool 共享。
4. **Phase 4 1 周预算**（`openspec/changes/INDEX.md` C-09）：避免大范围重构。

### 研究基础

Oracle 调研 AMD KFD（v6.10）+ Nvidia UVM（`open-gpu-kernel-modules main`），产出报告 `docs/05-advanced/kfd-nvidia-mempool-va-research.md` §1-§7。本 ADR 的 D1-D4 决策均基于该报告 §5（Specific Recommendation）。

---

## Decision

### D1: va_base 来自 Per-Device Global gpu_buddy（Option β）

**新建 `plugins/gpu_driver/sim/sim_device_va_allocator.{h,cpp}`**：

```cpp
// plugins/gpu_driver/sim/sim_device_va_allocator.h
namespace usr_linux_emu::gpu::sim {

constexpr uint64_t DEVICE_VA_BASE = 0x100000000ULL;    // 4 GiB
constexpr uint64_t DEVICE_VA_SIZE = 0x400000000ULL;    // 16 GiB

// First-fit 分配 device VA 子段给 pool create
// 内部持 gpu_buddy + std::mutex
extern "C" int sim_device_va_alloc(uint64_t size, uint64_t *base_out);
extern "C" int sim_device_va_free(uint64_t base);
extern "C" void sim_device_va_reset_for_test(void);

}  // namespace
```

**实现要点**：
- 内部 `struct gpu_buddy device_buddy_` + `gpu_buddy_init(buddy, DEVICE_VA_BASE, DEVICE_VA_SIZE)`
- 所有 alloc/free 持 `std::mutex device_buddy_mutex_`
- `size` 为 pool 创建请求的 `pool_size + POOL_VA_STRIDE`（替换现有 `next_pool_va_base_` counter 逻辑）

**为什么不选其他选项**：
- **Option α（保留合成 counter）**：va_base 仍是任意整数，"real VA" 名不副实。Oracle 报告 §5 Q1 评为"反模式 1"。
- **Option γ（扩 `VASpace` struct）**：~50 LOC 跨 struct/ioctl/sim，超 1 周预算。

### D2: Per-Pool gpu_buddy 实例（Option per-pool）

`mem_pool.cpp` 的 `PoolTableEntry` 新增字段：

```cpp
struct PoolTableEntry {
  sim_mem_pool_props_t props;
  std::map<uint64_t, PoolInternalEntry> allocated;
  uint64_t next_va_hint = 0;     // ← DEPRECATED: gpu_buddy 内部 first-fit,不需要 hint
  PoolAttrs attrs;
  void* mmap_base = nullptr;     // ← NEW: mmap backing for [va_base, va_limit)
  struct gpu_buddy pool_buddy_;  // ← NEW: per-pool buddy for sub-allocation
  std::mutex buddy_mutex;        // ← NEW: protects pool_buddy_
};
```

**生命周期**：
- `sim_mem_pool_create`：`gpu_buddy_init(&entry.pool_buddy_, va_base, pool_size)` 自然约束分配范围在 `[va_base, va_limit)`
- `sim_mem_pool_destroy`：`gpu_buddy_reset(&entry.pool_buddy_)`（不需显式 free，因 device_buddy 回收整个范围）
- `sim_mem_pool_alloc`：`std::lock_guard<std::mutex> lock(pool.buddy_mutex); gpu_buddy_alloc(&pool.pool_buddy_, size, &addr)`
- `sim_mem_pool_free_async`：`std::lock_guard<std::mutex> lock(pool.buddy_mutex); gpu_buddy_free(&pool.pool_buddy_, va)`

**为什么 per-pool 而非 per-VA-Space**：
- Nvidia UVM `uvm_range_allocator` 是 per-semaphore-pool 模式
- `VASpace` struct（`gpgpu_device.h:88-94`）无 va_base/va_limit 字段，per-VA-Space 需先扩 struct（落回 Option γ 代价）
- Per-pool 隔离性好：pool 销毁时 buddy 自动释放，无跨 pool 碎片

### D3: mmap(MAP_ANONYMOUS|MAP_PRIVATE|MAP_FIXED_NOREPLACE) at Pool Create

**在 `sim_mem_pool_create` 中**（替换/补强当前 `next_pool_va_base_` 逻辑）：

```cpp
int sim_mem_pool_create(sim_mem_pool_props_t *props, uint64_t *pool_handle_out) {
  // Step 1: from device-wide buddy, allocate VA subrange for this pool
  uint64_t pool_size_aligned = align_up_4k(props->size + POOL_VA_STRIDE);
  uint64_t va_base;
  int rc = sim_device_va_alloc(pool_size_aligned, &va_base);
  if (rc != 0) return SIM_POOL_ERR_NOMEM;

  props->va_base  = va_base;
  props->va_limit = va_base + props->size;  // 用户可见区间

  // Step 2: mmap backing the WHOLE pool range (including stride)
  void* backing = mmap(reinterpret_cast<void*>(va_base), pool_size_aligned,
                       PROT_READ | PROT_WRITE,
                       MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED_NOREPLACE, -1, 0);
  if (backing == MAP_FAILED) {
    sim_device_va_free(va_base);
    return SIM_POOL_ERR_NOMEM;
  }

  // Step 3: init per-pool buddy for sub-allocation
  struct PoolTableEntry entry;
  entry.mmap_base = backing;
  gpu_buddy_init(&entry.pool_buddy_, va_base, pool_size_aligned);

  // ... existing field initialization ...
  *pool_handle_out = /* generated handle */;
  return SIM_POOL_ERR_OK;
}

int sim_mem_pool_destroy(uint64_t pool_handle) {
  // ... existing lookup ...
  munmap(entry.mmap_base, align_up_4k(entry.props.size + POOL_VA_STRIDE));
  sim_device_va_free(entry.props.va_base);
  // ... existing removal ...
  return SIM_POOL_ERR_OK;
}
```

**关键约束**：
- `MAP_FIXED_NOREPLACE`（Linux 4.17+，UsrLinuxEmu 目标 Ubuntu 20.04+ kernel 5.4+ 已支持）防静默覆盖
- `MAP_ANONYMOUS | MAP_PRIVATE` = lazy physical page allocation（demand paging）：1 GiB pool 写 1 MB → 物理 ~1 MB
- `DEVICE_VA_BASE = 0x100000000`（4 GiB）位于 64 位 Linux 用户态高地址区，Day 2 验证 `/proc/self/maps` 确认空闲（Open Question #2）

### D4: std::mutex 保护 gpu_buddy

**当前 `mem_pool.h:12` 注释 `NOT required (single-threaded)`。本 ADR 升级为 thread-safe**：

```cpp
// Per-pool mutex (D2)
// Per-device mutex (D1)
```

**模式参照**：
- Linux `drm_buddy`：header 注释 "Locking should be handled by the user, a simple mutex around drm_buddy_alloc* and drm_buddy_free* should suffice."
- Nvidia UVM `uvm_range_allocator_t`：内嵌 `uvm_spin_lock_t lock`，所有 alloc/free 持锁
- AMD amdgpu `ttm_manager_func`：`struct mutex` 保护 VRAM 分配

**trading-off**：
- 优点：未来 multi-stream / async fence 路径（`sim_mem_pool_alloc_async`）已存在 fence_id 分配并发，扩展安全
- 代价：~5 LOC；单线程下可忽略 mutex 开销

### D5: libgpu_core 保持不变

**严格遵守 ADR-020**：`libgpu_core/include/gpu_buddy.h` 与 `libgpu_core/src/buddy.c` **无任何修改**。所有 VA 范围感知与 mmap 逻辑均在 sim 层 wrapper（`sim_device_va_allocator.{h,cpp}` + `mem_pool.cpp`）实现。

---

## Consequences

### 正面影响

- ✅ TaskRunner 真实 CUDA workload 可用 `cuMemPoolAlloc` 写入数据（dereferenceable）
- ✅ VA 真实由 gpu_buddy 管理，可 first-fit / free / coalesce，与 ADR-004 决策一致
- ✅ 与 AMD KFD（`drm_buddy` + interval tree）和 Nvidia UVM（`uvm_range_allocator` + per-pool）生产模式对齐
- ✅ `sim_mem_pool_alloc_async` 路径自动获得并发安全
- ✅ libgpu_core 纯度约束（ADR-020）保持不变，可直接编译到 Linux kernel
- ✅ Cross-repo TaskRunner 测试零破坏点（TaskRunner 测试用 MockGpuDriver，不打真实 sim，Oracle 报告 §2.5 确认）

### 负面影响与权衡

- ⚠️ **mmap 1 GiB pool = 1 GiB virtual address space**（物理页 demand paging，Open Question #2 验证 4 GiB 高地址区空闲）
- ⚠️ **mutex 开销**：单线程 alloc + ~50 ns mutex lock/unlock；多线程并发下可能成为瓶颈（Phase 5+ 可升级为 sharded pool）
- ⚠️ **`gpu_buddy` 内部碎片**：3 MB alloc → 4 MB block（向上取整 2 的幂），Phase 4 pool alloc 通常 4K 对齐可接受
- ⚠️ **32 位系统不支持**（`uint64_t` VA + 1 GiB pool mmap），但 UsrLinuxEmu 已是 64 位 only

### Open Questions（需要 owner 确认）

1. **TaskRunner 侧 `test_cu_mem_pool` 是否已断言 VA 可解引用？** Oracle 调研表明所有 TaskRunner 测试都用 MockGpuDriver（无 dereference 断言），但需 owner 确认跨仓 mmap backing 后是否需补 TaskRunner 侧 smoke test。
2. **`DEVICE_VA_BASE = 0x100000000` 是否与 UsrLinuxEmu CLI/tools 自身 mmap 冲突？** Day 2 验证 `cat /proc/self/maps` 确认 4 GiB 区空闲。若冲突改 `0x200000000`（8 GiB）或 `mmap(NULL, ...)` 动态 va_base。
3. **`gpu_buddy` `MAX_RECORDS=4096`** 单 pool 最大 4096 个 allocation。Phase 4 测试场景远低于此。
4. **ADR-039 (MEM_POOL_EXPORT IOCTL 0x68) 跨进程导出后的 mmap 共享**不在本 ADR 范围（Phase 4 acceptance 不含 cross-process dereference，Phase 5+ 升级为 memfd + `mmap(shared_fd)`）。

---

## Migration

### Day 1: 设计 + ADR（本 ADR）

- [x] Oracle 研究报告 `docs/05-advanced/kfd-nvidia-mempool-va-research.md`（414 行）
- [ ] ADR-058（本文件）review + Accepted
- [ ] 更新 `openspec/changes/2026-07-15-phase4-cu-mempool-alloc-real-va/proposal.md` §What Changes 加入 mmap + mutex 决策

### Day 2-3: 实现（~150 LOC 新增 + ~60 LOC delta）

| 文件 | 操作 | LOC | 内容 |
|------|------|----:|------|
| `plugins/gpu_driver/sim/sim_device_va_allocator.h` | 新建 | ~25 | API 声明 + namespace |
| `plugins/gpu_driver/sim/sim_device_va_allocator.cpp` | 新建 | ~50 | `device_buddy_` + `std::mutex` + 三个 extern "C" 函数 |
| `plugins/gpu_driver/sim/mem_pool.cpp` | 修改 | ~60 delta | `PoolTableEntry` 新增字段；create/destroy/alloc/free/reset 接入 |
| `plugins/gpu_driver/sim/CMakeLists.txt` | 修改 | ~1 | 加入新 .cpp |
| `docs/00_adr/README.md` | 修改 | ~3 | 索引表新增 ADR-058 行 |

### Day 4: 测试（~40 LOC delta）

| 文件 | 操作 | LOC | 内容 |
|------|------|----:|------|
| `tests/test_sim_mem_pool_standalone.cpp` | 修改 | ~40 delta | dereferenceable smoke test + first-fit 验证 + free+realloc 复用 |

新增 assertions：
- `REQUIRE(va >= props.va_base && va < props.va_limit)`（保留现有）
- `volatile uint64_t* p = (volatile uint64_t*)va; *p = 0xDEADBEEF; REQUIRE(*p == 0xDEADBEEF)`（**新增**）
- 连续 alloc 3 块，地址递增（**新增**）
- alloc A → free A → alloc B (same size)，验证 B == A（**新增**）

### Day 5: 验证 + commit

- [ ] `cd build && cmake .. && make -j4 && ctest -R "test_sim_mem_pool" -V` 全绿
- [ ] `cd external/TaskRunner/build && ctest -R "test_cu_mem_pool"` 全绿
- [ ] `tools/docs-audit.sh --strict` 无 warning
- [ ] commit: `feat(sim): real VA allocation in sim_mem_pool via gpu_buddy + mmap backing`

**总 LOC**：~210 LOC（vs Oracle 报告 §6 估算一致）

---

## Acceptance Criteria（继承自 change proposal.md）

- [ ] `sim_mem_pool_alloc` 返回 VA 在 `[va_base, va_limit)` 范围内
- [ ] VA dereferenceable（`*va = 0xDEADBEEF; CHECK(*va == 0xDEADBEEF)` 测试通过）
- [ ] libgpu_core `libgpu_core/include/gpu_buddy.h` 与 `libgpu_core/src/buddy.c` **无修改**（ADR-020 保持）
- [ ] First-fit 4K 对齐（gpu_buddy 原生行为）
- [ ] Free + realloc 复用测试通过
- [ ] Cross-repo TaskRunner `test_cu_mem_pool` 端到端 PASS
- [ ] 全部 ctest PASS（111/111+），无回归
- [ ] `tools/docs-audit.sh --strict` 无 warning

---

**维护者**: UsrLinuxEmu Architecture Team + Phase 4 owner
**最后更新**: 2026-07-11（初版）
**关联 Commit（待）**: Phase 4 cu-mempool-alloc-real-va merge to main