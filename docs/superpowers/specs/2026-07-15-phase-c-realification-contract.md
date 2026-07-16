# Phase C Realification Contract

> **目的**：精确定义 C-12 Phase C 中 sim_pfh/sim_pm/IOTLB/mm_shim 的"真实化"行为变更。
> **创建日期**：2026-07-15
> **状态**：✅ ACCEPTED（2026-07-16 C.0.1 review — Sisyphus Architecture Team lead: 5 段契约完整，ADR-063 D1-D6 全覆盖，边界隔离正确，10 条验收断言可测）
> **关联 ADR**：ADR-036（3 区分）、ADR-061（hal_iommu）、ADR-062（hal_event_signal）、ADR-063（本 spec 的 ADR 对应）
> **关联 Change**：`openspec/changes/2026-08-15-stage1-4-kfd-multi-file-integration/`

---

## §1. sim_pfh 真实化契约

### 当前行为（Phase B 基线）
- `sim_pfh_create(mm)` — 创建 handler，保存 `mm*` 指针
- `sim_pfh_inject_fault(pfh, addr, pfn_out)` — `fault_count++`，`last_fault_addr = addr`，`*pfn_out = addr / 4096`（幻数）
- `sim_pfh_get_fault_count(pfh)` — 返回 counter
- `sim_pfh_get_last_fault_addr(pfh)` — 返回 last_addr

### 真实化后行为
1. **pfn 计算**：非幻数。`pfn_out` 在映射存在时返回 `INVALID_PFN`（表示 GPU 侧不设 PFN，走 event 信号）；在有 backing 时返回真实 PFN。
2. **fault 分类**：引入 `fault_state`（READ/WRITE/INVALID），`inject_fault_with_cause` 根据 cause 写入状态。
3. **事件通知**：当注入 WRITE fault 时，若已通过 `sim_pfh_set_event_callback(pfh, cb, pasid)` 注册回调，则调用 `cb(pasid, addr, cause)`；回调由 ② dispatcher 注入，其内部实现走 `hal_event_signal(pasid, ...)` → `kernel_workqueue` → `sim_signal_event`（ADR-062 D3 异步路径）。READ fault 时仅记录，不触发回调。
4. **不变式**：`fault_count` 单调递增；`last_fault_addr`/`last_fault_cause` 原子更新（单线程环境下非原子可接受）。

### 不变式变更
- `fault_count > 0` ⇒ `last_fault_addr` 有效
- `last_fault_cause` ∈ {SIM_FAULT_CAUSE_READ, SIM_FAULT_CAUSE_WRITE}

### 不做的
- 不维护 per-page 状态机（per-page state 在 sim_pm 侧）
- 不连接 IOMMU iova_to_phys（sim_pfh 是请求接收方，不是映射验证方）
- 不做 per-process PASID lookup（PASID 由调用方传入）

---

## §2. sim_pm 真实化契约

### 当前行为（Phase B 基线）
- `sim_pm_create(device_mem_size)` — 分配 `device_memory` vector + `page_on_device` map
- `sim_pm_migrate_to_device(pm, offset, src, size)` — memcpy(src→device_memory[offset])，flag `page_on_device[offset]=true`，`page_table[offset]=offset/4096`（裸 PFN）
- `sim_pm_migrate_to_system(pm, offset, dst, size)` — memcpy(device_memory[offset]→dst)，flag `page_on_device[offset]=false`，erase `page_table[offset]`
- `sim_pm_is_page_on_device(pm, offset)` — 查 flag
- `sim_pm_lookup_pfn(pm, offset)` — 查 `page_table[offset]`
- `sim_pm_get_migration_count(pm)` — 返回 counter

### 真实化后行为
1. **IOMMU 同步**：`migrate_to_device` 成功后通过 `include/kernel/sim_proxy.h` 的 extern C 原型调用 `iommu_map(domain, va, paddr, size, prot)` 将新映射写入 ① IOMMU domain。`migrate_to_system` 前调用 `iommu_unmap(domain, va, size)` 清除映射。
   - **注意**：此调用路径为 ③→①，`sim_pm` 自身不直接持有 `iommu_domain*` 引用（opaque `void*`），通过 `sim_pm_attach_domain(pm, domain)` 间接绑定。sim_pm include `sim_proxy.h`（① µ-thin forward-prototype 头）获取 iommu_* 原型声明。
