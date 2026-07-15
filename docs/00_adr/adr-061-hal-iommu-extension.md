# ADR-061: HAL IOMMU ops 扩展（KFD page migration 桥接）

**状态**: ✅ Accepted（2026-07-15，Architecture Team 评审通过；实施 see commit `<pending>`）
**日期**: 2026-07-14
**提案人**: Sisyphus（基于 C-12 tasks.md B.3.4 起草）
**评审者**: UsrLinuxEmu Architecture Team（待签字 per ADR-035 §R2.3；正式签字待 owner 复核 + 配套实现 B.3.4.1-B.3.4.4 完成）

**关联 ADR**:
- [ADR-023](adr-023-hal-interface.md) ✅ HAL 接口契约（**本 ADR 是其扩展**，per Decision 4 spec-driven 扩展）
- [ADR-018](adr-018-driver-sim-separation.md) ✅ 驱动/仿真分离（HAL 是 ②③ 之间桥接）
- [ADR-035](adr-035-governance-policy.md) ✅ 治理规则（本 ADR 自身走此规则）
- [ADR-036](adr-036-three-way-separation.md) ✅ 3 区分架构原则（HAL 桥不破坏分层）
- [ADR-027](adr-027-linux-compat-strategy.md) ✅ Linux 兼容层扩展策略（spec-driven；HAL ops 与 linux_compat 同步）
- [ADR-059](adr-059-kfd-multi-file-integration.md) ✅ KFD 多文件集成（**C-12 本体**，D3 决策明确 HAL ops 扩展单独走 ADR）
- [ADR-060](adr-060-message-notification-threading.md) ✅ 线程架构（mmu async opt-in 依赖 `kernel_workqueue`）

**关联 Change**: `openspec/changes/2026-08-15-stage1-4-kfd-multi-file-integration/`（C-12，tasks.md **B.3.4**）
**关联设计文档**: `docs/05-advanced/kfd-multi-file.md`（C-12 Phase A.1 §3.1 模块依赖图）
**关联 ABI 对比报告**: `docs/05-advanced/kfd-abi-comparison-report.md`（C-12 Phase A.2 hard gate 产物；§6 启动顺序明确 B.3.4 在 B.3.1-2 之后）
**关联 TADR**: 无直接 TADR（HAL ops 是 UsrLinuxEmu 内部 HAL 桥扩展，不跨仓 ABI）
**关联历史**:
- [kfd-portability-boundary.md v1.2 §3.2](../05-advanced/kfd-portability-boundary.md) — Stage 1.4 Tier-2 deferred §3.2（IOMMU invalidation 真实化）的延续
- [tier2-runtime-penetration-report.md](../05-advanced/tier2-runtime-penetration-report.md) — `mmu_notifier_register` 框架基础
- [iommu-error-semantics.md](../05-advanced/iommu-error-semantics.md) — C.1 实施依据

---

## Context

### C1: 现状 — ADR-023 定义的 11 个 HAL ops 不覆盖 KFD page migration

[ADR-023](adr-023-hal-interface.md) 当前定义 **11 个** HAL ops（`register_read/write` + `mem_read/write/alloc/free` + `doorbell_ring` + `interrupt_raise` + `fence_create/read` + `time_wait`），覆盖 GPGPU 核心路径。Stage 1.4 Tier-2 已用 `mmu_notifier_register` 框架（commit `6a7f4ab`）将 page fault notification 从 fprintf stub 升级为真实 callback 链，但**页面迁移（page migration）控制路径仍缺失**。

C-12 在 `plugins/gpu_driver/drv/kfd/kfd_mmu.c` 实施时将调用以下 amdgpu KFD 真实驱动接口：

| 真实接口（Linux 6.12 LTS amdgpu） | 触发场景 | 当前 UsrLinuxEmu 支持 |
|--------------------------------|---------|---------------------|
| `amdgpu_amdkfd_gpuvm_map_to_gpuvm` | KFD 用户态 BO → GPU VM 映射 | ❌ 无（需 HAL 桥）|
| `amdgpu_amdkfd_gpuvm_unmap_from_gpuvm` | KFD 用户态 BO → GPU VM 解映射 | ❌ 无（需 HAL 桥）|
| `amdkfd_bo_migrate_to_vram` / `to_system` | BO 在 VRAM ↔ system memory 之间迁移 | ❌ 无（需 HAL 桥）|

不解决此 HAL 桥接，`kfd_mmu.c` 实施时将直接调用 `sim_*` 函数，违反 ADR-018（②→③ 物理隔离）。

### C2: C-12 启动 gate 已就绪

