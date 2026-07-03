## Context

### 背景

UsrLinuxEmu 阶段 1（[post-refactor-architecture.md §1.10](../../docs/02_architecture/post-refactor-architecture.md)）的目标是补齐 Linux 内核环境模拟（PCIe + IOMMU + DRM + UVM/HMM），使真实 KFD 驱动代码**逻辑零修改**即可在 UsrLinuxEmu 中编译运行 5 个核心 ioctl。前 3 个子阶段已完成：

- **1.0 PCIe Emulation**（已归档 `stage-1-0-pcie-emu/`）—— config space + MSI-X
- **1.1 IOMMU + ATS**（已归档 `2026-07-02-stage-1-1-iommu-ats/`）—— DMA remap + ATS 协议
- **1.2 DRM 子集**（已归档 `2026-07-02-stage-1-2-drm-subset/`）—— DRM/GEM/Prime + KFD 5 ioctl 编号预留

**1.3 UVM/HMM** 是阶段 1 的第 4 个子阶段（也是**风险最高**，Oracle 2026-07-02 评估），承担 KFD **SVM (Shared Virtual Memory) ioctl** 路径的内核环境模拟。SVM 路径深度依赖 `mmu_notifier` + `hmm_range` + `mmu_interval_notifier` 三个 Linux 内核子系统，缺一不可。

### Oracle 评估（2026-07-02）+ 阶段 1.2/1.3 边界契约

1.2 阶段已锁定 1.2/1.3 边界契约 G1-G4（[stage-1-2-drm-subset/design.md §Decision 5](../archive/2026-07-02-stage-1-2-drm-subset/design.md)），本 change 是在该契约上构建完整实现：

| 契约 | 1.2 保证 | 1.3 必须实现 |
|------|---------|-------------|
| G1 `drm_device` 生命周期 | = `GpgpuDevice` 生命周期 | uvm module 持有 `drm_device*` 指针，整个设备存在期间有效 |
| G2 `dma_buf_dynamic_attach` 等 API 签名 | 与 Linux 6.12 ABI 一致 | 沿用 1.2 锁定 |
| G3 4 项接口契约明确列出 | design.md Decision 5 | 完整化 4 项契约 |
| G4 不预先实现 1.3 完整 migrate | 仅留接口边界 | 1.3 完整实现 migrate；1.4 集成 KFD `kfd_svm.c` |

### Oracle 关键盲点（已处理）

| # | 盲点 | 处理 |
|---|------|------|
| 1 | `mmu_notifier` + `drm_device` 生命周期耦合 | 1.2/1.3 边界契约 G1-G4（已锁） |
| 2 | `struct hmm_mirror` 在 Linux 6.x 已移除 | 改用 `mmu_interval_notifier`（librarian 2026-07-02 验证 amdkpu 调用） |
| 3 | mmap 共享触发 page fault 的同步性 | 内部 PoC 先完成（userfaultfd + mmap 共享触发场景） |
| 4 | HMM range fault 性能与一致性 | 锁目标 LTS = 6.12；先 PoC，再铺全套 |
| 5 | migrate 接口签名 6.6 ↔ 6.12 断崖 | 锁 6.12，1.4 集成时再补 6.6 兼容矩阵 |

### 当前状态

- **已存在**：
  - `include/linux_compat/drm/drm_device.h`（1.2 完成，`drm_device` 嵌入 GpgpuDevice）
  - `src/kernel/drm/`（1.2 完成，DRM/GEM/Prime + render_node + 3 设备节点）
  - `src/kernel/iommu/`（1.1 完成，DMA remap + ATS 协议）
  - `src/kernel/pcie/`（1.0 完成，config space + MSI-X）
  - `tests/test_uvm_drm_lifecycle_standalone`（1.2 完成的 G1 骨架）
  - `scripts/check_gpu_ioctl_sync.sh`：1.2 已预留 5 个 KFD ioctl 编号 0x44-0x47
- **完全不存在**：
  - `src/kernel/uvm/` 框架（新增）
  - `include/linux_compat/{mmu_notifier,hmm}.h`（新增）
  - `plugins/gpu_driver/uvm/`（**仅当 KFD uvm 子模块需要**）
  - `plugins/gpu_driver/sim/page_fault_handler.cpp` + `page_migration.cpp`（新增）

### 约束

- **ADR-019**：DRM/GEM/TTM 对齐路径已 Accepted
- **ADR-024**：UMQ（User Mode Queue）已 Accepted —— UMQ **不在** 1.3 范围（与 UVM/HMM 在内核子系统层级上正交）
- **ADR-027**：Linux 兼容层扩展走 spec-driven 增量（禁止凭想象预先添加）
- **ADR-035**：HAL 接口扩展必须走 ADR 流程
- **ADR-036**：工作分层必须在 ① / ② / ③ 三区中明确归属
- **stage-1-2-drm-subset/design.md Decision 5**：1.2/1.3 边界契约 G1-G4 已锁
- **路线图 §1.3 验收 5 条**（详见 spec "Stage 1.3 验收" Requirement）

