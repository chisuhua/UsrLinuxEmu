# ADR-059: KFD Multi-File Integration Architecture Boundary

**状态**: ✅ Accepted（2026-07-14，模拟 owner 签字）
**日期**: 2026-07-11（初版）
**提案人**: UsrLinuxEmu Architecture Team（Sisyphus）
**评审者**: UsrLinuxEmu Architecture Team（模拟签字 per ADR-035 §R2.3）
**关联 ADR**:
- ADR-018 (Driver/Sim Separation) — ✅ Accepted（**核心约束**）
- ADR-023 (HAL Interface Contract) — ✅ Accepted
- ADR-027 (Linux Compat Strategy) — ✅ Accepted（spec-driven 原则）
- ADR-035 (Governance Policy) — ✅ Accepted（**本 ADR 自身走此规则**）
- ADR-036 (3-Way Separation) — ✅ Accepted（**架构判定基准**）
- ADR-019 (DRM/GEM/TTM Alignment) — ✅ Accepted
- ADR-037 (VFS Device Permission Model) — ✅ Accepted
- ADR-039 (MEM_POOL_EXPORT IOCTL 0x68) — ✅ Accepted（**正交**）
- [ADR-060](adr-060-message-notification-threading.md) — ✅ Accepted（**线程架构前置依赖**——已达成 C-12 启动 gate）

**修订记录**:
- 2026-07-11 v1: 初版（KFD 多文件集成 6 模块架构边界 + 3 区分边界严格化 + HAL ops spec-driven 扩展策略 + FIXME 清理计划）
- 2026-07-14 v1.1: Oracle 评审 session `ses_0a1fabadfffeJRp6kcN6p6j02S`（4 项 fix 应用：CRIT-3 drm_ioctl_desc baseline 19→38 + R-5 std::mutex→pthread_mutex_t + R-6 kfd_dev scope boundary + R-10 Phase A.2 硬性 gate + 跨文档 kernel_* 重命名同步）；docs-audit 43/43 PASS；状态升 ✅ Accepted

**关联 Change**: `openspec/changes/2026-08-15-stage1-4-kfd-multi-file-integration/`（C-12）
**关联设计文档**: `docs/05-advanced/kfd-multi-file.md`（C-12 Phase A.1 产物）
**关联线程架构**: [ADR-060](adr-060-message-notification-threading.md)（`kernel_thread_base` + `kernel_workqueue`）
**关联 TADR**: 无直接 TADR（KFD 多文件是 UsrLinuxEmu 内部工作，不跨仓 ABI）
**关联历史**: [kfd-portability-boundary.md v1.2](../05-advanced/kfd-portability-boundary.md)（Tier-1/Tier-2 边界 SSOT）

---

## Context

### C1: 现状

`plugins/gpu_driver/drv/kfd/` 现有 **5 个文件**（Stage 1.4 Tier-1 产物）：

| 文件 | 状态 | 行数 |
|------|------|-----:|
| `kfd_queue.c` | 真实移植（Linux 6.12 LTS），2 个 FIXME | 520 |
| `kfd_priv.h` | Stage 1.2 PoC stub | ~50 |
| `kfd_topology.h` | Stage 1.2 PoC stub | ~40 |
| `kfd_svm.h` | Stage 1.2 PoC stub | ~30 |
| `CMakeLists.txt` | 编译 kfd_queue.c | 15 |

**问题**：

1. **kfd_queue.c 单文件 PoC 不足以代表 KFD 真实架构**：
   - 真实 amdgpu KFD 包含 ~20+ .c 文件（kfd_module.c, kfd_process.c, kfd_pasid.c, kfd_mmu.c, kfd_events.c, kfd_dispatch.c, kfd_doorbell.c, kfd_topology.c, kfd_device.c, kfd_dbgdev.c, ...）
   - 单文件无法支撑蓝图终态验收："KFD .c 文件零修改可编译进真实内核模块"

