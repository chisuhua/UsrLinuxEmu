# ADR-062: HAL Event Signal ops 扩展（KFD 事件通知桥接）

**状态**: 📋 PROPOSED（2026-07-14，C-12 启动后由 Architecture Team 评审）
**日期**: 2026-07-14
**提案人**: Sisyphus（基于 C-12 tasks.md B.4.4 起草）
**评审者**: UsrLinuxEmu Architecture Team（待签字 per ADR-035 §R2.3；正式签字待 owner 复核 + 配套实现 B.4.4.1-B.4.4.4 完成）

**关联 ADR**:
- [ADR-023](adr-023-hal-interface.md) ✅ HAL 接口契约（**本 ADR 是其扩展**，per Decision 4 spec-driven 扩展）
- [ADR-018](adr-018-driver-sim-separation.md) ✅ 驱动/仿真分离（HAL 是 ②③ 之间桥接）
- [ADR-035](adr-035-governance-policy.md) ✅ 治理规则（本 ADR 自身走此规则）
- [ADR-036](adr-036-three-way-separation.md) ✅ 3 区分架构原则（HAL 桥不破坏分层）
- [ADR-027](adr-027-linux-compat-strategy.md) ✅ Linux 兼容层扩展策略（spec-driven；HAL ops 与 linux_compat 同步）
- [ADR-059](adr-059-kfd-multi-file-integration.md) ✅ KFD 多文件集成（**C-12 本体**，D3 决策明确 HAL ops 扩展单独走 ADR）
- [ADR-060](adr-060-message-notification-threading.md) ✅ 线程架构（**events 异步必依赖** `kernel_thread_base` + `kernel_workqueue`）
- [ADR-061](adr-061-hal-iommu-extension.md) 📋 PROPOSED（**姊妹 ADR**，同 C-12 同步评审；IOMMU 走 061，event signal 走 062）

**关联 Change**: `openspec/changes/2026-08-15-stage1-4-kfd-multi-file-integration/`（C-12，tasks.md **B.4.4**）
**关联设计文档**: `docs/05-advanced/kfd-multi-file.md`（C-12 Phase A.1 §3.1 模块依赖图）
**关联 ABI 对比报告**: `docs/05-advanced/kfd-abi-comparison-report.md`（C-12 Phase A.2 hard gate 产物；§6 启动顺序明确 B.4.4 在 B.4.6 之后）
**关联 TADR**: 无直接 TADR（HAL ops 是 UsrLinuxEmu 内部 HAL 桥扩展，不跨仓 ABI）
**关联历史**:
- [kfd-portability-boundary.md v1.2 §3](../05-advanced/kfd-portability-boundary.md) — Stage 1.4 Tier-1 deferred event notification
- [kfd-portability-report.md §4.2](../05-advanced/kfd-portability-report.md) — GCC 13 pthread bug（**events 异步路径规避方案来源**）
- [gpu_driver_architecture.md §HardwarePullerEmu](../05-advanced/gpu_driver_architecture.md) — 单线程 std::thread 参考（ADR-060 §1.4 复用模式）
- [plugin-development.md §3.4.4](../05-advanced/plugin-development.md) — 后台线程生命周期守则（ADR-060 §2.2 引用）

---

## Context

### C1: 现状 — KFD event notification 路径在 ADR-023 中缺失

[ADR-023](adr-023-hal-interface.md) 当前定义 **11 个** HAL ops 中，**事件通知路径仅有** `interrupt_raise`（弹射式触发 MSI-X 中断），但**无** 事件信号到 KFD user queue 的桥接。真实 Linux KFD 路径：

```
[GPU 硬件事件，如 page fault / eviction / fault / process exit]
    ↓
amdgpu KFD 内部: kfd_signal_event(event_id, events)
    ↓
amdgpu_kfd_event_page_set 写入 user-mapped event page
    ↓
用户态 KFD runtime (libamdhip64.so / libhsa-runtime64.so) 通过 mmap 读到 event
```

C-12 在 `plugins/gpu_driver/drv/kfd/kfd_events.c` 实施时将调用：

