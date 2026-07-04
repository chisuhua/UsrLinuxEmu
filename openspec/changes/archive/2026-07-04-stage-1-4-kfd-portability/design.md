## Context

### 背景

UsrLinuxEmu 阶段 1（[post-refactor-architecture.md §1.10](../../docs/02_architecture/post-refactor-architecture.md)）的目标是补齐 Linux 内核环境模拟（PCIe + IOMMU + DRM + UVM/HMM），使真实 KFD 驱动代码**逻辑零修改**即可在 UsrLinuxEmu 中编译运行。前 4 个子阶段已完成：

- **1.0 PCIe Emulation**（已归档 `stage-1-0-pcie-emu/`）—— config space 4KB + 5 capability 链 + MSI-X
- **1.1 IOMMU + ATS**（已归档 `2026-07-02-stage-1-1-iommu-ats/`）—— iommu_domain/group/ioasid + DMA remap + ATS 4 核心消息
- **1.2 DRM 子集**（已归档 `2026-07-02-stage-1-2-drm-subset/`）—— DRM/GEM/Prime + 19 IOCTL + KFD 5 ioctl 编号预留 + KFD 单文件 PoC `kfd_queue.c`
- **1.3 UVM/HMM**（已归档 `2026-07-04-stage-1-3-uvm-hmm/`）—— mmu_notifier/hmm_range/migrate + 9 个测试 + G1-G4 边界契约

**1.4 集成验证** 是阶段 1 的最终里程碑，验证 5 个 KFD ioctl 真实驱动路径可编译运行。本 change 设计围绕 4 条用户决策（2026-07-04）展开：

| 决策 | 内容 | 影响 |
|------|------|------|
| 决策 1 | 实施 1.4 代码时创建 worktree（`stage-1.4-kfd-portability`） | 与 main 分支隔离；合并前 rebase |
| 决策 2 | HAL ops 严格走 ADR 流程（不在 1.4 commit 中"顺手"添加） | 每个新 HAL op → 一个独立 ADR → 一个独立 commit |
| 决策 3 | 先跑回归测试再开始实施（避免 1.2/1.3 G1-G4 regression） | LC3 验证：kfd_queue.c + G1-G4 + KFD handler dispatch |
| 决策 A | 启动方式：先更新追踪文档 + 启动 OpenSpec change | 追踪 plan 已更新（`docs/superpowers/plans/2026-07-02-stage-1-kernel-emu-tracking.md` 73 checkbox 已勾选） |

### Oracle 评估（2026-07-04）+ 用户决策对齐

1.2 阶段已锁定 1.2/1.3 边界契约 G1-G4（[stage-1-2-drm-subset/design.md §Decision 5](../archive/2026-07-02-stage-1-2-drm-subset/design.md)），本 change 是在该契约上做集成验证：

| 契约 | 1.2/1.3 保证 | 1.4 验证 |
|------|------------|---------|
| G1 `drm_device` 生命周期 | = `GpgpuDevice` 生命周期 | KFD 通过 `GpgpuDevice::fd` 访问 `/dev/kfd`，生命周期一致 |
| G2 `dma_buf_dynamic_attach` 等 API 签名 | 与 Linux 6.12 ABI 一致 | KFD 调用一致；errors=0 |
| G3 4 项接口契约明确列出 | design.md Decision 5 | KFD 4 项接口调用通过 |
| G4 不预先实现 1.3 完整 migrate | 仅留接口边界 | 1.3 完整实现 migrate；1.4 KFD `kfd_process.c` 调用通过 |

### 用户决策 vs Oracle 盲点

| # | 用户决策 | Oracle 盲点 | 对齐 |
|---|---------|-----------|------|
| 1 | worktree 隔离 | "实施代码与 SSOT 同步冲突" | ✅ 完全对齐（决策 1） |
| 2 | HAL ops 走 ADR | "HAL ops 临时添加影响可移植性" | ✅ 完全对齐（决策 2） |
| 3 | 回归测试先行 | "1.2/1.3 G1-G4 boundary regression" | ✅ 完全对齐（决策 3） |
| 4 | 5 个 ioctl 而非完整 KFD | "KFD 代码量 ~50K 行" | ✅ 完全对齐（路线图 §5 缓解） |

