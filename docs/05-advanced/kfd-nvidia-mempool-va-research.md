# KFD / Nvidia Kernel Driver Memory Pool VA Allocation Research

> **目的**: 为 UsrLinuxEmu `phase4-cu-mempool-alloc-real-va` change 提供基于 AMD KFD 与 Nvidia UVM 生产驱动实现的研究基础。
> **内核 tag**: Linux v6.10 (AMD) / NVIDIA open-gpu-kernel-modules main branch (Nvidia UVM)
> **状态**: Oracle 研究报告 (read-only consultation)
> **日期**: 2026-07-11

---

## §1. Executive Summary

AMD KFD 与 Nvidia UVM 对 "memory pool VA allocation" 采用了截然不同但都成熟的设计。

**AMD KFD** 采用 **用户态选地址 + 内核 interval tree 跟踪** 模式：`kfd_ioctl_alloc_memory_of_gpu` 接收用户态传入的 `args->va_addr`（`drivers/gpu/drm/amd/amdkfd/kfd_chardev.c:1178`），内核不分配 VA，只负责将 BO 映射到 `amdgpu_vm` 的区间树（`amdgpu_vm_bo_map` → `amdgpu_bo_va_mapping` rb-tree node，`drivers/gpu/drm/amd/amdgpu/amdgpu_vm.c:1837`）。物理 VRAM 分配才使用 `drm_buddy`（经 `amdgpu_vram_mgr` → `drm_buddy_alloc_blocks`，`drivers/gpu/drm/drm_buddy.c`）。每个 process 拥有独立的 `amdgpu_vm`（`kfd_process_device_init_vm` → `amdgpu_vm_init`，`drivers/gpu/drm/amd/amdgpu/amdgpu_vm.c:2560`），per-process-per-device 的 `gpuvm_base`/`gpuvm_limit` aperture 定义了可用 VA 范围（`drivers/gpu/drm/amd/amdkfd/kfd_priv.h` `struct kfd_process_device`）。

**Nvidia UVM** 采用 **内核 range tree + mmap 驱动** 模式：`uvm_va_space_t` 是顶层容器，内部用自研 `uvm_range_tree_t`（rb-tree 区间树）跟踪所有 `uvm_va_range_t`（`kernel-open/nvidia-uvm/uvm_va_range.c:151` `uvm_va_range_initialize`）。VA range 来自用户态 mmap（managed range 由 `uvm_va_range_create_mmap` 在 mmap 回调中创建，`uvm_va_range.c`）或 ioctl 显式分配。UVM 还提供一个独立的 `uvm_range_allocator_t`（`kernel-open/nvidia-uvm/uvm_range_allocator.c`），它用 `uvm_range_tree` 跟踪空闲区间，brute-force first-fit + alignment + merge_prev/merge_next，专门用于 semaphore pool 等需要内核自动选地址的场景。`cuMemPoolExportToShareableHandle` 在 driver 层通过 POSIX FD（DMA-buf 或 memfd）导出，用户态通过 `cuMemPoolImportFromShareableHandle` + `cuMemPoolImportPointer` 在另一个进程重建映射。

### Top 3 可移植模式 (UsrLinuxEmu 应采纳)

1. **Per-VA-Space buddy allocator 实例**（Nvidia 模式）— `uvm_va_space` 持有 range tree，UsrLinuxEmu 的 sim 层应在 pool create 时为每个 pool 创建独立 `gpu_buddy` 实例，base=`va_base`，size=pool size。这避免修改 `VASpace` struct（Option γ 的代价），同时保持 pool 间隔离。
2. **mmap backing at pool create**（Nvidia managed range 模式）— `uvm_va_range_create_mmap` 在 mmap 时创建 range 并使 VA 可解引用。UsrLinuxEmu 应在 `sim_mem_pool_create` 时对 `[va_base, va_limit)` 做一次 `mmap(MAP_ANONYMOUS|MAP_PRIVATE)`，使整个 pool VA 可读可写，gpu_buddy 在其中做子分配。
3. **External mutex wrapping lockless allocator**（drm_buddy 模式）— `include/drm/drm_buddy.h:60` 明确注释 "Locking should be handled by the user, a simple mutex around drm_buddy_alloc* and drm_buddy_free* should suffice." UsrLinuxEmu 的 `gpu_buddy` 遵循 ADR-020 决策 3（完全无锁），sim 层 wrapper 应加 `std::mutex`。

### Top 2 反模式 (UsrLinuxEmu 应避免)

