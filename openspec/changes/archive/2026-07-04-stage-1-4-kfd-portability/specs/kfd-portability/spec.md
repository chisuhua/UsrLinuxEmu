## ADDED Requirements

### Requirement: kfd-portability 集成验证 capability

The system MUST provide KFD (AMD Kernel Fusion Driver) 真实驱动可移植性集成验证，满足路线图 §1.4 显式验收指标（仅 `#include` 路径调整 + 逻辑零修改 + 5 个核心 ioctl 跑通）：

- **可移植性边界**：5 个 KFD .c 文件（`kfd_module.c` / `kfd_queue.c` / `kfd_device.c` / `kfd_doorbell.c` / `kfd_process.c`）从 Linux 6.6/6.12 LTS `drivers/gpu/drm/amd/amdkfd/` 拷贝到 `plugins/gpu_driver/drv/kfd/`，**逻辑零修改**（仅 `#include` 路径调整）
- **ioctl 兼容性**：5 个核心 KFD ioctl 在 UsrLinuxEmu 中跑通（happy path + 至少 1 个 error path）：
  - `GPU_IOCTL_GET_PROCESS_APERTURE` (0x44) ← `AMDKFD_IOC_GET_PROCESS_APERTURES_NEW` (0x14)
  - `GPU_IOCTL_CREATE_QUEUE` (0x40, KFD-compat 扩展) ← `AMDKFD_IOC_CREATE_QUEUE` (0x02)
  - `GPU_IOCTL_UPDATE_QUEUE` (0x45) ← `AMDKFD_IOC_UPDATE_QUEUE` (0x07)
  - `GPU_IOCTL_MAP_MEMORY` (0x46) ← `AMDKFD_IOC_MAP_MEMORY_TO_GPU` (0x16)
  - `GPU_IOCTL_UNMAP_MEMORY` (0x47) ← `AMDKFD_IOC_UNMAP_MEMORY_FROM_GPU` (0x18)
- **编译验证**：`cmake --build build` errors = 0（warnings 数量记录在移植报告）
- **架构判定一致性**：所有工作按 [ADR-036](../../docs/00_adr/adr-036-three-way-separation.md) 的 3 区分原则组织
- **承接 1.2/1.3 边界契约 G1-G4**：1.2/1.3 已锁定的 4 项契约在 1.4 集成验证时仍保持（`tests/test_uvm_drm_lifecycle_standalone` G1-G4 全验证）

#### Scenario: 5 个 KFD .c 文件完成移植（仅 #include 路径调整）

- **WHEN** 实施者将 Linux 6.6/6.12 LTS `drivers/gpu/drm/amd/amdkfd/{kfd_module,kfd_queue,kfd_device,kfd_doorbell,kfd_process}.c` 拷贝到 `plugins/gpu_driver/drv/kfd/`
- **THEN** 实施者 MUST 仅调整 `#include` 路径（`<linux/...>` → `linux_compat/...`，`<drm/...>` → `linux_compat/drm/...`）
- **AND** 逻辑代码 MUST NOT 修改（即使是"修复 bug"）；任何逻辑修改 MUST 在 `docs/05-advanced/kfd-portability-report.md` 中详细说明理由
- **AND** `cmake --build build` MUST 通过（errors = 0）

#### Scenario: 5 个核心 KFD ioctl 跑通

- **WHEN** 用户调用 `ioctl(fd_kfd, GPU_IOCTL_GET_PROCESS_APERTURE, &args)` 或其他 4 个 ioctl 之一
- **THEN** ioctl handler MUST 正常返回（happy path：返回 0 或有效数据）
- **AND** 测试 MUST 覆盖每个 ioctl 的 happy path + 至少 1 个 error path（合计 ≥10 个测试 case）
- **AND** 错误码语义 MUST 与 Linux 内核一致（参考 `iommu-error-semantics.md` / `drm-error-semantics.md` / `uvm-error-semantics.md` + 新增 `kfd-error-semantics.md`）

#### Scenario: 1.2/1.3 边界契约 G1-G4 无 regression

- **WHEN** 5 个 KFD .c 移植完成且 5 个 ioctl 跑通后
- **THEN** `tests/test_uvm_drm_lifecycle_standalone` MUST 全绿（G1-G4 边界契约验证）
- **AND** `tests/test_drm_kfd_handlers_standalone` MUST 全绿（KFD handler dispatch 验证）
- **AND** `ctest --test-dir build --output-on-failure` MUST 保持 63+N/63+N PASS（N = 新增测试数）

#### Scenario: HAL ops 添加严格走 ADR 流程

