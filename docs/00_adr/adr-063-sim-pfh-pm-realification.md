# ADR-063: sim_pfh / sim_pm 真实化状态机边界

**状态**: ✅ Accepted（2026-07-15 Oracle 审查通过，5 项修订合并）
**日期**: 2026-07-15
**提案人**: Sisyphus（基于 Oracle+Metis Phase C 审查建议起草）
**评审者**: Oracle（2026-07-15 session `ses_09a32257dffeRfp9BR6eR1C4a7`，5 项 must-fix amendments 已合并）

**关联 ADR**:
- [ADR-036](adr-036-three-way-separation.md) ✅ 3 区分架构原则（**本 ADR 是其 ③ 层细化**）
- [ADR-018](adr-018-driver-sim-separation.md) ✅ 驱动/仿真分离（sim_pfh/sim_pm 在 ③，不直接调用 ②）
- [ADR-061](adr-061-hal-iommu-extension.md) ✅ HAL IOMMU ops（hal_iommu_map/unmap → sim_pm 是已有路由）
- [ADR-062](adr-062-hal-event-signal-extension.md) ✅ HAL Event Signal ops（hal_event_signal → kernel_workqueue → sim_signal_event）
- [ADR-060](adr-060-message-notification-threading.md) ✅ 线程架构（sim_pm invalidate 路径在 `flush_lock_` 保护下；锁延后 Phase 3）
- [ADR-027](adr-027-linux-compat-strategy.md) ✅ Linux 兼容层扩展策略（`sim_proxy.h` 为 ① µ-thin forward-prototype 头，不视为 violation）
- [ADR-011](adr-011-multiprocess-support.md) 🔄 提议中（C.2.3 经 G-C.0.3 降级 multi-thread single-process，本 ADR 无 ADR-011 阻塞风险）

**关联 Change**: `openspec/changes/2026-08-15-stage1-4-kfd-multi-file-integration/`（C-12，tasks.md **Phase C.1**）
**关联 Spec**: `docs/superpowers/specs/2026-07-15-phase-c-realification-contract.md`（本 ADR 的规格化输出）
**关联 SSOT**: `docs/05-advanced/kfd-portability-boundary.md` §3.2（IOMMU 集成层 Tier-2）

---

## Context

### C1: sim_pfh/sim_pm 是桩 — Stage 1.3 设计意图

Stage 1.3 UVM/HMM 阶段引入了 10 个 sim C 原语（page_fault_handler + page_migration），但实现是**最小可行桩**：

- `sim_pfh`: counter + last_addr + pfn=addr/4096（幻数）
- `sim_pm`: memcpy + bool flag + 裸 PFN map（offset/4096）

[ADR-036](adr-036-three-way-separation.md) Decision 3 明确 sim 层"模拟真实 GPU 硬件行为"，但 Stage 1.3 的桩实现仅满足编译通过，**不提供真实运行时行为**。

### C2: C-12 Phase B 后，sim ↔ KFD 通路已打通

| 通路 | Phase B 成果 | 状态 |
|------|-------------|------|
| hal_mock → sim_pm | `hal_iommu_map/unmap` → `sim_pm_migrate_to_device/system`（ADR-061） | ✅ |
| hal_mock → sim_event | `hal_event_signal` → kernel_workqueue → `sim_signal_event`（ADR-062） | ✅ |
| kfd_mmu → hal_iommu | `kfd_mmu.c` sync forwarder | ✅ |
| kfd_events → hal_event_signal | `kfd_events.c` async via kernel_workqueue | ✅ |

**结论**：通路已存在，但 sim_pfh/sim_pm 的桩行为让这些通路**不能产生真实端到端效果**。

### C3: 真实化的精确定义

Phase C 的真实化不是推翻重写，而是在现有 C ABI 不变（或最小追加）的前提下，**让桩行为具备可观测的运行时语义**。具体规格见 [phase-c-realification-contract.md](../../superpowers/specs/2026-07-15-phase-c-realification-contract.md)。

---