1. **全局单调 counter 分配 va_base**（当前 PoC `mem_pool.cpp:92` `next_pool_va_base_`）— 无 backing、无法回收、跨 pool 地址不可预测。AMD KFD 的 per-process `gpuvm_base` aperture 是固定 per-device 的，不是 counter。
2. **用户态完全自选 VA 无内核校验**（AMD KFD 模式的风险面）— AMD 依赖 `amdgpu_vm_bo_map` 的 interval tree 冲突检测。UsrLinuxEmu 若让用户态自选 VA 且无区间树，会丢失冲突检测能力。pool-scoped buddy 自动解决了这个问题。

---

## §2. AMD KFD Deep Dive

### 2.1 VA 范围来源：Per-Process-Per-Device Aperture

AMD KFD 的 VA 空间是 **per-process-per-device** 的。`struct kfd_process_device` 定义了固定 aperture：

```c
// drivers/gpu/drm/amd/amdkfd/kfd_priv.h — struct kfd_process_device
uint64_t lds_base;
uint64_t lds_limit;
uint64_t gpuvm_base;      // ← GPUVM VA aperture start
uint64_t gpuvm_limit;     // ← GPUVM VA aperture end
uint64_t scratch_base;
uint64_t scratch_limit;
```

这些 aperture 在 process-device 创建时由 `kfd_process_device_init_vm` → `amdgpu_vm_init` 初始化（`drivers/gpu/drm/amd/amdkfd/kfd_process.c:1736`，`drivers/gpu/drm/amd/amdgpu/amdgpu_vm.c:2560`）。每个 process 对每个 GPU 有独立的 `amdgpu_vm`，VM 内部用 interval tree（rb-tree）跟踪 `amdgpu_bo_va_mapping`。

### 2.2 VA 地址选择：用户态负责

关键发现：**AMD KFD 内核不分配 VA 地址**。`kfd_ioctl_alloc_memory_of_gpu` 接收用户态传入的 `args->va_addr`：

```c
// drivers/gpu/drm/amd/amdkfd/kfd_chardev.c:1178
err = amdgpu_amdkfd_gpuvm_alloc_memory_of_gpu(
    dev->adev, args->va_addr, args->size,
    pdd->drm_priv, (struct kgd_mem **) &mem, &offset,
    flags, false);
```

`amdgpu_amdkfd_gpuvm_alloc_memory_of_gpu`（`drivers/gpu/drm/amd/amdgpu/amdgpu_amdkfd_gpuvm.c:1704`）接收 `va` 参数，创建 `kgd_mem`，然后 `kfd_mem_attach` → `amdgpu_vm_bo_add` + `amdgpu_vm_bo_map` 将 VA 映射到 VM 的 interval tree。用户态 HSA runtime（如 ROCR）负责在 `gpuvm_base..gpuvm_limit` 范围内选择 VA 地址。

### 2.3 Allocator 选择：interval tree (rb-tree) for VA, drm_buddy for VRAM

- **VA 管理**: `amdgpu_vm_bo_map`（`drivers/gpu/drm/amd/amdgpu/amdgpu_vm.c:1837`）在 `amdgpu_vm::va` interval tree 中插入 `amdgpu_bo_va_mapping` node。这是标准 Linux `struct interval_tree_node`（rb-tree augmented with subtree_max）。支持冲突检测、split、merge。
- **物理 VRAM 分配**: `amdgpu_vram_mgr` 基于 `drm_buddy`（`drivers/gpu/drm/drm_buddy.c`）。`drm_buddy_alloc_blocks(mm, start, end, size, min_block_size, blocks, flags)` 支持 range-biased allocation（`DRM_BUDDY_RANGE_ALLOCATION` flag）和 trim（`drm_buddy_block_trim`）。

### 2.4 对齐与 Huge Page

- VA 对齐：4KB（`PAGE_SIZE`）最低，用户态负责满足 GPU page-table 对齐要求。
- VRAM：`amdgpu_amdkfd_reserve_mem_limit` 注释 "Conservatively round up the allocation requirement to 2 MB to avoid fragmentation caused by 4K allocations in the tail 2M BO chunk"（`amdgpu_amdkfd_gpuvm.c` §reserve_mem_limit）。即 VRAM 物理分配按 2MB 对齐以减少碎片。
- Huge page：amdgpu VM 支持 2MB/1GB PTE（`amdgpu_vm` fragment 层），但 KFD 分配路径本身不强制 huge page VA。

### 2.5 mmap Backing

