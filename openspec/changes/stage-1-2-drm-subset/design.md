## Context

### 背景

UsrLinuxEmu 现有架构（[post-refactor-architecture.md §1.10](../../docs/02_architecture/post-refactor-architecture.md)）在 `include/linux_compat/drm/` 已提供 `drm_driver.h` / `drm_gem.h` / `drm_ioctl.h` 头文件骨架（1.2 子阶段起点），`plugins/gpu_driver/drv/gpu_drm_driver.cpp`（288 行，含 7 个 IOCTL 的 `drm_ioctl_desc[]` 表）已嵌入 `struct drm_device`，但 **DRM / GEM / TTM 完整生命周期未实现**，`GpgpuDevice` 仍是 UsrLinuxEmu 自创接口。

阶段 1.2 子阶段（[stage-1-kernel-emu.md §子阶段 1.2](../../docs/roadmap/stage-1-kernel-emu.md)）是阶段 1 第三个子阶段，强依赖 1.1 的 IOMMU group 拓扑（用于 prime import 的 DMA remap），并被子阶段 1.3 UVM/HMM 所依赖（uvm module 是 `drm_device` 的子模块）。

### Oracle 评估（2026-07-02）

4 项决策全部 Recommended，启动条件为 **Conditional Go**：

| 决策 | 选项 | 推荐 | 缓解 |
|------|------|------|------|
| **D1** GpgpuDevice 重构粒度 | A. 保留 FileOperations + 内部重定向 / B. 移除 FileOperations | **A** | `gpgpu_device.cpp` 保留 `ioctl()` stub 内部重定向 `drm_ioctl()`；新增 `errno_to_linux()` 映射层 |
| **D2** KFD IOCTL 编号预留 | A. 1.2 阶段预留 / B. 1.4 阶段再添加 | **A** | `scripts/check_gpu_ioctl_sync.sh` 验证 UsrLinuxEmu ↔ TaskRunner 双端同步（**2026-07-02 已达成**，15 IOCTL diff=0）|
| **D3** amdkfd PoC 时机 | A. 早期（task group 9）/ B. 全 task 后 | **A** | `kfd_queue.c` PoC **2026-07-02 已达成**（errors=0, warnings=2）|
| **D4** HAL 扩展策略 | A. 条件性添加 / B. 全加 | **A** | 严守 ADR-023 的 11 ops 上限；每 op 必须 KFD 实际调用驱动 |

### Oracle 5 项盲点（已处理）

| # | 盲点 | 处理 |
|---|------|------|
| 1 | mmu_notifier + drm_device 生命周期耦合 | 1.2/1.3 边界契约 G1-G4（见 Decision 5） |
| 2 | dma_buf_attach 语义（librarian 验证） | **关键修正**：amdgpu 不调 `dma_buf_attach()`，改 `dma_buf_dynamic_attach()` |
| 3 | 错误码语义端到端一致性 | `test_drm_ioctl_dispatch_standalone` 含 errno mapping 验证 |
| 4 | render node 权限（VFS 零基础设施） | ADR-037 创建 + VFS-1~VFS-4 已完成 |
| 5 | Linux 6.6 vs 6.12 头文件差异 | **锁定目标 LTS = 6.12**（Linux 6.12 推荐） |

### 当前状态

- **已存在**：
  - `include/linux_compat/drm/{drm_driver.h, drm_gem.h, drm_ioctl.h}`（1.2 子阶段起点）
  - `plugins/gpu_driver/drv/gpu_drm_driver.cpp`（288 行，7 IOCTL）
  - `include/kernel/device/device.h`：含 `mode_t mode` / `uid_t uid` / `gid_t gid` 字段（VFS-1 已完成）
  - `src/kernel/vfs.cpp`：含 `chmod/chown/fchmod/access` + `check_permission()` hook（VFS-2~VFS-4 已完成）
  - `plugins/gpu_driver/shared/gpu_ioctl.h`：含 KFD 预留 IOCTL（`0x44-0x47`）+ CREATE_QUEUE (0x40) 字段扩展
  - `scripts/check_gpu_ioctl_sync.sh`：双端 IOCTL 同步验证脚本