## Decision

### D1: sim_pfh 真实化 — 添加 event 通知路径（callback injection）

在 `sim_pfh_inject_fault_with_cause` 中：

1. **pfn 计算改为 `INVALID_PFN`**（不再返回 addr/4096 幻数）
2. **WRITE fault → 调用注册的回调**：若已通过 `sim_pfh_set_event_callback(pfh, cb)` 注册，则调 `cb(pasid, addr, cause)`；回调由 ② dispatcher 注入，其内部实现走 `hal_event_signal(pasid, ...)` → `kernel_workqueue` → `sim_signal_event`（ADR-062 D3 异步路径）
3. **READ fault → 仅记录，不通知**
4. **回调失败处理**：cb 返回负值时 `fault_count` 仍递增 + `last_fault_cause` 仍更新 + 日志记录；不回滚

**C ABI 不变**：所有现有 `sim_pfh_*` 函数签名零修改。新增：

```c
// page_fault_handler.h（追加）
typedef int (*sim_pfh_event_cb)(unsigned long pasid, unsigned long addr, int cause);
void sim_pfh_set_event_callback(struct sim_page_fault_handler *pfh,
                                sim_pfh_event_cb cb,
                                unsigned long pasid);
```

**新引入的状态**：
```cpp
struct SimPageFaultHandler {
    // ... 现有字段 ...
    int last_fault_cause;              // 已有，但之前未充分使用
    sim_pfh_event_cb event_cb;         // 新增
    unsigned long event_cb_pasid;      // 新增
};
```

**调用链**（③→③→HAL→②，保持 ADR-036 边界）：
```
sim_pfh_inject_fault_with_cause (③ sim)
    → event_cb(pasid, addr, cause) (③ → callback trampoline)
        → hal_event_signal(pasid, event_type, addr) (HAL fn-ptr, injected by ②)
            → kernel_workqueue → sim_signal_event (③ sim, 异步)
```

**约束**：
- sim_pfh 不持有 `gpu_hal_ops*` 引用 — 仅通过注入的 `sim_pfh_event_cb` 间接通知
- 若调用方未提供 event 回调（如 Tier-1 测试），sim_pfh 行为**回退到纯记录模式**（counter + addr 记录，不触发通知）
- 回调 `pasid` 由 `sim_pfh_set_event_callback` 传入，sim_pfh 自身不做 PASID 解析

### D2: sim_pm 真实化 — 3 态 page 状态机 + IOMMU 同步 + invalidation

**现有 C ABI 不变**，追加 4 个新函数：

```c
int  sim_pm_attach_domain(struct sim_page_migration *pm, void *domain);
void sim_pm_invalidate(struct sim_page_migration *pm, unsigned long offset);
int  sim_pm_is_page_dirty(struct sim_page_migration *pm, unsigned long offset);
void sim_pm_mark_dirty(struct sim_page_migration *pm, unsigned long offset);
```

**内部状态机**：

```
  PAGE_CLEAN ──(migrate_to_device)──→ PAGE_CLEAN
  PAGE_CLEAN ──(sim_pm_mark_dirty)──→ PAGE_DIRTY
  PAGE_DIRTY  ──(migrate_to_system)─→ PAGE_EVICTED
  PAGE_DIRTY  ──(sim_pm_invalidate)─→ PAGE_EVICTED
  PAGE_CLEAN  ──(sim_pm_invalidate)─→ PAGE_EVICTED
  PAGE_EVICTED──(migrate_to_device)─→ PAGE_CLEAN
```

**IOMMU 同步契约**（thin C extern prototype 路径）：

`sim_pm` 通过新增 ① 头 `include/kernel/sim_proxy.h` 获取 `iommu_map` / `iommu_unmap` 的 `extern "C"` 前置声明（不 include `iommu_internal.h`）：