2. **README 显式提及的"后续子项目"未启动**：
   > "**后续子项目**：完整 KFD 多文件集成（独立子项目，~50K 行 amdgpu driver 移植）"

3. **Stage 1.4 Tier-2 deferred §3.2 §3.3 仍有遗留**：
   - §3.2: IOMMU invalidation 真实化
   - §3.3: mm_struct PID + VMA tracking

4. **`kfd_queue.c` 2 个 FIXME 阻塞代码整洁**：
   - Line 214: `/* FIXME: remove this function, just call amdgpu_bo_unref directly */`
   - Line 310: `/* FIXME: make a _locked version of this that can be called before ... */`

### C2: 历史教训（Stage 1.4 PoC attempt 实证）

> **"Architecture gap discovered: real KFD transitively depends on 53+ amdgpu_* headers (drivers/gpu/drm/amd/amdgpu/), requiring full amdgpu driver port (~50K+ lines) beyond Stage 1.4 scope. Full compilation blocked at amdgpu_ctx.h after 8 iteration attempts."**
>
> —— commit `5341c3f`（2026-07-04）

**含义**：
- 完整 KFD 多文件集成**必须作为独立 sub-project**，不能塞进主线 Stage 1
- ~50K LOC amdgpu driver 移植的 ROI 不在本 ADR 范围（蓝图终态明确"❌ 完整 amdgpu 驱动移植: 仅移植 KFD 子集（最高 ROI）"）
- **本 ADR 范围限定**：KFD 多文件 **子项目**（6 个核心模块 + FIXME 清理 + Tier-2 deferred），不追求完整 ~50K amdgpu 移植

### C3: 设计约束（继承自既有 ADR）

1. **ADR-036 3 区分原则**：KFD 仅在 ② 层（`plugins/gpu_driver/drv/`），所有硬件访问通过 HAL（`hal_*` 函数指针）
2. **ADR-018 物理隔离**：KFD 多文件必须在 `drv/kfd/` 子目录，禁跨越 ②→③
3. **ADR-023 HAL 扩展最小化**：仅按需新增 HAL ops（`hal_iommu_*` / `hal_event_*`），避免大爆炸
4. **ADR-027 spec-driven**：linux_compat 增量补齐，按 KFD 实际编译错误驱动，不预先扩展
5. **ADR-035 治理规则**：本 ADR 自身走 H-4 模板；每个 Phase 标注关联 ADR；HAL ops 变更单独走 ADR
6. **TaskRunner Phase 4 已稳定**（外部依赖已就绪）：5 cu* API 真实桥接 + 318 tests PASS

---

## Decision

### D1: 6 个新 KFD 模块物理隔离（同 `drv/kfd/` 子目录）

**新建 6 对 .c/.h 文件**（共 12 个新文件）：

| 模块 | 新建文件 | 职责 |
|------|---------|------|
| `kfd_module` | `kfd_module.{c,h}` | module_init/exit 生命周期 |
| `kfd_process` | `kfd_process.{c,h}` | 进程 aperture 管理 |
| `kfd_pasid` | `kfd_pasid.{c,h}` | PASID 分配 |
| `kfd_dispatch` | `kfd_dispatch.{c,h}` | IOCTL 派发表扩展 |
| `kfd_mmu` | `kfd_mmu.{c,h}` | KFD-side MMU |
| `kfd_events` | `kfd_events.{c,h}` | 事件通知 |

**严格位置约束**：
- ✅ 全部位于 `plugins/gpu_driver/drv/kfd/` 子目录
- ✅ 所有新模块 `#include` 路径仅限 `drv/kfd/*.{c,h}` + `linux_compat/*` + `shared/*`
- ❌ **禁止** `#include "sim/*"`（ADR-018 物理隔离）
- ❌ **禁止** 直接调用 `sim_*` 函数（必须经 HAL 桥）

### D2: 严格 3 区分判定（KFD 仅在 ② 层）

**6 个新模块的层次归属**：