## Decisions

### Decision 1: 实施使用 worktree 隔离（用户决策 1）

- **背景**：1.4 涉及 5 个 .c 移植 + 编译调通 + 5 个 ioctl 验证，工作量大（~5K 行），与 main 分支同步冲突概率高
- **方案**：使用 `superpowers/using-git-worktrees` skill 创建 `stage-1.4-kfd-portability` worktree，所有代码改动在 worktree 中进行
- **合并流程**：worktree 内 LC1-LC3 + 编译 + 5 个 ioctl 全绿 → rebase main → 提 PR → review → merge
- **何时创建**：本 change 启动阶段（proposal/design/tasks/specs 创建）可在 main 上完成；实际代码移植在 worktree 中进行

### Decision 2: HAL ops 严格走 ADR 流程（用户决策 2）

- **背景**：1.4 集成 KFD 时可能发现实际调用 `iommu_*` 或 `mmu_notifier_*` 内核 API
- **方案**：每个新 HAL op 走独立 ADR + 独立 commit + 独立 OpenSpec change
  - 检测到 KFD 调用 `iommu_map` → 创建 `hal-iommu-ops-extension` ADR → 新 OpenSpec change → 一个 commit
  - 检测到 KFD 调用 `mmu_interval_read_begin` → 创建 `hal-uvm-ops-extension` ADR → 新 OpenSpec change → 一个 commit
- **不允许**：在 1.4 commit 中"顺手"添加 HAL ops（违反 ADR-035 Rule 3）
- **审计**：每个 PR review 时检查"是否新增了非 HAL 的 HAL op"

### Decision 3: 回归测试先行（用户决策 3）

- **背景**：1.4 集成 5 个 .c 可能触发 1.2/1.3 边界契约 G1-G4 的 regression
- **方案**：在 worktree 中正式实施前，先跑 3 个关键测试：
  - `test_drm_kfd_handlers_standalone`（KFD handler dispatch）
  - `test_uvm_drm_lifecycle_standalone`（1.2/1.3 G1-G4 边界契约）
  - `test_kfd_queue_standalone` 或对应 `kfd_queue.c` PoC（1.2 单文件 PoC）
- **通过条件**：3 个测试全绿 + 无新增 warnings（与 1.2 阶段 `kfd_queue.c` PoC 时 warnings=2 基线一致）
- **失败处理**：回归测试失败 → 优先修复 1.2/1.3 regression → 暂缓 1.4 实施

### Decision 4: 移植策略（按文件名顺序）

- **背景**：5 个 .c 移植存在依赖关系（`kfd_module.c` → `kfd_device.c` → `kfd_process.c` → `kfd_queue.c` → `kfd_doorbell.c`）
- **方案**：按依赖顺序移植 + 每次移植后立即编译验证
  1. `kfd_module.c`（最小依赖，仅 module 入口）
  2. `kfd_queue.c`（1.2 阶段已有 PoC 基础，仅需补全依赖）
  3. `kfd_device.c`（依赖 module + queue）
  4. `kfd_doorbell.c`（依赖 device 拓扑）
  5. `kfd_process.c`（最重，依赖前 4 个 + 1.3 mmu_notifier）
- **每次移植后**：`cmake --build build --target gpu_kfd` 立即验证 errors=0
- **5 个 ioctl 验证**：5 个 .c 全部移植完成后跑端到端测试

### Decision 5: 错误码语义对齐（参考现有对照表）

- **背景**：1.0/1.1/1.2/1.3 阶段已建立 3 个错误码对照表（`iommu-error-semantics.md` / `drm-error-semantics.md` / `uvm-error-semantics.md`）
- **方案**：1.4 KFD 移植时参考 3 个对照表 + 编写 `kfd-error-semantics.md`（**新增**，KFD 路径特定错误码语义）
- **范围**：`-ENODEV` / `-ENOMEM` / `-EINVAL` / `-EREMOTEIO` / `-EFAULT` 等
- **验证方法**：5 个 ioctl happy path + 至少 1 个 error path 测试覆盖