- **完全不存在**：
  - `src/kernel/drm/` 框架（新增）
  - `include/linux_compat/drm/{drm_prime.h, drm_file_operations.h, drm_mode_config.h}`（新增）

### 约束

- **ADR-019**：DRM/GEM/TTM 对齐路径已 Accepted
- **ADR-027**：Linux 兼容层扩展走 spec-driven 增量（禁止凭想象预先添加）
- **ADR-035**：HAL 接口扩展必须走 ADR 流程
- **ADR-036**：工作分层必须在 ① / ② / ③ 三区中明确归属
- **ADR-037（已建）**：VFS Device 权限模型（render node 0666 默认、permission check hook）
- **路线图 §1.2 验收 7 条**（详见 spec "Stage 1.2 验收" Requirement）

## Goals / Non-Goals

### Goals

1. **支持真实 KFD 驱动代码零修改编译**（路线图 §1.2 验收第 1 条）：阶段 1.4 时拷贝 `drivers/gpu/drm/amd/amdkfd/*.c` 仅调整 `#include` 路径，逻辑零修改
2. **完整化 GEM object 生命周期**：`drm_gem_object_init` / `handle_create` / refcount / release（ASan 验证无泄漏）
3. **支持 prime 跨设备 buffer 共享**：实现 `dma_buf_dynamic_attach/detach/map_attachment/unmap_attachment/pin/unpin`（**关键修正**：amdgpu 不调 `dma_buf_attach()`）
4. **支持 render node 权限分离**：`/dev/dri/renderD128` 与 `/dev/dri/card0` 两类节点 + DRM 主节点控制
5. **`drm_ioctl_desc[]` 覆盖 ≥15 个 IOCTL**：Stage 1.4 的 5 个 KFD ioctl + 现有 System C 接口
6. **目标 LTS 锁定 = Linux 6.12 LTS**（按盲点 5 决策）：KFD/amdgpu 源码取自 6.12 LTS，兼容矩阵报告记录 6.6 ↔ 6.12 差异
7. **保持 3 区分架构**：主要工作在 ① 内核环境模拟层 + ② 可移植驱动层 + 微量 HAL 桥

### Non-Goals

1. **mmu_notifier 完整实现**：完整 `mmu_subsystem` 在 1.3 UVM/HMM；1.2 仅在 `drm_prime.h` 锁定与 1.3 的契约（G2 盲点）
2. **hmm_range / migrate 实现**：完整 HMM 在 1.3；1.2 通过 G4 锁边界契约，不预先实现
3. **多级页表 IOMMU**：1.1 已实现单级 4KB，1.2 复用
4. **HAL `hal_drm_*` ops**：本次**不**预先添加（按 Oracle D4 + ADR-027 + ADR-035，仅当 1.4 集成 KFD 时按需走 ADR 流程）
5. **生产性能**：阶段 1.2 仅要求"最小可用集"，不要求接近真机性能
6. **NVIDIA DRM ioctl**：NVIDIA 的 DRM 子驱动（29 个 ioctl）结构与 AMD 相同（`DRM_IOCTL_DEF_DRV` + per-struct），可复用框架，但**不在 1.2 范围**——1.2 完成 AMD 后，未来 Stage 可叠加

## Decisions

### Decision 1: 保留 FileOperations 入口（Oracle D1）

**选择**：GpgpuDevice 重构保留 `FileOperations` 入口，内部嵌入 `struct drm_device`，ioctl 派发通过 `drm_ioctl_desc[]`。

**理由**：
- TaskRunner 当前通过 `FileOperations` 接口使用 UsrLinuxEmu（system C ioctl 路径）
- System C IOCTL 编号保持不变（`scripts/check_gpu_ioctl_sync.sh` 验证双端 15 IOCTL 同步）
- `ioctl()` stub 内部重定向到 `drm_ioctl()`，保留 drm_ioctl_desc[] 分派路径
- 新增 `errno_to_linux()` 映射层，确保 UsrLinuxEmu 模拟器返回的 errno 与 Linux 6.12 ABI 一致