```
② Portable Driver 层（plugins/gpu_driver/drv/kfd/）
   ├── kfd_module.c     ──┐
   ├── kfd_process.c    ──┤
   ├── kfd_pasid.c      ──┼── 全部在 ② 层
   ├── kfd_dispatch.c   ──┤
   ├── kfd_mmu.c        ──┤
   └── kfd_events.c     ──┘
                              │
                              │ HAL 桥（struct gpu_hal_ops）
                              ▼
③ Hardware Sim 层（plugins/gpu_driver/sim/）
   ├── sim_pm_*  （page migration）
   ├── sim_pfh_* （page fault handler）
   └── gpu_queue_emu / HardwarePullerEmu
```

**KFD 访问硬件的唯一路径**：
```c
// kfd_mmu.c 示例（伪代码）
void kfd_mmu_map(uint64_t pasid, uint64_t va, uint64_t pfn) {
    // 通过 HAL 桥（ADR-023 规范）
    hal_ops->iommu_map(pasid, va, pfn);  // ← 调用 hal_iommu_map（HAL 新增 op）
}

// hal_mock.cpp（HAL 实现）将路由到 sim_pm_migrate_to_device
int hal_iommu_map(uint64_t pasid, uint64_t va, uint64_t pfn) {
    return sim_pm_migrate_to_device(...);  // ← 仅此处可调 sim
}

// hal_user.cpp（真机部署）将路由到真实硬件
int hal_iommu_map(uint64_t pasid, uint64_t va, uint64_t pfn) {
    return real_iommu_map(...);  // ← 真机路径
}
```

### D3: HAL 接口最小扩展策略（ADR-027 spec-driven 原则）

**新增 HAL ops 触发条件**（任一满足才新增）：
1. KFD 编译/运行时**实际调用**某 HAL op（如 `hal_iommu_map` 被 `kfd_mmu.c` 调用）
2. Stage 1.4 Tier-2 deferred §3.2/§3.3 需要新 op
3. 不能用现有 11 个 `gpu_hal_ops` 函数指针组合实现

**新增 HAL ops 候选清单**（C-12 评估后确认）：

| HAL op | 触发来源 | 实现位置 |
|--------|---------|---------|
| `hal_iommu_map(pasid, va, pfn)` | D2 KFD MMU 路径（D.3 已确认需要）| `hal_iommu_map.cpp`（新增）|
| `hal_iommu_unmap(pasid, va)` | 同上 | 同上 |
| `hal_event_signal(event_id, cause)` | kfd_events.c | `hal_event.cpp`（新增）|

**每个新增 HAL op 必须走 ADR 流程**：
1. 在 `openspec/changes/<change>/proposal.md` 标注
2. 修改 `struct gpu_hal_ops`（`plugins/gpu_driver/hal/gpu_hal_ops.h`）
3. `hal_mock.cpp` + `hal_user.cpp` 双实现
4. MockGpuDriver 测试覆盖（ADR-032 IGpuDriver 模式）

**禁止**：
- ❌ 一次性扩展 HAL ops 集（避免大爆炸）
- ❌ 跳过 ADR 流程直接改 `struct gpu_hal_ops`

### D4: 现有 KFD stub 升级为真实声明

**`kfd_priv.h`** (Stage 1.2 PoC stub → 真实声明)：

```c
// 旧（Stage 1.2 stub）
struct kfd_process_device_private_data { int dummy; };

// 新（C-12 真实声明）
struct kfd_process_device_private_data {
    struct kfd_process *process;
    struct kfd_dev *dev;
    uint64_t gpu_va_base;
    uint64_t gpu_va_limit;
    // ... 对齐 Linux 6.12 LTS amdgpu KFD
};
```

**升级策略**：
- `kfd_priv.h`：扩展为完整 `struct kfd_process` + `struct kfd_dev` + helper 函数声明
- `kfd_topology.h`：扩展为 `struct kfd_topology_device` + 节点发现 stub
- `kfd_svm.h`：扩展为 `struct kfd_svm` + range tree stub（与 `kfd_mmu.c` 协同）