AMD KFD 的 VA 可通过两种方式被 CPU 解引用：
1. **USERPTR 路径**: 用户先 `mmap` 普通 anon 内存，将用户态 VA 传给 KFD（`KFD_IOC_ALLOC_MEM_FLAGS_USERPTR`），内核通过 HMM 绑定。
2. **GTT/VRAM 路径**: 内核分配 BO，用户通过 DRM mmap（`amdgpu_mmap` → `ttm_bo_mmap_struct`）映射到 CPU VA。

对于 pool 场景，更接近的模式是 SVM（`drivers/gpu/drm/amd/amdkfd/kfd_svm.c`）：`svm_range_set_attr`（`kfd_svm.c:3705`）在 CPU mmap 的 VA 上建立 GPU 映射，CPU 和 GPU 共享同一 VA。底层依赖 HMM + `mmu_interval_notifier`。

### 2.6 Free / Trim 语义

- `amdgpu_amdkfd_gpuvm_free_memory_of_gpu` 释放 `kgd_mem`，从 VM interval tree 移除映射。
- VRAM 层面：`drm_buddy_free` 归还 block，自动 coalesce。
- 无显式 "trim to minSize" 概念；CUDA 的 `cuMemPoolTrimTo` 在 AMD 侧由 ROCm runtime 在用户态管理 pool cache，内核不感知。

### 2.7 Pool Ownership / Lifetime

- `amdgpu_vm` 生命周期绑定 `kfd_process_device`，后者绑定 `kfd_process`（per-process）。
- `alloc_idr`（`struct kfd_process_device::alloc_idr`，`kfd_priv.h`）用 IDR 管理 BO handle，process 退出时自动清理。
- 无 "pool" 概念；每个 BO 独立分配，pool 语义在用户态 runtime 实现。

### 2.8 Cross-Process Sharing

AMD KFD 通过 **DMA-buf** 实现跨进程共享：`amdgpu_gem_prime_export` → `dma_buf_export`，接收方 `amdgpu_gem_prime_import`。这是标准 DRM prime 机制，不涉及 KFD 特有路径。

---

## §3. Nvidia Kernel Driver Deep Dive

### 3.1 VA Space 模型

Nvidia UVM 的顶层容器是 `uvm_va_space_t`，每个打开 `/dev/nvidia-uvm` 的 fd 关联一个（`kernel-open/nvidia-uvm/uvm_va_space.c:265` `uvm_range_tree_init(&va_space->va_range_tree)`）。VA space 内部维护 `uvm_range_tree_t va_range_tree`，这是一个基于 rb-tree 的区间树。

VA range 类型（`uvm_va_range.h`）：
- `UVM_VA_RANGE_TYPE_MANAGED` — 用户 mmap 创建，含 `uvm_va_block_t` 数组
- `UVM_VA_RANGE_TYPE_EXTERNAL` — 外部分配（RM 管理）
- `UVM_VA_RANGE_TYPE_SEMAPHORE_POOL` — semaphore pool，由 `uvm_range_allocator` 分配地址
- `UVM_VA_RANGE_TYPE_DEVICE_P2P` — GPU P2P 内存
- `UVM_VA_RANGE_TYPE_CHANNEL` — channel VA

### 3.2 Range Tree (rb-tree interval tree)

`uvm_range_tree_t`（`kernel-open/nvidia-uvm/uvm_range_tree.c`）是 UVM 的核心数据结构：

```c
// uvm_range_tree.c — 核心操作
uvm_range_tree_node_t *uvm_range_tree_find(uvm_range_tree_t *tree, NvU64 addr);
NV_STATUS uvm_range_tree_add(uvm_range_tree_t *tree, uvm_range_tree_node_t *node);
void uvm_range_tree_split(uvm_range_tree_t *tree, uvm_range_tree_node_t *existing, uvm_range_tree_node_t *new);
uvm_range_tree_node_t *uvm_range_tree_merge_prev(uvm_range_tree_t *tree, uvm_range_tree_node_t *node);
uvm_range_tree_node_t *uvm_range_tree_merge_next(uvm_range_tree_t *tree, uvm_range_tree_node_t *node);
NV_STATUS uvm_range_tree_find_hole(uvm_range_tree_t *tree, NvU64 addr, NvU64 *start, NvU64 *end);
```

每个 `uvm_va_range_t` 内嵌 `uvm_range_tree_node node`（`uvm_va_range.h:228` "Storage in range tree. Also contains range start and end"）。插入时 `uvm_range_tree_add` 检测冲突。

### 3.3 VA Block (2MB 叶子节点)

