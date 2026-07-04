# mmu-notifier-callback-body Specification

## Purpose
TBD - created by archiving change stage-1-4-tier2-kfd-integration. Update Purpose after archive.
## Requirements
### Requirement: mmu_notifier callback body 完整化

The system SHALL 完整化 `mmu_interval_notifier` callback body，**不引入新 sim 原语**（沿用 1.3 阶段已暴露的 `sim_pfh_*` / `sim_pm_*` 10 个 C 接口），覆盖从用户态 munmap 到 sim 原语触发的完整调用图。

#### Scenario: mmu_interval_notifier_register 注册 callback

- **WHEN** KFD 驱动调用 `ioctl(fd, GPU_IOCTL_REGISTER_MMU_CB, &args)`（前置：spec `kfd-tier2-runtime-penetration` gpu_ioctl_register_mmu_cb）
- **THEN** system MUST 调 `mmu_interval_notifier_register(mm, &cb, args)` 完成注册
- **AND** 注册成功后，callback 函数指针 MUST 在 `mmu_interval_notifier.subscribers` 列表中

#### Scenario: user-space munmap 触发 callback

- **WHEN** 用户态进程 munmap 共享 VMA（mmu_interval_notifier 监控范围）
- **THEN** kernel MUST 调 `mmu_interval_notifier_invalidate()` 遍历 subscribers
- **AND** 每个 subscriber callback MUST 被触发
- **AND** callback MUST 调用 `sim_pfh_inject_fault(addr, &pfn)` 触发 sim page fault handler
- **AND** callback MUST 调用 `sim_pm_migrate_to_system(addr, dst, size)` 触发 sim page migration
- **AND** 测试 `tests/test_mmu_notifier_callback_runtime_standalone` MUST 验证 happy path（munmap → callback → sim 原语触发）+ error path（未注册 callback 时不触发）

#### Scenario: callback body 不破坏 G1-G4 边界契约

- **WHEN** mmu_notifier callback 触发 sim 原语
- **THEN** `tests/test_uvm_drm_lifecycle_standalone` MUST 仍全绿（G1-G4 边界契约）
- **AND** G1（`drm_device` 生命周期）MUST 保持
- **AND** G2（BO 引用计数）MUST 保持
- **AND** G3（prime 释放顺序）MUST 保持
- **AND** G4（fence 触发时机）MUST 保持

#### Scenario: callback 错误处理

- **WHEN** callback 调用过程中遇到错误（如 sim 原语返回 -ENOMEM）
- **THEN** callback MUST 返回错误码给 `mmu_interval_notifier_invalidate`
- **AND** 系统 MUST 正确传播错误（**不吞错**）
- **AND** 测试 MUST 覆盖 error path（sim 原语失败时 callback 优雅返回错误）