| 真实接口（Linux 6.12 LTS amdgpu）| 触发场景 | 当前 UsrLinuxEmu 支持 |
|--------------------------------|---------|---------------------|
| `kfd_signal_event(event_id, events)` | KFD 事件信号（如 VM fault）| ❌ 无（需 HAL 桥）|
| `kfd_event_page_set(kfd_process*, event_id)` | 写入 user-mapped event page | ❌ 无（需 HAL 桥）|
| `kfd_event_work_handler`（workqueue）| 异步事件分发 | ❌ 无（需 `kernel_workqueue`）|

不解决此 HAL 桥接 + 异步分发架构，`kfd_events.c` 实施时既违反 ADR-018（②→③ 直接调用），也无法正确模拟 Linux KFD 异步事件机制。

### C2: 与 ADR-060 线程架构的硬依赖

[ADR-060](adr-060-message-notification-threading.md) §2.1 明确 C-12 6 模块 sync/async 边界：

| 模块 | 同步/异步 | 原因 |
|------|----------|------|
| `kfd_module.c` | sync | 仅 module_init/exit，无运行时 |
| `kfd_process.c` | sync | 进程生命周期，无长时任务 |
| `kfd_pasid.c` | sync | PASID 分配是即时操作 |
| `kfd_dispatch.c` | sync | ioctl 派发必须同步 |
| `kfd_mmu.c` | sync（async opt-in）| 同步 + `kfd_mmu_get_workqueue()` accessor 暴露 |
| **`kfd_events.c`** | **async** | **事件通知是经典 workqueue 场景**（Linux 真实驱动语义）|

**结论**：`hal_event_signal()` 必须在异步上下文中执行（与 IOMMU 不同 — IOMMU 可选 sync/async），否则会阻塞其他 ioctl 处理。**这意味着本 ADR 与 ADR-060 是硬依赖**：实施前必须先完成 ADR-060 §1.1 `kernel_thread_base` + `kernel_workqueue` 基础设施。

✅ C-12 tasks.md B.1.10.1-B.1.10.10 已交付 `kernel_thread_base` + `kernel_workqueue` PoC + 10 个 TEST_CASE 通过（2026-07-14 commit `edb4ba3`），**本 ADR 实施的前置条件已就绪**。

### C3: 与 ADR-023 扩展规则的契合

ADR-023 Decision 4 显式说明 HAL 接口扩展应"预留扩展空间，Phase 2 可新增但不修改现有函数签名"。本 ADR 严格遵循此规则：

- ✅ 仅在 `struct gpu_hal_ops` 末尾**追加** 1 个新 fn-ptr（不修改现有 11 个 + ADR-061 即将加的 2 个）
- ✅ 现有 11 个 HAL ops 签名零修改
- ✅ 现有 `hal_user.cpp` / `hal_mock.cpp` 调用方零修改
- ✅ 移植到真机时新增 fn-ptr 留空不影响
- ✅ **条件 4 验证（组合不可行性）**：见 §D6 组合不可行性证据

**与 ADR-061 的关系**：
- ADR-061 加 2 个 fn-ptr（IOMMU：`iommu_map` / `iommu_unmap`）
- ADR-062（本 ADR）加 1 个 fn-ptr（event signal：`event_signal`）
- C-12 实施时建议在**同一 commit** 中同步追加（避免 `struct gpu_hal_ops` 反复改动）
- 但走**两个独立 ADR**（per ADR-059 D3 + ADR-035 §R3）— 因为 IOMMU 与 event signal 用途、错误码、依赖关系完全不同

### C4: 设计约束（继承自既有 ADR）

1. **ADR-036 3 区分原则**：event signal op 仍走 HAL 桥
2. **ADR-018 物理隔离**：KFD 代码只能调 HAL 函数指针
3. **ADR-023 spec-driven 扩展**：仅按 C-12 KFD 必需添加
4. **ADR-027 spec-driven**：HAL ops 与 `linux_compat` 增量补齐配套
5. **ADR-035 治理规则**：HAL ops 变更单独走 ADR
6. **ADR-059 D3**：KFD HAL ops 扩展必须"每个新增 op 有 ADR"
7. **ADR-060 §2.1 events 异步决策**：event_signal 必须在 `kernel_workqueue` 上下文中执行

---

## Decision

### D1: 在 `struct gpu_hal_ops` 末尾追加 1 个 fn-ptr