### Decision 6: 验证方法

- **编译验证**：`cmake --build build` errors = 0
- **warnings 基线**：1.2 阶段 `kfd_queue.c` PoC 时 warnings=2（已知）；1.4 移植后总 warnings 数量记录在 `kfd-portability-report.md`
- **ioctl 验证**：5 个 ioctl 每个跑 happy path + 1 个 error path（合计 ≥10 个测试用例）
- **回归验证**：LC3（决策 3）三个测试 + ctest 全量（63/63 应保持）
- **文档验证**：SSOT §1.10 标注 Stage 1 完成 + 路线图状态更新 + README badges

## Risks

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | KFD `kfd_process.c` 涉及 `mmu_notifier` 完整迁移路径 | 中 | 高 | 1.3 阶段已完整实现 mmu_notifier + sequence number 协议；G1-G4 边界契约已锁 |
| R2 | KFD `kfd_device.c` 涉及 IOMMU group 拓扑 | 中 | 中 | 1.1 阶段已实现 iommu_domain/group 完整 API + register API |
| R3 | HAL ops 临时添加（违反决策 2）| 中 | 中 | 决策 2 严格走 ADR + 每个 PR review 检查 |
| R4 | worktree 与 main 同步冲突 | 中 | 低 | 决策 1 worktree 隔离 + 合并前 rebase main |
| R5 | 1.2/1.3 G1-G4 regression | 低 | 高 | 决策 3 回归测试先行 + LC3 验证 |
| R6 | 5 个 ioctl happy path 跑通但 error path 不全 | 中 | 中 | 决策 5 错误码对照表 + 决策 6 error path 测试要求 |
| R7 | 文档审计失败（路线图 §5 风险）| 低 | 低 | 每次 commit 前跑 `tools/docs-audit.sh`（pre-commit hook 自动）|
| R8 | `kfd_process.c` 中包含 SVM 路径超出 1.4 范围 | 中 | 中 | 仅做 5 个核心 ioctl（路线图 §1.4 范围）；SVM 完整路径作为 follow-up |

## Cross-References

- 路线图 §1.4: [stage-1-kernel-emu.md §子阶段 1.4](../../docs/roadmap/stage-1-kernel-emu.md)
- 追踪 plan: [docs/superpowers/plans/2026-07-02-stage-1-kernel-emu-tracking.md §Sub-stage 1.4](../../docs/superpowers/plans/2026-07-02-stage-1-kernel-emu-tracking.md)
- SSOT §1.10: [post-refactor-architecture.md §1.10](../../docs/02_architecture/post-refactor-architecture.md)
- 边界契约 G1-G4: [stage-1-2-drm-subset/design.md §Decision 5](../archive/2026-07-02-stage-1-2-drm-subset/design.md)
- 错误码对照表（参考）：
  - [iommu-error-semantics.md](../../docs/05-advanced/iommu-error-semantics.md)（1.1）
  - [drm-error-semantics.md](../../docs/05-advanced/drm-error-semantics.md)（1.2）
  - [uvm-error-semantics.md](../../docs/05-advanced/uvm-error-semantics.md)（1.3）
- ADR 索引: [ADR-035 治理规则](../../docs/00_adr/adr-035-governance-policy.md) + [ADR-027 兼容层策略](../../docs/00_adr/adr-027-linux-compat-strategy.md) + [ADR-036 三区分原则](../../docs/00_adr/adr-036-three-way-separation.md) + [ADR-023 HAL 契约](../../docs/00_adr/adr-023-hal-interface.md)
- KFD 单文件 PoC: commit `c42e60e` `feat(drm): add amdkfd single-file PoC (kfd_queue.c from Linux 6.12 LTS)`
- KFD 5 ioctl 编号预留: commit `468a196` `fix(tasks)` + `9141da6` `test(drm): add 4 KFD handler dispatch tests`