## Goals / Non-Goals

### Goals

1. **支持 KFD SVM 驱动代码零修改编译**（路线图 §1.3 验收第 2 条）：阶段 1.4 时拷贝 `drivers/gpu/drm/amd/amdkfd/kfd_svm.c` 仅调整 `#include` 路径，逻辑零修改
2. **完整化 `mmu_notifier` 框架**：register/unregister + `invalidate_range_start`/`end` 派发 + sequence number 协议
3. **完整化 HMM range fault 路径**：`hmm_range_fault()` + `struct hmm_range` + `struct mmu_interval_notifier`（替代已移除的 `hmm_mirror`）
4. **完整化 migrate 框架**：page migration between CPU/GPU memory domain（`migrate_to_ram` / `migrate_to_dev`）
5. **支持 fault 注入路径**：user-space mmap 触发 page fault → 通过 mmu_notifier 通知 device driver
6. **zone_device 最小实现**：spm vma + page state machine
7. **承接 1.2/1.3 边界契约 G1-G4 完整化**：从 1.2 骨架到 1.3 完整实现的契约验证
8. **目标 LTS 锁定 = Linux 6.12 LTS**（按盲点 5 决策）
9. **保持 3 区分架构**：主要工作在 ① 内核环境模拟层 + ② 可移植驱动层（uvm module） + ③ 硬件模拟层（page fault handler + migration 状态机）
10. **HAL guardrail 延续 1.2 严格度**：不预先添加 `hal_uvm_*` ops

### Non-Goals

1. **完整 KFD SVM 集成验证**：1.3 仅完成 UVM/HMM 框架 + PoC；1.4 集成验证才把 `kfd_svm.c` / `kfd_process.c` 拷贝进来
2. **NUMA-aware page placement**：完整 NUMA 优化在 Stage 2+
3. **多级页表 IOMMU**：1.1 已实现单级 4KB，1.3 沿用
4. **Hardware-accelerated page migration**：1.3 仅做软件 fallback；硬件加速在 Stage 2+
5. **HAL `hal_uvm_*` ops**：本次**不**预先添加（按 Oracle + ADR-027 + ADR-035，仅当 1.4 集成 KFD 时按需走 ADR 流程）
6. **生产性能**：阶段 1.3 仅要求"最小可用集"，不要求接近真机性能
7. **UMQ (User Mode Queue)**：UMQ 由 ADR-024 完整覆盖，**不在** 1.3 范围

## Decisions

### Decision 1: 内部 PoC 先完成（路线图 §5 缓解）

**选择**：在 task group 2 之前，先做内部 PoC：userfaultfd + mmap 共享触发场景。

**理由**：
- UVM/HMM 链路深（mmu_notifier → hmm_range → fault_inject → zone_device → migrate → sim page fault handler），一上来铺全套容易陷入细节
- 内部 PoC 仅验证"① 内核环境模拟层 + ③ 硬件模拟层 page fault 链路通畅"
- PoC 验证通过后再按 1.2 模式展开（TDD：先 spec scenario → test → impl）

**备选**：
- ❌ 一上来铺全套：高风险，可能 1.3 闭环后才发现核心接口偏差
- ✅ 内部 PoC：低风险，与 1.2 模式一致

### Decision 2: `struct mmu_interval_notifier` 替代 `struct hmm_mirror`（盲点 2 关键修正）

**选择**：`hmm.h` 实现 `struct mmu_interval_notifier` + `struct mmu_interval_notifier_ops.invalidate` 回调，**不**为 `struct hmm_mirror` 提供 ABI。

**理由**（librarian 2026-07-02 验证）：
- `struct hmm_mirror` 在 Linux 6.x 已移除（用 `mmu_interval_notifier` 替代）
- amdkpu / amdgpu 实际调用 `mmu_interval_notifier_insert()`，不调 `hmm_mirror` 路径
- 序列号一致性协议：`mmu_interval_read_begin()` / `mmu_interval_read_retry()` / `mmu_interval_set_seq()`

**变动影响**：路线图 §1.3 原 "HMM API 子集（`hmm_range_fault/register/unregister` + `struct hmm_range` + `struct hmm_mirror`）" 修正为 "实现 `mmu_interval_notifier` + `hmm_range_fault` + `struct hmm_range` + 序列号协议"。

### Decision 3: 1.2/1.3 边界契约 G1-G4 完整化（承接 stage-1-2-drm-subset）

**选择**：1.2 阶段锁定 4 项契约（G1-G4），1.3 阶段在实现中验证 + 测试。