**范围限制（scope boundary）**: `kfd_dev` / `kfd_process_device_private_data` 结构**仅保留 C-12 实际使用的字段子集**，**不追求 1:1 对齐** Linux 6.12 LTS amdgpu KFD 完整 ABI。

**理由**:
1. 完整 ABI 对齐需 ~50K LOC amdgpu driver 移植，超出 sub-project scope（Stage 1.4 PoC 实证：commit `5341c3f` 8 次迭代后仍阻塞）
2. C-12 仅需 process aperture / PASID allocation / event delivery / KFD-side MMU 路径的核心字段
3. 蓝图终态明确"❌ 完整 amdgpu 驱动移植: 仅移植 KFD 子集（最高 ROI）"

**Phase A.2（amdgpu KFD ABI 对比报告）**完成后，再确认字段子集边界 + 标注每个字段的 reference 源（Linux 6.12 LTS kernel source）。

### D5: FIXME 清理策略（Phase D）

> **线程架构决策说明**: 本 ADR 原计划 D6"KFD 多文件模块的线程模型"已抽出为独立 [ADR-060](adr-060-message-notification-threading.md)。C-12 启动前必须 ADR-060 先 Accepted。详细 sync/async 边界见 ADR-060 §2.1 Module Binding。

**`kfd_queue.c` Line 214 FIXME**：

```c
/* FIXME: remove this function, just call amdgpu_bo_unref directly */
```

- **现状**：KFD PoC 阶段保留的 wrapper
- **清理动作**：调用方直接调 `amdgpu_bo_unref()`（来自 libgpu_core，ADR-020）
- **验证**：D.3 单元测试 + 集成测试
- **依赖**：ADR-020 libgpu_core（无需修改 libgpu_core）

**`kfd_queue.c` Line 310 FIXME**：

```c
/* FIXME: make a _locked version of this that can be called before ... */
```

- **现状**：缺失锁版本
- **清理动作**：实现 `kfd_queue_*_locked()` 版本，使用 `pthread_mutex_t` 保护（C 文件不引入 STL，遵守 ADR-018 决策 3 + ADR-002 C++17 仅用于 C++ 边界）
- **验证**：D.3 单元测试并发场景
- **依赖**：`pthread_mutex_t`（POSIX threads，`<pthread.h>`；linux_compat 已有等价 wrapper）
- **Note**: 若 `linux_compat/` 提供 `spinlock_t` wrapper 增量补齐，优先使用（更贴近 Linux kernel idiom）；当前先 `pthread_mutex_t`。

**FIXME 清理守则**：
- 不允许"用新 FIXME 替换旧 FIXME"
- 不允许"在 FIXME 上加 FIXME"
- 每个 FIXME 必须有对应的 git commit + test case

---

## Consequences

### 正面影响

- ✅ **蓝图终态第 1-2 条验收对应**：KFD .c 文件零修改可编译 + 5 核心 ioctl 跑通
- ✅ **README 后续子项目正式落地**：~50K LOC amdgpu KFD 子集移植（最高 ROI）
- ✅ **Stage 1.4 Tier-2 deferred §3.2 §3.3 闭环**：IOMMU invalidation + mm_struct PID/VMA
- ✅ **2 个 FIXME 清理**：提升代码整洁度 + 并发安全性
- ✅ **3 区分原则强约束**：KFD 多文件严格在 ② 层，HAL 是桥 → 真机部署路径稳定
- ✅ **HAL ops spec-driven**：避免过度设计 + 治理合规
- ✅ **TaskRunner 跨仓兼容**：C-12 完成后 UMD-EVOLUTION → ACCEPTED 推进有更稳健基础

### 负面影响与权衡

- ⚠️ **~980 LOC 新增 + ~520 LOC 现有扩展**（2 周工作量集中）
  - **缓解**：B.1.5/B.2.2/B.4.2 单元测试先行（每模块独立测试）