**备选**：
- ❌ 完全移除 `FileOperations`：破坏 TaskRunner 集成，违反 ADR-035 ABI 治理
- ✅ 保留入口 + 内部重定向：AB I 兼容 + DRM 框架复用

### Decision 2: KFD IOCTL 编号预留（Oracle D2，已完成）

**选择**：Stage 1.4 的 5 个 KFD ioctl 在 1.2 阶段提前锁定 System C 编号。CREATE_QUEUE (0x40) 通过追加字段扩展保持 ABI 向后兼容（**已由选项 B 完成**）。

**理由**：
- UsrLinuxEmu 与 TaskRunner 通过符号链接共享 `gpu_ioctl.h`
- 编号变更需要双端同步 + ADR-035 §Rule 5.1 四步协议 + dual-ack
- 提前锁死避免 1.4 时 rush 风险

**实施**（2026-07-02 完成）：
- `plugins/gpu_driver/shared/gpu_ioctl.h`：新增 4 个 IOCTL（`0x44-0x47`）+ CREATE_QUEUE (0x40) 字段扩展
- `scripts/check_gpu_ioctl_sync.sh`：自动验证双端 15 IOCTL 同步（当前 diff=0）

### Decision 3: amdkfd 早期 PoC（Oracle D3，已完成）

**选择**：在 task group 9 早期（截至 2026-07-15）执行 amdkfd 单文件编译验证，及时暴露 Linux API 缺口。

**理由**：
- PoC 过晚会导致 1.2 闭环后才发现内核 API 缺口（如 TTM migrate / prime 语义），1.4 集成时大范围回滚
- 早期 PoC 揭示的错误码语义不一致（如 `-ENOSPC`）及时修正

**实施**（2026-07-02 完成）：
- 取 `kfd_queue.c` 从 Linux 6.12 LTS，拷贝到 `plugins/gpu_driver/drv/kfd/kfd_queue.c`（**逻辑零修改，仅 #include**）
- 新增 5 个最小 compat 头：`linux_compat/{slab,list}.h` + KFD 本地 stub `{kfd_priv,kfd_topology,kfd_svm}.h`
- **PoC 结果：errors=0, warnings=2**（≤3）
- Artifacts：`openspec/evidence/amdkfd-poc-2026-07-02/{kfd_queue.o,build.log}`
- 41/41 既有测试零回归

### Decision 4: HAL 条件性扩展（Oracle D4）

**选择**：1.2 不预先添加 `hal_drm_*` ops。仅当 1.4 集成真实 KFD 时按需添加，每 op 必须提供 **call trace + compile log** 证明。

**理由**：
- 严守 ADR-023 的 11 ops 上限
- 按 ADR-027 spec-driven 增量原则，避免凭想象预先添加
- 每次新增 op 独立走 ADR-035 流程，双人 review

**实施**：
- 在 `openspec/changes/stage-1-2-drm-subset/specs/hal-drm-ops-audit.md` 中创建审计文档
- 即使 0 ops 也必须有文件（便于后续审计追踪）

### Decision 5: 1.2/1.3 边界契约（Oracle 盲点 1）

**目的**：锁定 1.2 与 1.3 的接口契约，避免 1.2 实施决策反向破坏 1.3 的 mmu_notifier / `struct hmm_range` 生命周期管理。

**接口契约（1.2 必须提供给 1.3）**：

| 契约 | 1.2 必须保证 | 1.3 期望 |
|------|-------------|---------|
| `struct drm_device` 生命周期 | 与 `GpgpuDevice` 同生命周期（创建时 init，析构前 shutdown） | uvm module 持有 `drm_device*` 指针，整个设备存在期间有效 |
| BO 引用计数 | `drm_gem_object` refcount 在 close(fd) 时全部 release | hmm_range 不会引用已 release 的 BO |
| prime import buffer 释放顺序 | `dma_buf_unmap` → `dma_buf_detach` → `dma_buf_put`（对标 Linux 6.12）| mmu_notifier invalidate 在 `dma_buf_detach` 前完成 |
| fence 触发时机 | GEM object release 前必须等待所有 fence signal | hmm_range fault 完成前不会 trigger GEM release |