2. **dirty bit**：引入 `dirty_[offset]` flag。`migrate_to_device` 时 dirty=false；通过 `sim_pm_mark_dirty(pm, offset)` 显式标记 dirty=true（setter API，由调用方注入的回调触发；真实 HardwarePullerEmu 集成留 Phase 4/5）；`migrate_to_system` 前检查 dirty=true 才触发完整迁移，dirty=false 时直接清 flag。
3. **page 状态机**：从 2 态（ON_DEVICE / NOT_ON_DEVICE）升级为 3 态：
   - `PAGE_CLEAN`（已迁移，无脏数据）
   - `PAGE_DIRTY`（已迁移，有脏数据需写回）
   - `PAGE_EVICTED`（已迁移回系统，device slot 可被复用）
4. **invalidation 回调**：新增 `sim_pm_invalidate(pm, offset)`，供 IOTLB flush 路径调用（§3），将 `PAGE_DIRTY`→`PAGE_EVICTED`（强制回写）或 `PAGE_CLEAN`→`PAGE_EVICTED`（直接释放）。

### 新增 API
```c
int  sim_pm_attach_domain(struct sim_page_migration *pm, void *domain);
void sim_pm_invalidate(struct sim_page_migration *pm, unsigned long offset);
int  sim_pm_is_page_dirty(struct sim_page_migration *pm, unsigned long offset);
void sim_pm_mark_dirty(struct sim_page_migration *pm, unsigned long offset);
```

### 不变式变更
- `page_on_device[offset]==false` ⇒ `page_table` 无该 entry 且 `dirty_[offset]==false`
- `page_on_device[offset]==true` ⇒ `migration_count >= 1`
- `sim_pm_attach_domain` 成功后 `domain` 非 NULL ⇒ `migrate_to_device` / `migrate_to_system` 触发的 IOMMU 同步使用该 domain

### 不做的
- 不做 LRU eviction 策略（单 page 粒度，不引入全局 eviction manager）
- 不做 NUMA-aware migration（单 device memory region）
- 不做 `sim_pm` 内部 `iommu_map/unmap` 调用（通过 hal_mock 间接，AD-018/036 边界保持）

---

## §3. IOTLB Flush 扩展契约

### 当前行为（Phase B 基线，commit `62d2353`）
- `default_flush_iotlb(d, iova, size)` 已做：
  1. vfio opt-in：若 `vfio_container` 可用，调 `us_iommu_vfio_invalidate(iova, sz)` 做真实硬件 invalidation
  2. user-space page-table walk：遍历 `state->iova_to_phys` 在 `[iova, iova+sz)` 范围内，删除 entry，计数
  3. 注释标注 "Tier-2 penetrated: 2026-07-05"

### 扩展行为
1. **sim_pm 失效通知**：在 page-table walk 内，每发现并删除一个 IOVA entry 后，调用 `sim_pm_invalidate(pm, offset)` 清除 sim_pm 侧对应 page 的 device 缓存状态（PAGE_DIRTY→PAGE_EVICTED 或 PAGE_CLEAN→PAGE_EVICTED）。
   - **绑定**：`iommu_domain_state` 需持有 `sim_page_migration*` 引用（通过 `iommu_domain_attach_mm_shim` 扩展路径或新 `iommu_domain_attach_sim_pm` accessor）。
2. **vfio path 不变**：vfio 成功路径不改动（真实硬件 invalidation 已做，不触发 sim_pm 失效）。
3. **原子性**：page-table walk + sim_pm_invalidate 在 `flush_lock_` 下执行，保证与 `iommu_map/unmap` 互斥。

### 新增依赖
- `iommu_domain_state` 新增字段 `struct sim_page_migration *sim_pm`（可空）
- `iommu_domain_attach_sim_pm(domain, pm)` accessor

### 不变式变更
- flush 后 `sim_pm_is_page_on_device(iova_to_offset)` ⇒ `false`
- `iova_to_phys` entry 与 `sim_pm.page_on_device` **同步清除**

### 不做的
- 不做 TLB shootdown broadcast（单 domain 场景）
- 不做 ATS invalidation completion protocol（不在 C-12 scope）

---

## §4. mm_shim Wire-Up 契约

### 当前行为（Phase B 基线）
- `mm_shim.h/.cpp` 已有完整 API（init/register_vma/unregister_vma/find_vma/foreach_in_range）
- `US_MM_SHIM_VMA_CAPACITY=16`，静态数组，无锁
- **零调用方**在 `drv/kfd/` 内（仅 `test_mmu_notifier_callback_runtime_standalone` 测试用）
- `iommu_domain_state` 已有 `mm_shim` 字段 + `iommu_domain_attach_mm_shim` accessor