**接口契约（1.3 必须验证）**：

| 契约 | 1.2 保证 | 1.3 验证点 |
|------|---------|-----------|
| `struct drm_device` 生命周期 | = `GpgpuDevice` 生命周期 | uvm module 持有 `drm_device*`，整个设备存在期间有效；`drm_device` 必须 outlive 所有 `mmu_interval_notifier` 和 `hmm_range` 实例 |
| BO 引用计数 | `close(fd)` 释放所有 GEM handle 引用 | `mmu_interval_notifier.invalidate` 和 `hmm_range_fault` 永不能引用 refcount=0 的 BO |
| prime import buffer 释放顺序 | `dma_buf_unmap → dma_buf_detach → dma_buf_put` | mmu_notifier invalidate 在 `dma_buf_detach` 前完成 |
| fence 触发时机 | 所有 fence signal 前不 release GEM | hmm_range fault 完成前不 trigger GEM release |

**强制验收**：
- `tests/test_uvm_drm_lifecycle_standalone`（1.2 骨架 + 1.3 完整化）—— G1
- 1.3 design.md "Decision D3: 1.2/1.3 边界契约" 章节列出 4 项接口契约 —— G3
- 1.3 不预先实现 1.4 完整 KFD SVM 集成 —— G4
- `mmu_interval_notifier` + `hmm_range_fault` API 签名与 Linux 6.12 ABI 一致（**关键修正**：不用 `hmm_mirror`）—— G2

### Decision 4: HAL 条件性扩展（承接 stage-1-2 Oracle D4）

**选择**：1.3 不预先添加 `hal_uvm_*` ops。仅当 1.4 集成真实 KFD 时按需添加，每 op 必须提供 **call trace + compile log** 证明。

**理由**：
- 严守 ADR-023 的 11 ops 上限
- 按 ADR-027 spec-driven 增量原则，避免凭想象预先添加
- 每次新增 op 独立走 ADR-035 流程，双人 review

**实施**：
- 在 `openspec/changes/stage-1-3-uvm-hmm/specs/hal-uvm-ops-audit.md` 中创建审计文档
- 即使 0 ops 也必须有文件（便于后续审计追踪）

### Decision 5: zone_device 最小实现（spm vma + page state machine）

**选择**：1.3 阶段 `zone_device` 仅做最简实现 —— spm vma + page state machine（`PAGE_STATE_CPU` / `PAGE_STATE_GPU` / `PAGE_STATE_MIGRATING` 三态机）。

**理由**：
- 完整 zone_device（含 migration helper / page reference count / device-private pages）在 Stage 2+
- 1.3 阶段只需要支持 HMM range fault + migrate 的最小路径
- 三态机足够覆盖 page migration 状态流转

### Decision 6: 目标 LTS 锁定 Linux 6.12（盲点 5）

**选择**：HMM/mmu_notifier 源码取自 Linux 6.12 LTS，API 与 6.12 ABI 对齐。

**理由**：
- Linux 6.12 包含最新 HMM 子集结构，KFD 主线合并更完整
- 与 ADR-027 "spec-driven 增量" 一致
- 兼容矩阵报告 `docs/05-advanced/hmm-compat-matrix.md` 记录 6.6 ↔ 6.12 差异；1.4 集成时按需补 6.6

**已知差异**（librarian 验证）：
- `struct hmm_mirror` 在 6.x 已移除（用 `mmu_interval_notifier` 替代）
- `mmu_interval_notifier_insert` 签名在 6.6 ↔ 6.12 之间有微调

### Decision 7: fault 注入路径（盲点 3）

**选择**：1.3 阶段 `fault_inject.cpp` 提供 user-space mmap → page fault → mmu_notifier 通知 device driver 的注入路径。

**理由**：
- 真实 KFD 测试需要可控的 fault 触发（不能依赖真实 mmap race）
- 注入路径仅在测试场景使用（生产路径走真实 page fault）
- 与 1.1 IOMMU 注入路径风格一致

## Risks / Trade-offs

### Risk 1: mmu_notifier + drm_device 生命周期耦合

- **概率**：高
- **影响**：高
- **缓解**：
  - Decision 3 边界契约 G1-G4 已锁（1.2 阶段）
  - `tests/test_uvm_drm_lifecycle_standalone` 1.3 完整化
  - 1.3 不预先实现 1.4 完整 KFD SVM 集成（G4）

### Risk 2: hmm_mirror API 误用

- **概率**：低
- **影响**：高
- **缓解**：
  - Decision 2 已修正目标 API 集（librarian 验证 amdkpu 实际调用）
  - `linux_compat/hmm.h` 对齐 Linux 6.12 LTS ABI（不声明 `hmm_mirror`）
  - 1.3 PoC 验证：mmap 共享 + userfaultfd + mmu_notifier 链路通畅