**强制验收**：
- `tests/test_uvm_drm_lifecycle_standalone`（1.2 骨架 + 1.3 完整化）—— G1
- 1.2 design.md "Decision D5: 1.2/1.3 边界契约" 章节列出 4 项接口契约 —— G3
- 1.2 不预先实现 1.3 mmu_notifier / hmm_range 代码 —— G4
- `dma_buf_attach/detach/map_attachment/unmap_attachment/pin/unpin` API 签名与 Linux 6.12 ABI 一致（**关键修正**：amdgpu 不用 `dma_buf_attach`，改 `dma_buf_dynamic_attach`）—— G2

### Decision 6: dma_buf API 目标修正（盲点 2 关键修正）

**选择**：`drm_prime.h` 实现 `dma_buf_dynamic_attach` + `dma_buf_detach` + `dma_buf_map_attachment` + `dma_buf_unmap_attachment` + `dma_buf_pin` + `dma_buf_unpin` + `struct dma_buf_attach_ops`（`allow_peer2peer` + `move_notify`）。**不**为 `dma_buf_attach()` 提供 ABI。

**理由**（librarian 2026-07-02 验证）：
- amdgpu `amdgpu_dma_buf.c:570` 使用 `dma_buf_dynamic_attach()`，**不调用** `dma_buf_attach()`
- `dma_buf_attach()` 签名 6.6 ↔ 6.12 无变化，但 KFD 编译路径不走它
- `map_dma_buf` 必须返回有效 `sg_table`，**不能**返回 -ENOSYS；IOMMU `map_page` 可旁路（恒等映射）

**变动影响**：1.2 Design 原计划 "drm_prime 实现 `dma_buf_attach/detach`" 修正为 "实现 `dma_buf_dynamic_attach/detach/map_attachment/unmap_attachment/pin/unpin` + `struct dma_buf_attach_ops`"。

### Decision 7: VFS 权限基础设施（盲点 4 + ADR-037）

**选择**：扩展 `Device` 结构体 + 多段路径 + chmod/chown 接口（已由 VFS-1~VFS-4 完成，2026-07-02）。

**理由**：
- UsrLinuxEmu 原 VFS 零权限基础设施（grep 全仓 0666/chmod/chown/i_mode 全部零命中）
- KFD 编译需求 `chmod`/`chown`/`fchmod`/`access` 接口存在 + 默认 0666 模式
- 单用户环境无强权限校验需求，但 API 完整是 KFD 编译前提

### Decision 8: 目标 LTS 锁定 Linux 6.12（盲点 5）

**选择**：amdkfd 源码取自 Linux 6.12 LTS，KFD 5 个核心 ioctl 与 6.12 ABI 对齐。

**理由**：
- Linux 6.12 包含最新 DRM 子集结构，KFD 主线合并更完整
- 与 ADR-027 "spec-driven 增量" 一致
- 兼容矩阵报告 `docs/05-advanced/drm-compat-matrix.md` 记录 6.6 ↔ 6.12 差异；1.4 集成时按需补 6.6

**已知差异（librarian 验证）**：
- `struct dma_buf.list_node` 在 6.12 受 `CONFIG_DEBUG_FS` 条件化（默认开 debugfs，影响可控）
- `dma_buf_attach()` 签名无变化，但 KFD 不调它（Decision 6）

## Risks / Trade-offs

### Risk 1: mmu_notifier + drm_device 生命周期耦合

- **概率**：中
- **影响**：高
- **缓解**：
  - Decision 5 边界契约 G1-G4 已锁
  - `tests/test_uvm_drm_lifecycle_standalone`（1.2 骨架 + 1.3 完整化）
  - 1.2 不预先实现 1.3 mmu_notifier 代码（G4）

### Risk 2: dma_buf API 实现偏差