- **WHEN** 1.4 集成 KFD 时发现 KFD 实际调用 `iommu_*` 或 `mmu_notifier_*` 内核 API
- **THEN** 实施者 MUST NOT 在 1.4 commit 中"顺手"添加 HAL ops
- **AND** 实施者 MUST 创建独立 `hal-ops-extension-kfd` ADR（按 [ADR-035](../../docs/00_adr/adr-035-governance-policy.md) Rule 3）
- **AND** 实施者 MUST 创建独立 OpenSpec change（如 `hal-iommu-ops-extension` / `hal-uvm-ops-extension`）
- **AND** 每个新 HAL op MUST 对应一个独立 commit

#### Scenario: worktree 隔离实施

- **WHEN** 实施者开始 1.4 代码移植
- **THEN** 实施者 MUST 使用 `superpowers/using-git-worktrees` skill 创建 `stage-1.4-kfd-portability` worktree
- **AND** 所有代码改动 MUST 在 worktree 中进行（不在 main 直接修改）
- **AND** 合并前 MUST rebase main + 提 PR + review + merge（遵守项目 PR 流程）

#### Scenario: 移植报告 + Stage 1 closeout

- **WHEN** 5 个 .c 移植 + 5 个 ioctl 跑通 + 回归测试全绿
- **THEN** 实施者 MUST 编写 `docs/05-advanced/kfd-portability-report.md`（含 warnings 数量 / 错误码一致性 / 决策点 / 后续改进建议）
- **AND** 实施者 MUST 更新 SSOT `docs/02_architecture/post-refactor-architecture.md §1.10` 标注 Stage 1 完成
- **AND** 实施者 MUST 更新路线图状态（`stage-1-kernel-emu.md` 顶部 + `README.md` 阶段 1 行）
- **AND** 实施者 MUST 归档 OpenSpec change：`openspec archive 2026-07-04-stage-1-4-kfd-portability`
- **AND** 实施者 MUST 跑 `tools/docs-audit.sh --strict` 通过

### Requirement: KFD 5 个 ioctl 编号与 KFD API 1:1 映射

The system MUST maintain 1:1 mapping between 5 个 System C IOCTL 编号（已由 1.2 阶段预留）与 Linux KFD ioctl：

- `GPU_IOCTL_GET_PROCESS_APERTURE` (0x44) MUST map to `AMDKFD_IOC_GET_PROCESS_APERTURES_NEW` (0x14)
- `GPU_IOCTL_CREATE_QUEUE` (0x40) MUST map to `AMDKFD_IOC_CREATE_QUEUE` (0x02)，结构体 MUST 扩展 KFD-compat 字段在末尾
- `GPU_IOCTL_UPDATE_QUEUE` (0x45) MUST map to `AMDKFD_IOC_UPDATE_QUEUE` (0x07)
- `GPU_IOCTL_MAP_MEMORY` (0x46) MUST map to `AMDKFD_IOC_MAP_MEMORY_TO_GPU` (0x16)
- `GPU_IOCTL_UNMAP_MEMORY` (0x47) MUST map to `AMDKFD_IOC_UNMAP_MEMORY_FROM_GPU` (0x18)

#### Scenario: KFD 1:1 映射验证

- **WHEN** 用户拷贝 KFD 真实驱动代码（含 `AMDKFD_IOC_*` 调用）到 UsrLinuxEmu
- **THEN** 用户 MUST 仅调整宏名称（`AMDKFD_IOC_GET_PROCESS_APERTURES_NEW` → `GPU_IOCTL_GET_PROCESS_APERTURE` 等）
- **AND** ioctl 语义 MUST 保持一致（参数 / 返回值 / 错误码）
- **AND** `plugins/gpu_driver/shared/gpu_ioctl.h` MUST 包含完整的 5 个 KFD ioctl 编号定义 + 注释引用对应的 KFD ioctl

### Requirement: 回归测试前置（用户决策 3）

The system MUST 在 1.4 实施前跑 3 个关键回归测试，确保 1.2/1.3 G1-G4 边界契约无 regression：

- `test_drm_kfd_handlers_standalone` MUST 全绿（KFD handler dispatch 验证）
- `test_uvm_drm_lifecycle_standalone` MUST 全绿（1.2/1.3 G1-G4 边界契约验证）
- KFD 单文件 PoC（`kfd_queue.c`）MUST 仍编译通过（1.2 阶段 commit `c42e60e`）

#### Scenario: 回归测试失败处理

- **WHEN** 回归测试（LC3）任一失败
- **THEN** 实施者 MUST 优先修复 1.2/1.3 regression
- **AND** 实施者 MUST 暂缓 1.4 实施（不在失败的 regression 上继续推进）
- **AND** 修复 MUST 走标准 commit 流程 + 记录在 `kfd-portability-report.md` 中作为"1.4 准备阶段发现"