Managed range 内部按 2MB block 切分（`uvm_va_block.h` `BUILD_BUG_ON(UVM_VA_BLOCK_SIZE != UVM_CHUNK_SIZE_2M)`）。每个 block 是独立锁粒度，跟踪 CPU + 所有 GPU 的 V→P 映射状态。`uvm_va_range_alloc_managed`（`uvm_va_range.c`）分配 `blocks` 数组，按需 `uvm_va_block_create`。

### 3.4 Range Allocator (内核自动选地址)

对于 semaphore pool 等需要内核自动选 VA 地址的场景，UVM 提供 `uvm_range_allocator_t`（`kernel-open/nvidia-uvm/uvm_range_allocator.c`）：

```c
// uvm_range_allocator.h
typedef struct {
    uvm_spin_lock_t lock;              // ← 自旋锁保护
    NvU64 size;                        // ← 总大小
    uvm_range_tree_t range_tree;       // ← 跟踪空闲区间
} uvm_range_allocator_t;

NV_STATUS uvm_range_allocator_init(NvU64 size, uvm_range_allocator_t *range_allocator);
NV_STATUS uvm_range_allocator_alloc(uvm_range_allocator_t *range_allocator, NvU64 size, NvU64 alignment, uvm_range_allocation_t *range_alloc);
void uvm_range_allocator_free(uvm_range_allocator_t *range_allocator, uvm_range_allocation_t *range_alloc);
```

分配算法（`uvm_range_allocator.c:90-131`）：**brute-force first-fit + alignment**，遍历 `uvm_range_tree` 中所有空闲 node，找第一个满足 `size + alignment` 的 hole，shrink 或 remove node，释放时 merge_prev/merge_next。注释明确："This is a very simple brute force going over all the free ranges"。

### 3.5 对齐与 Page Size

- `uvm_va_block` 固定 2MB 对齐（`UVM_VA_BLOCK_SIZE = UVM_CHUNK_SIZE_2M`）
- GPU PTE 支持 4KB/64KB/2MB chunk（`uvm_va_block_gpu_chunk_index_range`，`uvm_va_block.c`）
- `PAGE_SIZE` 是最低 chunk size（`uvm_va_block.c` `UVM_ASSERT(uvm_chunk_find_first_size(chunk_sizes) == PAGE_SIZE)`）

### 3.6 mmap Backing

Nvidia UVM managed range 由用户态 `mmap(/dev/nvidia-uvm)` 触发创建：

```c
// uvm.c — uvm_vm_open_managed (mmap 回调)
static void uvm_vm_open_managed(struct vm_area_struct *vma) {
    // ... 创建 uvm_va_range_managed_t ...
    managed_range = uvm_va_range_alloc_managed(va_space, vma->vm_start, vma->vm_end - 1);
    // ...
}
```

`uvm_va_range_create_mmap`（`uvm_va_range.c`）在 mmap 时调用，创建 managed range 并插入 `va_range_tree`。CPU 访问通过 vma 的 fault handler（`uvm_vm_fault` 系列）按需建立物理映射。这是 **fault-on-demand** 模式，不是 eager mmap。

Semaphore pool 和 device P2P 则是 **eager mmap**（`uvm_vm_open_semaphore_pool` / `uvm_vm_open_device_p2p`），在 mmap 时就建立完整映射。

### 3.7 cuMemPoolAlloc → UVM 路径

CUDA driver 的 `cuMemPoolAlloc` / `cuMemAllocFromPoolAsync` 在 UVM 侧的路径：
1. 用户态 CUDA runtime 调用 `cuMemPoolCreate` → RM (Resource Manager) 创建 pool 对象
2. `cuMemAllocFromPoolAsync` → RM 分配物理内存 + UVM 分配 VA range
3. VA range 通过 `uvm_api_create_semaphore_pool` 类似路径（如果是 semaphore pool）或 managed range（如果是普通 pool）注册到 `uvm_va_space`
4. CPU 访问通过 mmap UVM device

### 3.8 Cross-Process Export

`cuMemPoolExportToShareableHandle`（CUDA Driver API `group__CUDA__MALLOC__ASYNC`）在 kernel 层通过：
- `CU_MEM_HANDLE_TYPE_POSIX_FILE_DESCRIPTOR` → DMA-buf fd 或 memfd
- `CU_MEM_HANDLE_TYPE_FABRIC` → fabric handle（需 fabricmanager）

接收方 `cuMemPoolImportFromShareableHandle` → `cuMemPoolImportPointer` 重建映射。PyTorch 的 `ExpandableSegment::share`（`c10/cuda/CUDACachingAllocator.cpp`）使用 `cuMemExportToShareableHandle` + `CU_MEM_HANDLE_TYPE_POSIX_FILE_DESCRIPTOR` + `pidfd_getfd` 实现跨进程 FD 传递（per UCX issue #11548）。