```c
// include/kernel/sim_proxy.h（新文件，① 侧 µ-thin forward-prototype 头）
extern "C" {
struct iommu_domain;  // forward decl only
int  iommu_map(struct iommu_domain *domain, unsigned long iova,
               phys_addr_t paddr, size_t size, int prot);
size_t iommu_unmap(struct iommu_domain *domain, unsigned long iova, size_t size);
}
```

- `attach_domain(pm, domain)` 绑定 `iommu_domain*`（opaque `void*`）
- `migrate_to_device` 成功后：`iommu_map((struct iommu_domain*)pm->domain, va, paddr, size, prot)` — 通过 sim_proxy.h 的 extern C prototype 调用
- `migrate_to_system` 前：`iommu_unmap((struct iommu_domain*)pm->domain, va, size)` — 同上
- `invalidate(pm, offset)` 不触发 iommu 操作（IOTLB flush 由调用方 dma_remap 触发，见 D3）
- **dirty bit 触发者**：`sim_pm_mark_dirty` 是纯 setter API，由调用方注入回调触发；Phase C 仅验证 setter 可被调用，真实 HardwarePullerEmu 集成留 Phase 4/5
- `sim_pm` 自身 **不 include `iommu_internal.h`**（per ADR-027），`sim_proxy.h` 是 ① µ-thin forward-prototype 例外

### D3: IOTLB flush → sim_pm_invalidate 桥接（Plan A：iommu_unmap 快照）

**问题**：当前 `default_flush_iotlb` 被 `iommu_unmap` 调用时，被删 entry 已从 `iova_to_phys` 移除——无对象可迭代通知 sim_pm。

**Plan A 方案**（采纳）：`iommu_unmap` 在 erase 前**快照**被删 entry 的 `[iova, paddr]`，作为参数传给 `domain->ops->flush_iotlb(d, iova, size, *snapshot_list)`：

```cpp
// dma_remap.cpp iommu_unmap（修订）
std::vector<std::pair<unsigned long, phys_addr_t>> flushed;
auto it = state->iova_to_phys.find(iova);
if (it != state->iova_to_phys.end()) {
    flushed.push_back({it->first, it->second});
    state->iova_to_phys.erase(it);
}
if (domain->ops && domain->ops->flush_iotlb)
    domain->ops->flush_iotlb(domain, iova, size, &flushed);
```

`default_flush_iotlb` 在获取快照后遍历调用：

```cpp
// dma_remap.cpp default_flush_iotlb（尾部追加）
for (auto& [iova, paddr] : *flushed_entries) {
    if (state->sim_pm) {
        unsigned long offset = iova_to_offset(iova);
        sim_pm_invalidate(state->sim_pm, offset);
    }
}
```

**约束**：
- **仅 user-space page-table walk 路径**（非 vfio path）触发 `sim_pm_invalidate`
- **vfio path 不变**：vfio 成功时不触发 sim_pm 失效（per realification contract §3 item 2）
- `domain->sim_pm` 通过 `iommu_domain_attach_sim_pm(domain, pm)` 绑定
- `sim_pm_invalidate` 原型经 `include/kernel/sim_proxy.h` 声明（per D4 路径 X），dma_remap.cpp include 此 ① 头，**不 include ③ 的 page_migration.h**
- `flush_iotlb` ops 签名变更为追加 `void *flushed_entries`（opaque），后向兼容：旧调用方传 NULL 则仅做 page-table walk 不触发 sim_pm 失效

**锁策略**：Phase C 保持单线程语义（per `plugins/gpu_driver/sim/README.md` "All call sites run on the single-threaded UsrLinuxEmu driver dispatch path"）。`flush_lock_` 与 `mock_pm_lock_` 引入延后到 **Phase 3 多进程实施**。C.2.3 多线程测试仅验证 mm_shim 隔离逻辑，不验证锁竞争。

### D4: 分层边界 — 哪些在 ①，哪些在 ③