```c
// plugins/gpu_driver/hal/gpu_hal_ops.h（追加；与 ADR-061 同步提交）
struct gpu_hal_ops {
    void *ctx;

    /* ... 现有 11 个 fn-ptr（ADR-023 定义）... */

    /* --- ADR-061 扩展（2026-07-14, KFD page migration）--- */
    int  (*iommu_map)(void *ctx, u64 va, u64 size, u32 domain_id);
    int  (*iommu_unmap)(void *ctx, u64 va, u64 size);

    /* --- ADR-062 扩展（2026-07-14, KFD event signal）--- */
    int  (*event_signal)(void *ctx, u32 pasid, u32 event_id, u64 events);
};
```

**签名设计**：
- 返回 `int`（0 成功，负值 Linux 错误码；与 ADR-023 Decision 4 + ADR-061 D1 一致）
- `void *ctx` 透传
- `u32 pasid` 标识目标进程（与 `kfd_process` 数据结构对齐）
- `u32 event_id` 标识 event slot（KFD 真实驱动语义：`event_id` ∈ [0, KFD_EVENT_MAX_EVENTS)）
- `u64 events` 64-bit event mask（与 `KFD_EVENT_TYPE_*` 8 种类型对齐）

### D2: 新增 1 个 inline wrapper

```c
// plugins/gpu_driver/hal/gpu_hal.h（追加，零修改现有）
static inline int hal_event_signal(struct gpu_hal_ops *hal, u32 pasid, u32 event_id, u64 events) {
    return hal->event_signal(hal->ctx, pasid, event_id, events);
}
```

### D3: 三种实现策略

| 实现文件 | 路由 | 用途 |
|---------|------|------|
| `hal_mock.cpp` | `event_signal` → 投递到 `kfd_events_thread_` 的 `kernel_workqueue`（**关键异步路径**）<br/>workqueue item 调 `sim_signal_event(pasid, event_id, events)` | C-12 unit tests 路径（MockGpuDriver）|
| `hal_user.cpp`（C-12 新增）| 真机 KFD 路径：直接调 `kfd_signal_event` / `kfd_event_page_set`<br/>**当前 stage**：桩实现（`return -ENOSYS`）| 蓝图终态真机 KFD 移植用 |
| `hal_user.cpp`（当前 stage 既有）| 已有 11 个 fn-ptr 实现，**不动** | 现有 GpgpuDevice 路径 |

**关键设计 — `hal_mock.cpp` 异步投递**：
```cpp
// hal_mock.cpp 伪代码（具体实现 B.4.4.2）
int hal_mock_event_signal(void *ctx, u32 pasid, u32 event_id, u64 events) {
    auto* kfd_events_wq = kfd_events_get_workqueue();  // B.4.6 accessor
    if (!kfd_events_wq) return -ENOSYS;
    kfd_events_wq->enqueue([pasid, event_id, events]() {
        sim_signal_event(pasid, event_id, events);  // 同步执行实际事件信号
    });
    return 0;  // 立即返回（异步）
}
```

**为什么必须异步**：
- 真实 Linux KFD 路径：`kfd_signal_event` 在 workqueue 上下文执行（避免阻塞调用方）
- 同步执行会阻塞 ioctl 处理线程，影响其他进程 KFD 请求
- `sim_signal_event` 本身有 mutex 保护（per sim 内部），不需要调用方额外同步

### D4: 与 ADR-060 events 异步决策的协同

按 ADR-060 §2.1 events 异步决策：
- `kfd_events.c` 后台线程基于 `kernel_thread_base`（tasks.md B.4.6.1）
- `kfd_events_thread_` 接收 workqueue item 并调 `runLoop()`（B.4.6.4）
- `kfd_events_queue_` 用 `pthread_mutex_t` + `pthread_cond_t` 保护（per ADR-059 R-5：KFD 内部统一 C 习语，不用 std::mutex）

本 ADR 的 `event_signal` HAL op 是**触发器**，实际工作通过 workqueue 异步分发到 `kfd_events_thread_`。

**Day-1 (C-12 Phase B.4.4)**：`event_signal` 投到 workqueue → `kfd_events_thread_` 取出 → 调 `sim_signal_event` → 模拟事件信号完成
**Future (C-12 Phase C/E)**：可扩展为多 event queue 优先级（preempt vs normal）— **不在本 ADR 范围**