---

## §4. Comparative Analysis Table

| Aspect | AMD KFD (v6.10) | Nvidia UVM (open-gpu-kernel-modules) | UsrLinuxEmu (current PoC) | UsrLinuxEmu (target) |
|--------|-----------------|--------------------------------------|--------------------------|---------------------|
| **VA range source** | Per-process-per-device aperture (`gpuvm_base/limit`), user picks VA | Per-VA-Space `uvm_range_tree`, mmap or `uvm_range_allocator` | Global counter `next_pool_va_base_` (0x100000000, 64MB stride) | Per-pool `[va_base, va_limit)` from sim-level buddy |
| **Allocator** | interval tree (rb-tree) for VA mapping; `drm_buddy` for VRAM physical | `uvm_range_tree` (rb-tree) for ranges; `uvm_range_allocator` (first-fit in range tree) for auto-alloc | Linear scan `find_first_fit` over `std::map` | `gpu_buddy` (buddy, first-fit by order) |
| **Alignment** | 4KB min (user-managed); VRAM 2MB align to reduce fragmentation | 2MB VA block; 4KB/64KB/2MB PTE chunk | 4KB (`align_up_4k`) | 4KB (GPU_BUDDY_MIN_BLOCK_SIZE) |
| **Huge page support** | amdgpu VM supports 2MB/1GB PTE; KFD doesn't force | UVM block = 2MB; GPU chunk 4K-2M | None | None (Phase 4 scope: 4K only) |
| **mmap backing** | DRM mmap (GTT/VRAM) or USERPTR (user mmap + HMM) | mmap(/dev/nvidia-uvm) → managed range (fault-on-demand) or eager (semaphore pool) | None (VA is synthetic integer) | One `mmap(MAP_ANONYMOUS\|MAP_PRIVATE)` at pool create |
| **Free semantics** | `amdgpu_vm_bo_unmap` + `drm_buddy_free` (auto-coalesce) | `uvm_range_allocator_free` (merge_prev/next) + `uvm_va_range_destroy` | `pool.allocated.erase(va)` (no reclaim) | `gpu_buddy_free` (auto-coalesce) |
| **Trim semantics** | None at kernel level (runtime-side pool cache) | None direct; pool cache in user runtime | No-op (`sim_mem_pool_trim` returns OK) | No-op (Phase 4; Phase 5+ add release threshold) |
| **Cross-process export** | DMA-buf (`amdgpu_gem_prime_export/import`) | POSIX FD / fabric handle (`cuMemPoolExportToShareableHandle`) | ADR-039 `pipe2(O_CLOEXEC)` (PoC, not real share) | TBD (Phase 5+: memfd or DMA-buf) |
| **Locking** | `drm_exec` + VM reservation lock; `drm_buddy` user-provided mutex | `uvm_spin_lock_t` in range_allocator; `rwsem` for va_space | None (single-threaded assumption) | `std::mutex` in sim wrapper |
| **Lifetime** | `amdgpu_vm` binds to `kfd_process_device`; IDR cleanup on exit | `uvm_va_space` binds to fd; destroyed on close | `pool_table_` global map, manual destroy | Per-pool buddy instance, destroyed with pool |

---

## §5. Specific Recommendation for UsrLinuxEmu

### Q1: Where should va_base come from?

**推荐: Option β — Per-device global `gpu_buddy` in sim layer**

Option β 镜像 **Nvidia UVM 的 `uvm_range_allocator` 模式**：一个全局 range allocator 负责选 VA base，每个 pool 从中分配一个 sub-range。AMD KFD 的 per-process aperture 模式（Option γ 的思路）在 UsrLinuxEmu 当前 `VASpace` struct 无 `va_base/va_limit` 字段的情况下需要 ~50 LOC 扩展 struct + ioctl + sim，超出 1 周预算的性价比。

**代码草图** (`plugins/gpu_driver/sim/sim_device_va_allocator.cpp`, 新文件, ~40 LOC):

```cpp
// sim_device_va_allocator.cpp — per-device global VA range allocator
#include "gpu_buddy.h"
#include <mutex>

namespace {
  constexpr uint64_t DEVICE_VA_BASE = 0x100000000ULL;  // 4 GiB
  constexpr uint64_t DEVICE_VA_SIZE = 0x400000000ULL;  // 16 GiB total
  struct gpu_buddy device_buddy_;
  std::mutex device_buddy_mutex_;
  bool initialized_ = false;
}

extern "C" int sim_device_va_alloc(uint64_t size, uint64_t *out_base) {
  std::lock_guard<std::mutex> lock(device_buddy_mutex_);
  if (!initialized_) {
    gpu_buddy_init(&device_buddy_, DEVICE_VA_BASE, DEVICE_VA_SIZE);
    initialized_ = true;
  }
  return gpu_buddy_alloc(&device_buddy_, size, out_base);
}

extern "C" int sim_device_va_free(uint64_t base) {
  std::lock_guard<std::mutex> lock(device_buddy_mutex_);
  return gpu_buddy_free(&device_buddy_, base);
}
```