| 操作 | 位置 | 调用方向 | ADR 依据 | 原型声明策略 |
|------|------|---------|---------|------------|
| `sim_pfh_inject_fault` → event 通知 | ③→HAL→② | sim_pfh 调注入的 cb → cb 内部走 hal_event_signal | ADR-062 | callback injection（D1） |
| `sim_pm_migrate_*` → IOMMU 同步 | ③→① | sim_pm 通过 sim_proxy.h extern C 原型调 iommu_map/unmap | ADR-036 + ADR-061 | `include/kernel/sim_proxy.h`（D2） |
| `default_flush_iotlb` → `sim_pm_invalidate` | ①→③ | 直接调用（① 框架调 ③ sim primitive） | **本 ADR 决策** | `include/kernel/sim_proxy.h` 声明原型；dma_remap.cpp include sim_proxy.h，**不 include ③ 的 page_migration.h** |
| `iommu_domain_attach_sim_pm` | ①→③ | domain 持有 sim_pm 引用（opaque `void*`） | 同上 | `include/kernel/iommu/iommu_internal.h` 内 `struct sim_page_migration *sim_pm` forward decl |

**关键决策**：`default_flush_iotlb` → `sim_pm_invalidate` 是 **①→③ direct call**。为防止违反 ADR-036 line 67（"① 不直接包含 ②/③ 的头"），采用**路径 X — sim_proxy.h µ-thin forward-prototype 头**：

```c
// include/kernel/sim_proxy.h（新文件，① 侧 µ-thin forward-prototype 头）
// 仅含 extern "C" function prototypes + forward struct decls。
// 不暴露 ③ 内部实现细节（no include of page_migration.h internals）。
extern "C" {
struct sim_page_migration;  // forward decl only
int  sim_pm_attach_domain(struct sim_page_migration *pm, void *domain);
void sim_pm_invalidate(struct sim_page_migration *pm, unsigned long offset);
int  sim_pm_is_page_dirty(struct sim_page_migration *pm, unsigned long offset);
void sim_pm_mark_dirty(struct sim_page_migration *pm, unsigned long offset);
}
```

- `dma_remap.cpp`（①）include `include/kernel/sim_proxy.h`（① 头），**不 include `plugins/gpu_driver/sim/page_migration.h`**（③ 头）
- `sim_pm`（③）include `plugins/gpu_driver/sim/page_migration.h`（③ 头）声明同名 prototype + 实现
- Linker 解析：所有 extern 原型共享同一符号，sim 实现提供符号体
- `sim_pm_invalidate` 签名仅接受 `struct sim_page_migration*` 和 `unsigned long offset`，**不接受任何 ① 类型**

**ADR-027 豁免**：`sim_proxy.h` 是 ① 中对 ③ 的 µ-thin forward-prototype 头（仅 1 个 struct forward decl + 3 个 extern C 函数原型），不算 ADR-027 violation。

### D5: 实施范围 — 不做的

- 不做 per-page 全状态机（如 accessed bit、write-combining）
- 不做 NUMA-aware 迁移（单 device memory region）
- 不做 LRU eviction 策略（C.1 scope 内不做全局 page manager）
- 不做 sim_pfh per-process PASID 表（PASID 由调用方传入）
- 不做 ATS invalidation completion protocol（不在 C-12 scope）
- 不做 dirty bit 自动触发源（`sim_pm_mark_dirty` 是纯 setter，由调用方注入回调调用；HardwarePullerEmu 集成留 Phase 4/5）
- 不做真实多进程 IPC（C.2.3 经 mini-gate G-C.0.3 降级为 multi-thread single-process；ADR-011 保持 PROPOSED，不阻塞 Phase C）
- 不重写 `iommu_invalidate_register_notifier_internal`（mmu_notifier callback body 已在 Stage 1.4 Tier-2 实现，Phase C 不修改）
- 不做 Phase 3 级锁竞争（`flush_lock_` / `mock_pm_lock_` 留 Phase 3；Phase C 保持 single-threaded dispatch path 假设）

### D6: mm_shim wire-up（Phase C.2 架构决策）

> **来源**：[realification-contract §4](../../superpowers/specs/2026-07-15-phase-c-realification-contract.md)