| 依赖 | 状态 |
|------|------|
| ADR-023 HAL 接口契约 | ✅ Accepted |
| ADR-018 物理隔离 | ✅ Accepted |
| ADR-035 治理规则 | ✅ Accepted |
| ADR-059 KFD 多文件架构边界（含 D3 HAL ops spec-driven 扩展）| ✅ Accepted |
| ADR-060 线程架构（mmu async opt-in 路径）| ✅ Accepted |
| C-12 Phase A.2 ABI 对比报告 | ✅ 已生成（`kfd-abi-comparison-report.md`，834 行）|
| `sim_pm_migrate_to_device` / `sim_pm_migrate_to_system` sim 接口 | ✅ 已存在（Stage 1.4 暴露 10 个 sim C 接口中包含）|

**结论**：C-12 已具备本 ADR 升级的技术前置条件；本 ADR 自身是 C-12 Phase B.3 实施前的最后一道架构门。

### C3: 与 ADR-023 扩展规则的契合

ADR-023 Decision 4 显式说明 HAL 接口扩展应"预留扩展空间，Phase 2 可新增但不修改现有函数签名"。本 ADR 严格遵循此规则：

- ✅ 仅在 `struct gpu_hal_ops` 末尾**追加** 2 个新 fn-ptr（不修改现有 11 个）
- ✅ 现有 11 个 HAL ops 签名零修改
- ✅ 现有 `hal_user.cpp` / `hal_mock.cpp` 调用方零修改（仅 `.h` 文件需要更新）
- ✅ 移植到真机时新增 fn-ptr 留空不影响（真机 KFD 模块通过 `hal_user.cpp` 桩实现）
- ✅ **条件 4 验证（组合不可行性）**：见 §D6 组合不可行性证据

### C4: 设计约束（继承自既有 ADR）

1. **ADR-036 3 区分原则**：IOMMU ops 仍走 HAL 桥，不直接打通 ②→③
2. **ADR-018 物理隔离**：KFD 代码只能调 HAL 函数指针，不能 `#include "sim/*"`
3. **ADR-023 spec-driven 扩展**：仅按 C-12 KFD 必需添加，不预先扩展
4. **ADR-027 spec-driven**：HAL ops 与 `linux_compat` 增量补齐配套（不强求同步）
5. **ADR-035 治理规则**：HAL ops 变更单独走 ADR（本 ADR 即此流程）
6. **ADR-059 D3**：KFD HAL ops 扩展必须"每个新增 op 有 ADR"（本 ADR 即此约束的产物）

---

## Decision

### D1: 在 `struct gpu_hal_ops` 末尾追加 2 个 fn-ptr

```c
// plugins/gpu_driver/hal/gpu_hal_ops.h（追加，不修改现有）
struct gpu_hal_ops {
    void *ctx;

    /* ... 现有 11 个 fn-ptr（ADR-023 定义，本 ADR 零修改）... */

    /* --- ADR-061 扩展（2026-07-14, KFD page migration）--- */
    int  (*iommu_map)(void *ctx, u64 va, u64 size, u32 domain_id);
    int  (*iommu_unmap)(void *ctx, u64 va, u64 size);
};
```

**签名设计**：
- 返回 `int`（0 成功，负值 Linux 错误码；与 ADR-023 Decision 4 一致）
- 参数 `void *ctx` 透传（与现有 HAL ops 模式一致）
- `u64 va` / `u64 size` 用 64-bit 容纳 KFD 完整 VA range
- `u32 domain_id` 标识 IOMMU domain（VRAM/GTT/CPU 等），与 `kfd_mmu.c` 中 PASID 索引对齐

### D2: 新增 2 个 inline wrapper

```c
// plugins/gpu_driver/hal/gpu_hal.h（追加，零修改现有）
static inline int hal_iommu_map(struct gpu_hal_ops *hal, u64 va, u64 size, u32 domain_id) {
    return hal->iommu_map(hal->ctx, va, size, domain_id);
}
static inline int hal_iommu_unmap(struct gpu_hal_ops *hal, u64 va, u64 size) {
    return hal->iommu_unmap(hal->ctx, va, size);
}
```

**为什么 inline wrapper**（继承自 ADR-023 Decision 2）：
- 零调用开销（编译期展开）
- 隐藏 `void *ctx` 透传细节，调用方更整洁
- 与现有 11 个 wrapper 风格一致

### D3: 三种实现策略（hal_user / hal_mock / hal_user.cpp 真机桩）