### D5: 错误码语义

| 错误码 | 触发场景 |
|--------|---------|
| `0` | 成功，event 已入队 workqueue |
| `-EINVAL` | pasid 未注册 / event_id 越界 / events mask 为 0 |
| `-ENOMEM` | workqueue 分配失败（极端情况）|
| `-ENOSYS` | `hal_user.cpp` 真机桩（暂不实现）|
| `-EAGAIN` | kfd_events_thread_ 未启动（kfd_module_init 失败）|

---

## Consequences

### 正面后果

- ✅ `kfd_events.c` 可通过 HAL 桥触发 `sim_signal_event` 异步分发，C-12 Phase B.4 编译可过
- ✅ 异步路径（`kernel_workqueue`）保证 KFD event 不阻塞其他 ioctl
- ✅ `struct gpu_hal_ops` 11 → 13（IOMMU 061）→ 14（event 062）fn-ptr，遵循 ADR-023 扩展"追加不改"原则
- ✅ 与 ADR-060 硬依赖关系明确：events 异步路径在 B.1.10 thread PoC 完成后才能实施本 ADR
- ✅ `hal_user.cpp` 桩实现留有真机 KFD 部署接入点

### 负面后果

- ⚠️ `hal_event_signal` 异步语义与现有 11 个 fn-ptr 的同步语义不同（需要调用方理解）
- ⚠️ 单元测试需要 mock workqueue（per tasks.md B.4.6.6 TSan 覆盖）— 增加测试复杂度
- ⚠️ 错误码 `-EAGAIN` 语义新增（其他 HAL ops 没用过），需调用方正确处理
- ⚠️ `sim_signal_event` 必须 thread-safe（mutex 保护）— sim 内部需配套

### 风险

| 风险 | 等级 | 缓解 |
|------|------|------|
| `kernel_workqueue` 未就绪时实施 B.4.4 | 🟢 低 | C-12 tasks B.1.10 已 ✅ 交付（commit `edb4ba3` 2026-07-14）|
| `hal_user.cpp` 桩忘记实现导致编译失败 | 🟢 低 | CMake 检查 `hal_user.cpp` 与 `struct gpu_hal_ops` 字段数一致（per ADR-023 既有约束）|
| `kfd_events_thread_` 未启动导致 `-EAGAIN` | 🟡 中 | `kfd_module_init` 必须先于 ioctl 派发路径（per tasks.md B.4.6.5）|
| `sim_signal_event` race condition | 🟡 中 | TSan 测试覆盖（tasks.md B.4.6.6 明确）|
| 异步 event signal 跨进程泄漏 | 🟡 中 | 单元测试 `test_kfd_events_standalone` + `test_kfd_concurrent_processes_standalone` 覆盖（tasks.md B.4.5 + E.0.3）|

---

## Migration / 实施步骤

### Phase 1: 头文件扩展（C-12 tasks.md **B.4.4.1**）

1. 修改 `plugins/gpu_driver/hal/gpu_hal_ops.h`，在 `struct gpu_hal_ops` 末尾追加 1 个 fn-ptr（**与 ADR-061 的 2 个 fn-ptr 同一 commit**）
2. 在 `plugins/gpu_driver/hal/gpu_hal.h` 添加 1 个 inline wrapper
3. 验证：现有 GpgpuDevice / HardwarePullerEmu 等调用方零修改

### Phase 2: hal_mock.cpp 完整实现（tasks.md **B.4.4.2**）

1. 实现 `event_signal(pasid, event_id, events)` → 投递到 `kfd_events_thread_` 的 `kernel_workqueue`
2. workqueue item 调 `sim_signal_event(pasid, event_id, events)`
3. 错误码透传：workqueue 入队失败 → `-ENOMEM`；`kfd_events_thread_` 未启动 → `-EAGAIN`
4. **必须先有 `kfd_events_get_workqueue()` accessor**（per tasks.md B.4.6.5）

### Phase 3: hal_user.cpp 桩实现（tasks.md **B.4.4.3**）