- **概率**：低
- **影响**：中
- **缓解**：
  - Decision 6 已修正目标 API 集（librarian 验证 amdgpu 实际调用）
  - `linux_compat/drm/drm_prime.h` 对齐 Linux 6.12 LTS ABI
  - 1.2 PoC 验证：amdkfd `kfd_queue.c` 编译通过（errors=0）

### Risk 3: GpgpuDevice 重构破坏 TaskRunner 集成

- **概率**：低
- **影响**：高（破坏双向集成）
- **缓解**：
  - Decision 1 保留 `FileOperations` 入口（Oracle D1）
  - `errno_to_linux()` 映射层确保 errno 语义
  - `scripts/check_gpu_ioctl_sync.sh` 验证双端 15 IOCTL 同步（已通过）

### Risk 4: render node 权限模型与 Linux 偏离

- **概率**：低
- **影响**：中
- **缓解**：
  - ADR-037 已 Approval（2026-07-02 Proposed）
  - VFS-1~VFS-4 已实施（2026-07-02），41/41 测试零回归
  - mode/uid/gid 默认 0666/0/0 对齐 Linux udev 默认

### Risk 5: HAL ops 越界（>11）

- **概率**：低
- **影响**：中
- **缓解**：
  - Decision 4 条件性扩展（ADR-035）
  - `hal-drm-ops-audit.md` 每 op 需 call trace + compile log
  - Oracle C4 阻断条件：> 2 ops 触发 merge 阻断

### Risk 6: KFD 5 个 ioctl 在 1.4 时编号变更需求

- **概率**：低
- **影响**：中
- **缓解**：
  - Decision 2 已锁编号（`0x44-0x47`）+ CREATE_QUEUE (0x40) 字段扩展
  - `scripts/check_gpu_ioctl_sync.sh` 持续验证
  - 若需变更，按 ADR-035 §Rule 5.1 + kfd-portability-report.md 记录

## Migration Plan

### Rollout

1. **Phase A（VFS 基础）**——已完成 2026-07-02：VFS-1~VFS-4 实现（含 ADR-037）
2. **Phase B（IOCTL 预留）**——已完成 2026-07-02：4 个 KFD IOCTL + scripts/check_gpu_ioctl_sync.sh
3. **Phase C（amdkfd PoC）**——已完成 2026-07-02：kfd_queue.c 编译 errors=0
4. **Phase D（DRM 实现）**——**本次**：src/kernel/drm/ + linux_compat/drm 扩展 + GpgpuDevice 重构 + render node
5. **Phase E（CI 验证）**：所有现有 41 测试 + 4 新测试全绿
6. **Phase F（触发下一子阶段）**：归档本 change 后启动 `stage-1-3-uvm-hmm`

### Rollback

由于 1.2 不修改 System C ioctl 编号（仅新增 + CREATE_QUEUE 扩展后端兼容），不引入 HAL ops，不修改既有 capability，回滚策略：

```bash
git revert <stage-1.2 commit>
```

回滚后 UsrLinuxEmu 退回子阶段 1.1 状态；但 gpgpu_ioctl.h 新增字段保留（已与 TaskRunner 双端同步，回滚需双端同步 revert）。

## Open Questions

1. **Q：1.2 是否需要支持 NVIDIA DRM ioctls？**
   A：1.2 完成 AMD 后，NVIDIA 28 个 DRM ioctls 可叠加（结构完全一致），预留 `nv_drm_common_ioctl.h` 等效头文件待 Stage 2+

2. **Q：render node 权限硬执行何时？**
   A：当前阶段 1.2 no-op；硬执行留到 Stage 2+ 多用户场景，ADR-037 已预留 `check_permission()` hook

3. **Q：drm_mode_config / KMS 是否在 1.2 范围？**
   A：不在（路线图 §1.2 仅要求"基础结构占位，最小可用即可"）。完整 KMS 在 Stage 2+ 评估

4. **Q：5 个 KFD ioctl 真实 KFD 编译时若发现还需新增 ioctl 怎么办？**
   A：按 ADR-035 走 change 流程，新增 ioctl + struct + update SSOT 附录 A + 更新 scripts/check_gpu_ioctl_sync.sh