`us_mm_shim` API 已在 Stage 2.1.2 完整实现（`src/kernel/uvm/mm_shim.cpp`），Phase C.2 重点是**集成**：

1. **kfd_process 绑定**：`kfd_process_create()` 中 `us_mm_shim_init(&shim, pid)` + 存入 `kfd_process->mm_shim` 字段（`void *` opaque，不暴露 `struct us_mm_shim` 内部）。`kfd_process_destroy()` 中释放。
2. **iommu_domain 绑定**：`kfd_process_create()` 中调用 `iommu_domain_attach_mm_shim(domain, &process->mm_shim)`（Stage 2.1.2 已提供 accessor），将 mm_shim 与 IOMMU domain 关联。
3. **VMA 自动注册**：KFD `MAP_MEMORY` handler 成功后调 `us_mm_shim_register_vma`（通过 iommu_domain 间接），`UNMAP_MEMORY` handler 中注销。
4. **边界隔离**：`drv/kfd/` **不直接** `#include "kernel/uvm/mm_shim.h"`（per ADR-027）。`kfd_process` 仅持 `void *mm_shim_cookie`，通过 `iommu_domain_attach_mm_shim` 间接绑定。
5. **容量**：`US_MM_SHIM_VMA_CAPACITY=16`，Phase C 测试不超此限制。
6. **一致性不变式**：`iommu_domain_state->sim_pm` 与 `iommu_domain_state->mm_shim` 绑定同一 `kfd_process`；`kfd_process_create` 同时绑定两者；`kfd_process_destroy` 解除所有绑定。

---

## Consequences

### 正面后果

- ✅ sim_pfh 的 event 通知通路打通，KFD events 模块可收到真正的 page fault 事件
- ✅ sim_pm 的 IOMMU 同步打通，`iommu_iova_to_phys` 可查迁移后的映射
- ✅ IOTLB flush 后 sim_pm 缓存同步失效，避免 stale mapping
- ✅ 3 区分边界保持：sim 层不持有 ① 类型引用（opaque pointer pattern）

### 负面后果

- ⚠️ `default_flush_iotlb` 直接调 `sim_pm_invalidate`，引入 ①→③ 耦合点
- ⚠️ sim_pm 3 态状态机增加了测试复杂度（3 倍于 2 态的状态组合）
- ⚠️ `iommu_domain_state` 新增 `sim_pm` 字段可能增加内存占用（pointer，可忽略）

### 风险

| 风险 | 等级 | 缓解 |
|------|------|------|
| ①→③ direct call 被滥用 | 🟡 中 | 仅限 `sim_pm_invalidate` + sim_proxy.h 中 3 个 extern C 原型；代码 review 检查 |
| sim_pm_invalidate 与 hal_mock 锁竞争 | 🟢 低 | Phase C 保持 single-threaded dispatch；锁引入延后 Phase 3（见 D3 锁策略） |
| dirty bit 语义与实际 GPU 行为不一致 | 🟢 低 | 仅用于 C-12 e2e 测试；蓝图终态真机 KFD 不依赖 sim_pm dirty bit |
| sim_pfh event 通知失败静默丢失 | 🟡 中 | 回调失败时 `fault_count` 仍递增 + `last_fault_cause` 仍更新 + 日志记录（D1 §4） |
| ADR-011 未 Accepted | 🟢 低 | 经 G-C.0.3 降级 multi-thread single-process；ADR-011 PROPOSED 不阻塞 Phase C |
| sim_proxy.h 原型与 sim 实现签名不同步 | 🟢 低 | 编译期 `-Wmissing-prototypes` + LSP 检查；`tools/ci/check_kfd_includes.sh --strict` 验证 |
| `iommu_unmap` flush_iotlb 签名变更破坏后向兼容 | 🟢 低 | 追加 `void *flushed_entries`（opaque），旧调用方传 NULL → 仅做 page-table walk |

---

## Migration / 实施步骤（Oracle 推荐顺序）

### Phase 1: sim_proxy.h 头 + iommu_domain_state 扩展（C-12 tasks.md C.1.3a.1）

