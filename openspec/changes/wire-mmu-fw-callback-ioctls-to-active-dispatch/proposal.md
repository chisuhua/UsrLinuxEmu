## Why

System C IOCTL 端到端测试完备性审计（2026-08-02） 发现 `wire-mmu-fw-callback-ioctls-to-active-dispatch` 的能力缺口 — 活跃派发表 + handler 与 ABI 头不同步，导致运行时 `ioctl()` 返回 `-EINVAL`。

**架构依据**:
- **ADR-036（3 区分架构）** — `GpgpuDevice` 是 ② 可移植驱动层唯一入口；活跃派发表是权威派发表（issue #11 kernel SHARED 修复后 VFS 单例化）
- **ADR-018（drv/sim 分离）** — KFD callback 应由 drv 层 handler 转发到 sim/bridge（`kfd_sim_*`），不直接跨层
- **ADR-023（HAL 接口契约）** — `GpgpuDevice::ioctl` 是 ② 内部实现；handler 调 sim 经 `kfd_sim_bridge`，不需经过 HAL fn-ptr（同 0x40-0x47 KFD handler 模式）
- **ADR-059（KFD 多文件集成）** — Phase B 已交付 KFD 6 模块；0x02/0x03 是 KFD 层 MMU notifier + firmware hot-load 入口，必须可达

**Why Now**: P0 优先级 — `wire-mmu-fw-callback-ioctls-to-active-dispatch` 当前处于「声明存在但运行时不可达」的不一致状态，会直接挂起 KFD 集成 + E2E 测试扩展。修复该漂移是 IOCTL 测试完备性审计 (2026-08-02) 中识别的最高优先事项之一。

**现状摘录**:
- `plugins/gpu_driver/drv/gpgpu_device.h:26` `kNumIoctls = 36`（应为 38）
  - `plugins/gpu_driver/drv/gpgpu_device.cpp:96-137` 派发表缺 `GPU_IOCTL_REGISTER_MMU_EVENT_CB` / `GPU_IOCTL_REGISTER_FIRMWARE_CB`
  - `plugins/gpu_driver/drv/gpu_drm_driver.cpp:700-741` 死代码 DRM 表含 0x02/0x03 handler（line 707-708），无运行时消费者
  - handler 已实现：`gpu_ioctl_register_mmu_cb` → `kfd_sim_register_mmu_cb`（`kfd_sim_bridge.cpp:204`）；`gpu_ioctl_register_firmware_cb` → `kfd_sim_register_firmware_cb`（`kfd_sim_bridge.cpp:231`）
  - 经 Stage 1.4 Tier-2（2026-07-05 commit `6a7f4ab`）完成升级，但活跃派发表未同步
  - 通过 `/dev/gpgpu0` 发起的 0x02/0x03 当前返回


## What Changes

- `plugins/gpu_driver/drv/gpgpu_device.h:26` `kNumIoctls` 由 36 改为 38
  - `plugins/gpu_driver/drv/gpgpu_device.cpp:96-137` `getIoctlTablePtr()` 静态表新增 2 项：
    - `GPU_IOCTL_REGISTER_MMU_EVENT_CB` → `handleRegisterMMUCB`
    - `GPU_IOCTL_REGISTER_FIRMWARE_CB` → `handleRegisterFirmwareCB`
  - `GpgpuDevice` 私有新增 2 个 `handle*(void*)` 转发函数，逻辑等价于 `gpu_ioctl_register_mmu_cb` / `gpu_ioctl_register_firmware_cb`（直接调 `kfd_sim_register_mmu_cb` / `kfd_sim_register_firmware_cb`）
  - 新增 CTest-registered 端到端测试 `test_register_cb_ioctl_standalone.cpp`：
    - plugin 加载 + VFS open + `fops->ioctl`
    - 验证 sim 侧 MMU notifier / firmware 回调注册表变更
    - 通过 sim 触发回调，测试侧收到并断言回调计数 + 入参
  - AGENTS.md / README 中 IOCTL 列表确认 0x02/0x03 现为活跃命令
- (Not in scope: DRM 表（`gpu_drm_driver.cpp:700-741`）删除或接活（独立 follow-up 提案）)
  - (Not in scope: `gpu_ioctl_stub` / `STUB_HANDLER` 死宏清理（随 DRM 表清理提案）)
  - (Not in scope: `kfd_dispatch.c` 0x40-0x47 脚手架接活（ADR-059 KFD 集成未来阶段）)
  - (Not in scope: `/dev/dri/renderD128` 等 render node 注册（ADR-037 未来阶段）)
  - (Not in scope: TaskRunner 侧 API 变更（ABI 兼容，仅活跃路径新增入口）)

## Capabilities

### New Capabilities

- `dispatch-consistency`: System C IOCTL 端到端测试完备性审计（2026-08-02） — 第一段实施

### Modified Capabilities

`ioctl-dispatch-consistency`: System C IOCTL 端到端测试完备性审计（2026-08-02） 中识别的能力缺口
  - 修复方向: `plugins/gpu_driver/drv/gpgpu_device.h:26` `kNumIoctls` 由 36 改为 38

## Impact

- `test_register_cb_ioctl_standalone.cpp`

## Dependencies (out of scope)

本提案不修改 ABI 头（`plugins/gpu_driver/shared/gpu_ioctl.h`），仅活跃派发表变化。