**改动文件**:
- 新增 `plugins/gpu_driver/sim/sim_device_va_allocator.{cpp,h}` (~40 LOC)
- 修改 `plugins/gpu_driver/sim/mem_pool.cpp` `sim_mem_pool_create`：替换 `next_pool_va_base_` counter 为 `sim_device_va_alloc` 调用 (~5 LOC delta)
- 修改 `sim_mem_pool_destroy`：加 `sim_device_va_free(pool.props.va_base)` (~2 LOC)
- 修改 `plugins/gpu_driver/sim/CMakeLists.txt`：加入新 .cpp (~1 LOC)

**集成风险**: 低。`gpu_buddy` 的 `GPU_BUDDY_MAX_ORDER=21`（8GB max block），`DEVICE_VA_SIZE=16GB` 会产生 2 个 root block，完全在限制内。`GPU_BUDDY_MAX_RECORDS=4096` 足够 pool 数量。

### Q2: VA backing — mmap or just gpu_buddy_alloc?

**推荐: Option ii — One `mmap(MAP_ANONYMOUS|MAP_PRIVATE)` at pool create**

Option ii 镜像 **Nvidia UVM semaphore pool 的 eager mmap 模式**（`uvm_vm_open_semaphore_pool`，`uvm.c`）。AMD KFD 的 USERPTR 模式也类似（用户先 mmap，再传给内核）。Option iii（fault-on-demand）过于复杂，UVM managed range 才用，Phase 4 不需要。

**代码草图** (修改 `plugins/gpu_driver/sim/mem_pool.cpp`, ~25 LOC delta):

```cpp
// mem_pool.cpp — PoolTableEntry 新增 backing 字段
struct PoolTableEntry {
  sim_mem_pool_props_t props;
  std::map<uint64_t, PoolInternalEntry> allocated;
  uint64_t next_va_hint = 0;
  PoolAttrs attrs;
  void* mmap_base = nullptr;     // ← NEW: mmap backing for [va_base, va_limit)
  struct gpu_buddy pool_buddy_;  // ← NEW: per-pool buddy for sub-alloc
};

// sim_mem_pool_create 中：
int sim_mem_pool_create(sim_mem_pool_props_t *props, uint64_t *pool_handle_out) {
  // ... va_base from sim_device_va_alloc ...
  props->va_base  = va_base;
  props->va_limit = va_base + props->size;

  // NEW: mmap backing for the entire pool range
  void* backing = mmap(reinterpret_cast<void*>(va_base), props->size,
                       PROT_READ | PROT_WRITE,
                       MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED_NOREPLACE, -1, 0);
  if (backing == MAP_FAILED) return SIM_POOL_ERR_NOSPC;

  // NEW: init per-pool buddy
  gpu_buddy_init(&entry.pool_buddy_, va_base, props->size);
  entry.mmap_base = backing;
  // ...
}

// sim_mem_pool_alloc 中：
int sim_mem_pool_alloc(uint64_t pool_handle, uint64_t size, uint64_t *va_out) {
  // ... find pool ...
  uint64_t addr;
  int rc = gpu_buddy_alloc(&pool.pool_buddy_, size, &addr);  // ← buddy sub-alloc
  if (rc != 0) return SIM_POOL_ERR_NOSPC;
  *va_out = addr;
  return SIM_POOL_ERR_OK;  // addr is now dereferenceable via mmap backing
}
```

**生命周期管理**: `sim_mem_pool_destroy` 中 `munmap(entry.mmap_base, entry.props.size)`。

**内存开销分析**: 1 GiB pool mmap = 1 GiB virtual address space，但 `MAP_ANONYMOUS|MAP_PRIVATE` 是 **lazy physical page allocation**（Linux demand paging）。只有实际写入的 4KB page 才分配物理内存。1 GiB pool 若只写 1 MB，物理占用 ~1 MB。这完全可接受。