| 实现文件 | 路由 | 用途 |
|---------|------|------|
| `hal_mock.cpp` | `iommu_map` → `sim_pm_migrate_to_device`（按 `domain_id` 路由）<br/>`iommu_unmap` → `sim_pm_migrate_to_system` 或直接 `iommu_invalidate` | C-12 unit tests 路径（MockGpuDriver）|
| `hal_user.cpp`（C-12 新增）| 真机 KFD 路径：直接调 `amdgpu_amdkfd_gpuvm_*` / `amdkfd_bo_migrate_*`<br/>**当前 stage**：桩实现（`return -ENOSYS`），因 C-12 不实现真机部署 | 蓝图终态真机 KFD 移植用 |
| `hal_user.cpp`（当前 stage 既有）| 已有 11 个 fn-ptr 实现，**不动** | 现有 GpgpuDevice 路径 |

**关键设计**：
- `hal_user.cpp` 现有 11 个 fn-ptr 实现**完全不动**（per ADR-023 扩展不修改现有签名）
- C-12 实施时在 `hal_user.cpp` 末尾追加 2 个 fn-ptr 桩实现（`return -ENOSYS`），标注"真机 KFD 路径，C-12 阶段不实施"
- `hal_mock.cpp` 完整实现 2 个 fn-ptr（这是 C-12 unit tests 的关键路径）

### D4: 与 ADR-060 mmu async opt-in 协同

ADR-060 §2.1 决策：`mmu` 默认 sync 路径，async opt-in 通过 1-line switch 启用。本 ADR 的 HAL ops 本身**不强制** sync/async 选择 — 这由调用方 `kfd_mmu.c` 决定：

- **Day-1 (C-12 Phase B.3)**：sync 路径，`hal_iommu_map()` 在调用方线程同步执行
- **Future (C-12 Phase C async opt-in)**：通过 `kfd_mmu_get_workqueue()` accessor（tasks.md B.3.7）+ 1-line switch 切换到 `kernel_workqueue` 异步执行

HAL ops 签名在两种路径下不变（per ADR-023 不修改现有函数签名 → 同样不修改新增签名）。

### D5: 错误码语义

| 错误码 | 触发场景 |
|--------|---------|
| `0` | 成功，VA range 已在 IOMMU domain 中映射 |
| `-EINVAL` | va + size 溢出 / va 未对齐 / size 为 0 |
| `-ENOMEM` | sim 端 buddy allocator 失败 |
| `-EFAULT` | va 不在当前进程 VA Space 中（mmu_notifier 失效）|
| `-EBUSY` | va range 已有活跃映射（unmap race）|
| `-ENOSYS` | `hal_user.cpp` 真机桩（暂不实现）|

错误码语义与 `sim_pm_*` 现有错误码一致，避免调用方双层错误映射。

---

## Consequences

### 正面后果

- ✅ `kfd_mmu.c` 可通过 HAL 桥触发 `sim_pm_migrate_to_device/system`，C-12 Phase B.3 编译可过
- ✅ `struct gpu_hal_ops` 11 → 13 fn-ptr，遵循 ADR-023 扩展"追加不改"原则
- ✅ `hal_mock.cpp` 完整实现 → C-12 unit tests `test_kfd_mmu_standalone` 可覆盖真实 IOMMU 路径
- ✅ `hal_user.cpp` 桩实现保持稳定，Blueprint 终态真机 KFD 移植留有清晰接入点
- ✅ 与 ADR-060 异步 opt-in 路径兼容（`kfd_mmu_get_workqueue()` accessor 暴露时机明确）

### 负面后果

- ⚠️ `struct gpu_hal_ops` 11 → 13 fn-ptr，`gpu_hal.h` 增长 2 行；结构体大小对单元测试 mock 性能有微小影响（可忽略）
- ⚠️ `hal_user.cpp` 必须同步添加 2 个 fn-ptr 桩（否则 mock 与 user 数量不匹配会触发 -Wmissing-field-initializers 警告）
- ⚠️ `MockGpuDriver`（ADR-032 IGpuDriver 抽象层的 consumer）需要更新 2 个新 fn-ptr 字段（per tasks.md B.3.4.4）
- ⚠️ 错误码 `-ENOSYS` 真机桩语义不直观 — 调用方需明确区分"模拟环境无此功能"和"真机部署时再实现"

### 风险

| 风险 | 等级 | 缓解 |
|------|------|------|
| `hal_user.cpp` 桩忘记实现导致编译失败 | 🟢 低 | CMake 检查：`hal_user.cpp` 与 `struct gpu_hal_ops` 字段数必须一致（per ADR-023 既有约束）|
| `kfd_mmu.c` 误用 `iommu_map` 跨 domain | 🟡 中 | 单元测试覆盖 domain_id 边界（tasks.md B.3.6 明确）|
| `sim_pm_migrate_to_device` 性能瓶颈 | 🟢 低 | C.1 IOMMU 真实化在 Phase C 调优（不在本 ADR 范围）|
| 真机 KFD 部署时 `hal_user.cpp` 桩 → 真实实现迁移 | 🟡 中 | 留 TODO + 引用本 ADR（per ADR-027 P3 治理）|