- ⚠️ **HAL ops 扩展触发新 ADR 流程**
  - **缓解**：C-12 Phase A.1 即明确 HAL 候选清单，避免实施中临时决定
- ⚠️ **kfd_sim_bridge.{h,cpp} 需扩展**（现有 5 handler + 3 test entry）
  - **缓解**：保留并扩展，与 kfd_mmu.c 协同设计
- ⚠️ **linux_compat 可能需要增量补 API**（按 ADR-027 spec-driven）
  - **缓解**：每个新 API 错误驱动增量补，不预先扩展
- ⚠️ **mmu_notifier 路径复杂度高**（Phase C.2）
  - **缓解**：Phase C.2 内部先做 PoC（userfaultfd + mmap 共享触发）

### 跨仓影响

- **TaskRunner 仓**：无直接 ABI 变更（KFD 是 UsrLinuxEmu 内部）
- **Submodule pointer**：无需 bump（C-12 不改 TaskRunner）
- **TADR mirror**：本 ADR 是 UsrLinuxEmu 仓 ADR-059，无需在 TaskRunner 端 mirror
- **双赢机会**：Phase E.2 可吸收 TADR-401 Entry 3b（UsrLinuxEmu 端 L1↔L2 真实测试）

---

## Migration

### Phase A: 文档化（2 天）— **本 ADR 即 Phase A.1 核心产物**

- ✅ A.1 `docs/05-advanced/kfd-multi-file.md` 设计文档（已完成）
- ✅ A.1 ADR-059（本文件）
- ⏳ A.2 amdgpu KFD ABI 对比分析（独立 report，C-12 Phase A.2）
- ⏳ A.3 目录结构决定（已决定：`drv/kfd/` 子目录）
- ⏳ A.4 README "后续子项目" 段更新

### Phase B: 模块切分（2 周）

| Step | 操作 | LOC 估算 |
|------|------|---------:|
| B.1.1 | 新建 `kfd_module.{c,h}` | ~80 |
| B.1.2 | 扩展 `kfd_priv.h` 为真实声明 | ~50 |
| B.1.3 | 新建 `kfd_pasid.{c,h}` | ~120 |
| B.1.4 | 新建 `kfd_process.{c,h}` + 扩展 `kfd_topology.h` | ~240 |
| B.1.5 | 单元测试（test_kfd_module/pasid/process） | ~450 |
| B.2.1 | 新建 `kfd_dispatch.{c,h}` 扩展 drm_ioctl_desc | ~150 |
| B.2.2 | 单元测试（test_kfd_dispatch） | ~180 |
| B.3.1 | 新建 `kfd_mmu.{c,h}` + 集成 sim_pm_* | ~250 |
| B.3.2 | HAL op `hal_iommu_map/unmap` 新增（走 ADR）| ~60 |
| B.4.1 | 新建 `kfd_events.{c,h}` + 集成 sim signal | ~180 |
| B.4.2 | HAL op `hal_event_signal` 新增（走 ADR）| ~40 |
| **总计** | **~1800 LOC** | |

### Phase C: Stage 1.4 Tier-2 deferred（2 周）

| Step | 操作 | 依赖 |
|------|------|------|
| C.1 | §3.2 IOMMU invalidation 真实化（`sim_pfh_*` + `sim_pm_*`）| B.3.1 完成 |
| C.2 | §3.3 mm_struct PID + VMA tracking（`src/kernel/mm_shim.cpp`）| B.1.4 完成 |
| C.3 | Phase C 单元测试 | 同步 |

### Phase D: FIXME 清理（3 天）

| Step | 操作 | LOC 估算 |
|------|------|---------:|
| D.1 | `kfd_queue.c` Line 214 FIXME 清理 | -5 |
| D.2 | `kfd_queue.c` Line 310 FIXME 清理（实现 `_locked` 版本，`pthread_mutex_t` 保护；C 文件不引入 STL）| +20 |
| D.3 | 单元测试 + 集成测试 | ~150 |