1. 在 `hal_user.cpp` 末尾追加 1 个 fn-ptr 实现：
   ```cpp
   int hal_user_event_signal(void *ctx, u32 pasid, u32 event_id, u64 events) {
       (void)ctx; (void)pasid; (void)event_id; (void)events;
       return -ENOSYS;  // 真机 KFD 路径，C-12 阶段不实施
   }
   ```
2. 标注"Blueprint 终态 — 真机 KFD kfd_signal_event 桥接（per ADR-036 蓝图 §蓝图验收第 1-2 条）"

### Phase 4: MockGpuDriver 测试覆盖（tasks.md **B.4.4.4**）

1. 更新 `plugins/gpu_driver/shared/mock_gpu_driver.cpp` 字段
2. 添加 1 个 mock 实现（默认 `return 0`；可注入错误码用于 negative tests）
3. 验证：`test_kfd_events_standalone` 通过

### Phase 5: 关联基础设施（per ADR-060 §2.1）

1. **前置依赖**：tasks.md B.1.10 线程基础设施（已 ✅ 交付）
2. **同步实施**：tasks.md B.4.6 kfd_events 后台线程（与 B.4.4.2 同步提交）
3. **依赖关系**：B.4.4.2 → B.4.6.5（kfd_module_init/exit 必须调 kfd_events_thread_ start/stop）

### Phase 6: 治理文档同步

1. 本 ADR 创建 + 升级 ✅ Accepted（owner 签字后）
2. `docs/00_adr/README.md` 状态分布表更新（PROPOSED → Accepted 时）
3. `docs/00_adr/README.md` 关系图更新：在"Linux 内核消息通知线程架构"子树之后加 "KFD HAL ops 扩展" 子树（含 ADR-061 + ADR-062）
4. `kfd-abi-comparison-report.md` §6 启动顺序核对：确认 B.4.4 在 B.4.6 之后（依赖 `kfd_events_get_workqueue()`）

---

## 关联检查清单

- [x] ADR-023 Decision 4 spec-driven 扩展：本 ADR 严格"追加不改"
- [x] ADR-035 §R2 状态标记：本 ADR 从 📋 PROPOSED 启动
- [x] ADR-035 §R3 治理：本 ADR 是 C-12 tasks B.4.4.5 流程的产物
- [x] ADR-059 D3：每个新增 HAL op 有 ADR（本 ADR 覆盖 B.4.4 的 1 个 op）
- [x] ADR-059 D3 条件 4 组合不可行性验证：见 §D6
- [x] ADR-060 §2.1：events 异步依赖 `kernel_workqueue`（B.1.10 已交付）
- [x] ADR-060 硬依赖：与 ADR-061 同步提交 fn-ptr 扩展到 `struct gpu_hal_ops`
- [ ] C-12 tasks B.4.4.1-B.4.4.5 + B.4.6 全完成（升级 ✅ Accepted 前置）

---

### D6: 条件 4 组合不可行性证据（per ADR-059 D3 条件 4）

按 ADR-059 D3 条件 4 要求，证明 `hal_event_signal` 不能用 ≤ 5 行 wrapper 通过现有 11 个 ops 组合实现：

```c
// 目标语义: KFD 事件信号投递（含 user-mapped event page 写入 + 异步分发）
//         涉及 workqueue 异步 + 用户态 event page mmap 区域写入
//
// 尝试组合现有 ops:
// 1. hal_interrupt_raise(irq_no) + 用户态 poll
//    → 缺陷：中断路径与 KFD event page 语义不同；KFD event 通过 mmap 暴露，
//      不是 MSI-X 中断（用 interrupt_raise 触发的是 PCI 中断，不是 event page 写入）
//    → 多步操作无法保证 event page 写入 + 信号投递的原子性
// 2. hal_mem_write(event_page_addr, event_data, sizeof)
//    → 需要知道 KFD event page 物理地址 → 违反 3 区分（② 知道 ③ 内部地址）
//    → 且 KFD event signal 必须 workqueue 异步分发（per ADR-060 §2.1），
//      hal_mem_write 是同步操作，无法表达异步语义
//
// 结论: 不可行
```

---

**维护者**: UsrLinuxEmu Architecture Team
**最后更新**: 2026-07-14（v1 初版；C-12 tasks B.4.4 + B.4.6 实施时升级 ✅ Accepted）
**关联 Issue**: 暂无（#22 占位待补 per tasks.md §Open Issue 队列）