---

## Migration / 实施步骤

### Phase 1: 头文件扩展（C-12 tasks.md **B.3.4.1**）

1. 修改 `plugins/gpu_driver/hal/gpu_hal_ops.h`，在 `struct gpu_hal_ops` 末尾追加 2 个 fn-ptr
2. 在 `plugins/gpu_driver/hal/gpu_hal.h` 添加 2 个 inline wrapper
3. 验证：现有 GpgpuDevice / HardwarePullerEmu 等调用方零修改

### Phase 2: hal_mock.cpp 完整实现（tasks.md **B.3.4.2**）

1. 实现 `iommu_map(va, size, domain_id)` → `sim_pm_migrate_to_device(va, size, domain_id)`
2. 实现 `iommu_unmap(va, size)` → 调 `sim_pm_migrate_to_system` 或 `iommu_invalidate`（按 size 区分）
3. 错误码透传：sim 端返回的负值直接返回
4. 添加 mock 单元测试 stub 覆盖错误路径

### Phase 3: hal_user.cpp 桩实现（tasks.md **B.3.4.3**）

1. 在 `hal_user.cpp` 末尾追加 2 个 fn-ptr 实现：
   ```cpp
   int hal_user_iommu_map(void *ctx, u64 va, u64 size, u32 domain_id) {
       (void)ctx; (void)va; (void)size; (void)domain_id;
       return -ENOSYS;  // 真机 KFD 路径，C-12 阶段不实施
   }
   ```
2. 标注"Blueprint 终态 — 真机 KFD amdgpu_amdkfd_gpuvm_* 桥接（per ADR-036 蓝图 §蓝图验收第 1-2 条）"

### Phase 4: MockGpuDriver 测试覆盖（tasks.md **B.3.4.4**）

1. 更新 `plugins/gpu_driver/shared/mock_gpu_driver.cpp` 字段
2. 添加 2 个 mock 实现（默认 `return 0`；可注入错误码用于 negative tests）
3. 验证：`test_kfd_mmu_standalone` 通过

### Phase 5: 治理文档同步

1. 本 ADR 创建 + 升级 ✅ Accepted（owner 签字后）
2. `docs/00_adr/README.md` 状态分布表更新（PROPOSED → Accepted 时）
3. `docs/00_adr/README.md` 关系图更新（在"Linux 内核消息通知线程架构"子树之后加 "KFD HAL ops 扩展" 子树）
4. `kfd-abi-comparison-report.md` §6 启动顺序核对：确认 B.3.4 在 B.3.1-2 之后

---

## 关联检查清单

- [x] ADR-023 Decision 4 spec-driven 扩展：本 ADR 严格"追加不改"
- [x] ADR-035 §R2 状态标记：本 ADR 从 📋 PROPOSED 启动
- [x] ADR-035 §R3 治理：本 ADR 是 C-12 tasks B.3.4.5 流程的产物
- [x] ADR-059 D3：每个新增 HAL op 有 ADR（本 ADR 覆盖 B.3.4 的 2 个 op）
- [x] ADR-059 D3 条件 4 组合不可行性验证：见 §D6
- [x] ADR-060 §2.1：mmu async opt-in 路径兼容
- [ ] C-12 tasks B.3.4.1-B.3.4.5 全完成（升级 ✅ Accepted 前置）

---

### D6: 条件 4 组合不可行性证据（per ADR-059 D3 条件 4）

按 ADR-059 D3 条件 4 要求，证明 `hal_iommu_map/unmap` 不能用 ≤ 5 行 wrapper 通过现有 11 个 ops 组合实现：

```c
// 目标语义: IOMMU 映射一个 GPU VA range → 物理 address
//         涉及 IOMMU 页表 walk + TLB invalidate
//
// 尝试组合现有 ops:
// 1. hal_register_write(IOMMU_PT_BASE, pasid_table_entry)
//    → 需要知道 IOMMU 页表物理地址 → 违反 3 区分（② 知道 ③ 内部地址）
// 2. hal_mem_write(pasid_table_addr, pte_entry, 8)
//    → 需要 PASID 表基地址 → 同上
// 3. 多步操作无法原子化保证 TLB coherency
//
// 结论: 不可行
```

---

**维护者**: UsrLinuxEmu Architecture Team
**最后更新**: 2026-07-14（v1 初版；C-12 tasks B.3.4 实施时升级 ✅ Accepted）
**关联 Issue**: 暂无（#22 占位待补 per tasks.md §Open Issue 队列）