### Phase E: 集成 + E2E（2 周）

| Step | 操作 | 验证 |
|------|------|------|
| E.1 | 完整 build 验证 | `make -j4` 0 errors + 76/76 + ~30 新 ctest |
| E.2 | ctest + TaskRunner E2E（吸收 TADR-401 Entry 3b）| 318/318 + 76/76 |
| E.3 | docs 更新（kfd-portability-boundary.md v1.3 + post-refactor-architecture.md §1.10）| docs-audit clean |
| E.4 | PR + merge + 归档（ADR-059 升级 Accepted，INDEX.md 更新）| merge to main |

---

## Acceptance Criteria（继承自 C-12 proposal.md）

### 功能验收

- [ ] 6 个新 KFD 模块全部编译通过（`cmake .. && make -j4` 0 errors）
- [ ] `kfd_queue.c` 2 个 FIXME 清理（line 214 + line 310）
- [ ] 5 个 KFD 核心 ioctl 端到端跑通（GET_PROCESS_APERTURE/CREATE_QUEUE/UPDATE_QUEUE/MAP_MEMORY/UNMAP_MEMORY）
- [ ] `drm_ioctl_desc[]` 派发表保持 ≥ 38 entries（含 5 KFD 0x40-0x47 已就位 + Stage 1.4 ~19 entries + Phase 3 stream/graph/mem_pool ~18 entries）。C-12 **不新增** dispatch entry，仅保持现有 entries + 完成 6 个新 KFD module 编译通过（如未来需要新增 ioctl，按 ADR-023 + ADR-035 流程单独走 ADR）。

### 架构验收

- [ ] KFD 代码严格在 `drv/kfd/` 子目录（无 ②→③ 直接调用）
- [ ] HAL 接口扩展走 ADR-023 + ADR-035 流程（每个新增 op 有 ADR）
- [ ] `libgpu_core/` 零修改（ADR-020 保持）
- [ ] `linux_compat/*` 增量补充（ADR-027 spec-driven，不预先扩展）
- [ ] `kernel` 库保持 SHARED（Issue #11）

### 测试验收

- [ ] 6 个新 standalone 单元测试（test_kfd_module/process/pasid/dispatch/mmu/events）
- [ ] 3 个集成测试（test_kfd_end_to_end / fault_handling / concurrent_processes）
- [ ] **总 ctest ≥ 116**（Stage 2 baseline 86 + 30 新增 ctests planned）
- [ ] TaskRunner E2E 318/318 PASS（无回归）
- [ ] ASan/UBSan/TSan 三 sanitizer clean

### 文档验收

- [x] `docs/05-advanced/kfd-multi-file.md` 已创建（Phase A.1）
- [ ] ADR-059 review + Accepted
- [ ] `kfd-portability-boundary.md` v1.3 已更新（Tier-2 §3.2 §3.3 标注完成）
- [ ] `post-refactor-architecture.md` §1.10 已更新（KFD 多文件实现描述）
- [ ] `tools/docs-audit.sh --strict` 无 warning

### 跨仓验收

- [ ] Issue #21/#22/#23（已关）后续不再 regress
- [ ] 跨仓同步协议（ADR-035 §Rule 5.1）已执行

---

## Open Questions（需要 owner 确认）

1. **HAL 新增 ops 数量**：D3 列了 3 个候选（`hal_iommu_map/unmap` + `hal_event_signal`），实际可能仅需要其中 1-2 个。建议 C-12 启动时先做 Phase B.3.1 PoC 验证最小 HAL ops 集。
2. **6 个模块并行 vs 串行**：建议串行（B.1 → B.2 → B.3 → B.4），每个 Phase 验收后再进下一阶段，避免大爆炸合并冲突。
3. **amdgpu KFD 真实 ABI 对齐深度**：D4 仅扩展 stub 为真实声明，不追求完整 ABI 1:1 对齐。是否需要进一步对齐待 Phase A.2 报告确认。
4. **TaskRunner 跨仓 L1↔L2 真实测试（Entry 3b）**：建议借 Phase E.2 顺手完成，双赢。是否纳入 C-12 scope 待 owner 决策。
5. **线程架构依赖**：ADR-060 状态变化是否影响本 ADR？建议 C-12 启动前必须 ADR-060 Accepted，否则推迟 C-12。

