# kfd-tier2-runtime-penetration Specification

## Purpose
TBD - created by archiving change stage-1-4-tier2-kfd-integration. Update Purpose after archive.
## Requirements
### Requirement: 9 个 STUB_HANDLER 升级为真实运行时行为

The system SHALL upgrade 9 个 Tier-1 STUB_HANDLER（`gpu_ioctl_register_mmu_cb` / `gpu_ioctl_register_firmware_cb` / `gpu_ioctl_create_va_space` / `gpu_ioctl_destroy_va_space` / `gpu_ioctl_register_gpu` / `gpu_ioctl_create_queue` / `gpu_ioctl_destroy_queue` / `gpu_ioctl_map_queue_ring` / `gpu_ioctl_query_queue`）为真实运行时行为，基于现有 GpgpuDevice 既有方法与 sim 原语，**不引入新 HAL ops**（按 ADR-027 + ADR-035）。

#### Scenario: gpu_ioctl_register_mmu_cb 注册 mmu_notifier callback

- **WHEN** KFD 驱动调用 `ioctl(fd, GPU_IOCTL_REGISTER_MMU_CB, &args)` 注册 mmu callback
- **THEN** system MUST 调用 `mmu_interval_notifier_register` 完成注册
- **AND** 后续用户态 munmap MUST 触发 callback 调用（详见 `mmu-notifier-callback-body` spec）
- **AND** 测试 `tests/test_register_mmu_cb_runtime_standalone` MUST 覆盖 happy path（注册成功）+ error path（重复注册返回 `-EALREADY`）

#### Scenario: gpu_ioctl_register_firmware_cb 注册 firmware callback

- **WHEN** KFD 驱动调用 `ioctl(fd, GPU_IOCTL_REGISTER_FIRMWARE_CB, &args)` 注册 firmware callback
- **THEN** system MUST 调用 `fops->register_fw_cb` 占位实现（**不实际加载 firmware**）
- **AND** 测试 `tests/test_register_firmware_cb_runtime_standalone` MUST 覆盖 happy path + error path

#### Scenario: gpu_ioctl_create_va_space 桥接 GpgpuDevice::create_va_space

- **WHEN** KFD 驱动调用 `ioctl(fd, GPU_IOCTL_CREATE_VA_SPACE, &args)` 创建 VA Space
- **THEN** system MUST 调 `gpgpu_device_->create_va_space(args)` 既有实现
- **AND** args->va_space_handle MUST 被填入有效句柄
- **AND** 测试 MUST 覆盖 happy path（创建成功）+ error path（参数非法返回 `-EINVAL`）

#### Scenario: gpu_ioctl_destroy_va_space 桥接 GpgpuDevice::destroy_va_space

- **WHEN** KFD 驱动调用 `ioctl(fd, GPU_IOCTL_DESTROY_VA_SPACE, &args)` 销毁 VA Space
- **THEN** system MUST 调 `gpgpu_device_->destroy_va_space(args)` 既有实现
- **AND** 测试 MUST 覆盖 happy path（销毁成功）+ error path（句柄非法返回 `-EINVAL`）
- **AND** G1 边界契约 MUST 保持（`drm_device` 生命周期 = `GpgpuDevice` 生命周期，详见 `drm-subset` spec G1）

#### Scenario: gpu_ioctl_register_gpu 注册 GPU device

- **WHEN** KFD 驱动调用 `ioctl(fd, GPU_IOCTL_REGISTER_GPU, &args)` 注册 GPU
- **THEN** system MUST 调 `gpgpu_device_->register_gpu(args)` 既有实现（一次性）
- **AND** 测试 MUST 覆盖 happy path + error path（重复注册返回 `-EALREADY`）

#### Scenario: gpu_ioctl_create_queue 桥接 GpgpuDevice::create_queue

- **WHEN** KFD 驱动调用 `ioctl(fd, GPU_IOCTL_CREATE_QUEUE, &args)` 创建 Queue（**0x40 KFD-compat 扩展**）
- **THEN** system MUST 调 `gpgpu_device_->create_queue(args)` 既有实现
- **AND** args->queue_handle MUST 被填入有效句柄
- **AND** 测试 MUST 覆盖 happy path + error path

#### Scenario: gpu_ioctl_destroy_queue 桥接 GpgpuDevice::destroy_queue

- **WHEN** KFD 驱动调用 `ioctl(fd, GPU_IOCTL_DESTROY_QUEUE, &args)` 销毁 Queue
- **THEN** system MUST 调 `gpgpu_device_->destroy_queue(args)` 既有实现
- **AND** 测试 MUST 覆盖 happy path + error path

#### Scenario: gpu_ioctl_map_queue_ring 映射 ring buffer

- **WHEN** KFD 驱动调用 `ioctl(fd, GPU_IOCTL_MAP_QUEUE_RING, &args)` 映射 ring buffer 到 user-space
- **THEN** system MUST 完成 `mmap` 或 `dma_buf_mmap` 映射
- **AND** args->ring_addr / args->doorbell_offset MUST 被填入有效地址
- **AND** 测试 MUST 覆盖 happy path + error path

#### Scenario: gpu_ioctl_query_queue 查询 queue 状态

- **WHEN** KFD 驱动调用 `ioctl(fd, GPU_IOCTL_QUERY_QUEUE, &args)` 查询 queue 状态
- **THEN** system MUST 调 `gpgpu_device_->query_queue(args)` 既有实现
- **AND** args MUST 填入 queue 当前状态（pending / running / completed）
- **AND** 测试 MUST 覆盖 happy path + error path

### Requirement: Tier-2 升级后所有 runtime test 必须全绿

The system SHALL 在 Tier-2 完成后，所有新增 runtime test（9 个 STUB 各 1 个 + mmu_notifier callback test + IOTLB flush test）全绿，且**不破坏** Tier-1 回归测试（`test_drm_kfd_handlers_standalone` + `test_uvm_drm_lifecycle_standalone` G1-G4 + 5 个 stage-1 核心测试）。

#### Scenario: Tier-2 完成时全量 ctest 通过

- **WHEN** Tier-2 全部 STUB 升级 + mmu_notifier callback + IOTLB flush 实施完毕
- **THEN** `ctest --test-dir build --output-on-failure` MUST 通过，新增 runtime test 全绿
- **AND** Tier-1 测试 MUST 保持全绿（无 regression）
- **AND** G1-G4 边界契约 MUST 保持（`tests/test_uvm_drm_lifecycle_standalone` 全验证）

### Requirement: 诚实标记替换

The system SHALL 在 Tier-2 升级完成后，handler 注释从 `STUB_HANDLER` 宏改为显式函数体（含 "Tier-2 penetrated" 注释 + 引用 boundary §3.x），且 `docs/05-advanced/kfd-portability-boundary.md` Tier-2 §3.x 状态从 "Stub / Logging / TODO" 改为 "Penetrated" + 完成时间戳。

#### Scenario: handler 注释替换

- **WHEN** Tier-2 STUB 升级完成
- **THEN** handler 注释 MUST 替换为 `// Tier-2 penetrated: [date] - references [boundary §3.x]` 格式
- **AND** "deferred to Stage 1.4" / "stub" 旧标记 MUST 删除（**不抹平**—— 替换为穿透标记）

#### Scenario: boundary 文档同步

- **WHEN** Tier-2 全部完成
- **THEN** `docs/05-advanced/kfd-portability-boundary.md` MUST 更新 Tier-2 §3.x 状态
- **AND** 新增 `docs/05-advanced/tier2-runtime-penetration-report.md` 记录每个 handler 演进

