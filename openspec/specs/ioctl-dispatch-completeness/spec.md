# ioctl-dispatch-completeness Specification

## Purpose
TBD - created by archiving change wire-mmu-fw-callback-ioctls-to-active-dispatch. Update Purpose after archive.
## Requirements
### Requirement: Active ioctl dispatch table completeness

The `wire-mmu-fw-callback-ioctls-to-active-dispatch` change MUST ensure the contract in proposal.md is observable from `/dev/gpgpu0` through the plugin path. It applies to functional changes in category `core-impl`.

Architectural references:
- **ADR-036（3 区分架构）** — `GpgpuDevice` 是 ② 可移植驱动层唯一入口；活跃派发表是权威派发表（issue #11 kernel SHARED 修复后 VFS 单例化）
- **ADR-018（drv/sim 分离）** — KFD callback 应由 drv 层 handler 转发到 sim/bridge（`kfd_sim_*`），不直接跨层
- **ADR-023（HAL 接口契约）** — `GpgpuDevice::ioctl` 是 ② 内部实现；handler 调 sim 经 `kfd_sim_bridge`，不需经过 HAL fn-ptr（同 0x40-0x47 KFD handler 模式）

#### Scenario: 1

- **GIVEN** plugin 已加载，`/dev/gpgpu0` 通过 VFS 打开，活跃派发表 kNumIoctls=38
- **WHEN** `ioctl(fd, GPU_IOCTL_REGISTER_MMU_EVENT_CB, &args)` 携带合法 mmu_event_cb 参数
- **THEN** 返回 0；`kfd_sim_bridge` 中 MMU notifier 注册表长度 +1；后续 `sim_mmu_invalidate_va(va, len)` 触发已注册回调，测试侧收到并验证回调计数 +1.

#### Scenario: 2

- **GIVEN** plugin 已加载，活跃派发表含 0x03
- **WHEN** `ioctl(fd, GPU_IOCTL_REGISTER_FIRMWARE_CB, &args)` 携带合法 firmware_load_cb 参数
- **THEN** 返回 0；`kfd_sim_bridge` 中 firmware 回调注册表长度 +1；通过 `sim_firmware_load(fw_id)` 触发回调，测试侧入参 `fw_id` 与入参一致.

#### Scenario: 3

- **GIVEN** 活跃派发表 kNumIoctls=38
- **WHEN** `GpgpuDevice::dispatchCount()` 被调用
- **THEN** 返回 38（与 `kNumIoctls` 严格一致）.

#### Scenario: 4

- **GIVEN** 任意已注册 ioctl
- **WHEN** `argp == nullptr`（针对 0x02/0x03 测）
- **THEN** 返回 `-EINVAL`（与既有 0x40-0x47 KFD handler 同构）.

#### Scenario: 5

- **GIVEN** 活跃派发表 kNumIoctls=38
- **WHEN** `ioctl(fd, 0xDEADBEEF, &args)` 发送未声明的 request
- **THEN** 返回 `-EINVAL`（活跃派发表 fallback 行为不被新加入项影响）.