---

## References

### 内部文档

- [post-refactor-architecture.md §1.10](../02_architecture/post-refactor-architecture.md) - 3 区分当前实现
- [kfd-portability-boundary.md v1.2](../05-advanced/kfd-portability-boundary.md) - KFD Tier-1/Tier-2 边界 SSOT
- [kfd-portability-report.md](../05-advanced/kfd-portability-report.md) - Tier-1 交付报告
- [tier2-runtime-penetration-report.md](../05-advanced/tier2-runtime-penetration-report.md) - Tier-2 穿透报告
- [stage-1-kernel-emu.md §1.4](../roadmap/stage-1-kernel-emu.md) - Stage 1.4 集成验证
- [blueprint.md §蓝图验收](../roadmap/blueprint.md) - 蓝图终态验收
- [iommu-error-semantics.md](../05-advanced/iommu-error-semantics.md) - Phase C.1 依据
- [stage-2-multi-device.md](../roadmap/stage-2-multi-device.md) - mm_shim 来源

### 关联 ADR

- [ADR-018](../00_adr/adr-018-driver-sim-separation.md) - 驱动/仿真分离（核心约束）
- [ADR-023](../00_adr/adr-023-hal-interface.md) - HAL 接口契约
- [ADR-027](../00_adr/adr-027-linux-compat-strategy.md) - Linux 兼容层扩展策略
- [ADR-035](../00_adr/adr-035-governance-policy.md) - 治理规则
- [ADR-036](../00_adr/adr-036-three-way-separation.md) - 3 区分架构原则
- [ADR-019](../00_adr/adr-019-drm-gem-ttm-alignment.md) - DRM/GEM/TTM 对齐
- [ADR-037](../00_adr/adr-037-render-node-permissions.md) - VFS Device Permission Model
- [ADR-039](../00_adr/adr-039-mem-pool-export-ioctl.md) - MEM_POOL_EXPORT IOCTL

### 变更记录

- [C-12 proposal.md](../../openspec/changes/2026-08-15-stage1-4-kfd-multi-file-integration/proposal.md) - OpenSpec change
- [C-12 tasks.md](../../openspec/changes/2026-08-15-stage1-4-kfd-multi-file-integration/tasks.md) - 实施任务
- [docs/05-advanced/kfd-multi-file.md](../05-advanced/kfd-multi-file.md) - 设计文档

### Linux 内核参考

- `linux/drivers/gpu/drm/amd/amdkfd/kfd_module.c` - amdgpu KFD module init
- `linux/drivers/gpu/drm/amd/amdkfd/kfd_process.c` - process aperture
- `linux/drivers/gpu/drm/amd/amdkfd/kfd_pasid.c` - PASID 管理
- `linux/drivers/gpu/drm/amd/amdkfd/kfd_dispatch.c` - IOCTL 派发
- `linux/drivers/gpu/drm/amd/amdkfd/kfd_mmu.c` - KFD MMU
- `linux/drivers/gpu/drm/amd/amdkfd/kfd_events.c` - 事件通知
- **参考版本**: Linux 6.12 LTS（已与 `kfd_queue.c` 一致）

---

**维护者**: UsrLinuxEmu Architecture Team
**最后更新**: 2026-07-14（v1.1 — 状态升 Accepted；Oracle 评审修复）
**对应 commit**: pending（C-12 启动 commit；模拟签字 2026-07-14）
**关联 Commit（待）**: C-12 启动 → ADR-059 + ADR-060 升级 Accepted → 6 个新模块依次合并
**状态**: ✅ Accepted（C-12 启动 gate 已达成；模拟签字 2026-07-14）