**关键注意**: 必须用 `MAP_FIXED_NOREPLACE`（Linux 4.17+）而非 `MAP_FIXED`，后者会静默覆盖已有映射。`va_base` 来自 `sim_device_va_alloc` 保证不与已有 pool 重叠，但需防与程序自身 .text/.data 重叠——`DEVICE_VA_BASE=0x100000000`（4 GiB）在 64 位 Linux 用户态是安全的高地址区。

### Q3: Per-pool or per-VA-Space gpu_buddy?

**推荐: Per-pool gpu_buddy 实例**

理由：
1. **Nvidia 模式支持**: UVM 的 `uvm_range_allocator` 是 per-semaphore-pool 的，不是 per-VA-space。每个 semaphore pool 创建时初始化自己的 range allocator。
2. **AMD 模式兼容**: AMD 的 `amdgpu_vm` 是 per-process-per-device，pool 概念在用户态。UsrLinuxEmu 的 per-pool buddy 等于 AMD 用户态 pool cache 的内核等价物。
3. **`VASpace` struct 无 va_base/va_limit**: 当前 `struct VASpace`（`gpgpu_device.h:88`）只有 `handle/page_size/flags/created_at/attached_queues`，没有 VA range 字段。若 per-VA-space buddy，需先扩展 VASpace（Option γ, ~50 LOC），再在 VASpace create 时 init buddy——超出 1 周预算。
4. **隔离性**: per-pool buddy 保证 pool 销毁时 buddy 自动释放，无跨 pool 碎片。

**trade-off**: per-pool buddy 无法支持 "pool 间共享 VA space 预留" 语义（CUDA 的 `cuMemPoolSetAccess` 跨 device）。但 Phase 4 不涉及跨 device pool access，Phase 5+ 可升级。

### Q4: Thread safety?

**推荐: sim wrapper 加 `std::mutex`**

`gpu_buddy` header（`libgpu_core/include/gpu_buddy.h:6-7`）明确："不进行任何内存分配，不自加锁。调用者负责外部同步。" ADR-020 决策 3 重申 "完全无锁"。

- **drm_buddy 模式**: `include/drm/drm_buddy.h:60` "Locking should be handled by the user, a simple mutex around drm_buddy_alloc* and drm_buddy_free* should suffice." — amdgpu/i915 都在 TTM manager 层加 `mutex`。
- **Nvidia UVM 模式**: `uvm_range_allocator_t` 内嵌 `uvm_spin_lock_t lock`，所有 alloc/free 持锁。

UsrLinuxEmu 当前 `mem_pool.h:12` 注释 "NOT required (single-threaded)"。但 `sim_mem_pool_alloc_async` 已有 fence_id 分配路径，未来多线程扩展可能性高。**建议在 sim wrapper 层加 `std::mutex`**，代价 ~5 LOC，防御性编程。

```cpp
// mem_pool.cpp PoolTableEntry 新增
struct PoolTableEntry {
  // ... existing fields ...
  std::mutex buddy_mutex;  // ← protects pool_buddy_
};

// sim_mem_pool_alloc 中：
std::lock_guard<std::mutex> lock(pool.buddy_mutex);
int rc = gpu_buddy_alloc(&pool.pool_buddy_, size, &addr);
```

---

## §6. Migration Path for Phase 4 (1 Week)

### Day 1: Design + ADR draft (4h)

- [ ] 写 ADR-058 "sim_mem_pool real VA via gpu_buddy + mmap backing" (~100 LOC)
  - Decision 1: Option β (per-device global buddy for va_base)
  - Decision 2: Option ii (mmap at pool create)
  - Decision 3: per-pool buddy instance
  - Decision 4: std::mutex wrapping
- [ ] 更新 `openspec/changes/2026-07-15-phase4-cu-mempool-alloc-real-va/proposal.md` §What Changes 加入 mmap + mutex 决策

### Day 2-3: Implement sim layer wrapper (2 days)

- [ ] 新建 `plugins/gpu_driver/sim/sim_device_va_allocator.{h,cpp}` (~50 LOC)
  - `sim_device_va_alloc(size, out_base)` / `sim_device_va_free(base)`
  - 内部 `struct gpu_buddy device_buddy_` + `std::mutex`
- [ ] 修改 `plugins/gpu_driver/sim/mem_pool.cpp` (~60 LOC delta):
  - `PoolTableEntry` 加 `void* mmap_base`, `struct gpu_buddy pool_buddy_`, `std::mutex buddy_mutex`
  - `sim_mem_pool_create`: 调 `sim_device_va_alloc` + `mmap(MAP_ANONYMOUS|MAP_PRIVATE|MAP_FIXED_NOREPLACE)` + `gpu_buddy_init`
  - `sim_mem_pool_alloc`: 调 `gpu_buddy_alloc` (替换 `find_first_fit`)
  - `sim_mem_pool_destroy`: 加 `munmap` + `sim_device_va_free`
  - `sim_mem_pool_free_async`: 调 `gpu_buddy_free`
  - `sim_mem_pool_reset_for_test`: 加 `sim_device_va_reset`