### Wire-Up 后行为
1. **kfd_process 绑定**：`kfd_process_create()` 中创建 `struct us_mm_shim` 实例，`us_mm_shim_init(&shim, pid)`，存入 `kfd_process->mm_shim`。`kfd_process_destroy()` 中释放。
2. **iommu_domain 绑定**：`kfd_process_create()` 中调用 `iommu_domain_attach_mm_shim(domain, &process->mm_shim)`，将 mm_shim 与进程关联。
3. **VMA 自动注册**：在 KFD `MAP_MEMORY` handler（`gpu_ioctl_map_memory`）成功后将 VA range 注册到 `us_mm_shim_register_vma`。在 `UNMAP_MEMORY` handler 中注销。
4. **边界隔离**：`drv/kfd/` 不直接 `#include "kernel/uvm/mm_shim.h"`（违反 ADR-027）。改为通过 `iommu_domain_attach_mm_shim` 间接绑定 — `kfd_process` 持 opaque `void *mm_shim_cookie`，KFD 侧不访问 `struct us_mm_shim` 内部字段。

### kfd_process.h 扩展
```c
// plugins/gpu_driver/drv/kfd/kfd_process.h（追加）
struct kfd_process {
    // ... 现有字段（pasid, is_32bit_user_mode, 等）
    void *mm_shim;  // opaque, bound via iommu_domain_attach_mm_shim
};
```

### 不变式变更
- `kfd_process_create()` 后 `kfd_process->mm_shim != NULL`
- `iommu_domain_attach_mm_shim` 后 `iommu_domain_state->mm_shim` 指向 kfd_process 的 mm_shim
- `kfd_process_destroy()` 后 `mm_shim.pid == 0`（清零语义）
- C.2.3 并发测试：两个不同 PID 的 kfd_process 各自有独立 mm_shim，一方 unmap 不影响另一方

### 不做的
- 不做真实 linux `mm_struct` 内核类型对齐（`struct us_mm_shim` 是 user-space 简化版）
- 不做 VMA 合并或自动扩展（capacity=16 够用 Phase C 测试）
- 不做 `iommu_domain_attach_mm_shim` 到 hal_mock 的桥接（mm_shim 仅在 ① 内核层，② 通过 iommu_domain 间接访问）

---

## §5. 验收标准（极简版）

| 契约 | 验收方式 | 断言示例 |
|------|---------|---------|
| sim_pfh evt通知 | `sim_pfh_set_event_callback(pfh, cb, pasid)` → `sim_pfh_inject_fault(pfh, addr, &pfn, SIM_FAULT_CAUSE_WRITE)` → cb 被调用 | `CHECK(event_cb_called == 1)` |
| sim_pfh pfn非幻数 | 无映射时 `*pfn_out == INVALID_PFN` | `CHECK(*pfn == INVALID_PFN)` |
| sim_pfh cb失败回退 | cb 返回 -1 → fault_count 仍递增 | `CHECK(fault_count == 1)` |
| sim_pm iommu同步 | `sim_pm_migrate_to_device(pm, offset, src, sz)` → `iommu_iova_to_phys(va)` 可查 | `CHECK(iommu_iova_to_phys(va) != INVALID_PFN)` |
| sim_pm dirty bit | 迁移后 dirty=false；sim_pm_mark_dirty(offset)→dirty=true | `CHECK(!sim_pm_is_page_dirty(pm, offset))` |
| sim_pm invalidation | `sim_pm_invalidate(pm, offset)` → `sim_pm_is_page_on_device(pm, offset)==false` | `CHECK(!sim_pm_is_page_on_device(pm, offset))` |
| IOTLB→sim_pm | `default_flush_iotlb(d, iova, sz)` → sim_pm 侧 page_on_device==false | `CHECK(!sim_pm_is_page_on_device(pm, offset))` |
| mm_shim wire-up | `kfd_process_create(pid=0x1001)` → `process->mm_shim` 可查 | `CHECK(process->mm_shim != NULL)` |
| mm_shim 隔离 | PID A unmap 后 PID B 的 VMA 仍在 | 见 C.2.3 TEST_CASE |
| mm_shim 容量 | capacity=16 内正常，第 17 个 VMA → -ENOSPC | `CHECK(us_mm_shim_register_vma(&m, ...) == -ENOSPC)` |

---

## §6. 关联 Gate 项

| Gate | 产出物 | 状态 |
|------|--------|------|
| G-C.0.1 | 本文档 | ✅ ACCEPTED（2026-07-16，Sisyphus Architecture Team lead sign-off）|
| G-C.0.2 | ADR-063（sim_pfh/sim_pm 真实化）| 📋 待创建 |
| G-C.0.3 | ADR-011 决策 | 📋 待决策 |
| G-C.0.4 | tasks.md §C 重排 | 📋 待重排 |
| G-C.0.5 | test_iommu_invalidate_runtime_standalone 骨架 | 📋 待创建 |

---

**维护者**：UsrLinuxEmu Architecture Team
**最后更新**：2026-07-15