### Risk 3: HMM range fault 性能与一致性

- **概率**：中
- **影响**：中
- **缓解**：
  - Decision 1 内部 PoC 先完成（避免一上来铺全套）
  - sequence number 协议严格按 Linux 6.12 实现
  - 1.3 阶段不要求接近真机性能（仅"最小可用集"）

### Risk 4: migrate 接口签名 6.6 ↔ 6.12 断崖

- **概率**：低
- **影响**：中
- **缓解**：
  - Decision 6 已锁 6.12 LTS
  - 兼容矩阵报告 `docs/05-advanced/hmm-compat-matrix.md` 记录差异
  - 1.4 集成时按需补 6.6 兼容

### Risk 5: HAL ops 越界（>11）

- **概率**：低
- **影响**：中
- **缓解**：
  - Decision 4 条件性扩展（ADR-035）
  - `hal-uvm-ops-audit.md` 每 op 需 call trace + compile log
  - 1.3 阶段不预先添加 `hal_uvm_*` ops

### Risk 6: 1.3/1.4 边界（mmu_notifier 完整化 vs KFD 集成）

- **概率**：中
- **影响**：中
- **缓解**：
  - 1.3 仅完成 UVM/HMM 框架 + PoC
  - 1.4 集成验证才把 `kfd_svm.c` / `kfd_process.c` 拷贝进来
  - `plugins/gpu_driver/uvm/svm_ioctl.cpp` **仅当 KFD uvm 子模块需要时创建**（按路线图 §1.3 ②）

## Migration Plan

### Rollout

1. **Phase A（PoC 验证）**：内部 PoC（userfaultfd + mmap 共享触发场景）—— 验证 ① + ③ page fault 链路通畅
2. **Phase B（头文件 + 框架）**：`include/linux_compat/{mmu_notifier,hmm}.h` + `src/kernel/uvm/{mmu_notifier,hmm_range,migrate,fault_inject,zone_device,page_state_machine}.cpp`
3. **Phase C（sim 端）**：`plugins/gpu_driver/sim/{page_fault_handler,page_migration}.cpp`（page state machine + fault handler）
4. **Phase D（uvm module 条件性）**：`plugins/gpu_driver/uvm/svm_ioctl.cpp`（**仅当 KFD uvm 子模块需要**）
5. **Phase E（HAL 条件性）**：`hal_uvm_*` ops（**仅当 1.4 集成 KFD 时按需走 ADR 流程**；本次 0 ops）
6. **Phase F（CMake 集成）**：`src/CMakeLists.txt` 添加 `src/kernel/uvm/*.cpp` 到 kernel SHARED 库
7. **Phase G（测试交付）**：3 个 Catch2 standalone 测试（mmu_notifier / hmm_range / svm_ioctl）
8. **Phase H（文档）**：`docs/05-advanced/uvm-error-semantics.md` + `hmm-compat-matrix.md` + `post-refactor-architecture.md §1.10` 标注 1.3 完成
9. **Phase I（CI 验证）**：所有现有 52 测试 + 3 新测试全绿（55 总）
10. **Phase J（触发下一子阶段）**：归档本 change 后启动 `stage-1-4-kfd-portability`

### Rollback

由于 1.3 不修改 System C ioctl 编号（沿用 1.2 预留），不引入 HAL ops（承接 1.2 严格度），不修改既有 capability，回滚策略：

```bash
git revert <stage-1.3 commit>
```

回滚后 UsrLinuxEmu 退回子阶段 1.2 状态；`include/linux_compat/{mmu_notifier,hmm}.h` 新增字段保留（与 1.2 模式一致，回滚需检查依赖）。

## Open Questions

1. **Q：1.3 是否需要支持 NVIDIA HMM API？**
   A：1.3 完成 AMD 后，NVIDIA HMM API（结构与 AMD 相似但 ops 集不同）可叠加（结构完全一致），预留 `nv_hmm_ops` 等效头文件待 Stage 2+

2. **Q：zone_device 完整化何时做？**
   A：完整 zone_device（含 migration helper / page reference count / device-private pages）在 Stage 2+；1.3 仅最简实现

3. **Q：fault_inject 仅在测试场景使用还是生产路径？**
   A：仅在测试场景使用；生产路径走真实 page fault（user-space mmap 触发）

4. **Q：plugins/gpu_driver/uvm/svm_ioctl.cpp 是否在 1.3 范围？**
   A：**条件性**。仅当 1.3 PoC 发现 KFD uvm 子模块需要 driver 侧 svm_ioctl handler 时才创建；否则留到 1.4 集成验证一并创建

5. **Q：5 个 KFD ioctl 在 1.4 集成时是否够用？**
   A：1.3 不新增 ioctl；1.4 集成时按需按 ADR-035 走 change 流程