1. 创建 `include/kernel/sim_proxy.h` — 4 个 extern C prototype（sim_pm_attach_domain / sim_pm_invalidate / sim_pm_is_page_dirty / sim_pm_mark_dirty）+ `struct sim_page_migration` forward decl
2. `iommu_domain_state` 追加 `struct sim_page_migration *sim_pm` 字段（可空）
3. `iommu_domain_attach_sim_pm(domain, pm)` accessor
4. 验证：`cmake --build build` 0 errors

### Phase 2: sim_pm 真实化（C-12 tasks.md C.1.2）

1. 追加 4 个新 API（attach_domain / invalidate / is_page_dirty / mark_dirty）
2. 实现 3 态状态机（PAGE_CLEAN / PAGE_DIRTY / PAGE_EVICTED）
3. `migrate_to_device/system` 内集成 IOMMU map/unmap（通过 sim_proxy.h extern C 原型）
4. `page_migration.h` 更新 API 声明
5. `page_migration.cpp` include `sim_proxy.h`

### Phase 3: sim_pfh 真实化（C-12 tasks.md C.1.1，可并行 Phase 2）

1. 修改 `page_fault_handler.cpp`：`pfn_out` 返回 `INVALID_PFN`
2. 追加 `sim_pfh_set_event_callback` / `sim_pfh_event_cb` typedef / 内部字段
3. WRITE fault 时调用注册的回调；回调失败时 `fault_count` 仍递增 + 日志

### Phase 4: IOTLB flush 扩展（C-12 tasks.md C.1.3a + C.1.3b）

1. `iommu_unmap` 改成 erase 前快照 → 传入 `flush_iotlb` ops（Plan A）
2. `default_flush_iotlb` 尾部追加 sim_pm_invalidate 遍历（dma_remap.cpp include sim_proxy.h）
3. 保持 vfio path 不变
4. 扩展 `test_iommu_invalidate_runtime_standalone` 追加 invalidation 路径断言

### Phase 5: mm_shim wire-up（C-12 tasks.md C.2.1，可并行 Phase 2-4）

1. `kfd_process.h` 追加 `void *mm_shim` 字段
2. `kfd_process_create/destroy` 绑定/解绑
3. `MAP_MEMORY` / `UNMAP_MEMORY` handler → VMA register/unregister

### Phase 6: 测试（C-12 tasks.md C.2.2 + C.2.3）

1. `test_mm_shim_standalone`：PID init / VMA register/unregister / 容量上限
2. `test_kfd_concurrent_processes_standalone`：multi-thread 隔离验证

---

## 关联检查清单

- [x] ADR-036 3 区分：①→③ direct call 仅限 `sim_pm_invalidate` + sim_proxy.h 中 3 个 extern C（签名无 ① 类型）；dma_remap.cpp include sim_proxy.h 而非 ③ 头
- [x] ADR-018 驱动/仿真分离：sim_pfh/sim_pm 不持有 drv/kfd/ 头文件引用
- [x] ADR-061 HAL IOMMU ops：sim_pm 不直接调 iommu_*，通过 sim_proxy.h extern C 原型
- [x] ADR-062 HAL Event Signal：sim_pfh event 通知走 callback injection → hal_event_signal → kernel_workqueue → sim_signal_event
- [x] ADR-060 线程架构：Phase C single-threaded dispatch；锁延后 Phase 3
- [x] ADR-027 sim_proxy.h 豁免：µ-thin forward-prototype 头（仅 1 struct fwd + 3 func prototypes），不视为 violation
- [x] ADR-011 多进程：经 G-C.0.3 降级 multi-thread single-process，本 ADR 无阻塞风险
- [ ] C-12 tasks.md §C 全部完成 + 99/99 ctest PASS（升级 ✅ Accepted 前置）

---

**维护者**: UsrLinuxEmu Architecture Team
**最后更新**: 2026-07-15（v1 初版；C-12 Phase C 审查后升级）