- [ ] 修改 `plugins/gpu_driver/sim/CMakeLists.txt` (1 LOC: 加新 .cpp)

### Day 4: Update tests + add dereferenceable smoke test (1 day)

- [ ] 修改 `tests/test_sim_mem_pool_standalone.cpp` (~40 LOC delta):
  - 加 VA range check: `REQUIRE(va >= props.va_base && va < props.va_limit)`
  - 加 dereferenceable smoke test: `*((volatile uint64_t*)va) = 0xDEADBEEF; REQUIRE(*(uint64_t*)va == 0xDEADBEEF)`
  - 加 first-fit 行为测试: 连续 alloc 3 块，验证地址递增
  - 加 free+realloc 复用测试: alloc A, free A, alloc B (same size), 验证 B == A
- [ ] 验证 existing 11 cases 不回归

### Day 5: Validate + commit (1 day)

- [ ] `cd build && cmake .. && make -j4 && ctest -R "test_sim_mem_pool" -V` 全绿
- [ ] `cd external/TaskRunner/build && ctest -R "test_cu_mem_pool"` 全绿（可能需调整 TaskRunner 侧 expectation）
- [ ] `tools/docs-audit.sh --strict` 无 warning
- [ ] commit: `feat(sim): real VA allocation in sim_mem_pool via gpu_buddy + mmap backing`

**总 LOC 估算**: ~150 LOC 新增 + ~60 LOC delta = ~210 LOC

---

## §7. Risks & Open Questions

### Risks

| # | Risk | Severity | Mitigation |
|---|------|----------|------------|
| 1 | `MAP_FIXED_NOREPLACE` 在某些旧内核（<4.17）不可用 | Medium | UsrLinuxEmu 目标 Ubuntu 20.04+（kernel 5.4+），已支持。fallback: 用 `mmap(NULL, ...)` 然后调整 `va_base` 到返回地址（放弃 fixed address，但破坏 `va_base` 可预测性） |
| 2 | `gpu_buddy` 的 `round_up_pow2` 导致大额内部碎片（e.g. 3 MB alloc → 4 MB block） | Low | Phase 4 pool alloc 通常是 2 的幂或 4K 对齐，碎片可接受。Phase 5+ 可加 trim 支持（`drm_buddy_block_trim` 模式） |
| 3 | 1 GiB pool mmap 在 32 位系统不可用 | Low | UsrLinuxEmu 要求 64 位（`uint64_t` VA），32 位不支持 |
| 4 | `sim_mem_pool_export_shareable` (ADR-039) 的 pipe2 fd 与 mmap backing 无关联，跨进程 import 后 VA 不可解引用 | Medium | Phase 4 acceptance criteria 不含 cross-process dereference。Phase 5+ 升级为 memfd + `mmap(shared_fd)` 实现真共享 |
| 5 | Per-pool buddy 的 `GPU_BUDDY_MAX_RECORDS=4096` 限制单 pool 最大 4096 个 allocation | Low | Phase 4 测试场景远低于此。超限时 `gpu_buddy_alloc` 返回 -1，sim 层转 `SIM_POOL_ERR_NOSPC` |

### Open Questions

1. **TaskRunner 侧 `test_cu_mem_pool` 是否已断言 VA 可解引用？** 若是，需确认 mmap backing 后 test 直接 PASS；若否，需加 TaskRunner 侧 smoke test。需 TaskRunner owner 确认。
2. **`DEVICE_VA_BASE=0x100000000` 是否与 UsrLinuxEmu CLI/tools 的自身 mmap 冲突？** 需在 Day 2 验证 `cat /proc/self/maps` 确认 4 GiB 区空闲。若冲突，改用 `0x200000000`（8 GiB）或 `mmap(NULL, ...)` + 动态 va_base。
3. **是否需要 ADR-058 正式编号？** 当前 ADR 最大 057，058 可用。但 ADR-020 已覆盖 libgpu_core 纯度约束，ADR-058 只需记录 sim 层的 mmap + mutex 决策。建议走 lightweight ADR。

---

**报告结束**

**Oracle 研究基础**: Linux v6.10 kernel source (torvalds/linux) + NVIDIA open-gpu-kernel-modules main branch + UsrLinuxEmu repo (commit fb75ed2, Stage 2)
